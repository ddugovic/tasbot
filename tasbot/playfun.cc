/* Tries playing a game (deliberately not customized to any particular
   ROM) using an objective function learned by learnfun.

   This is the third iteration. It attempts to fix a problem where
   playfun-futures would get stuck in local maxima, like the overhang
   in Mario's world 1-2.
*/

#include <vector>
#include <string>
#include <set>
#include <cmath>

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "tasbot.h"

#include "config.h"
#include "basis-util.h"
#include "emulator.h"
#include "simplefm2.h"
#include "weighted-objectives.h"
#include "motifs.h"
#include "util.h"

#ifdef MARIONET
#include "marionet.pb.h"
#include "netutil.h"
#endif

// This is the factor that determines how quickly a motif changes
// weight. When a motif is chosen because it yields the best future,
// we check its immediate effect on the state (normalized); if an
// increase, then we divide its weight by alpha. If a decrease, then
// we multiply. Should be a value in (0, 1] but usually around 0.8.
#define MOTIF_ALPHA 0.8
// Largest fraction of the total weight that any motif is allowed to
// have when being reweighted up. We don't reweight down to the cap,
// but prevent it from going over. Also, this can be violated if one
// motif is at the max and another has its weight reduced, but still
// keeps motifs from getting weighted out of control.
#define MOTIF_MAX_FRAC 0.1
// Minimum fraction allowed when reweighting down. We don't decrease
// below this, but don't increase to meet the fraction, either.
#define MOTIF_MIN_FRAC 0.00001

struct Scoredist {
  Scoredist() : startframe(0), chosen_idx() {}
  explicit Scoredist(size_t startframe) : startframe(startframe),
					  chosen_idx(0) {}
  size_t startframe;
  vector<double> immediates;
  vector<double> positives;
  vector<double> negatives;
  vector<double> norms;
  size_t chosen_idx;
};

static void SaveDistributionSVG(const size_t totalframes,
				const vector<Scoredist> &dists,
				const string &filename) {
  const double SPAN = 50;
  const double WIDTH = totalframes * 2;
  const double HEIGHT = 768.0;

  // Add slop for radii.
  string out = TextSVG::Header(WIDTH + 12, HEIGHT + 12);

  // immediates, positives, negatives all are in same value space
  double minval = 1.0, maxval = 0.0;
  for (int i = 0; i < dists.size(); i++) {
    const Scoredist &dist = dists[i];
    minval =
      VectorMin(VectorMin(VectorMin(minval, dist.negatives),
			  dist.positives),
		dist.immediates);
    maxval =
      VectorMax(VectorMax(VectorMax(maxval, dist.negatives),
			  dist.positives),
		dist.immediates);
  }

  for (int i = 0; i < dists.size(); i++) {
    const Scoredist &dist = dists[i];
    double xf = dist.startframe / (double)totalframes;
    out += DrawDots(WIDTH, HEIGHT,
		    "#33A", xf, dist.immediates, minval, maxval, dist.chosen_idx);
    out += DrawDots(WIDTH, HEIGHT,
		    "#090", xf, dist.positives, minval, maxval, dist.chosen_idx);
    out += DrawDots(WIDTH, HEIGHT,
		    "#A33", xf, dist.negatives, minval, maxval, dist.chosen_idx);
    out += DrawDots(WIDTH, HEIGHT,
		    "#000", xf, dist.norms, minval, maxval, dist.chosen_idx);
  }

  // XXX args?
  out += SVGTickmarks(WIDTH, totalframes, SPAN, 20.0, 12.0);

  out += TextSVG::Footer();
  Util::WriteFile(filename, out);
  printf("Wrote distributions to %s.\n", filename.c_str());
}

// Is a namespace necessary?
// Why is there no playfun.h?
namespace {
struct Future {
  vector<uint8> inputs;
  bool weighted;
  size_t desired_length;
  // TODO
  int rounds_survived;
  bool is_mutant;
  Future() : weighted(true), desired_length(0), rounds_survived(0),
	     is_mutant(false) {}
  Future(bool w, int d) : weighted(w),
			  desired_length(d),
			  rounds_survived(0),
			  is_mutant(false) {}
};

// For backtracking.
struct Replacement {
  vector<uint8> inputs;
  double score;
  string method;
};
}  // namespace

