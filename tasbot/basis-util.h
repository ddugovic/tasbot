
#ifndef __BASIS_UTIL_H
#define __BASIS_UTIL_H

#include <vector>
#include <string>

#include "fceu/types.h"

using namespace std;

struct BasisUtil {
  // Emulator::Initialize must have been called and we must be
  // on the first frame (or the one you want). Inputs gives the
  // inputs to play, and the basis is captured at the given frame
  // (0 indexed). If we already have a file, we just load that.
  static vector<uint8> LoadOrComputeBasis(const vector<uint8> &inputs,
                                          int frame,
                                          const string &basisfile);
};

#endif
