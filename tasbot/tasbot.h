
#ifndef __TASBOT_H
#define __TASBOT_H

#include "../cc-lib/util.h"
#include "../cc-lib/heap.h"
#include "../cc-lib/base/stringprintf.h"

#include "fceu/drivers/common/args.h"
#include "fceu/utils/md5.h"
#include "fceu/utils/xstring.h"
#include "fceu/driver.h"
#include "fceu/fceu.h"
#include "fceu/state.h"
#include "fceu/types.h"
#include "fceu/version.h"

#ifdef __GNUC__
#include <tr1/unordered_map>
#include <tr1/unordered_set>
using std::tr1::hash;
#else
#include <hash_map>
#include <hash_set>
#endif

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

using namespace std;

#endif
