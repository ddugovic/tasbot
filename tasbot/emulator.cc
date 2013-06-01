
#include "emulator.h"

#include <algorithm>
#include <string>
#include <vector>
#include <zlib.h>
#ifdef __GNUC__
#include <tr1/unordered_map>
using tr1::unordered_map;
#else
#include <unordered_map>
#endif

#include "config.h"
#include "fceu/driver.h"
#include "fceu/fceu.h"
#include "fceu/types.h"
#include "fceu/utils/md5.h"
#include "fceu/version.h"
#include "fceu/state.h"

#include "tasbot.h"
#include "../cc-lib/city/city.h"

// Joystick data. I think used for both controller 0 and 1. Part of
// the "API".
static uint32 joydata = 0;
static bool initialized = false;

struct StateCache {
  // These vectors are allocated with new.
  // Input and starting state (uncompresed).
  typedef pair<uint8, const vector<uint8> *> Key;
  // Sequence number and output state (uncompressed).
  typedef pair<uint64, vector<uint8> *> Value;

  struct HashFunction {
    size_t operator ()(const Key &k) const {
      CHECK(k.second);
      return CityHash64WithSeed((const char *)&(k.second->at(0)),
				k.second->size(),
				k.first);
    }
  };

  // Use value equality on the vectors, not pointer equality
  // (which would be the default for ==).
  struct KeyEquals {
    size_t operator ()(const Key &l, const Key &r) const {
      return l.first == r.first &&
	*l.second == *r.second;
    }
  };

  typedef unordered_map<Key, Value, HashFunction, KeyEquals> Hash;

  StateCache() : limit(0ULL), count(0ULL), next_sequence(0ULL), 
		 slop(10000ULL), hits(0ULL), misses(0ULL) {
  }

  void Resize(uint64 ll, uint64 ss) {
    printf("Resize cache %llu %llu\n", ll, ss);
    // Recover memory.
    for (Hash::iterator it = hashtable.begin(); 
	 it != hashtable.end(); /* in loop */) {
      Hash::iterator next(it);
      ++next;
      delete it->first.second;
      delete it->second.second;
      hashtable.erase(it);
      it = next;
    }
    CHECK(hashtable.size() == 0);

    limit = ll;
    slop = ss;
    CHECK(limit >= 0);
    CHECK(slop >= 0);
    hits = misses = next_sequence = count = 0ULL;
    printf("OK.\n");
  }

  // Even if Remember() doesn't leak it consumes gigabytes of memory.
  // Assumes it's not present. If it is, then you'll leak.
  void Remember(uint8 input, const vector<uint8> &start,
		const vector<uint8> &result) {
    vector<uint8> *startcopy = new vector<uint8>(start);
    vector<uint8> *resultcopy = new vector<uint8>(result);
    pair<Hash::iterator, bool> it =
      hashtable.insert(make_pair(make_pair(input, startcopy), 
				 make_pair(next_sequence++, resultcopy)));
    CHECK(it.second);
    CHECK(NULL != GetKnownResult(input, *startcopy));
    CHECK(NULL != GetKnownResult(input, start));
    count++;
    MaybeResize();
  }

  // Return a pointer to the result state (and update its LRU
  // sequence) or NULL if it is not known.
  vector<uint8> *GetKnownResult(uint8 input, const vector<uint8> &start) {
    Hash::iterator it = hashtable.find(make_pair(input, &start));
    if (it == hashtable.end()) {
      misses++;
      return NULL;
    }

    hits++;
    it->second.first = next_sequence++;
    return it->second.second;
  }

