/* Tries playing a game (deliberately not customized to any particular
   ROM) using an objective function learned by learnfun. 

   This is the second iteration. It attempts to fix a problem with the
   first (playfun-nobacktrack) which is that although the objective
   functions all obviously tank when the player dies, the algorithm
   can't see far enough ahead (or something ..?) to avoid taking a
   path with such an awful score. This one works by keeping a set of
   possible futures and scoring an immediate step based on how it does
   in that set of futures. It is also the first version that supports
   MARIONET, for utilizing an arbitrary number of CPUs. This version
   still does not backtrack.

   We also keep track of a range of values for the objective functions
   so that we have some sense of their absolute values, not just their
   relative ones.
*/

#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "tasbot.h"

#include "fceu/utils/md5.h"
#include "config.h"
#include "fceu/driver.h"
#include "fceu/drivers/common/args.h"
#include "fceu/state.h"
#include "basis-util.h"
#include "emulator.h"
#include "fceu/fceu.h"
#include "fceu/types.h"
#include "simplefm2.h"
#include "weighted-objectives.h"
#include "motifs.h"
#include "../cc-lib/arcfour.h"
#include "util.h"
#include "../cc-lib/textsvg.h"
#include "game.h"

#ifdef MARIONET
#include "SDL.h"
#include "SDL_net.h"
#include "marionet.pb.h"
#include "netutil.h"
#endif

// This is the factor that determines how quickly a motif changes
// weight. When a motif is chosen because it yields the best future,
// we check its immediate effect on the state (normalized); if an
// increase, then we divide its weight by alpha. If a decrease, then
// we multiply. Should be a value in (0, 1] but usually around 0.8.
#define ALPHA 0.8

static const double WIDTH = 1024.0;
static const double HEIGHT = 1024.0;

struct Scoredist {
  Scoredist() : startframe(0), chosen_idx() {}
  explicit Scoredist(int startframe) : startframe(startframe),
				       chosen_idx(0) {}
  int startframe;
  vector<double> immediates;
  vector<double> positives;
  vector<double> negatives;
  vector<double> norms;
  int chosen_idx;
};

static void SaveDistributionSVG(const vector<Scoredist> &dists,
				const string &filename) {
  // Add slop for radii.
  string out = TextSVG::Header(WIDTH + 12, HEIGHT + 12);
  
  // immediates, positives, negatives all are in same value space
  double maxval = 0.0;
  for (int i = 0; i < dists.size(); i++) {
    const Scoredist &dist = dists[i];
    maxval =
      VectorMax(VectorMax(VectorMax(maxval, dist.negatives),
			  dist.positives),
		dist.immediates);
  }
  
  int totalframes = dists.back().startframe;
  
  for (int i = 0; i < dists.size(); i++) {
    const Scoredist &dist = dists[i];
    double xf = dist.startframe / (double)totalframes;
    out += DrawDots(WIDTH, HEIGHT,
		    "#33A", xf, dist.immediates, maxval, dist.chosen_idx);
    out += DrawDots(WIDTH, HEIGHT,
		    "#090", xf, dist.positives, maxval, dist.chosen_idx);
    out += DrawDots(WIDTH, HEIGHT,
		    "#A33", xf, dist.negatives, maxval, dist.chosen_idx);
    // out += DrawDots("#000", xf, dist.norms, 1.0, dist.chosen_idx);
  }

  // XXX args?
  out += SVGTickmarks(WIDTH, totalframes, 50.0, 20.0, 12.0);

  out += TextSVG::Footer();
  Util::WriteFile(filename, out);
  printf("Wrote distributions to %s.\n", filename.c_str());
}

namespace {
struct Future {
  vector<uint8> inputs;
  bool weighted;
  int desired_length;
  // TODO
  int rounds_survived;
  Future() : weighted(true), desired_length(0), rounds_survived(0) {}
  Future(bool w, int d) : weighted(w), 
			  desired_length(d), 
			  rounds_survived(0) {}
};
}

