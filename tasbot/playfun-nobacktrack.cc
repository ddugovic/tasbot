/* Tries playing a game (deliberately not customized to any particular
   ROM) using an objective function learned by learnfun. This version
   just looks at the single-motif horizon, scores each one of them,
   and plays the best one. It never backtracks. I got about 1/3 through
   world 1-2 in Mario with it. */

#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

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

#define GAME "mario"
#define MOVIE "mario-cleantom.fm2"

struct PlayFun {
  PlayFun() : rc("playfun") {
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
    while (start < solution.size()) {
      Emulator::Step(solution[start]);
      movie.push_back(solution[start]);
      if (solution[start] != 0) break;
      start++;
    }

    printf("Skipped %ld frames until first keypress.\n", start);
  }

  // XXX should probably have AvoidLosing and TryWinning.
  // This looks for the min over random play in the future,
  // so tries to avoid getting us screwed.
  //
  // Look fairly deep into the future playing randomly. 
  // DESTROYS THE STATE.
  double AvoidBadFutures(const vector<uint8> &base_memory) {
    // XXX should be based on motif size
    // XXX should learn it somehow? this is tuned by hand
    // for mario.
    // was 10, 50
    static const int DEPTHS[] = { 20, 75 };
    // static const int NUM = 2;
    vector<uint8> base_state;
    Emulator::SaveUncompressed(&base_state);

    double total = 1.0;
    for (int i = 0; i < (sizeof (DEPTHS) / sizeof (int)); i++) {
      if (i) Emulator::LoadUncompressed(&base_state);
      for (int d = 0; d < DEPTHS[i]; d++) {
	const vector<uint8> &m = motifs->RandomWeightedMotif();
	for (int x = 0; x < m.size(); x++) {
	  Emulator::CachingStep(m[x]);

	  // PERF inside the loop -- scary!
	  vector<uint8> future_memory;
	  Emulator::GetMemory(&future_memory);
	  // XXX min? max?
	  if (i || d || x) {
	    total = min(total, 
			objectives->Evaluate(base_memory, future_memory));
	  } else {
	    total = objectives->Evaluate(base_memory, future_memory);
	  }

	}
      }

      // total += objectives->Evaluate(base_memory, future_memory);
    }

    // We're allowed to destroy the current state, so don't
    // restore.
    return total;
  }

  // DESTROYS THE STATE
  double SeekGoodFutures(const vector<uint8> &base_memory) {
    // XXX should be based on motif size
    // XXX should learn it somehow? this is tuned by hand
    // for mario.
    // was 10, 50
    static const int DEPTHS[] = { 30, 30, 50 };
    // static const int NUM = 2;
    vector<uint8> base_state;
    Emulator::SaveUncompressed(&base_state);

    double total = 1.0;
    for (int i = 0; i < (sizeof (DEPTHS) / sizeof (int)); i++) {
      if (i) Emulator::LoadUncompressed(&base_state);
      for (int d = 0; d < DEPTHS[i]; d++) {
	const vector<uint8> &m = motifs->RandomWeightedMotif();
	for (int x = 0; x < m.size(); x++) {
	  Emulator::CachingStep(m[x]);

	}
      }

      // PERF inside the loop -- scary!
      vector<uint8> future_memory;
      Emulator::GetMemory(&future_memory);

      if (i) {
	total = max(total, 
		    objectives->Evaluate(base_memory, future_memory));
      } else {
	total = objectives->Evaluate(base_memory, future_memory);
      }

      // total += objectives->Evaluate(base_memory, future_memory);
    }

    // We're allowed to destroy the current state, so don't
    // restore.
    return total;
  }


#if 0
  // Search all different sequences of motifs of length 'depth'.
  // Choose the one with the highest score.
  void ExhaustiveMotifSearch(int depth) {
    vector<uint8> current_state, current_memory;
    Emulator::Save(&current_state);
    Emulator::GetMemory(&current_memory);

  }
#endif