static void SaveFuturesHTML(const vector<Future> &futures,
			    const string &filename) {
  string out;
  for (int i = 0; i < futures.size(); i++) {
    out += StringPrintf("<div>%d. len %zu/%zu. %s %s\n", i,
			futures[i].inputs.size(),
			futures[i].desired_length,
			futures[i].is_mutant ? "mutant" : "fresh",
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
  PlayFun(Config config) : config(config), watermark(0), log(NULL), rc("playfun") {
    Emulator::Initialize(config);
    objectives = WeightedObjectives::LoadFromFile((config.game+ ".objectives").c_str());
    CHECK(objectives);
    fprintf(stderr, "Loaded %zu objective functions\n", objectives->Size());

    motifs = Motifs::LoadFromFile((config.game+ ".motifs").c_str());
    CHECK(motifs);

    Emulator::ResetCache(100000, 10000);

    motifvec = motifs->AllMotifs();

    // PERF basis?

    solution = SimpleFM2::ReadInputs(config.movie.c_str());

    size_t start = 0;
    while (solution[start] == 0 && start < solution.size()) {
      Commit(solution[start], "warmup");
      watermark++;
      start++;
    }
    while (start < config.fastforward && start < solution.size()) {
      Commit(solution[start], "warmup");
      watermark++;
      start++;
    }

    CHECK(start > 0 && "Currently, there needs to be at least "
	  "one observation to score.");

    printf("Skipped %zu frames until first keypress/ffwd.\n", start);
  }

  // PERF. Shouldn't really save every memory, but
  // we're using it for drawing SVG for now. This saves one
  // in OBSERVE_EVERY memories, and isn't truncated when we
  // backtrack.
  vector< vector<uint8> > memories;

  // Contains the movie we record (partial solution).
  vector<uint8> movie;

  // Keeps savestates.
  struct Checkpoint {
    vector<uint8> save;
    // such that truncating movie to length movenum
    // produces the savestate.
    size_t movenum;
    Checkpoint(const vector<uint8> save, size_t movenum)
      : save(save), movenum(movenum) {}
    // For putting in containers.
    Checkpoint() : movenum(0) {}
  };
  vector<Checkpoint> checkpoints;

  // Index below which we should not backtrack (because it
  // contains pre-game menu stuff, for example).
  Config config;
  size_t watermark;

  // Number of real futures to push forward.
  // XXX the more the merrier! Made this small to test backtracking.
  static const int NFUTURES = 40;

  // Number of futures that should be generated from weighted
  // motifs as opposed to totally random.
  static const int NWEIGHTEDFUTURES = 35;

  // Drop this many of the worst futures and replace them with
  // totally new futures.
  static const int DROPFUTURES = 5;
  // Drop this many of the worst futures and replace them with
  // variants on the best future.
  static const int MUTATEFUTURES = 7;

  // Note that backfill motifs are not necessarily this length.
  static const int INPUTS_PER_NEXT = 10;

  // Number of inputs in each future.
  static const int MINFUTURELENGTH = 50;
  static const int MAXFUTURELENGTH = 800;

  static const bool TRY_BACKTRACK = true;
  // Make a checkpoint this often (number of inputs).
  static const int CHECKPOINT_EVERY = 100;
  // In inputs.
  static const int TRY_BACKTRACK_EVERY = 180;
  // In inputs.
  static const int MIN_BACKTRACK_DISTANCE = 300;

  // Observe the memory (for calibrating objectives and drawing
  // SVG) this often (number of inputs).
  static const int OBSERVE_EVERY = 10;
  // Save this often (number of inputs).
  static const int SAVE_EVERY = 5;

  // Should always be the same length as movie.
  vector<string> subtitles;

  void Commit(uint8 input, const string &message) {
    Emulator::CachingStep(input);
    movie.push_back(input);
    subtitles.push_back(message);
    if (movie.size() < watermark || movie.size() < config.fastforward)
      return;

    size_t inputs = movie.size() - config.fastforward;
    if (inputs % CHECKPOINT_EVERY == 0) {
      vector<uint8> savestate;
      Emulator::Save(&savestate);
      checkpoints.push_back(Checkpoint(savestate, movie.size()));
    }

    // PERF: This is very slow...
    if (inputs % OBSERVE_EVERY == 0) {
      vector<uint8> mem;
      Emulator::GetMemory(&mem);
      memories.push_back(mem);
      objectives->Observe(mem);
    }
  }

  void Rewind(size_t movenum) {
    // Is it possible / meaningful to rewind stuff like objectives
    // observations?
    CHECK(movenum >= 0);
    CHECK(movenum < movie.size());
    CHECK(movie.size() == subtitles.size());
    movie.resize(movenum);
    subtitles.resize(movenum);
    // Pop any checkpoints since movenum.
    while (!checkpoints.empty() &&
	   checkpoints.back().movenum > movenum) {
      checkpoints.pop_back();
    }
  }

  // DESTROYS THE STATE
  void ScoreByFuture(const Future &future,
		     const vector<uint8> &base_memory,
		     vector<uint8> *base_state,
		     double *positive_scores,
		     double *negative_scores,
		     double *integral_score) {
    vector<uint8> future_memory;
    double integral = ScoreIntegral(base_state, future.inputs, &future_memory);

    *integral_score = integral / future.inputs.size();
    *positive_scores = objectives->WeightedLess(base_memory, future_memory);
    // Note negation; WeightedLess always returns non-negative score.
    *negative_scores = -objectives->WeightedLess(future_memory, base_memory);
  }

  #ifdef MARIONET
  typedef ::google::protobuf::Message Message;
  static void ReadBytesFromProto(const string &pf, vector<uint8> *bytes) {
    // PERF iterators.
    for (int i = 0; i < pf.size(); i++) {
      bytes->push_back(pf[i]);
    }
  }

  void Helper(int port) {
    SingleServer server(port);

    fprintf(stderr, "[%d] " ANSI_CYAN " Ready." ANSI_RESET "\n",
	    port);

    // Cache the last few request/responses, so that we don't
    // recompute if there are connection problems. The master
    // prefers to ask the same helper again on failure.
    RequestCache cache(8);

    InPlaceTerminal term(1);
    int connections = 0;
    for (;;) {
      server.Listen();

      connections++;
      string line = StringPrintf("[%d] Connection #%d from %s",
				 port,
				 connections,
				 server.PeerString().c_str());
      #ifdef NOEMUCACHE
      term.Output(line + "\n");
      #endif

      HelperRequest hreq;
      if (server.ReadProto(&hreq)) {

	if (const Message *res = cache.Lookup(hreq)) {
	  #ifndef NOEMUCACHE
	  line += ", " ANSI_GREEN "cached!" ANSI_RESET;
	  term.Output(line + "\n");
	  #endif
	  if (!server.WriteProto(*res)) {
	    term.Advance();
	    fprintf(stderr, "Failed to send cached result...\n");
	    // keep going...
	  }

	} else if (hreq.has_playfun()) {
	  const PlayFunRequest &req = hreq.playfun();
	  #ifndef NOEMUCACHE
	  line += ", " ANSI_YELLOW "playfun" ANSI_RESET;
	  term.Output(line + "\n");
	  #endif
	  vector<uint8> next, current_state;
	  ReadBytesFromProto(req.current_state(), &current_state);
	  ReadBytesFromProto(req.next(), &next);
	  vector<Future> futures;
	  for (int i = 0; i < req.futures_size(); i++) {
	    Future f;
	    ReadBytesFromProto(req.futures(i).inputs(), &f.inputs);
	    futures.push_back(f);
	  }

	  double immediate_score, normalized_score,
            best_future_score, worst_future_score, future_score;
	  vector<double> futurescores(futures.size(), 0.0);

	  // Do the work.
	  InnerLoop(next, futures, &current_state,
		    &immediate_score, &normalized_score,
		    &best_future_score, &worst_future_score,
		    &future_score, &futurescores);

	  PlayFunResponse res;
	  res.set_immediate_score(immediate_score);
	  res.set_normalized_score(normalized_score);
	  res.set_best_future_score(best_future_score);
	  res.set_worst_future_score(worst_future_score);
	  res.set_futures_score(future_score);
	  for (int i = 0; i < futurescores.size(); i++) {
	    res.add_futurescores(futurescores[i]);
	  }

	  // fprintf(stderr, "Result: %s\n", res.DebugString().c_str());
	  cache.Save(hreq, res);
	  if (!server.WriteProto(res)) {
	    term.Advance();
	    fprintf(stderr, "Failed to send playfun result...\n");
	    // But just keep going.
	  }
	} else if (hreq.has_tryimprove()) {
	  const TryImproveRequest &req = hreq.tryimprove();
	  #ifndef NOEMUCACHE
	  line += ", " ANSI_PURPLE "tryimprove " +
	    TryImproveRequest::Approach_Name(req.approach()) +
	    ANSI_RESET;
	  term.Advance();
	  term.Output(line + "\n");
	  #endif

	  // This thing prints.
	  TryImproveResponse res;
	  DoTryImprove(req, &res);

	  cache.Save(hreq, res);
	  if (!server.WriteProto(res)) {
	    term.Advance();
	    fprintf(stderr, "Failed to send tryimprove result...\n");
	    // Keep going...
	  }
	} else {
	  term.Advance();
	  fprintf(stderr, ".. unknown request??\n");
	}
      } else {
	term.Advance();
	fprintf(stderr, "Failed to read request...\n");
      }
      server.Hangup();
    }
  }

  template<class F, class S>
  struct CompareByFirstDesc {
    bool operator ()(const pair<F, S> &a,
		     const pair<F, S> &b) {
      return b.first < a.first;
    }
  };

  void DoTryImprove(const TryImproveRequest &req,
		    TryImproveResponse *res) {
    vector<uint8> start_state, end_state;
    ReadBytesFromProto(req.start_state(), &start_state);
    ReadBytesFromProto(req.end_state(), &end_state);
    const double end_integral = req.end_integral();

    vector<uint8> improveme;
    ReadBytesFromProto(req.improveme(), &improveme);

    // Get the memories so that we can score.
    vector<uint8> start_memory, end_memory;
    Emulator::Load(&end_state);
    Emulator::GetMemory(&end_memory);

    Emulator::Load(&start_state);
    Emulator::GetMemory(&start_memory);

    InPlaceTerminal term(1);

    vector< pair< double, vector<uint8> > > repls;

    ArcFour rc(req.seed());

    set< vector<uint8> > tried;
    for (int i = 0; i < req.iters(); i++) {
      vector<uint8> inputs(improveme);
      for (int depth = 1; i < req.iters(); i++, depth++) {
	size_t start, len;
	// Use exponent of 2 (prefer smaller spans).
	GetRandomSpan(inputs, 2.0, &rc, &start, &len);
	uint32 word = RandomInt32(&rc);
	uint8 byte = rc.Byte();
	if (len == 0 && start != inputs.size()) len = 1;
	switch (req.approach()) {
	case TryImproveRequest::RANDOM:
	  inputs = GetRandomInputs(&rc, improveme.size());
	  break;
	case TryImproveRequest::DUALIZE:
	  TryDualizeAndReverse(&term, (double)i / req.iters(),
			     &start_state, start_memory,
			     &inputs, start, len,
			     end_memory, end_integral, &repls,
			     byte & 1);
	  break;
	case TryImproveRequest::ABLATE:
	  inputs = improveme;
	  // No sense in getting a mask that keeps everything.
	  do { byte = rc.Byte(); } while (byte == 0xFF);
	  for (int j = start; j < start+len; j++) {
	    if (RandomInt32(&rc) < word) {
	      inputs[j] &= byte;
	    }
	  }
	  break;
	case TryImproveRequest::CHOP:
	  ChopOut(inputs, start, len);
	  break;
	case TryImproveRequest::SHUFFLE:
	  vector<uint8>::iterator begin = inputs.begin() + start;
	  random_shuffle(begin, begin + len);
	  break;
	}
	double score = 0.0;
	// If we already tried this or it isn't an improvement,
	// try something else.
	// Disabled empty inputs to prevent segmentation fault
	// although empty inputs should be permissible.
	//if (inputs.empty() || tried.count(inputs) ||
	if (inputs.size() < INPUTS_PER_NEXT || tried.count(inputs) ||
	    !IsImprovement((double) i / req.iters(),
			   &start_state, start_memory,
			   inputs,
			   end_memory, end_integral,
			   &score)) {
	    break;
	}
	term.Advance();
	LOG("Improved (%s %zu for %zu depth %d)! %f\n",
		req.seed().c_str(), start, len, depth, score);
	repls.push_back(make_pair(score, inputs));
	tried.insert(inputs);
      }
    }

    const int nimproved = repls.size();

    if (repls.size() > req.maxbest()) {
      std::sort(repls.begin(), repls.end(),
		CompareByFirstDesc< double, vector<uint8> >());
      repls.resize(req.maxbest());
    }

    for (int i = 0; i < repls.size(); i++) {
      res->add_inputs(&repls[i].second[0], repls[i].second.size());
      res->add_score(repls[i].first);
    }

    // XXX I think that some can produce more than iters outputs,
    // so better could be greater than 100%.
    res->set_iters_tried(req.iters());
    res->set_iters_better(nimproved);

    LOG("In %d iters (%s), %d were improvements (%.1f%%)\n",
	    req.iters(),
	    TryImproveRequest::Approach_Name(req.approach()).c_str(),
	    nimproved, (100.0 * nimproved) / req.iters());
  }

  // Exponent controls the length of the span.
  // Large exponents yield smaller spans.
  // Note that len > 0 unless inputs is empty.
  void GetRandomSpan(const vector<uint8> &inputs, double exponent,
		     ArcFour *rc, size_t *start, size_t *len) {
    if (inputs.empty()) {
      *start = *len = 0;
      return;
    }
    double d = pow(RandomDouble(rc), exponent);
    *len = (int)(d * (inputs.size() - 1)) + 1;
    *start = (int)(RandomDouble(rc) * (inputs.size() - *len));
  }

  void ChopOut(vector<uint8> &inputs, size_t start, size_t len) {
    const vector<uint8>::iterator begin = inputs.begin() + start;
    inputs.erase(begin, begin + len);
  }

  void TryDualizeAndReverse(InPlaceTerminal *term, double frac,
			    vector<uint8> *start_state,
			    const vector<uint8> &start_memory,
			    vector<uint8> *inputs, int startidx, int len,
			    const vector<uint8> &end_memory,
			    double end_integral,
			    vector< pair< double, vector<uint8> > > *repls,
			    bool keepreversed) {

    Dualize(inputs, startidx, len);
    double score = 0.0;
    if (IsImprovement(frac,
		      start_state,
		      start_memory,
		      *inputs,
		      end_memory, end_integral,
		      &score)) {
      LOG("Improved! %f\n", score);
      repls->push_back(make_pair(score, *inputs));
    }

    ReverseRange(inputs, startidx, len);

    if (IsImprovement(frac,
		      start_state,
		      start_memory,
		      *inputs,
		      end_memory, end_integral,
		      &score)) {
      LOG("Improved (rev)! %f\n", score);
      repls->push_back(make_pair(score, *inputs));
    }

    if (!keepreversed) {
      ReverseRange(inputs, startidx, len);
    }
  }

  static void ReverseRange(vector<uint8> *v, int start, int len) {
    CHECK(start >= 0);
    CHECK((start + len) <= v->size());
    vector<uint8> vnew = *v;
    for (int i = 0; i < len; i++) {
      vnew[i] = (*v)[(start + len - 1) - i];
    }
    v->swap(vnew);
  }
  #endif

  static void Dualize(vector<uint8> *v, int start, int len) {
    CHECK(start >= 0);
    CHECK((start + len) <= v->size());
    for (int i = 0; i < len; i++) {
      uint8 input = (*v)[start + i];
      uint8 r = !!(input & INPUT_R);
      uint8 l = !!(input & INPUT_L);
      uint8 d = !!(input & INPUT_D);
      uint8 u = !!(input & INPUT_U);
      uint8 t = !!(input & INPUT_T);
      uint8 s = !!(input & INPUT_S);
      uint8 b = !!(input & INPUT_B);
      uint8 a = !!(input & INPUT_A);

      uint8 newinput = 0;
      if (r) newinput |= INPUT_L;
      if (l) newinput |= INPUT_R;
      if (d) newinput |= INPUT_U;
      if (u) newinput |= INPUT_D;
      if (t) newinput |= INPUT_S;
      if (s) newinput |= INPUT_T;
      if (b) newinput |= INPUT_A;
      if (a) newinput |= INPUT_B;

      (*v)[start + i] = newinput;
    }
  }

  // Computes the score as the sum of the scores of each step over the
  // input. You might want to normalize the score by the input length,
  // if comparing inputs of different length. Also swaps in the
  // final memory if non-NULL.
  double ScoreIntegral(vector<uint8> *start_memory,
		       const vector<uint8> &inputs,
		       vector<uint8> *final_memory) {
    Emulator::Load(start_memory);
    vector<uint8> previous_memory;
    Emulator::GetMemory(&previous_memory);
    double sum = 0.0;
    for (vector<uint8>::const_iterator it = inputs.begin();
      it != inputs.end(); it++) {
      Emulator::CachingStep(*it);
      vector<uint8> new_memory;
      Emulator::GetMemory(&new_memory);
      // PERF Does path integral actually improve accuracy?
      // Using a path integral could enable other calculations
      // (R-squared, variance, derivative, etc.) but Evaluate
      // should preserve the addition property (new > end if and
      // only if new - start > end - start) right?
      sum += objectives->Evaluate(previous_memory, new_memory);
      previous_memory.swap(new_memory);
    }
    if (final_memory != NULL) {
      final_memory->swap(previous_memory);
    }
    return sum;
  }

  // Note that this does NOT normalize the scores by input length
  // so there is a bias toward longer inputs (unless score decreases
  // at the end of longer inputs). If we had an approach that didn't
  // bound maximum input length, we would need to be careful with
  // this function.
  bool IsImprovement(double frac,
		     vector<uint8> *start_state,
		     const vector<uint8> &start_memory,
		     const vector<uint8> &inputs,
		     const vector<uint8> &end_memory,
		     const double &e_minus_s,
		     double *score) {
    //             e_minus_s
    //                     ....----> end
    //         ....----````           |
    //    start                       |  n_minus_e
    //         ````----....           v
    //                     ````----> new
    //             n_minus_s
    //
    vector<uint8> new_memory(start_memory);

    // Comparison with path integral
    // The _integral scores are comparing the path integrals from start
    // to end or new. We have intermediate states for these so we can
    // compute integrals with the thought that those are more accurate.
    double n_minus_s = ScoreIntegral(start_state, inputs, &new_memory);

    // n_minus_e is comparing end and new without using a path
    // (since there is no known path from end to new).
    double n_minus_e = objectives->Evaluate(end_memory, new_memory);

    // End is a better state from our perspective.
    if (n_minus_e <= 0) return false;

    *score = (n_minus_s - e_minus_s) + n_minus_e;
    return true;
  }

  vector<uint8> GetRandomInputs(ArcFour *rc, int len) {
    vector<uint8> inputs;
    inputs.reserve(len);
    while(inputs.size() < len) {
      const vector<uint8> &m =
	motifs->RandomWeightedMotifWith(rc);
      if (inputs.size() + m.size() < len) {
	inputs.insert(inputs.end(), m.begin(), m.end());
      } else {
	len -= inputs.size();
	inputs.insert(inputs.end(), m.begin(), m.begin() + len);
      }
    }
    return inputs;
  }

  void InnerLoop(const vector<uint8> &next,
		 const vector<Future> &futures_orig,
		 vector<uint8> *current_state,
		 double *immediate_score,
		 double *normalized_score,
		 double *best_future_score,
		 double *worst_future_score,
		 double *future_score,
		 vector<double> *futurescores) {

    // Make copy so we can make fake futures.
    vector<Future> futures = futures_orig;

    Emulator::Load(current_state);

    vector<uint8> current_memory;
    Emulator::GetMemory(&current_memory);

    // Take steps.
    for (int j = 0; j < next.size(); j++)
      Emulator::CachingStep(next[j]);

    vector<uint8> new_memory;
    Emulator::GetMemory(&new_memory);

    vector<uint8> new_state;
    Emulator::Save(&new_state);

    // Used to be BuggyEvaluate = WeightedLess? XXX
    *immediate_score = objectives->Evaluate(current_memory, new_memory);

    // Data visualization is more important than performance
    // PERF unused except for drawing
    // XXX probably shouldn't do this since it depends on local
    // storage.
    *normalized_score = objectives->GetNormalizedValue(new_memory);

    *best_future_score = -1e80;
    *worst_future_score = 1e80;


    // XXX reconsider whether this is really useful
    {
      // Synthetic future where we keep holding the last
      // button pressed.
      // static const int NUM_FAKE_FUTURES = 1;
      int total_future_length = 0;
      for (int i = 0; i < futures.size(); i++) {
	total_future_length += futures[i].inputs.size();
      }

      const int average_future_length = (int)((double)total_future_length /
					      (double)futures.size());

      Future fakefuture_hold;
      for (int z = 0; z < average_future_length; z++) {
	fakefuture_hold.inputs.push_back(next.back());
      }
      futures.push_back(fakefuture_hold);
    }

    *future_score = 0.0;
    double future_integral_scores[futures.size()];
    for (int f = 0; f < futures.size(); f++) {
      if (f != 0) Emulator::Load(&new_state);
      double positive_score, negative_score, integral_score;
      ScoreByFuture(futures[f], new_memory, &new_state,
		    &positive_score, &negative_score,
		    &integral_score);
      CHECK(positive_score >= 0);
      CHECK(negative_score <= 0);

      // For scoring the futures themselves (pruning and duplicating),
      // we want to disprefer futures that kill the player or get
      // stuck or whatever. So count both the positive and negative
      // components, plus the normalized integral.
      if (f < futures_orig.size()) {
	(*futurescores)[f] += integral_score +
	  positive_score + negative_score;
      }

      // Caches integral scores, sorts them, and weights more
      // positive scores stronger.
      future_integral_scores[f] = integral_score;

      // Unused except for diagnostics.
      if (positive_score > *best_future_score)
	*best_future_score = positive_score;
      if (negative_score < *worst_future_score)
	*worst_future_score = negative_score;
    }
    // Aggregates scores weighting better scores more.
    std::sort(future_integral_scores,
      future_integral_scores + futures.size());
    for (int f = 0; f < futures.size(); f++) {
      *future_score = *future_score/2 + future_integral_scores[f]/2;
    }

    // Discards the copy.
    // futures.resize(futures.size() - NUM_FAKE_FUTURES);
  }

  // The parallel step. We either run it in serial locally
  // (without MARIONET) or as jobs on helpers, via TCP.
  void ParallelStep(const vector< vector<uint8> > &nexts,
		    const vector<Future> &futures,
		    // morally const
		    vector<uint8> &current_state,
		    const vector<uint8> &current_memory,
		    vector<double> *futuretotals,
		    int *best_next_idx) {
    uint64 start_time = time(NULL);
    fprintf(stderr, "Parallel step with %zu nexts, %zu futures.\n",
	    nexts.size(), futures.size());
    CHECK(nexts.size() > 0);
    *best_next_idx = 0;

    double best_score = 0.0;
    Scoredist distribution(movie.size());

#ifdef MARIONET
    // One piece of work per request.
    vector<HelperRequest> requests;
    requests.resize(nexts.size());
    for (int i = 0; i < nexts.size(); i++) {
      PlayFunRequest *req = requests[i].mutable_playfun();
      req->set_current_state(&(current_state[0]), current_state.size());
      req->set_next(&nexts[i][0], nexts[i].size());
      for (int f = 0; f < futures.size(); f++) {
	FutureProto *fp = req->add_futures();
	fp->set_inputs(&futures[f].inputs[0],
		       futures[f].inputs.size());
      }
      // if (!i) fprintf(stderr, "REQ: %s\n", req->DebugString().c_str());
    }

    GetAnswers<HelperRequest, PlayFunResponse> getanswers(ports_, requests);
    getanswers.Loop();

    const vector<GetAnswers<HelperRequest, PlayFunResponse>::Work> &work =
      getanswers.GetWork();

    for (int i = 0; i < work.size(); i++) {
      const PlayFunResponse &res = work[i].res;
      for (int f = 0; f < res.futurescores_size(); f++) {
	CHECK(f <= futuretotals->size());
	(*futuretotals)[f] += res.futurescores(f);
      }

      const double score = res.immediate_score() + res.futures_score();

      distribution.immediates.push_back(res.immediate_score());
      distribution.positives.push_back(res.best_future_score());
      distribution.negatives.push_back(res.worst_future_score());
      // Even if it's not globally accurate, data is better than no data
      // XXX norm score can't be computed in a distributed fashion.
      distribution.norms.push_back(res.normalized_score());

      if (score > best_score) {
	best_score = score;
	*best_next_idx = i;
      }
    }

#else
    // Local version.
    for (int i = 0; i < nexts.size(); i++) {
      double immediate_score, normalized_score,
	     best_future_score, worst_future_score, future_score;
      vector<double> futurescores(NFUTURES, 0.0);
      InnerLoop(nexts[i], futures, &current_state,
		&immediate_score, &normalized_score,
		&best_future_score, &worst_future_score,
		&future_score, &futurescores);

      for (int f = 0; f < futurescores.size(); f++) {
	(*futuretotals)[f] += futurescores[f];
      }

      double score = immediate_score + future_score;
      distribution.immediates.push_back(immediate_score);
      distribution.positives.push_back(best_future_score);
      distribution.negatives.push_back(worst_future_score);
      // Even if it's not globally accurate, data is better than no data
      // XXX norm score can't be computed in a distributed fashion.
      distribution.norms.push_back(normalized_score);

      if (score > best_score) {
	best_score = score;
	*best_next_idx = i;
      }
    }
#endif
    distribution.chosen_idx = *best_next_idx;
    distributions.push_back(distribution);

    uint64 end_time = time(NULL);
    fprintf(stderr, "Parallel step took %d seconds, score %f.\n",
	    (int)(end_time - start_time), best_score);
  }

  void PopulateFutures(vector<Future> *futures) {
    int num_currently_weighted = 0;
    for (vector<Future>::const_iterator it = futures->begin();
	 it != futures->end(); it++) {
      if (it->weighted) {
	num_currently_weighted++;
      }
    }

    int num_to_weight = max(NWEIGHTEDFUTURES - num_currently_weighted, 0);
    #ifdef DEBUGFUTURES
    fprintf(stderr, "there are %d futures, %d cur weighted, %d need\n",
	    futures->size(), num_currently_weighted, num_to_weight);
    #endif
    while (futures->size() < NFUTURES) {
      // Keep the desired length around so that we only
      // resize the future if we drop it. Randomize between
      // MIN and MAX future lengths.
      size_t flength = MINFUTURELENGTH +
	(int)
	((double)(MAXFUTURELENGTH - MINFUTURELENGTH) *
	 RandomDouble(&rc));

      if (num_to_weight > 0) {
	futures->push_back(Future(true, flength));
	num_to_weight--;
      } else {
	futures->push_back(Future(false, flength));
      }
    }

    // Make sure we have enough futures with enough data in.
    // PERF: Should avoid creating exact duplicate futures.
    for (int i = 0; i < NFUTURES; i++) {
      vector<uint8> &inputs = (*futures)[i].inputs;
      const Future &future = (*futures)[i];
      while (inputs.size() < future.desired_length) {
	const vector<uint8> &m = future.weighted ?
	  motifs->RandomWeightedMotif() :
	  motifs->RandomMotif();
	if (m.size() > inputs.size() + future.desired_length) {
	  size_t length = future.desired_length - inputs.size();
	  inputs.insert(inputs.end(), m.begin(), m.begin() + length);
	  break;
	}
	inputs.insert(inputs.end(), m.begin(), m.end());
      }
    }

    #ifdef DEBUGFUTURES
    for (int f = 0; f < futures->size(); f++) {
      fprintf(stderr, "%d. %s %d/%d: ...\n",
	      f, (*futures)[f].weighted ? "weighted" : "random",
	      (*futures)[f].inputs.size(),
	      (*futures)[f].desired_length);
    }
    #endif
  }

  Future MutateFuture(const Future &input) {
    Future out;
    out.is_mutant = true;
    out.weighted = input.weighted;
    if ((rc.Byte() & 7) == 0) out.weighted = !out.weighted;
    out.inputs = input.inputs;

    out.desired_length = input.desired_length;

    // Replace tail with something random.
    const size_t size = MINFUTURELENGTH;
    out.inputs.resize(max(size, out.desired_length / 2));

    // Occasionally, try something very different.
    if ((rc.Byte() & 7) == 0) {
      Dualize(&out.inputs, 0, out.inputs.size());
    }
    // TODO: More interesting mutations here (chop, ablate, reverse..)

    return out;
  }

  // Consider every possible next step along with every possible
  // future. Commit to the step that has the best score among
  // those futures. Remove the futures that didn't perform well
  // overall, and replace them. Reweight motifs according... XXX
  void TakeBestAmong(const vector< vector<uint8> > &nexts,
		     const vector<string> &nextplanations,
		     vector<Future> *futures,
		     bool chopfutures) {
    vector<uint8> current_state;
    vector<uint8> current_memory;

    if (futures->size() != NFUTURES) {
      fprintf(stderr, "?? Expected futures to have size %d but "
	      "it has %zu.\n", NFUTURES, futures->size());
    }

    // Save our current state so we can try many different branches.
    Emulator::Save(&current_state);
    Emulator::GetMemory(&current_memory);

    // Total score across all motifs for each future.
    vector<double> futuretotals(futures->size(), 0.0);

    // Most of the computation happens here.
    int best_next_idx = -1;
    ParallelStep(nexts, *futures,
		 current_state, current_memory,
		 &futuretotals,
		 &best_next_idx);
    CHECK(best_next_idx >= 0);
    CHECK(best_next_idx < nexts.size());

    if (chopfutures) {
      // Chop the head off each future.
      LOG("Chop futures.\n");
      const int choplength = nexts[best_next_idx].size();
      for (vector<Future>::iterator it = futures->begin();
           it != futures->end(); it++) {
	vector<uint8> newf(it->inputs.begin() + choplength, it->inputs.end());
	it->inputs.swap(newf);
      }
    }

    // XXX: Don't drop the future if it was the one we got the
    // max() score for. Right? It might have had very poor scores
    // otherwise, but we might be relying on it existing.
    // TODO: Consider duplicating the future that we got the max()
    // score from.

    // Discard the futures with the worst total.
    // They'll be replaced the next time around the loop.
    // PERF don't really need to make DROPFUTURES passes,
    // but there are not many futures and not many dropfutures.
    static const int TOTAL_TO_DROP = DROPFUTURES + MUTATEFUTURES;
    for (int t = 0; t < TOTAL_TO_DROP; t++) {
      // fprintf(stderr, "Drop futures (%d/%d).\n", t, DROPFUTURES);
      CHECK(!futures->empty());
      CHECK(futures->size() <= futuretotals.size());
      double worst_total = futuretotals[0];
      int worst_idx = 0;
      for (int i = 1; i < futures->size(); i++) {
	if (worst_total < futuretotals[i]) {
	  worst_total = futuretotals[i];
	  worst_idx = i;
	}
      }

      // Delete it by swapping.
      if (worst_idx != futures->size() - 1) {
	(*futures)[worst_idx] = (*futures)[futures->size() - 1];
	// Also swap in the futuretotals so the scores match.
	// This was a bug before -- it always dropped the lowest
	// scoring one and then the tail of the array (because this
	// slot would still have the lowest score).
	futuretotals[worst_idx] = futuretotals[futures->size() - 1];
      }
      futures->resize(futures->size() - 1);
    }

    // Now get the future with the best score.
    CHECK(!futures->empty());
    int best_future_idx = 0;
    double best_future_score = futuretotals[0];
    for (int i = 1; i < futures->size(); i++) {
      if (futuretotals[i] > best_future_score) {
	best_future_score = futuretotals[i];
	best_future_idx = i;
      }
    }

    for (int t = 0; t < MUTATEFUTURES; t++) {
      futures->push_back(MutateFuture((*futures)[best_future_idx]));
    }

    // If in single mode, this is probably cached, but with
    // MARIONET this is usually a full replay.
    // fprintf(stderr, "Replay %d moves\n", nexts[best_next_idx].size());
    Emulator::Load(&current_state);
    for (int j = 0; j < nexts[best_next_idx].size(); j++) {
      Commit(nexts[best_next_idx][j], nextplanations[best_next_idx]);
    }

    // Now, if the motif we used was a local improvement to the
    // score, reweight it.
    // This should be a motif in the normal case where we're trying
    // each motif, but when we use this to implement the best
    // backtrack plan, it usually won't be.
    if (motifs->IsMotif(nexts[best_next_idx])) {
      double total = motifs->GetTotalWeight();
      motifs->Pick(nexts[best_next_idx]);
      vector<uint8> new_memory;
      Emulator::GetMemory(&new_memory);
      double oldval = objectives->GetNormalizedValue(current_memory);
      double newval = objectives->GetNormalizedValue(new_memory);
      double *weight = motifs->GetWeightPtr(nexts[best_next_idx]);
      // Already checked it's a motif.
      CHECK(weight != NULL);
      if (newval > oldval) {
	// Increases its weight.
	double d = *weight / MOTIF_ALPHA;
	if (d / total < MOTIF_MAX_FRAC) {
	  *weight = d;
	} else {
	  fprintf(stderr, "motif is already at max frac: %.2f\n", d);
	}
      } else {
	// Decreases its weight.
	double d = *weight * MOTIF_ALPHA;
	if (d / total > MOTIF_MIN_FRAC) {
	  *weight = d;
	} else {
	  fprintf(stderr, "motif is already at min frac: %f\n", d);
	}
      }
    }

    PopulateFutures(futures);
  }

  // Main loop for the master, or when compiled without MARIONET support.
  // Helpers is an array of helper ports, which is ignored unless MARIONET
  // is active.
  void Master(const vector<int> &helpers) {
    // XXX
    ports_ = helpers;

    log = fopen((config.game+ "-log.html").c_str(), "w");
    CHECK(log != NULL);
    fprintf(log,
	    "<!DOCTYPE html>\n"
	    "<link rel=\"stylesheet\" href=\"log.css\" />\n"
	    "<h1>%s started at %s %s.</h1>\n",
	    config.game.c_str(),
	    DateString(time(NULL)).c_str(),
	    TimeString(time(NULL)).c_str());
    fflush(log);

    fprintf(stderr, "[MASTER] Beginning "
	    ANSI_YELLOW "%s" ANSI_RESET ".\n", config.game.c_str());

    // This version of the algorithm looks like this. At some point in
    // time, we have the set of motifs we might play next. We'll
    // evaluate all of those. We also have a series of possible
    // futures that we're considering. At each step we play our
    // candidate motif (ignoring that many steps as in the future --
    // but note that for each future, there should be some motif that
    // matches its head). Then we play all the futures. The motif with the
    // best overall score is chosen; we chop the head off each future,
    // and add a random motif to its end.
    // (XXX docs are inaccurate now)
    // XXX recycling futures...
    vector<Future> futures;

    int rounds_until_backtrack = TRY_BACKTRACK_EVERY / INPUTS_PER_NEXT;
    uint64 iters = 1;

    PopulateFutures(&futures);
    for (;; iters++) {

      // XXX TODO this probably gets confused by backtracking.
      motifs->Checkpoint(movie.size());

      vector< vector<uint8> > nexts;
      vector<string> nextplanations;
      MakeNexts(futures, &nexts, &nextplanations);

      TakeBestAmong(nexts, nextplanations, &futures, true);

      fprintf(stderr, "%llu rounds, "
	      ANSI_CYAN "%zu inputs" ANSI_RESET ". backtrack in %d. "
	      "%zu Cxpoints at ",
	      iters, movie.size(), rounds_until_backtrack, checkpoints.size());

      for (int i = 0, j = checkpoints.size() - 1; i < 3 && j >= 0; i++) {
	fprintf(stderr, "%zu, ", checkpoints[j].movenum);
	j--;
      }
      fprintf(stderr, "...\n");

      if (iters % SAVE_EVERY == 0) {
	SaveMovie(iters);
	SaveDiagnostics(futures);
      }

      // In theory diagnostics could assist backtrack, right?
      // So do this last.
      MaybeBacktrack(iters, &rounds_until_backtrack, &futures);
    }
  }

  // Make the nexts that we should try for this round.
  void MakeNexts(const vector<Future> &futures,
		 vector< vector<uint8> > *nexts,
		 vector<string> *nextplanations) {

    map< vector<uint8>, string > todo;
    for (int i = 0; i < futures.size(); i++) {
      if (futures[i].inputs.size() >= INPUTS_PER_NEXT) {
	vector<uint8> nf(futures[i].inputs.begin(),
			 futures[i].inputs.begin() + INPUTS_PER_NEXT);
	if (!todo.count(nf)) {
	  todo.insert(make_pair(nf, StringPrintf("ftr-%d", i)));
	}
      }
    }

    // There may be duplicates (typical, in fact). Insert motifs
    // as long as we can.
    while (todo.size() < NFUTURES) {
      const vector<uint8> *motif = motifs->RandomWeightedMotifNotIn(todo);
      if (motif == NULL) {
	fprintf(stderr, "No more motifs (have %zu todo).\n", todo.size());
	break;
      }

      todo.insert(make_pair(*motif, "backfill"));
    }

    // Now populate nexts and explanations.
    nexts->clear();
    nextplanations->clear();
    for (map< vector<uint8>, string >::const_iterator it = todo.begin();
	 it != todo.end(); ++it) {
      nexts->push_back(it->first);
      nextplanations->push_back(it->second);
    }
  }

  void TryImprove(Checkpoint *start,
		  const vector<uint8> &improveme,
		  const vector<uint8> &current_state,
		  vector<Replacement> *replacements,
		  double *improvability) {

    uint64 start_time = time(NULL);
    fprintf(stderr, "TryImprove step on %zu inputs.\n",
	    improveme.size());
    CHECK(replacements);
    replacements->clear();

    const double current_integral =
      ScoreIntegral(&start->save, improveme, NULL);

    fprintf(log, "<li>Trying to improve frames %zu&ndash;%zu, %f</li>\n",
	    start->movenum, movie.size(), current_integral);

    #ifdef MARIONET
    static const int MAXBEST = 2;

    // For random, we could compute the right number of
    // tasks based on the number of helpers...
    static const int NUM_IMPROVE_RANDOM = 10;
    static const int RANDOM_ITERS = 200;

    static const int NUM_ABLATE = 10;
    static const int ABLATE_ITERS = 200;

    static const int NUM_CHOP = 10;
    static const int CHOP_ITERS = 200;

    static const int NUM_SHUFFLE = 10;
    static const int SHUFFLE_ITERS = 200;

    // Note that some of these have a fixed number
    // of iterations that are tried, independent of
    // the iters field. So try_opposites = true and
    // opposites_ites = 0 does make sense.
    static const bool TRY_DUALIZE = true;
    static const int DUALIZE_ITERS = 200;

    // One piece of work per request.
    vector<HelperRequest> requests;

    // Every request shares this stuff.
    TryImproveRequest base_req;
    base_req.set_start_state(&start->save[0], start->save.size());
    base_req.set_improveme(&improveme[0], improveme.size());
    base_req.set_end_state(&current_state[0], current_state.size());
    base_req.set_end_integral(current_integral);
    base_req.set_maxbest(MAXBEST);

    if (TRY_DUALIZE) {
      TryImproveRequest req = base_req;
      req.set_approach(TryImproveRequest::DUALIZE);
      req.set_iters(DUALIZE_ITERS);
      req.set_seed(StringPrintf("dualize%zu", start->movenum));

      HelperRequest hreq;
      hreq.mutable_tryimprove()->MergeFrom(req);
      requests.push_back(hreq);
    }

    for (int i = 0; i < NUM_ABLATE; i++) {
      TryImproveRequest req = base_req;
      req.set_iters(ABLATE_ITERS);
      req.set_seed(StringPrintf("ablate%zu.%d", start->movenum, i));
      req.set_approach(TryImproveRequest::ABLATE);

      HelperRequest hreq;
      hreq.mutable_tryimprove()->MergeFrom(req);
      requests.push_back(hreq);
    }

    for (int i = 0; i < NUM_CHOP; i++) {
      TryImproveRequest req = base_req;
      req.set_iters(CHOP_ITERS);
      req.set_seed(StringPrintf("chop%zu.%d", start->movenum, i));
      req.set_approach(TryImproveRequest::CHOP);

      HelperRequest hreq;
      hreq.mutable_tryimprove()->MergeFrom(req);
      requests.push_back(hreq);
    }

    for (int i = 0; i < NUM_SHUFFLE; i++) {
      TryImproveRequest req = base_req;
      req.set_iters(SHUFFLE_ITERS);
      req.set_seed(StringPrintf("shuffle%zu.%d", start->movenum, i));
      req.set_approach(TryImproveRequest::SHUFFLE);

      HelperRequest hreq;
      hreq.mutable_tryimprove()->MergeFrom(req);
      requests.push_back(hreq);
    }

    for (int i = 0; i < NUM_IMPROVE_RANDOM; i++) {
      TryImproveRequest req = base_req;
      req.set_iters(RANDOM_ITERS);
      req.set_seed(StringPrintf("random%zu.%d", start->movenum, i));
      req.set_approach(TryImproveRequest::RANDOM);

      HelperRequest hreq;
      hreq.mutable_tryimprove()->MergeFrom(req);
      requests.push_back(hreq);
    }

    GetAnswers<HelperRequest, TryImproveResponse>
      getanswers(ports_, requests);
    getanswers.Loop();

    const vector<GetAnswers<HelperRequest,
			    TryImproveResponse>::Work> &work =
      getanswers.GetWork();

    fprintf(log, "<li>Attempts at improving:\n<ul>");
    int numer = 0, denom = 0;
    for (int i = 0; i < work.size(); i++) {
      const TryImproveRequest &req = work[i].req->tryimprove();
      const TryImproveResponse &res = work[i].res;
      CHECK(res.score_size() == res.inputs_size());
      for (int j = 0; j < res.inputs_size(); j++) {
	Replacement r;
	r.method =
	  StringPrintf("%s-%d-%s",
		       TryImproveRequest::Approach_Name(req.approach()).c_str(),
		       req.iters(),
		       req.seed().c_str());
	ReadBytesFromProto(res.inputs(j), &r.inputs);
	r.score = res.score(j);
	replacements->push_back(r);
      }

      fprintf(log, "<li>%s: %d/%d</li>\n",
	      TryImproveRequest::Approach_Name(req.approach()).c_str(),
	      res.iters_better(),
	      res.iters_tried());

      numer += res.iters_better();
      denom += res.iters_tried();
    }
    fprintf(log, "</ul></li><li> ... (total %d/%d = %.1f%%)</li>\n",
	    numer, denom, (100.0 * numer) / denom);
    *improvability = (double)numer / denom;

    #else
    // This is optional, so if there's no MARIONET, skip for now.
    fprintf(stderr, "TryImprove requires MARIONET...\n");
    #endif

    uint64 end_time = time(NULL);
    fprintf(stderr, "TryImprove took %d seconds.\n",
	    (int)(end_time - start_time));
  }

  // Get a checkpoint that is at least MIN_BACKTRACK_DISTANCE inputs
  // in the past, or return NULL.
  Checkpoint *GetRecentCheckpoint() {
    for (int i = checkpoints.size() - 1; i >= 0; i--) {
      if ((movie.size() - checkpoints[i].movenum) >= MIN_BACKTRACK_DISTANCE &&
	  checkpoints[i].movenum > watermark) {
	return &checkpoints[i];
      }
    }
    return NULL;
  }


  void MaybeBacktrack(int iters,
		      int *rounds_until_backtrack,
		      vector<Future> *futures) {
    if (!TRY_BACKTRACK)
      return;

    // Now consider backtracking.
    // TODO: We could trigger a backtrack step whenever we feel
    // like we aren't making significant progress, like when
    // there's very little difference between the futures we're
    // looking at, or when we haven't made much progress since
    // the checkpoint, or whatever. That would probably help
    // since part of the difficulty here is going to be deciding
    // whether the current state or some backtracked-to state is
    // actually better, and if we know the current state is bad,
    // then we have less opportunity to get it wrong.
    --*rounds_until_backtrack;
    if (*rounds_until_backtrack <= 0) {
      *rounds_until_backtrack = TRY_BACKTRACK_EVERY / INPUTS_PER_NEXT;
      LOG(" ** backtrack time. **\n");
      uint64 start_time = time(NULL);

      fprintf(log,
	      "<h2>Backtrack at iter %d, end frame %zu, %s.</h2>\n",
	      iters,
	      movie.size(),
	      TimeString(start_time).c_str());
      fflush(log);

      // Backtracking is like this. Call the last checkpoint "start"
      // (technically it could be any checkpoint, so think about
      // principled ways of finding a good starting point.) and
      // the current point "now". There are N inputs between
      // start and now.
      //
      // The goal is, given what we know, to see if we can find a
      // different N inputs that yield a better outcome than what
      // we have now. The purpose is twofold:
      //  - We may have just gotten ourselves into a local maximum
      //    by bad luck. If the checkpoint is before that bad
      //    choice, we have some chance of not making it (but
      //    that's basically random).
      //  - We now know more about what's possible, which should
      //    help us choose better. For examples, we can try
      //    variations on the sequence of N moves between start
      //    and now.

      // Morally const, but need to load state from it.
      Checkpoint *start_ptr = GetRecentCheckpoint();
      if (start_ptr == NULL) {
	fprintf(stderr, "No checkpoint to try backtracking.\n");
        *rounds_until_backtrack = 1;
	return;
      }
      // Copy, because stuff we do in here can resize the
      // checkpoints array and cause disappointment.
      Checkpoint start = *start_ptr;

      // Inputs to be improved.
      vector<uint8> improveme(movie.begin() + start.movenum, movie.end());
      const size_t nmoves = improveme.size();
      CHECK(nmoves > 0);

      vector<uint8> current_state;
      Emulator::Save(&current_state);
      vector<Replacement> replacements;
      double improvability = 0.0;
      TryImprove(&start, improveme, current_state,
		 &replacements, &improvability);
      if (replacements.empty()) {
	fprintf(stderr,
		ANSI_GREEN "There were no superior replacements."
		ANSI_RESET "\n");
	return;
      } else if (improvability < 0.05) {
	fprintf(stderr,
		"Improvability only " ANSI_GREEN "%.2f%% :)" ANSI_RESET "\n",
		100.0 * improvability);
      } else if (improvability > 0.30) {
	fprintf(stderr,
		"Improvability high at " ANSI_RED "%.2f%% :(" ANSI_RESET "\n",
		100.0 * improvability);
      } else {
	fprintf(stderr, "Improvability is " ANSI_CYAN "%.2f%%" ANSI_RESET "\n",
		100.0 * improvability);
      }

      // Rather than trying to find the best immediate one (we might
      // be hovering above a pit about to die, so we do need to look
      // into the future), use the standard TakeBestAmong to score all
      // the potential improvements, as well as the current best.
      fprintf(stderr,
	      "There are %zu+1 possible replacements for last %zu moves...\n",
	      replacements.size(),
	      nmoves);

      for (int i = 0; i < replacements.size(); i++) {
	fprintf(log,
		"<li>%zu inputs via %s, %.2f</li>\n",
		replacements[i].inputs.size(),
		replacements[i].method.c_str(),
		replacements[i].score);
      }
      fflush(log);

      // PERF Perhaps movie is already rewound?
      Rewind(start.movenum);
      Emulator::Load(&start.save);

      set< vector<uint8> > tryme;
      vector< vector<uint8> > tryvec;
      vector<string> trysplanations;
      // Allow the existing sequence to be chosen if it's
      // still better despite seeing these alternatives.
      tryme.insert(improveme);
      tryvec.push_back(improveme);
      // XXX better to keep whatever annotations were already there!
      trysplanations.push_back("original");

      for (int i = 0; i < replacements.size(); i++) {
	// Currently ignores scores and methods. Make TakeBestAmong
	// take annotated nexts so it can tell you which one it
	// preferred. (Consider weights too..?)
	if (!tryme.count(replacements[i].inputs)) {
	  tryme.insert(replacements[i].inputs);
	  tryvec.push_back(replacements[i].inputs);
	  trysplanations.push_back(replacements[i].method);
	}
      }

      // vector< vector<uint8> > tryvec(tryme.begin(), tryme.end());
      if (tryvec.size() != replacements.size() + 1) {
	fprintf(stderr, "... but there were %zu duplicates (removed).\n",
		(replacements.size() + 1) - tryvec.size());
	fprintf(log, "<li><b>%zu total but there were %zu duplicates (removed)."
		"</b></li>\n",
		replacements.size() + 1,
		(replacements.size() + 1) - tryvec.size());
	fflush(log);
      }

      // PERF could be passing along the end state for these, to
      // avoid the initial replay. If they happen to go back to the
      // same helper that computed it in the first place, it'd be
      // cached, at least.
      TakeBestAmong(tryvec, trysplanations, futures, false);

      fprintf(stderr, "Write improvement movie.\n");
      SimpleFM2::WriteInputsWithSubtitles(
	  StringPrintf((config.game+ "-playfun-backtrack-%llu.fm2").c_str(), iters),
	  (config.game+ ".nes").c_str(),
	  config,
	  movie,
	  subtitles);

      // What to do about futures? This is simplest, I guess...
      uint64 end_time = time(NULL);
      fprintf(stderr,
	      "Backtracking took %llu seconds in total. "
	      "Back to normal search...\n",
	      end_time - start_time);
      fprintf(log,
	      "<li>Backtracking took %llu seconds in total.</li>\n",
	      end_time - start_time);
      fflush(log);
    }
  }

  void SaveMovie(uint64 &iters) {
    printf("                     - writing movie -\n");
    SimpleFM2::WriteInputsWithSubtitles(StringPrintf((config.game+ "-playfun-%llu.fm2").c_str(), iters),
	(config.game+ ".nes").c_str(),
	config,
	movie,
	subtitles);
    Emulator::PrintCacheStats();
  }

  void SaveDiagnostics(const vector<Future> &futures) {
    printf("                     - writing diagnostics -\n");
    SaveFuturesHTML(futures, (config.game+ "-playfun-futures.html").c_str());
    #ifdef DEBUGFUTURES
    vector<uint8> fmovie = movie;
    const size_t size = fmovie.size();
    for (int i = 0; i < futures.size(); i++) {
      const vector<uint8> &inputs = futures[i].inputs;
      fmovie.insert(fmovie.end(), inputs.begin(), inputs.end());
      SimpleFM2::WriteInputs(StringPrintf((config.game+ "-playfun-future-%d.fm2").c_str(), i),
	  (config.game+ ".nes").c_str(),
	  config,
	  fmovie);
      fmovie.resize(size);
    }
    printf("Wrote %zu movie(s).\n", futures.size() + 1);
    #endif
    SaveDistributionSVG(movie.size(), distributions, (config.game+ "-playfun-scores.svg").c_str());
    objectives->SaveSVG(memories, (config.game+ "-playfun-futures.svg").c_str());
    motifs->SaveHTML((config.game+ "-playfun-motifs.html").c_str());
    printf("                     (wrote)\n");
  }

  // Ports for the helpers.
  vector<int> ports_;

  // For making SVG.
  vector<Scoredist> distributions;

  // Used to ffwd to gameplay.
  vector<uint8> solution;

  FILE *log;
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

  Config config(argc, argv);
  PlayFun pf(config);
  #ifdef MARIONET
  if (config.helpers.empty()) {
    fprintf(stderr, "Starting helper on port %d...\n", config.port);
    pf.Helper(config.port);
  } else {
    pf.Master(config.helpers);
  }
  #else
  pf.Master(config.helpers);
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