static void SaveFuturesHTML(const vector<Future> &futures,
			    const string &filename) {
  string out;
  for (int i = 0; i < futures.size(); i++) {
    out += StringPrintf("<div>%d. len %d/%d. %s\n", i, 
			futures[i].inputs.size(),
			futures[i].desired_length,
			futures[i].weighted ? "weighted" : "random");
    for (int j = 0; j < futures[i].inputs.size(); j++) {
      out += SimpleFM2::InputToColorString(futures[i].inputs[j]);
    }
    out += "</div>\n";
  }
  Util::WriteFile(filename, out);
  printf("Wrote futures to %s\n", filename.c_str());
}

struct PlayFun {
  PlayFun() : watermark(0), rc("playfun") {
    Emulator::Initialize(GAME ".nes");
    objectives = WeightedObjectives::LoadFromFile(GAME ".objectives");
    CHECK(objectives);
    fprintf(stderr, "Loaded %d objective functions\n", objectives->Size());

    motifs = Motifs::LoadFromFile(GAME ".motifs");
    CHECK(motifs);

    Emulator::ResetCache(100000, 10000);

    motifvec = motifs->AllMotifs();

    // PERF basis?

    solution = SimpleFM2::ReadInputs(MOVIE);

    size_t start = 0;
    bool saw_input = false;
    while (start < solution.size()) {
      Commit(solution[start]);
      watermark++;
      saw_input = saw_input || solution[start] != 0;
      if (start > FASTFORWARD && saw_input) break;
      start++;
    }

    CHECK(start > 0 && "Currently, there needs to be at least "
	  "one observation to score.");

    printf("Skipped %ld frames until first keypress/ffwd.\n", start);
  }

  void Commit(uint8 input) {
    Emulator::CachingStep(input);
    movie.push_back(input);
    // PERF
    vector<uint8> mem;
    Emulator::GetMemory(&mem);
    memories.push_back(mem);
    objectives->Observe(mem);
  }

  
  // Fairly unprincipled attempt.

  // All of these are the same size:

  // PERF. Shouldn't really save every memory, but
  // we're using it for drawing SVG for now...
  // The nth element contains the memory after playing
  // the nth input in the movie. The initial memory
  // is not stored.
  vector< vector<uint8> > memories;
  // Contains the movie we record.
  vector<uint8> movie;

  // Index below which we should not backtrack (because it
  // contains pre-game menu stuff, for example).
  int watermark;

  // Number of real futures to push forward.
  static const int NFUTURES = 24;

  // Number of futures that should be generated from weighted
  // motifs as opposed to totally random.
  static const int NWEIGHTEDFUTURES = 28;

  // Drop this many of the worst futures.
  static const int DROPFUTURES = 4;
  // TODO: copy some of the best futures over bad futures,
  // randomizing the tails.

  // Number of inputs in each future.
  static const int MINFUTURELENGTH = 50;
  static const int MAXFUTURELENGTH = 600;

  // DESTROYS THE STATE
  double ScoreByFuture(const Future &future,
		       const vector<uint8> &base_memory,
		       const vector<uint8> &base_state) {
    for (int i = 0; i < future.inputs.size(); i++) {
      Emulator::CachingStep(future.inputs[i]);
    }

    vector<uint8> future_memory;
    Emulator::GetMemory(&future_memory);

    return objectives->Evaluate(base_memory, future_memory);
  }

  #ifdef MARIONET
  static void ReadBytesFromProto(const string &pf, vector<uint8> *bytes) {
    // PERF iterators.
    for (int i = 0; i < pf.size(); i++) {
      bytes->push_back(pf[i]);
    }
  }

