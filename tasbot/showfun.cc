/* This program attempts to learn an objective function for a
   particular game by watching movies of people playing it. The
   objective function can then be used by (NONEXISTENT PROGRAM)
   to try to play the game.
 */

#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>

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
#include "objective.h"
#include "weighted-objectives.h"

static void SaveMemory(vector< vector<uint8> > *memories) {
  memories->resize(memories->size() + 1);
  vector<uint8> *v = &memories->back();
  for (int i = 0; i < 0x800; i++) {
    v->push_back((uint8)RAM[i]);
  }
}

struct MemSpan {
  void Observe(int idx, const vector< vector<uint8> > &memories,
	       const vector<int> &ordering) {
    vector<uint8> thisrow;
    for (int i = 0; i < ordering.size(); i++) {
      int p = ordering[i];
      thisrow.push_back(memories[idx][p]);
    }

    if (low == -1) {
      low = high = idx;
      values = thisrow;
    } else if (thisrow != values || idx != high + 1) {
      Flush();
      low = high = idx;
      values = thisrow;
    } else {
      high++;
    }
  }
  void Flush() {
    if (low == high) {
      printf("        %6d: ", low);
    } else {
      printf("%6d -""%6d: ", low, high); 
    }

    for (int i = 0; i < values.size(); i++) {
      printf(" %3d ", values[i]);
    }
    printf("\n");

    low = high = -1;
    values.clear();
  }

  MemSpan() : low(-1), high(-1) {}
  int low, high;
  vector<uint8> values;
};

int main(int argc, char *argv[]) {
  Emulator::Initialize("mario.nes");
  vector<uint8> movie = SimpleFM2::ReadInputs("mario.fm2");

  vector< vector<uint8> > memories;
  memories.reserve(movie.size() + 1);

  // The very beginning of the game starts with RAM initialization,
  // which we really should ignore for building an objective function.
  // So skip until there's a button press in the movie.
  size_t start = 0;

  #if 0
  if (true || argc == 1) {
    printf("Skipping frames without argument.\n");
    while (movie[start] == 0 && start < movie.size()) {
      Emulator::Step(movie[start]);
      start++;
    }
  }

  printf("Skipped %ld frames until first keypress.\n"
	 "Playing %ld frames...\n", start, movie.size() - start);
  #endif

  SaveMemory(&memories);

  for (int i = start; i < movie.size(); i++) {
    if (i % 1000 == 0) {
      printf("  [% 3.1f%%] %d/%ld\n", 
	     ((100.0 * i) / movie.size()), i, movie.size());
      // exit(0);
    }
    Emulator::Step(movie[i]);
    SaveMemory(&memories);
  }

  printf("Recorded %ld memories.\n", memories.size());


  WeightedObjectives *objectives = LoadFromFile("mario.objectives");
  CHECK(objectives);

#if 0
    MakeObjectives(memories);
  } else {
    vector<int> ordering;
    printf("      " "     " "     ");
    for (int c = 1; c < argc; c++) {
      int loc = atoi(argv[c]);
      ordering.push_back(loc);
      printf("%04x ", loc);
    }
    printf("\n");
    MemSpan m;
    for (int i = 0; i < memories.size(); i++) {
      m.Observe(i, memories, ordering);
    }
    m.Flush();
  }
#endif  

  Emulator::Shutdown();

  // exit the infrastructure
  FCEUI_Kill();
  return 0;
}
