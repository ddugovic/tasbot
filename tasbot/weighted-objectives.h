/* A set of objective functions which may be weighted, and which may
   carry observations that allow them to be scored in absolute terms. */

#ifndef __WEIGHTED_OBJECTIVES_H
#define __WEIGHTED_OBJECTIVES_H

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../cc-lib/arcfour.h"
#include "../cc-lib/textsvg.h"
#include "fceu/types.h"
#include "motifs.h"
#include "tasbot.h"
#include "util.h"

using namespace std;

struct WeightedObjectives {
  explicit WeightedObjectives(const std::vector< vector<int> > &objs);
  static WeightedObjectives *LoadFromFile(const std::string &filename);

  void WeightByExamples(const vector< vector<uint8> > &memories);

  // Does not save observations.
  void SaveToFile(const std::string &filename) const;

  // XXX version that uses observations?
  void SaveSVG(const vector< vector<uint8> > &memories,
               const string &filename) const;

  size_t Size() const;

  // Scoring function which is just the sum of the weights of
  // objectives where mem1 < mem2.
  double WeightedLess(const vector<uint8> &mem1,
                      const vector<uint8> &mem2) const;

  // Scoring function which is the count of objectives
  // that where mem1 < mem2 minus the number where mem1 > mem2.
  double Evaluate(const vector<uint8> &mem1,
                  const vector<uint8> &mem2) const;

  // Observe a game state. This informs us about the values that
  // the objective functions can take on, which lets us score the
  // magnitude of their changes. Not necessary for GetNumLess() or
  // Evaluate().
  //
  // Currently, calling this reserves (forever) memory proportional to
  // the total size (number of positions) of the objective functions.
  // It's also quadratic time (in the number of calls), in order to
  // make scoring the objectives logarithmic and to try to preserve
  // memory. It should be called for "big" state transitions during
  // exploration, not each step of speculative search.
  void Observe(const vector<uint8> &memory);

  // Get the (current) value of the memory in terms of observations.
  // The value is the unweighted average of the value of each objective
  // function relative to the values we've seen before for it; 1 means
  // that this is the higest value we've ever seen for that objective.
  // Does not observe the memory.
  double GetNormalizedValue(const vector<uint8> &memory) const;

  // XXX weighted version, unnormalized version?

 private:
  WeightedObjectives();
  struct Info;
  typedef std::map< std::vector<int>, Info* > Weighted;
  Weighted weighted;

  NOT_COPYABLE(WeightedObjectives);
};

#endif