  void Helper(int port) {
    SingleServer server(port);

    for (;;) {
      server.Listen();

      fprintf(stderr, "[%d] Connection from %s\n", 
	      port,
	      server.PeerString().c_str());

      PlayFunRequest req;
      if (server.ReadProto(&req)) {

	// fprintf(stderr, "Request: %s\n", req.DebugString().c_str());
	// abort(); // XXX

	vector<uint8> next, current_state;
	ReadBytesFromProto(req.current_state(), &current_state);
	ReadBytesFromProto(req.next(), &next);
	vector<Future> futures;
	for (int i = 0; i < req.futures_size(); i++) {
	  Future f;
	  ReadBytesFromProto(req.futures(i).inputs(), &f.inputs);
	  futures.push_back(f);
	}

	double immediate_score, best_future_score, worst_future_score,
	  futures_score;
	vector<double> futurescores(futures.size(), 0.0);

	// Do the work.
	InnerLoop(next, futures, &current_state,
		  &immediate_score, &best_future_score,
		  &worst_future_score, &futures_score,
		  &futurescores);

	PlayFunResponse res;
	res.set_immediate_score(immediate_score);
	res.set_best_future_score(best_future_score);
	res.set_worst_future_score(worst_future_score);
	res.set_futures_score(futures_score);
	for (int i = 0; i < futurescores.size(); i++) {
	  res.add_futurescores(futurescores[i]);
	}

	// fprintf(stderr, "Result: %s\n", res.DebugString().c_str());

	if (!server.WriteProto(res)) {
	  fprintf(stderr, "Failed to send result...\n");
	  // But just keep going.
	}
      } else {
	fprintf(stderr, "Failed to read request...\n");
      }
      server.Hangup();
    }
  }
  #endif

  void InnerLoop(const vector<uint8> &next,
		 const vector<Future> &futures_orig, 
		 vector<uint8> *current_state, 
		 double *immediate_score,
		 double *best_future_score,
		 double *worst_future_score,
		 double *futures_score,
		 vector<double> *futurescores) {

    // Make copy so we can make fake futures.
    vector<Future> futures = futures_orig;

    Emulator::LoadUncompressed(current_state);

    vector<uint8> current_memory;
    Emulator::GetMemory(&current_memory);

    // Take steps.
    for (int j = 0; j < next.size(); j++)
      Emulator::CachingStep(next[j]);

    vector<uint8> new_memory;
    Emulator::GetMemory(&new_memory);

    vector<uint8> new_state;
    Emulator::SaveUncompressed(&new_state);

    *immediate_score = objectives->Evaluate(current_memory, new_memory);

    // PERF unused except for drawing
    // XXX probably shouldn't do this since it depends on local
    // storage.
    // double norm_score = objectives->GetNormalizedValue(new_memory);

    *best_future_score = -9999999.0;
    *worst_future_score = 9999999.0;

    // Synthetic future where we keep holding the last
    // button pressed.
    // static const int NUM_FAKE_FUTURES = 1;
    int total_future_length = 0;
    for (int i = 0; i < futures.size(); i++) {
      total_future_length += futures[i].inputs.size();
    }
    int average_future_length = (int)((double)total_future_length /
				      (double)futures.size());

    Future fakefuture_hold;
    for (int z = 0; z < average_future_length; z++) {
      fakefuture_hold.inputs.push_back(next.back());
    }
    futures.push_back(fakefuture_hold);

    *futures_score = 0.0;
    for (int f = 0; f < futures.size(); f++) {
      if (f != 0) Emulator::LoadUncompressed(&new_state);
      const double future_score =
	ScoreByFuture(futures[f], new_memory, new_state);
      // Only count real futures.
      if (f < futures_orig.size()) {
	(*futurescores)[f] += future_score;
      }
      *futures_score += future_score;
      if (future_score > *best_future_score)
	*best_future_score = future_score;
      if (future_score < *worst_future_score)
	*worst_future_score = future_score;
    }
    
    // Discards the copy.
    // futures.resize(futures.size() - NUM_FAKE_FUTURES);
  }

