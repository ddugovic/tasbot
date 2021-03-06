
#ifndef __MOTIFS_H
#define __MOTIFS_H

#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <sstream>
#include <vector>

#include "motifs-style.h"
#include "simplefm2.h"
#include "tasbot.h"
#include "weighted-objectives.h"

// Right now, segment into 10-input chunks.
static const int MOTIF_SIZE = 10;

struct Motifs {
  // Create empty.
  Motifs();

  static Motifs *LoadFromFile(const std::string &filename);

  // Does not save checkpoints.
  void SaveToFile(const std::string &filename) const;

  void AddInputs(const vector<uint8> &inputs, const size_t &fastforward);

  // Returns a motif uniformly at random.
  // Linear time.
  const vector<uint8> &RandomMotif();

  // Returns one according to current weights.
  // Linear time.
  const vector<uint8> &RandomWeightedMotif();

  const vector<uint8> &RandomMotifWith(ArcFour *rc);
  const vector<uint8> &RandomWeightedMotifWith(ArcFour *rc);

  // Returns NULL if none can be found.
  template<class Container>
  const vector<uint8> *RandomWeightedMotifNotIn(const Container &c);

  // Return the total weight, which allows a single weight to
  // be interpreted as a fraction of the total (for example
  // for capping weights.)
  double GetTotalWeight() const;

  vector< vector<uint8> > AllMotifs() const;

  bool IsMotif(const vector<uint8> &inputs);

  // Increment a counter (just used for diagnostics) that says
  // how many times this motif was picked.
  void Pick(const vector<uint8> &inputs);

  // Returns a modifiable double for the input,
  // or NULL if it has been added with AddInputs (etc.).
  double *GetWeightPtr(const vector<uint8> &inputs);

  // Save the current weights at the frame number (assumed
  // to be monotonically increasing), so that they can be
  // drawn with DrawSVG.
  void Checkpoint(int framenum);

  void SaveHTML(const string &filename) const;

private:
  struct Info {
  Info() : weight(0.0), picked(0) {}
  Info(double w) : weight(w), picked(0) {}
    double weight;
    int picked;
    // Optional, for diagnostics.
    vector< pair<int, double> > history;
  };

  struct Resorted;
  static bool WeightDescending(const Resorted &a, const Resorted &b);

  // XXX accessors or something?
  typedef map<vector<uint8>, Info> Weighted;
  Weighted motifs;
  ArcFour rc;

  NOT_COPYABLE(Motifs);
};


// Template implementations follow.

// See the related methods in the .cc file for commentary.
template<class Container>
const vector<uint8> *Motifs::RandomWeightedMotifNotIn(const Container &c) {
  double totalweight = 0.0;
  for (Weighted::const_iterator it = motifs.begin();
       it != motifs.end(); ++it) {
    if (!c.count(it->first)) {
      totalweight += it->second.weight;
    }
  }

  // "index" into the continuous bins
  double sample = RandomDouble(&rc) * totalweight;

  for (Weighted::const_iterator it = motifs.begin();
       it != motifs.end(); ++it) {
    if (!c.count(it->first)) {
      if (sample <= it->second.weight) {
        return &it->first;
      }
      sample -= it->second.weight;
    }
  }

  return NULL;
}

#endif