  void Greedy() {
    // Let's just try a greedy version for the lols.
    // At every state we just try the input that increases the
    // most objective functions.

    // PERF
    // For drawing SVG.
    vector< vector<uint8> > memories;

    vector<uint8> current_state;
    vector<uint8> current_memory;

    vector< vector<uint8> > nexts = motifvec;

    // 10,000 is about enough to run out the clock, fyi
    // XXX not frames, #motifs
    static const int NUMFRAMES = 10000;
    for (int framenum = 0; framenum < NUMFRAMES; framenum++) {

      #if 0
      if (movie.size() > 100) {
	printf("Done.\n");
	Emulator::PrintCacheStats();
	exit(0);
      }
      #endif

      // Save our current state so we can try many different branches.
      Emulator::SaveUncompressed(&current_state);    
      Emulator::GetMemory(&current_memory);
      memories.push_back(current_memory);

      // To break ties.
      Shuffle(&nexts);

      double best_score = -999999999.0;
      double best_future = 0.0, best_immediate = 0.0;
      vector<uint8> *best_input = &nexts[0];
      for (int i = 0; i < nexts.size(); i++) {
	// (Don't restore for first one; it's already there)
	if (i != 0) Emulator::LoadUncompressed(&current_state);
	for (int j = 0; j < nexts[i].size(); j++)
	  Emulator::CachingStep(nexts[i][j]);

	vector<uint8> new_memory;
	Emulator::GetMemory(&new_memory);
	vector<uint8> new_state;
	Emulator::SaveUncompressed(&new_state);
	double immediate_score =
	  objectives->Evaluate(current_memory, new_memory);

	double future_score = AvoidBadFutures(new_memory);

	// XXX I fixed a bug here and never retested it -- avoid
	// bad and seek good both destroy the state.
	Emulator::LoadUncompressed(&new_state);
	future_score += SeekGoodFutures(new_memory);

	double score = immediate_score + future_score;

	if (score > best_score) {
	  best_score = score;
	  best_immediate = immediate_score;
	  best_future = future_score;
	  best_input = &nexts[i];
	}
      }

      printf("%8d best score %.2f (%.2f + %.2f future):\n", 
	     movie.size(),
	     best_score, best_immediate, best_future);
      // SimpleFM2::InputToString(best_input).c_str());

      // This is very likely to be cached now.
      Emulator::LoadUncompressed(&current_state);
      for (int j = 0; j < best_input->size(); j++) {
	Emulator::CachingStep((*best_input)[j]);
	movie.push_back((*best_input)[j]);
      }

      if (framenum % 10 == 0) {
	SimpleFM2::WriteInputs(GAME "-playfun-motif-progress.fm2", GAME ".nes",
			       // XXX
			       "base64:Ww5XFVjIx5aTe5avRpVhxg==",
			       // "base64:jjYwGG411HcjG/j9UOVM3Q==",
			       movie);
	objectives->SaveSVG(memories, GAME "-playfun.svg");
	Emulator::PrintCacheStats();
	printf("                     (wrote)\n");
      }
    }

    SimpleFM2::WriteInputs(GAME "-playfun-motif-final.fm2", GAME ".nes",
			   // XXX
			   "base64:Ww5XFVjIx5aTe5avRpVhxg==",
			   // "base64:jjYwGG411HcjG/j9UOVM3Q==",
			   movie);
  }

  // Used to ffwd to gameplay.
  vector<uint8> solution;
  // Contains the movie we record.
  vector<uint8> movie;

  ArcFour rc;
  WeightedObjectives *objectives;
  Motifs *motifs;
  vector< vector<uint8> > motifvec;
};

/**
 * The main loop for the SDL.
 */
int main(int argc, char *argv[]) {
  PlayFun pf;

  fprintf(stderr, "Starting...\n");

  pf.Greedy();

  Emulator::Shutdown();

  // exit the infrastructure
  FCEUI_Kill();
  return 0;
}
