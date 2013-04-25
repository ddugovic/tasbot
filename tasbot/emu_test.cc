/* This tests the interface to the emulator library
   for correctness, by playing back the movie karate.fm2
   against the rom karate.nes, and checking that
   the game is won and that the RAM has the right
   contents. It also does some simple timing. */

#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "fceu/utils/md5.h"

#include "config.h"

#include "fceu/driver.h"
#include "fceu/drivers/common/args.h"

#include "fceu/state.h"

#include "fceu/fceu.h"
#include "fceu/types.h"

#include "../cc-lib/util.h"
#include "../cc-lib/timer.h"

#include "simplefm2.h"
#include "emulator.h"
#include "basis-util.h"
#include "tasbot.h"

static void CheckLoc(int frame, uint32 expected) {
  fprintf(stderr, "Frame %d expect %u\n", frame, expected);
  uint32 loc = (RAM[0x0080] << 24) |
    (RAM[0x0081] << 16) |
    (RAM[0x0082] << 8) |
    (RAM[0x0083]);
  if (loc != expected) {
    fprintf(stderr, "At frame %d, expected %u, got %u\n",
	    frame, expected, loc);
    abort();
  }
}

// Note that fceu_frame is 1 plus the index in the input loop,
// because the UI displays the first frame as #1.
static void CheckCheckpoints(int fceu_frame) {
  // XXX read from golden file.
  switch (fceu_frame) {
  case 20: CheckLoc(fceu_frame, 0); break;
  case 21: CheckLoc(fceu_frame, 65536); break;
  case 4935: CheckLoc(fceu_frame, 196948); break;
  case 7674: CheckLoc(fceu_frame, 200273); break;
  case 7675: CheckLoc(fceu_frame, 200274); break;
  case 8123: CheckLoc(fceu_frame, 262144); break;
  case 11213: CheckLoc(fceu_frame, 265916); break;
  default:;
  }
}

static uint64 CrapHash(int a) {
  uint64 ret = ~a;
  ret *= 31337;
  ret ^= 0xDEADBEEF;
  ret = (ret >> 17) | (ret << (64 - 17));
  ret -= 911911911911;
  ret *= 65537;
  ret ^= 0xCAFEBABE;
  return ret;
}

static bool CompareByHash(int a, int b) {
  return CrapHash(a) < CrapHash(b);
}