  void MaybeResize() {
    // Don't always do this, since it is linear time.
    if (count > limit + slop) {
      const int num_to_remove = count - limit;
      printf("Resizing (currently %llu) to remove %d\n", count, num_to_remove);

      // PERF: This can be done much more efficiently using a flat
      // heap.
      vector<uint64> all_sequences;
      all_sequences.reserve(count);

      // First pass, get the num_to_remove oldest (lowest) sequences.
      for (Hash::const_iterator it = hashtable.begin(); 
	   it != hashtable.end(); ++it) {
	all_sequences.push_back(it->second.first);
      }
      std::sort(all_sequences.begin(), all_sequences.end());
      
      CHECK(num_to_remove < all_sequences.size());
      const uint64 minseq = all_sequences[num_to_remove];

      printf("Removing everything below %llu\n", minseq);

      for (Hash::iterator it = hashtable.begin(); it != hashtable.end(); 
	   /* in loop */) {
	if (it->second.first < minseq) {
	  Hash::iterator next(it);
	  ++next;
	  delete it->first.second;
	  delete it->second.second;
	  // Note g++ does not return the "next" iterator.
	  hashtable.erase(it);
	  count--;
	  it = next;
	} else {
	  ++it;
	}
      }
      printf("Size is now %llu (internally %zu)\n", count, hashtable.size());
    }
  }

  void PrintStats() {
    printf("Current cache size: %llu / %llu. next_seq %llu\n"
	   "%llu hits and %llu misses\n", 
	   count, limit, next_sequence,
	   hits, misses);
  }

  Hash hashtable;
  uint64 limit;
  uint64 count;
  uint64 next_sequence;
  // Number of states I'm allowed to be over my limit before
  // forcing a GC.
  uint64 slop;

  uint64 hits, misses;
};
static StateCache *cache = NULL;

void Emulator::GetMemory(vector<uint8> *mem) {
  mem->resize(0x800);
  memcpy(&((*mem)[0]), RAM, 0x800);
}

uint64 Emulator::RamChecksum() {
  md5_context ctx;
  md5_starts(&ctx);
  md5_update(&ctx, RAM, 0x800);
  uint8 digest[16];
  md5_finish(&ctx, digest);
  uint64 res = 0;
  for (int i = 0; i < 8; i++) {
    res <<= 8;
    res |= 255 & digest[i];
  }
  return res;
}

/**
 * Initialize all of the subsystem drivers: video, audio, and joystick.
 */
static int DriverInitialize(FCEUGI *gi) {
  // Used to init video. I think it's safe to skip.

  // Here we initialized sound. Assuming it's safe to skip,
  // because of an early return if config turned it off.

  // Used to init joysticks. Don't care about that.

  // No fourscore support.
  // eoptions &= ~EO_FOURSCORE;

  // Why do both point to the same joydata? -tom
  FCEUI_SetInput (0, SI_GAMEPAD, &joydata, 0);
  FCEUI_SetInput (1, SI_GAMEPAD, &joydata, 0);

  FCEUI_SetInputFourscore (false);
  return 1;
}

/**
 * Closes a game.  Frees memory, and deinitializes the drivers.
 */
int CloseGame() {
  FCEUI_CloseGame();
  GameInfo = 0;
  return 1;
}

/**
 * Loads a game, given a full path/filename.  The driver code must be
 * initialized after the game is loaded, because the emulator code
 * provides data necessary for the driver code(number of scanlines to
 * render, what virtual input devices to use, etc.).
 */
int LoadGame(const char *path) {
  CloseGame();
  if(!FCEUI_LoadGame(path, 1)) {
    return 0;
  }

  // Here we used to do ParseGIInput, which allows the gameinfo
  // to override our input config, or something like that. No
  // weird stuff. Skip it.

  // RefreshThrottleFPS();

  if(!DriverInitialize(GameInfo)) {
    return(0);
  }
	
  // Set NTSC (1 = pal)
  FCEUI_SetVidSystem(GIV_NTSC);

  return 1;
}

void Emulator::Shutdown() {
  CloseGame();
}

