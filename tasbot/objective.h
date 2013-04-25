/* This library is used to learn objective functions from
   examples. It's specialized to learning NES games: These
   have 2kb of RAM and memory locations usually store
   meaningful single-byte quantities.

   An objective function is a lexicographic ordering on a subset of
   memory locations L1..Ln. Specifically, MEM < MEM' when

   MEM[L1..Li-1] = MEM'[L1..Li-1] and
   MEM[Li] < MEM'[Li]

   The value of Li+1 through Ln are irrelevant in the order.
   Basically, when MEM != MEM', there is a unique first location
   that has a different value, and that one determines the order.

   Right now this is all based on unsigned bytes, but extending
   to signed values would not be too hard. Note that multi-byte
   numbers are already handled for free. In fact, consider doing
   this just for bits, since that generalizes the unsigned byte
   case (though it probably leads to over/under-fitting).

   We learn from a series of memories that are supposed to
   be following the objective function, but we expect that
   sometimes they will violate it. Optimizing the inexact problem
   is hard, so here is the first try:

   First, let's consider the case where the series of observations
   exactly obey the objective function. Start with all possible
   objective functions (don't worry, there are only about 2^N * N!
   possibilities), and eliminate them when MEM[i] < MEM[i+1] is
   violated. Boy, still a lot of work! Almost every one of these
   will be eliminated in the very first round.

   Here's a candidate. An ordering must have a most significant byte,
   which only ever gets larger or stays the same. Every byte that
   never changes value could be in a lexicographic ordering in any
   spot, so first just remove those (we could add them later if we
   want). Now the most significant byte must get larger at least once,
   and only get larger. Loop over all such candidate bytes and
   now consider completing the ordering as follows:

     In the general case we have a prefix of an ordering L1..Ln and
     the invariant that MEM[i] <= MEM[i+1] for it, for all i. Our goal
     is to extend it. Since the location is allowed to have any value
     in the transitions when MEM[i] < MEM[i+1] (strictly) for the
     prefix, we look at only the cases where MEM[i] == MEM[i+1]. Now
     among these we look for candidate extension bytes that only
     increase, and increase at least once. Actually that's not right:
     They have to increase during spans where MEM[i]..MEM[j] are
     all equal in the prefix. So we try each byte (that's not already
     in the prefix) in each span, and for each one that works, we
     continue with it as a candidate. Note that this generalizes
     the thing we did in the very first step, where the prefix is
     empty and so we have only one span, which is the entire range
     of observations.

   This is great easy.
 */

#include <vector>

#include "fceu/types.h"

using namespace std;

struct Objective {

  // Matrix of memories must be non-empty and rectangular.
  explicit Objective(const vector< vector<uint8> > &memories);

  // TODO: Make it possible to enumerate 10 lex orderings
  // that aren't necessarily the FIRST 10. Just shuffle
  // after EnumerateFull? Maintain a queue?

  // TODO: Fitness of a lex ordering. Does it increment a
  // lot of times? Is it long? Are few permutations of the
  // indices also lex orderings? Are there consecutive runs
  // of indices?

  // Run the callback on up to limit number of lex orderings.
  // -1 means no limit.
  void EnumerateFull(const vector<int> &look,
                     void (*f)(const vector<int> &ordering),
                     int limit, int seed);

  void EnumerateFullAll(void (*f)(const vector<int> &ordering),
                        int limit, int seed);

private:

  // Look gives the memory indices to look at.
  // Prefix is memory locations forming a lexicographic ordering.
  // Left contains the indices of memory locations left to consider
  // for extending the prefix. These may not overlap the prefix.
  // (Invariant: mem[look[i]] <= mem[look[j]] according to the prefix, when
  // 0 <= i < j < look.size(). At least one pair is strictly less.)
  // All arguments are morally constant, but can be modified and replaced
  // during recursion.
  // XXX docs
  void EnumeratePartial(const vector<int> &look,
                        vector<int> *prefix,
                        const vector<int> &left,
                        vector<int> *remain,
                        vector<int> *candidates);

  void EnumeratePartialRec(const vector<int> &look,
                           vector<int> *prefix,
                           const vector<int> &left,
                           void (*f)(const vector<int> &ordering),
                           int *limit, int seed);

  const vector< vector<uint8> > &memories;
};
