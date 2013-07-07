
#ifndef __TASBOT_H
#define __TASBOT_H

#include "basis-util.h"
#include "config.h"
#include "emulator.h"
#include "simplefm2.h"

// TODO: Use good logging package.
#define CHECK(condition) \
  while (!(condition)) {                                    \
    fprintf(stderr, "%s:%d. Check failed: %s\n",            \
            __FILE__, __LINE__, #condition                  \
            );                                              \
    abort();                                                \
  }

#define NOT_COPYABLE(classname) \
  private: \
  classname(const classname &); \
  classname &operator =(const classname &)

#endif