  // The parallel step. We either run it in serial locally
  // (without MARIONET) or as jobs on helpers, via TCP.
  void ParallelStep(const vector< vector<uint8> > &nexts,
		    const vector<Future> &futures,
		    // morally const
		    vector<uint8> *current_state,
		    const vector<uint8> &current_memory,
		    vector<double> *futuretotals,
		    int *best_next_idx) {
    uint64 start = time(NULL);
    fprintf(stderr, "Parallel step with %d nexts, %d futures.\n",
	    nexts.size(), futures.size());

    double best_score = 0.0;
    Scoredist distribution(movie.size());

#ifdef MARIONET
    // One piece of work per request.
    vector<PlayFunRequest> requests;
    requests.resize(nexts.size());
    for (int i = 0; i < nexts.size(); i++) {
      PlayFunRequest *req = &requests[i];
      req->set_current_state(&((*current_state)[0]), current_state->size());
      req->set_next(&nexts[i][0], nexts[i].size());
      for (int f = 0; f < futures.size(); f++) {
	FutureProto *fp = req->add_futures();
	fp->set_inputs(&futures[f].inputs[0],
		       futures[f].inputs.size());
      }
      // if (!i) fprintf(stderr, "REQ: %s\n", req->DebugString().c_str());
    }
    
    GetAnswers<PlayFunRequest, PlayFunResponse> getanswers(ports_, requests);
    getanswers.Loop();

    fprintf(stderr, "GOT ANSWERS.\n");
    const vector<GetAnswers<PlayFunRequest, PlayFunResponse>::Work> &work =
      getanswers.GetWork();

    for (int i = 0; i < work.size(); i++) {
      const PlayFunResponse &res = work[i].res;
      for (int f = 0; f < res.futurescores_size(); f++) {
	CHECK(f <= futuretotals->size());
	(*futuretotals)[f] += res.futurescores(f);
      }

      const double score = res.immediate_score() + res.futures_score();

      distribution.immediates.push_back(res.immediate_score());
      distribution.positives.push_back(res.futures_score());	
      distribution.negatives.push_back(res.worst_future_score());
      // XXX norm score is disabled because it can't be
      // computed in a distributed fashion.
      distribution.norms.push_back(0);

      if (score > best_score) {
	best_score = score;
	*best_next_idx = i;
      }
    }
    
#else
    // Local version.
    for (int i = 0; i < nexts.size(); i++) {
      double immediate_score, best_future_score, worst_future_score, 
	futures_score;
      vector<double> futurescores(NFUTURES, 0.0);
      InnerLoop(nexts[i],
		futures, 
		current_state,
		&immediate_score,
		&best_future_score,
		&worst_future_score,
		&futures_score,
		&futurescores);

      for (int f = 0; f < futurescores.size(); f++) {
	(*futuretotals)[f] += futurescores[f];
      }

      double score = immediate_score + futures_score;

      distribution.immediates.push_back(immediate_score);
      distribution.positives.push_back(futures_score);	
      distribution.negatives.push_back(worst_future_score);
      // XXX norm score is disabled because it can't be
      // computed in a distributed fashion.
      distribution.norms.push_back(0);

      if (score > best_score) {
	best_score = score;
	*best_next_idx = i;
      }
    }
#endif
    distribution.chosen_idx = *best_next_idx;
    distributions.push_back(distribution);

    uint64 end = time(NULL);
    fprintf(stderr, "Parallel step took %d seconds.\n", (int)(end - start));
  }

  // XXX
  vector<int> ports_;

