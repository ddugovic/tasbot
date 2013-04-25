
/* Alleged RC4 algorithm.
   The RC4 name is trademarked by RSA DSI.
   This implementation is based on the algorithm
   published in Applied Cryptography. */

// Note: I ported this from my SML version and
// never tested it. -tom7

#ifndef __CCLIB_ARCFOUR_H
#define __CCLIB_ARCFOUR_H

#include <string>
#include <vector>

struct ArcFour {
  typedef unsigned char uint8;

  explicit ArcFour(const std::vector<uint8> &v);
  explicit ArcFour(const std::string &s);

  // Get the next byte.
  uint8 Byte();

  // Discard n bytes from the stream. It is
  // strongly recommended that new uses of
  // arcfour discard at least 1024 bytes after
  // initialization, to prevent against the
  // 2001 attach by Fluhrer, Mantin, and Shamir.
  void Discard(int n);

private:
  uint8 ii, jj;
  uint8 ss[256];
};

#endif
