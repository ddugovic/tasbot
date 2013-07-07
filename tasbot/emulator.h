/*
  Library interface to FCEUX.

  Ideally this would be a proper class (really no reason not to),
  but because FCEUX uses loads of global variables, this has
  to be treated as a singleton.
*/

#ifndef __EMULATOR_H
#define __EMULATOR_H

#include <algorithm>
#include <string>
#include <vector>
#include <zlib.h>
#ifdef __GNUC__
#include <tr1/unordered_map>
#include <tr1/unordered_set>
using std::tr1::unordered_map;
#else
#include <hash_map>
#include <hash_set>
#endif

#include "config.h"
#include "tasbot.h"
#include "util.h"

using namespace std;

struct Emulator {
  // Returns false upon error. Only initialize once.
  static bool Initialize(Config &config);
  // Calls some internal FCEUX stuff, probably not necessary.
  static void Shutdown();

  static void Save(vector<uint8> *out);
  // Doesn't modify its argument.
  static void Load(vector<uint8> *in);

  // Make one emulator step with the given input.
  // Bits from MSB to LSB are
  //    RLDUTSBA (Right, Left, Down, Up, sTart, Select, B, A)
  static void Step(uint8 inputs);

  // Copy the 0x800 bytes of RAM.
  static void GetMemory(vector<uint8> *mem);

  // Fancy stuff.

  // Reset the state cache. Set the maximum number of states that can
  // be stored. (A state is a starting state, an input, and the output
  // state that results.) The number of actual states stored can
  // exceed numstates by a constant amount called slop. Clears the cache.
  static void ResetCache(uint64 numstates, uint64 slop = 10000ULL);

  // Equivalent to Step. Does some extra work to consult the cache and
  // save the result, which may make it much faster. However, when
  // iterating steps, checking the cache and saving results are pure
  // overhead.
  static void CachingStep(uint8 input);

  static void PrintCacheStats();

  // States often only differ by a small amount, so a way to reduce
  // their entropy is to diff them against a representative savestate.
  // This gets an uncompressed basis for the current state, which can
  // be used in the SaveEx and LoadEx routines.
  static void GetBasis(vector<uint8> *out);

  // Save and load uncompressed. The memory will always be the same
  // size (Save and SaveEx may compress, which makes their output
  // significantly smaller), but this is the fastest in terms of CPU.
  static void SaveUncompressed(vector<uint8> *out);
  static void LoadUncompressed(vector<uint8> *in);

  // Save and load with a basis vector. The vector can contain anything, and
  // doesn't even have to be the same length as an uncompressed save state,
  // but a state needs to be loaded with the same basis as it was saved.
  // basis can be NULL, and then these behave the same as Save/Load.
  static void SaveEx(vector<uint8> *out, const vector<uint8> *basis);
  static void LoadEx(vector<uint8> *in, const vector<uint8> *basis);
};

#endif
