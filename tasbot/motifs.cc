
#include "motifs.h"

#include <algorithm>
#include <set>
#include <string>
#include <iostream>
#include <sstream>

#include "tasbot.h"
#include "../cc-lib/arcfour.h"
#include "util.h"
#include "../cc-lib/util.h"
#include "simplefm2.h"
#include "motifs-style.h"

Motifs::Motifs() : rc("motifs") {}

static string InputsToString(const vector<uint8> &inputs) {
  string s;
  for (int i = 0; i < inputs.size(); i++) {
    char d[64] = {0};
    sprintf(d, "%s%d", (i ? " " : ""), inputs[i]);
    s += d;
  }
  return s;
}

void Motifs::Pick(const vector<uint8> &inputs) {
  Weighted::iterator it = motifs.find(inputs);
  if (it == motifs.end()) return;
  else it->second.picked++;
}

bool Motifs::IsMotif(const vector<uint8> &inputs) {
  return motifs.find(inputs) != motifs.end();
}

void Motifs::Checkpoint(int framenum) {
  // PERF could maybe just remove spans here, which makes
  // printing much simpler and this data structure more
  // compact!
  for (Weighted::iterator it = motifs.begin(); 
       it != motifs.end(); ++it) {
    it->second.history.push_back(make_pair(framenum, it->second.weight));
  }
}

Motifs *Motifs::LoadFromFile(const string &filename) {
  Motifs *mm = new Motifs;
  vector<string> lines = Util::ReadFileToLines(filename);
  for (int i = 0; i < lines.size(); i++) {
    stringstream ss(lines[i], stringstream::in);
    double d;
    ss >> d;
    vector<uint8> inputs;
    while (!ss.eof()) {
      int i;
      ss >> i;
      inputs.push_back((uint8)i);
    }

    // printf("MOTIF: %f | %s\n", d, InputsToString(inputs).c_str());
    mm->motifs.insert(make_pair(inputs, Info(d)));
  }

  return mm;
}

void Motifs::SaveToFile(const string &filename) const {
  string out;
  for (Weighted::const_iterator it = motifs.begin(); 
       it != motifs.end(); ++it) {
    const vector<uint8> &inputs = it->first;
    string s = StringPrintf("%f ", it->second.weight);
    s += InputsToString(inputs);
    out += s + "\n";
  }
  // printf("%s\n", out.c_str());
  printf("Wrote %lld motifs to %s.\n", motifs.size(), filename.c_str());
  Util::WriteFile(filename, out);
}

void Motifs::AddInputs(const vector<uint8> &inputs) {
  // Right now, just chunk into 10-input parts.
  static const int CHUNK_SIZE = 10;
  vector<uint8> current;

  for (int i = 0; i < inputs.size(); i++) {
    current.push_back(inputs[i]);
    if (current.size() == CHUNK_SIZE) {
      motifs[current].weight += 1.0;
      current.clear();
    }
  }

  if (!current.empty()) {
    motifs[current].weight += 1.0;
  }
}

vector< vector<uint8> > Motifs::AllMotifs() const {
  vector< vector<uint8> > motifvec;
  for (Weighted::const_iterator it = motifs.begin();
       it != motifs.end(); ++it) {
    motifvec.push_back(it->first);
  }
  return motifvec;
}

const vector<uint8> &Motifs::RandomMotifWith(ArcFour *rrc) {
  const vector<uint8> *res = NULL;
  uint32 best = ~0;

  CHECK(!motifs.empty());
  for (Weighted::const_iterator it = motifs.begin();
       it != motifs.end(); ++it) {
    uint32 thisone = RandomInt32(rrc);
    if (res == NULL || thisone < best) {
      best = thisone;
      res = &it->first;
    }
  }

  return *res;
}

const vector<uint8> &Motifs::RandomMotif() {
  return RandomMotifWith(&rc);
}

double *Motifs::GetWeightPtr(const vector<uint8> &inputs) {
  Weighted::iterator it = motifs.find(inputs);
  if (it == motifs.end()) return NULL;
  else return &it->second.weight;
}