bool Emulator::Initialize(const string &romfile) {
  if (initialized) {
    fprintf(stderr, "Already initialized.\n");
    abort();
    return false;
  }

  cache = new StateCache;

  int error;

  fprintf(stderr, "Starting " FCEU_NAME_AND_VERSION "...\n");

  // (Here's where SDL was initialized.)

  // Initialize the configuration system
  InitConfig();
  if (!global_config) {
    return -1;
  }

  // initialize the infrastructure
  error = FCEUI_Initialize();
  if (error != 1) {
    fprintf(stderr, "Error initializing.\n");
    return -1;
  }

  // (init video was here.)
  // I don't think it's necessary -- just sets up the SDL window and so on.

  // (input config was here.) InputCfg(string value of --inputcfg)

  // UpdateInput(g_config) was here.
  // This is just a bunch of fancy stuff to choose which controllers we have
  // and what they're mapped to.
  // I think the important functions are FCEUD_UpdateInput()
  // and FCEUD_SetInput
  // Calling FCEUI_SetInputFC ((ESIFC) CurInputType[2], InputDPtr, attrib);
  //   and FCEUI_SetInputFourscore ((eoptions & EO_FOURSCORE) != 0);
	
  // No HUD recording to AVI.
  FCEUI_SetAviEnableHUDrecording(false);

  // No Movie messages.
  FCEUI_SetAviDisableMovieMessages(false);

  // defaults
  const int ntsccol = 0, ntsctint = 56, ntschue = 72;
  FCEUI_SetNTSCTH(ntsccol, ntsctint, ntschue);

  // Set NTSC (1 = pal)
  FCEUI_SetVidSystem(GIV_NTSC);

  FCEUI_SetGameGenie(0);

  // Default. Sound thing.
  FCEUI_SetLowPass(0);

  // Default.
  FCEUI_DisableSpriteLimitation(1);

  // Defaults.
  const int scanlinestart = 0, scanlineend = 239;

  FCEUI_SetRenderedLines(scanlinestart + 8, scanlineend - 8, 
			 scanlinestart, scanlineend);


  {
    extern int input_display, movieSubtitles;
    input_display = 0;
    extern int movieSubtitles;
    movieSubtitles = 0;
  }

  // Load the game.
  if (1 != LoadGame(romfile.c_str())) {
    fprintf(stderr, "Couldn't load [%s]\n", romfile.c_str());
    return false;
  }


  // Default.
  newppu = 0;

  initialized = true;
  return true;
}

// Make one emulator step with the given input.
// Bits from MSB to LSB are
//    RLDUTSBA (Right, Left, Down, Up, sTart, Select, B, A)
void Emulator::Step(uint8 inputs) {
  int32 *sound;
  int32 ssize;

  // The least significant byte is player 0 and
  // the bits are in the same order as in the fm2 file.
  joydata = (uint32) inputs;

  // Limited ability to skip video and sound.
  const int SKIP_VIDEO_AND_SOUND = 2;

  // Emulate a single frame.
  FCEUI_Emulate(NULL, &sound, &ssize, SKIP_VIDEO_AND_SOUND);
}

void Emulator::Save(vector<uint8> *out) {
  SaveEx(out, NULL);
}

void Emulator::GetBasis(vector<uint8> *out) {
  FCEUSS_SaveRAW(out);
}

void Emulator::SaveUncompressed(vector<uint8> *out) {
  FCEUSS_SaveRAW(out);
}

void Emulator::LoadUncompressed(vector<uint8> *in) {
  if (!FCEUSS_LoadRAW(in)) {
    fprintf(stderr, "Couldn't restore from state\n");
    abort();
  }
}

void Emulator::Load(vector<uint8> *state) {
  LoadEx(state, NULL);
}

// Compression yields 2x slowdown, but states go from ~80kb to 1.4kb
// Without screenshot, ~1.3kb and only 40% slowdown
// XXX External interface now allows client to specify, so maybe just
// make this a guarantee.
#define USE_COMPRESSION 0

#if USE_COMPRESSION

