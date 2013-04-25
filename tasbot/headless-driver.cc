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

#include "fceu/driver.h"
// #include "fceu/drivers/common/config.h"
#include "fceu/drivers/common/args.h"

#include "fceu/drivers/common/cheat.h"
#include "fceu/fceu.h"
#include "fceu/movie.h"
#include "fceu/version.h"

// #include "fceu/drivers/common/configSys.h"
#include "fceu/oldmovie.h"
#include "fceu/types.h"

extern double g_fpsScale;

int CloseGame(void);

// external dependencies
bool turbo = false;
int closeFinishedMovie = 0;

int gametype = 0;

/**
 * Prints an error string to STDOUT.
 */
void FCEUD_PrintError(char *s) {
  puts(s);
}

/**
 * Prints the given string to STDOUT.
 */
void FCEUD_Message(char *s) {
  fputs(s, stdout);
}

/**
 * Opens a file, C++ style, to be read a byte at a time.
 */
FILE *FCEUD_UTF8fopen(const char *fn, const char *mode) {
  return fopen(fn,mode);
}

/**
 * Opens a file to be read a byte at a time.
 */
EMUFILE_FILE* FCEUD_UTF8_fstream(const char *fn, const char *m)
{
  std::ios_base::openmode mode = std::ios_base::binary;
  if(!strcmp(m,"r") || !strcmp(m,"rb"))
    mode |= std::ios_base::in;
  else if(!strcmp(m,"w") || !strcmp(m,"wb"))
    mode |= std::ios_base::out | std::ios_base::trunc;
  else if(!strcmp(m,"a") || !strcmp(m,"ab"))
    mode |= std::ios_base::out | std::ios_base::app;
  else if(!strcmp(m,"r+") || !strcmp(m,"r+b"))
    mode |= std::ios_base::in | std::ios_base::out;
  else if(!strcmp(m,"w+") || !strcmp(m,"w+b"))
    mode |= std::ios_base::in | std::ios_base::out | std::ios_base::trunc;
  else if(!strcmp(m,"a+") || !strcmp(m,"a+b"))
    mode |= std::ios_base::in | std::ios_base::out | std::ios_base::app;
  return new EMUFILE_FILE(fn, m);
  //return new std::fstream(fn,mode);
}

/**
 * Returns the compiler string.
 */
const char *FCEUD_GetCompilerString() {
  return "g++ " __VERSION__;
}

void FCEUD_DebugBreakpoint() {}
void FCEUD_TraceInstruction() {}

// Prints a textual message without adding a newline at the end.
void FCEUD_Message(const char *text) {
  fputs(text, stdout);
}

// Print error to stderr.
void FCEUD_PrintError(const char *errormsg) {
  fprintf(stderr, "%s\n", errormsg);
}

// dummy functions

#define DUMMY(__f) void __f(void) {printf("%s\n", #__f); FCEU_DispMessage("Not implemented.",0);}
DUMMY(FCEUD_HideMenuToggle)
DUMMY(FCEUD_MovieReplayFrom)
DUMMY(FCEUD_ToggleStatusIcon)
DUMMY(FCEUD_AviRecordTo)
DUMMY(FCEUD_AviStop)
void FCEUI_AviVideoUpdate(const unsigned char* buffer) { }
int FCEUD_ShowStatusIcon(void) {return 0;}
bool FCEUI_AviIsRecording(void) {return false;}
void FCEUI_UseInputPreset(int preset) { }
bool FCEUD_PauseAfterPlayback() { return false; }
FCEUFILE* FCEUD_OpenArchiveIndex(ArchiveScanRecord& asr, std::string &fname, int innerIndex) { return 0; }
FCEUFILE* FCEUD_OpenArchive(ArchiveScanRecord& asr, std::string& fname, std::string* innerFilename) { return 0; }
ArchiveScanRecord FCEUD_ScanArchive(std::string fname) { return ArchiveScanRecord(); }

// tom7 dummy
void FCEUD_VideoChanged() {}
void FCEUD_SetEmulationSpeed(int) {}
void FCEUD_SoundVolumeAdjust(int) {}

void FCEUD_SaveStateAs() {
  abort();
  // FCEUI_SaveState("FAKE");
}
void FCEUD_LoadStateFrom() {
  abort();
  // FCEUI_LoadState("FAKE");
}

void FCEUD_MovieRecordTo() {
  abort();
  // FCEUI_SaveMovie ("FAKEMOVIE", MOVIE_FLAG_FROM_POWERON, NULL);
}

void FCEUD_SoundToggle() {}

// for movie playback?
void FCEUD_SetInput (bool fourscore, bool microphone, ESI port0, ESI port1, ESIFC fcexp) {
  abort();
}

int FCEUDnetplay=0;

int FCEUD_NetworkConnect(void) {
  abort();
}

int FCEUD_SendData(void *data, uint32 len) {
  abort();
}

int FCEUD_RecvData(void *data, uint32 len) {
  abort();
  return 0;
}

void FCEUD_NetworkClose(void) {}

void FCEUD_NetplayText(uint8 *text) {}


void FCEUD_SetPalette(uint8 index, uint8 r, uint8 g, uint8 b) {}

// Gets the color for a particular index in the palette.
void FCEUD_GetPalette(uint8 index, uint8 *r, uint8 *g, uint8 *b) {}

bool FCEUI_AviEnableHUDrecording() { return false; }
void FCEUI_SetAviEnableHUDrecording(bool enable) {}

// ?
bool FCEUI_AviDisableMovieMessages() { return false; }
void FCEUI_SetAviDisableMovieMessages(bool disable) {}

bool FCEUD_ShouldDrawInputAids() { return false; }

// morally FCEUD_
unsigned int *GetKeyboard() {
  abort();
}

void FCEUD_TurboOn() {  }
void FCEUD_TurboOff() { }
void FCEUD_TurboToggle() { }

void FCEUD_DebugBreakpoint(int bp_num) {}
void FCEUD_TraceInstruction(unsigned char*, int) {}


/**
 * Update the video, audio, and input subsystems with the provided
 * video (XBuf) and audio (Buffer) information.
 */
void FCEUD_Update(uint8 *XBuf, int32 *Buffer, int Count) {}

/**
 * Get the time in ticks.
 */
uint64 FCEUD_GetTime() {
  //	return SDL_GetTicks();
  fprintf(stderr, "(FCEUD_GetTime) In headless mode, nothing should "
	  "try to do timing.\n");
  abort();
  return 0;
}

/**
 * Get the tick frequency in Hz.
 */
uint64 FCEUD_GetTimeFreq(void) {
  // SDL_GetTicks() is in milliseconds
  fprintf(stderr, "(FCEUD_GetTimeFreq) In headless mode, nothing should "
	  "try to do timing.\n");
  abort();
  return 1000;
}
