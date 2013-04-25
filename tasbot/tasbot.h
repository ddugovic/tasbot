
#ifndef __TASBOT_H
#define __TASBOT_H

#include "../cc-lib/util.h"
#include "../cc-lib/heap.h"

#include "../cc-lib/base/stringprintf.h"

#ifdef __GNUC__
#include <ext/hash_map>
#include <ext/hash_set>
#else
#include <hash_map>
#include <hash_set>
#endif

#ifdef __GNUC__
namespace std {
using namespace __gnu_cxx;
}

// Needed on cygwin, at least. Maybe not all gnuc?
namespace __gnu_cxx {
template<>
struct hash< unsigned long long > {
  size_t operator()( const unsigned long long& x ) const {
    return x;
  }
};
}
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
