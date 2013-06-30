
#include "arcfour.h"

#include <string>
#include <vector>

using namespace std;

typedef unsigned char uint8;

// Check that char is one byte.
enum { char_must_be_one_byte = 1 / !(uint8)256 };

static void Initialize(const uint8 (&kk)[256],
		       uint8 (&ss)[256]) {
  uint8 i = 0, j = 0;
  for (int n = 256; n--;) {
    j += ss[i] + kk[i];
    uint8 t = ss[i];
    ss[i] = ss[j];
    ss[j] = t;
    i++;
  }
}

ArcFour::ArcFour(const vector<uint8> &v) : ii(0), jj(0) {
  uint8 kk[256];
  for (int i = 0; i < 256; i++) {
    ss[i] = i;
    kk[i] = v[i % v.size()];
  }
  Initialize(kk, ss);
}

ArcFour::ArcFour(const string &s) : ii(0), jj(0) {
  uint8 kk[256];
  for (int i = 0; i < 256; i++) {
    ss[i] = i;
    kk[i] = (uint8)s[i % s.size()];
  }
  Initialize(kk, ss);
}

uint8 ArcFour::Byte() {
  ii++;
  jj += ss[ii];
  uint8 ti = ss[ii];
  uint8 tj = ss[jj];
  ss[ii] = tj;
  ss[jj] = ti;

  return ss[(ti + tj) & 255];
}

void ArcFour::Discard(int n) {
  while (n--) (void)Byte();
}


