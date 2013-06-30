#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <limits.h>
#include <math.h>

#include <zlib.h>

#include "fceu/utils/md5.h"

#include "config.h"

#include "fceu/driver.h"
// #include "fceu/drivers/common/config.h"
#include "fceu/drivers/common/args.h"

#include "fceu/state.h"

#include "fceu/drivers/common/cheat.h"
#include "fceu/fceu.h"
#include "fceu/movie.h"
#include "fceu/version.h"

// #include "fceu/drivers/common/configSys.h"
#include "fceu/oldmovie.h"
#include "fceu/types.h"

#include "../cc-lib/util.h"

int CloseGame(void);

static int inited = 0;

int eoptions=0;

static void DriverKill(void);
static int DriverInitialize(FCEUGI *gi);

static int noconfig;

// Joystick data. I think used for both controller 0 and 1. Part of
// the "API".
static uint32 joydata = 0;

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

/**
 * Closes a game.  Frees memory, and deinitializes the drivers.
 */
int CloseGame() {
  FCEUI_CloseGame();
  DriverKill();
  GameInfo = 0;
  return 1;
}

static void DoFun(int frameskip) {
  uint8 *gfx;
  int32 *sound;
  int32 ssize;
  static int opause = 0;

  // fprintf(stderr, "In DoFun..\n");

  // Limited ability to skip video and sound.
  #define SKIP_VIDEO_AND_SOUND 2

  // Emulate a single frame.
  FCEUI_Emulate(NULL, &sound, &ssize, SKIP_VIDEO_AND_SOUND);

  // This was the only useful thing from Update. It's called multiple
  // times; I don't know why.
  // --- update input! --

  uint8 v = RAM[0x0009];
  uint8 s = RAM[0x000B];  // Should be 77.
  uint32 loc = (RAM[0x0080] << 24) |
    (RAM[0x0081] << 16) |
    (RAM[0x0082] << 8) |
    (RAM[0x0083]);
  // fprintf(stderr, "%02x %02x\n", v, s);

#if 0
  std::vector<uint8> savestate;
  EMUFILE_MEMORY ms(&savestate);
  // Compression yields 2x slowdown, but states go from ~80kb to 1.4kb
  // Without screenshot, ~1.3kb and only 40% slowdown
  FCEUSS_SaveMS(&ms, Z_DEFAULT_COMPRESSION /* Z_NO_COMPRESSION */);
  // TODO
  // Saving is not as efficient as we'd like for a pure in-memory operation
  //  - uses tags to tell you what's next, even though we could already know
  //  - takes care for endianness; no point
  //  - saves the backing buffer (write-only, used for display)
  //  - might save some other write-only data (sound?)
  //  - compresses the output using zlib, which may be good for space
  //    but not good for time!

  ms.trim();
  fprintf(stderr, "SS: %lld\n", savestate.size());
#endif

  // uint8 FCEUI_MemSafePeek(uint16 A);
  // void FCEUI_MemPoke(uint16 a, uint8 v, int hl);

//  if(opause!=FCEUI_EmulationPaused()) {
//    opause=FCEUI_EmulationPaused();
//    SilenceSound(opause);
//  }
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
 * Shut down all of the subsystem drivers: video, audio, and joystick.
 */
static void DriverKill() {
#if 0
  if (!noconfig)
    g_config->save();

  if(inited&2)
    KillJoysticks();
  if(inited&4)
    KillVideo();
  if(inited&1)
    KillSound();
  inited=0;
#endif
}

/**
 * Update the video, audio, and input subsystems with the provided
 * video (XBuf) and audio (Buffer) information.
 */
void FCEUD_Update(uint8 *XBuf, int32 *Buffer, int Count) {
}

static int64 DumpMem() {
  for (int i = 0; i < 0x800; i++) {
    fprintf(stderr, "%02x", (uint8)RAM[i]);
    // if (i % 40 == 0) fprintf(stderr, "\n");
  }
  md5_context ctx;
  md5_starts(&ctx);
  md5_update(&ctx, RAM, 0x800);
  uint8 digest[16];
  md5_finish(&ctx, digest);
  fprintf(stderr, "  MD5: ");
  for (int i = 0; i < 16; i++)
    fprintf(stderr, "%02x", digest[i]);
  fprintf(stderr, "\n");
  return *(int64*)digest;
}

/**
 * The main loop for the SDL.
 */
int main(int argc, char *argv[]) {
  int error, frameskip;

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

  if (argc != 2) {
    fprintf(stderr, "Need a ROM on the command line, and nothing else.\n");
    return -1;
  }
  
  const char *romfile = argv[1];

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
  if (1 != LoadGame(romfile)) {
    DriverKill();
    return -1;
  }


  // Default.
  newppu = 0;

  // Default.
  frameskip = 0;

  // loop playing the game

#define BENCHMARK 1

#ifdef BENCHMARK
  for (int i = 0; i < 20000; i++) {
    if (!GameInfo) {
      fprintf(stderr, "Gameinfo became null?\n");
      return -1;
    }
    DoFun(frameskip);
  }

  if (0x3f55c3584d2c71ecLL != DumpMem()) {
    fprintf(stderr, "WRONG CHECKSUM\n");
    return -1;
  } else {
    fprintf(stderr, "OK.\n");
  }

#else
  while(GameInfo)
    DoFun(frameskip);
#endif

  CloseGame();

  // exit the infrastructure
  FCEUI_Kill();
  return 0;
}
