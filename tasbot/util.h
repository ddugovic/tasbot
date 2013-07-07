/* TASBOT utilities not general-purpose enough to go into cc-lib. */

#ifndef __TASBOT_UTIL_H
#define __TASBOT_UTIL_H

#include "../cc-lib/base/stringprintf.h"
#include "../cc-lib/city/city.h"
#include "../cc-lib/util.h"
#include "../cc-lib/arcfour.h"
#include "../cc-lib/heap.h"
#include "../cc-lib/textsvg.h"

#include "fceu/drivers/common/args.h"
#include "fceu/utils/md5.h"
#include "fceu/utils/xstring.h"
#include "fceu/driver.h"
#include "fceu/fceu.h"
#include "fceu/state.h"
#include "fceu/types.h"
#include "fceu/version.h"

#include "tasbot.h"
#include "time.h"

#define ANSI_RESET "\x1B[0m"
#define ANSI_GREY "\x1B[30m"
#define ANSI_RED "\x1B[31m"
#define ANSI_GREEN "\x1B[32m"
#define ANSI_YELLOW "\x1B[33m"
#define ANSI_BLUE "\x1B[34m"
#define ANSI_PURPLE "\x1B[35m"
#define ANSI_CYAN "\x1B[36m"
#define ANSI_WHITE "\x1B[37m"

// TODO: Don't inject special characters into stdout/stderr when
//       trying to redirect those streams to log files.
// TODO: Verbose logging in plaintext format (for grep)
#define LOG(...) printf(__VA_ARGS__)
#define TERM_START(n) InPlaceTerminal term(n)
#define TERM(term, s) term.Output(s)
#define TERM_FLUSH(term) term.Advance()

using namespace std;

inline string TimeString(time_t t) {
  char str[256];
  strftime(str, 255, "%H:%M:%S", localtime(&t));
  return str;
}

inline string DateString(time_t t) {
  char str[256];
  strftime(str, 255, "%d %b %Y", localtime(&t));
  return str;
}

template<class T>
static void Shuffle(vector<T> *v) {
  static ArcFour rc("shuffler");
  for (int i = 0; i < v->size(); i++) {
    uint32 h = 0;
    h = (h << 8) | rc.Byte();
    h = (h << 8) | rc.Byte();
    h = (h << 8) | rc.Byte();
    h = (h << 8) | rc.Byte();

    int j = h % v->size();
    if (i != j) {
      swap((*v)[i], (*v)[j]);
    }
  }
}

inline uint32 RandomInt32(ArcFour *rc) {
  uint32 b = rc->Byte();
  b = (b << 8) | rc->Byte();
  b = (b << 8) | rc->Byte();
  b = (b << 8) | rc->Byte();
  return b;
}

inline string RandomColor(ArcFour *rc) {
  // For a white background there must be at least one color channel that
  // is half off. Mask off one of the three top bits at random:
  uint8 rr = 0x7F, gg = 0xFF, bb = 0xFF;
  for (int i = 0; i < 30; i++) {
    if (rc->Byte() & 1) {
      uint8 tt = rr;
      rr = gg;
      gg = bb;
      bb = tt;
    }
  }

  return StringPrintf("#%02x%02x%02x",
                      rr & rc->Byte(), gg & rc->Byte(), bb & rc->Byte());
}

// Random double in [0,1]. Note precision issues.
inline double RandomDouble(ArcFour *rc) {
  return (double)RandomInt32(rc) / (double)(uint32)0xFFFFFFFF;
}

template<class T>
T VectorMin(T def, const vector<T> &v) {
#if 0
  for (int i = 0; i < v.size(); i++) {
    if (v[i] < def) def = v[i];
  }
  return def;
#else
  if (v.empty())
    return def;
  T min = *min_element(v.begin(), v.end());
  if (min < def)
    return min;
  return def;
#endif
}
template<class T>
T VectorMax(T def, const vector<T> &v) {
#if 0
  for (int i = 0; i < v.size(); i++) {
    if (v[i] > def) def = v[i];
  }
  return def;
#else
  if (v.empty())
    return def;
  T max = *max_element(v.begin(), v.end());
  if (max > def)
    return max;
  return def;
#endif
}

// Truncate unnecessary trailing zeroes to save space.
inline string Coords(double x, double y) {
  char s[25];
  sprintf(s, "%.2f,%.2f", x, y);
  return (string)s;
}

// This is for when a process is doing something where it'd
// like to report progress by overwriting something like a
// percentage or graph or something on a fixed number of lines,
// but also wants to be able to log exceptional events without
// overwriting them.
struct InPlaceTerminal {
  explicit InPlaceTerminal(int lines);

  // Output should contain one newline per line.
  void Output(const string &s);

  // Call this before any output not done with Output, which
  // will advance the cursor past the in-place stuff and
  // ensure that the next call to Output doesn't overwrite
  // what the other call wrote.
  void Advance();

 private:
  int lines;
  bool last_was_output;
};

// Width of graphic in pixels, max value of x axis, width of span
// between tickmarks in terms of the units of the x axis,
// the tick height in pixels, the tick font height.
string SVGTickmarks(double width, double maxx, double span,
                    double tickheight, double tickfont);

// Draw a column of dots (as an SVG string), given a vector of values.
// xf is the fraction of the screen width that this column should be
// centered on. maxval is the value considered to be the top of the
// drawing; values above this or below zero are drawn outside the box.
// If chosen_idx is in [0, values.size()) then draw that one bigger.
// Color is an svg color string like "#f00".
string DrawDots(const double width, const double height,
                const string &color, double xf,
                const vector<double> &values, double minval, double maxval,
                int chosen_idx);


#endif