  // Main loop for the master, or when compiled without MARIONET support.
  // Helpers is an array of helper ports, which is ignored unless MARIONET
  // is active.
  void Master(const vector<int> &helpers) {
    // XXX
    ports_ = helpers;

    vector<uint8> current_state;
    vector<uint8> current_memory;

    vector< vector<uint8> > nexts = motifvec;

    // This version of the algorithm looks like this. At some point in
    // time, we have the set of motifs we might play next. We'll
    // evaluate all of those. We also have a series of possible
    // futures that we're considering. At each step we play our
    // candidate motif (ignoring that many steps as in the future --
    // but note that for each future, there should be some motif that
    // matches its head). Then we play all the futures. The motif with the
    // best overall score is chosen; we chop the head off each future,
    // and add a random motif to its end.
    //
    // XXX recycling futures...
    vector<Future> futures;

    int64 iters = 0;
    for (;; iters++) {

      motifs->Checkpoint(movie.size());

      int num_currently_weighted = 0;
      for (int i = 0; i < futures.size(); i++) {
	if (futures[i].weighted) {
	  num_currently_weighted++;
	}
      }
      
      int num_to_weight = max(NWEIGHTEDFUTURES - num_currently_weighted, 0);
      #ifdef DEBUGFUTURES
      fprintf(stderr, "there are %d futures, %d cur weighted, %d need\n",
	      futures.size(), num_currently_weighted, num_to_weight);
      #endif
      while (futures.size() < NFUTURES) {
	// Keep the desired length around so that we only
	// resize the future if we drop it. Randomize between
	// MIN and MAX future lengths.
	int flength = MINFUTURELENGTH +
	  (int)
	  ((double)(MAXFUTURELENGTH - MINFUTURELENGTH) *
	   RandomDouble(&rc));

	if (num_to_weight > 0) {
	  futures.push_back(Future(true, flength));
	  num_to_weight--;
	} else {
	  futures.push_back(Future(false, flength));
	}
      }

      // Make sure we have enough futures with enough data in.
      // PERF: Should avoid creating exact duplicate futures.
      for (int i = 0; i < NFUTURES; i++) {
	while (futures[i].inputs.size() <
	       futures[i].desired_length) {
	  const vector<uint8> &m = 
	    futures[i].weighted ? 
	    motifs->RandomWeightedMotif() :
	    motifs->RandomMotif();
	  for (int x = 0; x < m.size(); x++) {
	    futures[i].inputs.push_back(m[x]);
	    if (futures[i].inputs.size() ==
		futures[i].desired_length) {
	      break;
	    }
	  }
	}
      }

      #ifdef DEBUGFUTURES
      for (int f = 0; f < futures.size(); f++) {
	fprintf(stderr, "%d. %s %d/%d: ...\n",
		f, futures[f].weighted ? "weighted" : "random",
		futures[f].inputs.size(),
		futures[f].desired_length);
      }
      #endif

      // Save our current state so we can try many different branches.
      Emulator::SaveUncompressed(&current_state);    
      Emulator::GetMemory(&current_memory);

      // XXX should be a weighted shuffle.
      // XXX does it even matter that they're shuffled any more?
      Shuffle(&nexts);

      // Total score across all motifs for each future.
      vector<double> futuretotals(NFUTURES, 0.0);

      // Most of the computation happens here.
      int best_next_idx;
      ParallelStep(nexts, futures,
		   &current_state, current_memory,
		   &futuretotals,
		   &best_next_idx);

      // Chop the head off each future.
      const int choplength = nexts[best_next_idx].size();
      for (int i = 0; i < futures.size(); i++) {
	vector<uint8> newf;
	for (int j = choplength; j < futures[i].inputs.size(); j++) {
	  newf.push_back(futures[i].inputs[j]);
	}
	futures[i].inputs.swap(newf);
      }

      // Discard the futures with the worst total.
      // They'll be replaced the next time around the loop.
      // PERF don't really need to make DROPFUTURES passes,
      // but there are not many futures and not many dropfutures.
      for (int t = 0; t < DROPFUTURES; t++) {
	CHECK(!futures.empty());
	CHECK(futures.size() <= futuretotals.size());
	double worst_total = futuretotals[0];
	int worst_idx = 0;
	for (int i = 1; i < futures.size(); i++) {
	  #ifdef DEBUGFUTURES
	  fprintf(stderr, "%d. %s %d/%d: %f\n",
		  i, futures[i].weighted ? "weighted" : "random",
		  futures[i].inputs.size(),
		  futures[i].desired_length,
		  futuretotals[i]);
	  #endif

	  if (worst_total < futuretotals[i]) {
	    worst_total = futuretotals[i];
	    worst_idx = i;
	  }
	}

	// Delete it by swapping.
	if (worst_idx != futures.size() - 1) {  
	  futures[worst_idx] = futures[futures.size() - 1];
	}
	futures.resize(futures.size() - 1);
      }

      // This is very likely to be cached now.
      Emulator::LoadUncompressed(&current_state);
      for (int j = 0; j < nexts[best_next_idx].size(); j++) {
	Commit(nexts[best_next_idx][j]);
      }

      // Now, if the motif we used was a local improvement to the
      // score, reweight it.
      {
	motifs->Pick(nexts[best_next_idx]);
	vector<uint8> new_memory;
	Emulator::GetMemory(&new_memory);
	double oldval = objectives->GetNormalizedValue(current_memory);
	double newval = objectives->GetNormalizedValue(new_memory);
	double *weight = motifs->GetWeightPtr(nexts[best_next_idx]);
	if (weight == NULL) {
	  printf(" * ERROR * Used a motif that doesn't exist?\n");
	} else {
	  if (newval > oldval) {
	    // Increases its weight.
	    *weight /= ALPHA;
	  } else {
	    // Decreases its weight.
	    *weight *= ALPHA;
	  }
	}
      }


      if (iters % 10 == 0) {
	SaveMovie();
	SaveQuickDiagnostics(futures);
	if (iters % 50 == 0) {
	  SaveDiagnostics(futures);
	}
      }
    }
  }