// Save and load with a basis vector. The vector can contain anything, and
// doesn't even have to be the same length as an uncompressed save state,
// but a state needs to be loaded with the same basis as it was saved.
// basis can be NULL, and then these behave the same as Save/Load.
void Emulator::SaveEx(vector<uint8> *state, const vector<uint8> *basis) {
  // TODO
  // Saving is not as efficient as we'd like for a pure in-memory operation
  //  - uses tags to tell you what's next, even though we could already know
  //  - takes care for endianness; no point
  //  - might save some other write-only data (sound?)

  vector<uint8> raw;
  FCEUSS_SaveRAW(&raw);

  // Encode.
  int blen = (basis == NULL) ? 0 : (min(basis->size(), raw.size()));
  for (int i = 0; i < blen; i++) {
    raw[i] -= (*basis)[i];
  }

  // Compress.
  int len = raw.size();
  // worst case compression:
  // zlib says "0.1% larger than sourceLen plus 12 bytes"
  uLongf comprlen = (len >> 9) + 12 + len;

  // Make sure there is contiguous space. Need room for header too.
  state->resize(4 + comprlen);

  if (Z_OK != compress2(&(*state)[4], &comprlen, &raw[0], len, 
			Z_DEFAULT_COMPRESSION)) {
    fprintf(stderr, "Couldn't compress.\n");
    abort();
  }

  *(uint32*)&(*state)[0] = len;

  // Trim to what we actually needed.
  // PERF: This almost certainly does not actually free the memory. 
  // Might need to copy.
  state->resize(4 + comprlen);
}

// Save and load with a basis vector. The vector can contain anything, and
// doesn't even have to be the same length as an uncompressed save state,
// but a state needs to be loaded with the same basis as it was saved.
// basis can be NULL, and then these behave the same as Save/Load.
void Emulator::LoadEx(vector<uint8> *state, const vector<uint8> *basis) {
  // Decompress. First word tells us the decompressed size.
  int uncomprlen = *(uint32*)&(*state)[0];
  vector<uint8> uncompressed;
  uncompressed.resize(uncomprlen);
 
  switch (uncompress(&uncompressed[0], (uLongf*)&uncomprlen,
		     &(*state)[4], state->size() - 4)) {
  case Z_OK: break;
  case Z_BUF_ERROR:
    fprintf(stderr, "Not enough room in output\n");
    abort();
    break;
  case Z_MEM_ERROR:
    fprintf(stderr, "Not enough memory\n");
    abort();
    break;
  default:
    fprintf(stderr, "Unknown decompression error\n");
    abort();
    break;
  }
  // fprintf(stderr, "After uncompression: %d\n", uncomprlen);
  
  // Why doesn't this equal the result from before?
  uncompressed.resize(uncomprlen);

  // Decode.
  int blen = (basis == NULL) ? 0 : (min(basis->size(), uncompressed.size()));
  for (int i = 0; i < blen; i++) {
    uncompressed[i] += (*basis)[i];
  }

  if (!FCEUSS_LoadRAW(&uncompressed)) {
    fprintf(stderr, "Couldn't restore from state\n");
    abort();
  }
}

#else

// When compression is disabled, we ignore the basis (no point) and
// don't store any size header. These functions become very simple.
void Emulator::SaveEx(vector<uint8> *state, const vector<uint8> *basis) {
  FCEUSS_SaveRAW(state);
}

void Emulator::LoadEx(vector<uint8> *state, const vector<uint8> *basis) {
  if (!FCEUSS_LoadRAW(state)) {
    fprintf(stderr, "Couldn't restore from state\n");
    abort();
  }
}


#endif

// Cache stuff.
// Consumes gigabytes of memory with uncompressed data.  It seems as if
// state (or "start" and "result") data could be compressed except during
// Step(), and algorithm wouldn't be affected.

// static
void Emulator::ResetCache(uint64 numstates, uint64 slop) {
#ifndef NOEMUCACHE
  CHECK(cache != NULL);
  cache->Resize(numstates, slop);
#endif
}

// static
void Emulator::CachingStep(uint8 input) {
#ifndef NOEMUCACHE
  vector<uint8> start;
  Save(&start);
  if (vector<uint8> *cached = cache->GetKnownResult(input, start)) {
    Load(cached);
  } else {
#endif
    Step(input);
#ifndef NOEMUCACHE
    vector<uint8> result;
    Save(&result);
    cache->Remember(input, start, result);

    // PERF
    CHECK(NULL != cache->GetKnownResult(input, start));
  }
  if ((cache->hits + cache->misses) % 10000 == 0) {
    PrintCacheStats();
  }
#endif
}

void Emulator::PrintCacheStats() {
  CHECK(cache != NULL);
  cache->PrintStats();
}
