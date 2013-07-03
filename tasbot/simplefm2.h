/* Simplified FM2 reader. Only supports one gamepad.
   Assumes that the movie starts with hard power-on
   in the first frame. Ignores everything else. */

#ifndef __SIMPLEFM2_H
#define __SIMPLEFM2_H

#include <vector>
#include <string>

#include "tasbot.h"

#define INPUT_R (1<<7)
#define INPUT_L (1<<6)
#define INPUT_D (1<<5)
#define INPUT_U (1<<4)
#define INPUT_T (1<<3)
#define INPUT_S (1<<2)
#define INPUT_B (1<<1)
#define INPUT_A (1   )

using namespace std;

struct SimpleFM2 {
  static vector<uint8> ReadInputs(const string &filename);

  static void WriteInputs(const string &outputfile,
                          const string &romfilename,
                          const MD5DATA &romchecksum,
                          const vector<uint8> &inputs);

  static void WriteInputsWithSubtitles(const string &outputfile,
                                       const string &romfilename,
                                       const MD5DATA &romchecksum,
                                       const vector<uint8> &inputs,
                                       const vector<string> &subtitles);

  static string InputToString(uint8 input);
  static string InputToColorString(uint8 input);
};

#endif
