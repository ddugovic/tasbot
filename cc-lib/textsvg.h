
#ifndef __CCLIB_TEXTSVG_H
#define __CCLIB_TEXTSVG_H

#include <string>

struct TextSVG {
  static std::string Header(double width, double height);
  static std::string Footer();
};

#endif