  void SaveMovie() {
    printf("                     - writing movie -\n");
    SimpleFM2::WriteInputs(GAME "-playfun-futures-progress.fm2",
			   GAME ".nes",
			   "base64:jjYwGG411HcjG/j9UOVM3Q==",
			   movie);
    Emulator::PrintCacheStats();
  }

  void SaveQuickDiagnostics(const vector<Future> &futures) {
    printf("                     - quick diagnostics -\n");
    SaveFuturesHTML(futures, GAME "-playfun-futures.html");
  }

  void SaveDiagnostics(const vector<Future> &futures) {
    printf("                     - slow diagnostics -\n");
    // This is now too expensive because the futures aren't cached
    // in this process.
    #if 0
    for (int i = 0; i < futures.size(); i++) {
      vector<uint8> fmovie = movie;
      for (int j = 0; j < futures[i].inputs.size(); j++) {
	fmovie.push_back(futures[i].inputs[j]);
	SimpleFM2::WriteInputs(StringPrintf(GAME "-playfun-future-%d.fm2",
					    i),
			       GAME ".nes",
			       "base64:jjYwGG411HcjG/j9UOVM3Q==",
			       fmovie);
      }
    }
    printf("Wrote %d movie(s).\n", futures.size() + 1);
    #endif
    SaveDistributionSVG(distributions, GAME "-playfun-scores.svg");
    objectives->SaveSVG(memories, GAME "-playfun-futures.svg");
    motifs->SaveHTML(GAME "-playfun-motifs.html");
    printf("                     (wrote)\n");
  }

  // For making SVG.
  vector<Scoredist> distributions;

  // Used to ffwd to gameplay.
  vector<uint8> solution;

  ArcFour rc;
  WeightedObjectives *objectives;
  Motifs *motifs;
  vector< vector<uint8> > motifvec;
};

/**
 * The main loop for the SDL.
 */
int main(int argc, char *argv[]) {
  #ifdef MARIONET
  fprintf(stderr, "Init SDL\n");

  /* Initialize SDL and network, if we're using it. */
  CHECK(SDL_Init(0) >= 0);
  CHECK(SDLNet_Init() >= 0);
  fprintf(stderr, "SDL initialized OK.\n");
  #endif

  PlayFun pf;

  #ifdef MARIONET
  if (argc >= 2) {
    if (0 == strcmp(argv[1], "--helper")) {
      if (argc < 3) {
	fprintf(stderr, "Need one port number after --helper.\n");
	abort();
      }
      int port = atoi(argv[2]);
      fprintf(stderr, "Starting helper on port %d...\n", port);
      pf.Helper(port);
      fprintf(stderr, "helper returned?\n");
    } else if (0 == strcmp(argv[1], "--master")) {
      vector<int> helpers;
      for (int i = 2; i < argc; i++) {
	int hp = atoi(argv[i]);
	if (!hp) {
	  fprintf(stderr, 
		  "Expected a series of helper ports after --master.\n");
	  abort();
	}
	helpers.push_back(hp);
      }
      pf.Master(helpers);
      fprintf(stderr, "master returned?\n");
    }
  } else {
    vector<int> empty;
    pf.Master(empty);
  }
  #else
  vector<int> nobody;
  pf.Master(nobody);
  #endif

  Emulator::Shutdown();

  // exit the infrastructure
  FCEUI_Kill();

  #ifdef MARIONET
  SDLNet_Quit();
  SDL_Quit();
  #endif
  return 0;
}