double Motifs::GetTotalWeight() const {
  double totalweight = 0.0;
  for (Weighted::const_iterator it = motifs.begin();
       it != motifs.end(); ++it) {
    totalweight += it->second.weight;
  }
  return totalweight;
}

// Note there are several fancy ways to do this, but I
// have seen them have numerical stability problems in
// practice. I'm favoring correctness and simplicity
// here.
const vector<uint8> &Motifs::RandomWeightedMotifWith(ArcFour *rrc) {
  double totalweight = GetTotalWeight();

  // "index" into the continuous bins
  double sample = RandomDouble(rrc) * totalweight;
  
  for (Weighted::const_iterator it = motifs.begin();
       it != motifs.end(); ++it) {
    if (sample <= it->second.weight) {
      return it->first;
    }
    sample -= it->second.weight;
  }
  
  // Arbitrarily award roundoff errors to the first one.
  printf("roundoff error of %f in RandomWeightedMotif\n", sample);
  CHECK(!motifs.empty());
  return motifs.begin()->first;
}

const vector<uint8> &Motifs::RandomWeightedMotif() {
  return RandomWeightedMotifWith(&rc);
}


static string ShowRange(int lastframe, double val,
			int thisframe) {
  int finframe = thisframe - 1;
  if (lastframe == finframe) {
    return StringPrintf("<span class=\"range\">%d:&nbsp;"
			"<span class=\"value\">%.2f</span></span>", 
			lastframe, val);
  } else {
    return StringPrintf("<span class=\"range\">%d&ndash;%d:&nbsp;"
			"<span class=\"value\">%.2f</span></span>",
			lastframe, finframe, val);
  }
}

struct Motifs::Resorted {
  Resorted(double w, vector<uint8> i, Motifs::Info info)
    : weight(w), inputs(i), info(info) {}
  double weight;
  vector<uint8> inputs;
  Motifs::Info info;
};

bool Motifs::WeightDescending(const Resorted &a, const Resorted &b) {
  return b.weight < a.weight;
}

void Motifs::SaveHTML(const string &filename) const {
  string out = MOTIFS_STYLE;
  vector<Resorted> resorted;
  for (Weighted::const_iterator it = motifs.begin();
       it != motifs.end(); ++it) {
    resorted.push_back(Resorted(it->second.weight, it->first, it->second));
  }

  std::sort(resorted.begin(), resorted.end(), WeightDescending);

  for (int r = 0; r < resorted.size(); r++) {
    const vector<uint8> &inputs = resorted[r].inputs;
    const Info &info = resorted[r].info;
    
    out += "<div class=\"motif\">\n"
      "<div class=\"inputs\">";
    string last = "";
    for (int i = 0; i < inputs.size(); i++) {
      out += "<span class=\"input\">";
      string s = SimpleFM2::InputToColorString(inputs[i]);
      if (s == last) {
	out += "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";
      } else {
	out += s;
      }
      last = s;
      out += "</span> ";
    }
    out += "</div>\n";
    out += "<div class=\"values\">\n";
    out += StringPrintf("<span class=\"picked\">%d</span>",
			info.picked);
    // XXX this is really garbage code. Probably not even correct.
    if (!info.history.empty()) {
      int lastframe = info.history[0].first;
      double lastval = info.history[0].second;
      for (int i = 1; i < info.history.size(); i++) {
	if (lastval != info.history[i].second) {
	  out += ShowRange(lastframe, lastval,
			   info.history[i].first);
	  lastframe = info.history[i].first;
	  lastval = info.history[i].second;
	}
      }
      if (info.history.size() == 1 ||
	  lastframe != info.history.back().first) {
	out += ShowRange(lastframe, lastval, 
			 info.history.back().first + 1);
      }
    }
    out += "</div>\n";  // values
    out += "</div>\n";  // motif
  }

  Util::WriteFile(filename, out);
  printf("Wrote weighted motifs to %s\n", filename.c_str());
}