int main(int argc, char *argv[]) {
  Emulator::Initialize("karate.nes");
  // loop playing the game
  vector<uint8> inputs = SimpleFM2::ReadInputs("karate.fm2");

  vector<uint8> basis = BasisUtil::LoadOrComputeBasis(inputs, 4935, "karate.basis");

  // The nth savestate is from before issuing the nth input.
  vector< vector<uint8> > savestates;

  vector<uint8> beginning;
  Emulator::Save(&beginning);

  int64 ss_total = 0;

  // XXXXXX
  // inputs.resize(10);

  fprintf(stderr, "Running %d steps...\n", inputs.size());
  for (int i = 0; i < inputs.size(); i++) {
    // XXX don't think this should ever happen.
    if (!GameInfo) {
      fprintf(stderr, "Gameinfo became null?\n");
      return -1;
    }

    vector<uint8> v;
    Emulator::SaveEx(&v, &basis);
    ss_total += v.size();
    savestates.push_back(v);

    Emulator::Step(inputs[i]);

    // The FCEUX UI indexes frames starting at 1.
    CheckCheckpoints(i + 1);
  }

#if 0
  /*
  PrintSavestate(savestates[0]);
  PrintSavestate(savestates[4935]);
  PrintSavestate(savestates[8123]);
  */

  if (savestates.size() > 9000) {
    vector<uint8> diff;
    for (int i = 0; i < savestates[4935].size(); i++) {
      diff.push_back(savestates[8123][i] - savestates[4935][i]);
    }

    PrintSavestate(diff);
  }
#endif

  if (0x30ea6ab51357e746 == Emulator::RamChecksum()) {
    fprintf(stderr, "Memory OK.\n");
  } else {
    fprintf(stderr, "WRONG CHECKSUM %x\n",
	    Emulator::RamChecksum());
    return -1;
  }

  fprintf(stderr, "\nTest random replay of savestates:\n");
  // Now run through each state in random order. Load it, then execute a step,
  // then check that we get to the same state as before.
  vector<int> order;
  for (int i = 0; i < inputs.size(); i++) {
    order.push_back(i);
  }
  std::sort(order.begin(), order.end(), CompareByHash);
  for (int i = 0; i < order.size(); i++) {
    int frame = order[i];
    Emulator::LoadEx(&savestates[frame], &basis);
    Emulator::Step(inputs[frame]);
    vector<uint8> res;
    Emulator::SaveEx(&res, &basis);
    CheckCheckpoints(frame + 1);
    if (frame + 1 < savestates.size()) {
      const vector<uint8> &expected = savestates[frame + 1];
      if (res != expected) {
	fprintf(stderr, "Got a different savestate from frame %d to %d.\n",
		frame, frame + 1);
	abort();
      }
    }
  }
  fprintf(stderr, "Savestates are ok.\n");

  fprintf(stderr, "Total for %d savestates: %.2fmb (avg %.2f bytes)\n",
          savestates.size(), ss_total / (1024.0 * 1024.0),
	  ss_total / (double)savestates.size());

  // Again with caching.
  Emulator::ResetCache(100, 10);

  for (int i = 0; i < order.size(); i++) {
    int idx = i;
    int frame = order[idx];
    Emulator::LoadEx(&savestates[frame], &basis);
    Emulator::CachingStep(inputs[frame]);
    vector<uint8> res;
    Emulator::SaveEx(&res, &basis);
    CheckCheckpoints(frame + 1);
    if (frame + 1 < savestates.size()) {
      const vector<uint8> &expected = savestates[frame + 1];
      if (res != expected) {
	fprintf(stderr, "Got a different savestate from "
		"frame %d to %d. (caching version)\n",
		frame, frame + 1);
	abort();
      }
    }
  }

  CHECK(order.size() > 150);
  for (int i = 0; i < order.size(); i++) {
    int idx = i % 150;
    int frame = order[idx];
    Emulator::LoadEx(&savestates[frame], &basis);
    Emulator::CachingStep(inputs[frame]);
    vector<uint8> res;
    Emulator::SaveEx(&res, &basis);
    CheckCheckpoints(frame + 1);
    if (frame + 1 < savestates.size()) {
      const vector<uint8> &expected = savestates[frame + 1];
      if (res != expected) {
	fprintf(stderr, "Got a different savestate from "
		"frame %d to %d. (caching version #2)\n",
		frame, frame + 1);
	abort();
      }
    }
  }

  fprintf(stderr, "\nTiming tests.\n");

  Emulator::Load(&beginning);
  {
    uint64 cxsum = 0x0;
    Timer steps;
    static int kNumSteps = 20000;
    for (int i = 0; i < kNumSteps; i++) {
      Emulator::Step(i & 255);
      cxsum += RAM[i % 0x800];
    }
    steps.Stop();
    fprintf(stderr, "%.8f seconds per step %d\n", 
	    (double)steps.Seconds() / (double)kNumSteps,
	    cxsum);
  }

  Emulator::Load(&beginning);
  Emulator::ResetCache(50000, 10);
  {
    uint64 cxsum = 0x0;
    Timer steps;
    static int kNumSteps = 20000;
    for (int i = 0; i < kNumSteps; i++) {
      Emulator::CachingStep(i & 255);
      cxsum += RAM[i % 0x800];
    }
    steps.Stop();
    fprintf(stderr, "%.8f seconds per caching step (miss)%d\n", 
	    (double)steps.Seconds() / (double)kNumSteps,
	    cxsum);
  }

  Emulator::Load(&beginning);
  {
    uint64 cxsum = 0x0;
    Timer steps;
    static int kNumSteps = 20000;
    for (int i = 0; i < kNumSteps; i++) {
      Emulator::CachingStep(i & 255);
      cxsum += RAM[i % 0x800];
    }
    steps.Stop();
    fprintf(stderr, "%.8f seconds per caching step (hit)%d\n", 
	    (double)steps.Seconds() / (double)kNumSteps,
	    cxsum);
  }

  Emulator::Load(&beginning);
  {
    uint64 cxsum = 0x0;
    Timer loads;
    static int kNumLoads = 20000;
    for (int i = 0; i < kNumLoads; i++) {
      Emulator::Load(&beginning);
      cxsum += RAM[i % 0x800];
    }
    loads.Stop();
    fprintf(stderr, "%.8f seconds per Load (regular) %d\n", 
	    (double)loads.Seconds() / (double)kNumLoads,
	    cxsum);
  }

  Emulator::Load(&beginning);
  {
    uint64 cxsum = 0x0;
    vector<uint8> saveme;
    Timer saves;
    static int kNumSaves = 20000;
    for (int i = 0; i < kNumSaves; i++) {
      Emulator::Save(&saveme);
      cxsum += RAM[i % 0x800];
      cxsum += saveme[i % saveme.size()];
    }
    saves.Stop();
    fprintf(stderr, "%.8f seconds per Save (regular) %d\n", 
	    (double)saves.Seconds() / (double)kNumSaves,
	    cxsum);
  }

  Emulator::Load(&beginning);
  vector<uint8> uncompressed;
  Emulator::SaveUncompressed(&uncompressed);
  {
    uint64 cxsum = 0x0;
    Timer loads;
    static int kNumLoads = 20000;
    for (int i = 0; i < kNumLoads; i++) {
      Emulator::LoadUncompressed(&uncompressed);
      cxsum += RAM[i % 0x800];
    }
    loads.Stop();
    fprintf(stderr, "%.8f seconds per Load (uncompressed) %d\n", 
	    (double)loads.Seconds() / (double)kNumLoads,
	    cxsum);
  }

  Emulator::Load(&beginning);
  {
    uint64 cxsum = 0x0;
    vector<uint8> saveme;
    Timer saves;
    static int kNumSaves = 20000;
    for (int i = 0; i < kNumSaves; i++) {
      Emulator::SaveUncompressed(&saveme);
      cxsum += RAM[i % 0x800];
      cxsum += saveme[i % saveme.size()];
    }
    saves.Stop();
    fprintf(stderr, "%.8f seconds per Save (uncompressed) %d\n", 
	    (double)saves.Seconds() / (double)kNumSaves,
	    cxsum);
  }

  Emulator::Shutdown();

  // exit the infrastructure
  FCEUI_Kill();

  fprintf(stderr, "SUCCESS.\n");
  return 0;
}
