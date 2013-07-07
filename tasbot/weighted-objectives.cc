
#include "weighted-objectives.h"

struct WeightedObjectives::Info {
  explicit Info(double w) : weight(w) {}
  double weight;

  // Sorted, ascending.
  vector< vector<uint8> > observations;
};

WeightedObjectives::WeightedObjectives() {}

WeightedObjectives::WeightedObjectives(const vector< vector<int> > &objs) {
  for (int i = 0; i < objs.size(); i++) {
    weighted[objs[i]] = new Info(1.0);
  }
}

static string ObjectiveToString(const vector<int> &obj) {
  string s;
  for (int i = 0; i < obj.size(); i++) {
    char d[64] = {0};
    sprintf(d, "%s%d", (i ? " " : ""), obj[i]);
    s += d;
  }
  return s;
}

WeightedObjectives *
WeightedObjectives::LoadFromFile(const string &filename) {
  WeightedObjectives *wo = new WeightedObjectives;
  vector<string> lines = Util::ReadFileToLines(filename);
  for (int i = 0; i < lines.size(); i++) {
    stringstream ss(lines[i], stringstream::in);
    double d;
    ss >> d;
    vector<int> locs;
    while (!ss.eof()) {
      int i;
      ss >> i;
      locs.push_back(i);
    }

    // printf("GOT: %f | %s\n", d, ObjectiveToString(locs).c_str());
    wo->weighted.insert(make_pair(locs, new Info(d)));
  }

  return wo;
}
  
void WeightedObjectives::SaveToFile(const string &filename) const {
  string out;
  for (Weighted::const_iterator it = weighted.begin(); 
       it != weighted.end(); ++it) {
    if (it->second->weight > 0) {
      const vector<int> &obj = it->first;
      const Info &info = *it->second;
      out += StringPrintf("%f %s\n", info.weight,
			  ObjectiveToString(obj).c_str());
    }
  }
  Util::WriteFile(filename, out);
  printf("Saved weighted objectives to %s\n", filename.c_str());
}

size_t WeightedObjectives::Size() const {
  return weighted.size();
}

void WeightedObjectives::Observe(const vector<uint8> &memory) {
  // Isn't it desirable that particular states' values change
  // as more observations are made? We're not observing every
  // frame, so the probability of overfitting is low.
  //
  // PERF Currently, we just keep a sorted vector for each objective's
  // value at each observation. This is not very efficient. Worse, it
  // may have the undesirable effect that a particular state's value
  // can change (arbitrarily) with future observations, even just by
  // observing states we've already seen again (changes mass
  // distribution).
  for (Weighted::iterator it = weighted.begin();
       it != weighted.end(); ++it) {
    // PERF Randomly forget observations like a human does.
    // Assume N=64 should be sufficiently large.
    const vector<int> &obj = it->first;
    Info *info = it->second;
    const size_t size = info->observations.size();
    if (size < 64) {
      info->observations.resize(info->observations.size() + 1);
    }
    vector<uint8> &cur = size >= 64 ?
      info->observations[rand() % size] :
      info->observations.back();
    cur.clear();
    cur.reserve(obj.size());

    for (vector<int>::const_iterator i = obj.begin();
         i != obj.end(); i++) {
      cur.push_back(memory[*i]);
    }

    // PERF sorted insert is O(n/2); dunno how std::sort
    // behaves on nearly-sorted vectors.
    std::sort(info->observations.begin(), info->observations.end());

    // Maybe should just keep the unique values? Otherwise
    // lower_bound is doing something kind of funny when there
    // are lots of the same value...
  }
}

static bool LessObjective(const vector<uint8> &mem1, 
			  const vector<uint8> &mem2,
			  const vector<int> &order) {
  // PERF Is faster than Order(mem1, mem2, order) < 0.0 and should
  // return the same result.
  for (vector<int>::const_iterator it = order.begin();
       it != order.end(); ++it) {
    const int &p = *it;
    if (mem1[p] > mem2[p])
      return false;
    if (mem1[p] < mem2[p])
      return true;
  }

  // Equal.
  return false;
}

#define RADIX 2
static double Order(const vector<uint8> &mem1, 
		 const vector<uint8> &mem2,
		 const vector<int> &order) {
  // TODO: Rather than doing weighted lexicographic comparisons
  // let each objective function be a weight vector [w0 w1 .. wn]
  // to dot product with corresponding memory segments.
  // At search time, weight changes in objective functions by the magnitude
  // of the change, not just the number that went up or down.
  // Below is equivalent to weight vector [RADIX^n RADIX^(n-1) .. 1]
  // for the objective function [0 1 .. n]
  double val = 0.0;
  for (vector<int>::const_reverse_iterator it = order.rbegin();
       it != order.rend(); ++it) {
    val += (mem2[*it] - mem1[*it]);
    val /= RADIX;
  }
  return val;
}


double WeightedObjectives::WeightedLess(const vector<uint8> &mem1,
					const vector<uint8> &mem2) const {
  double score = 0.0;
  for (Weighted::const_iterator it = weighted.begin();
       it != weighted.end(); ++it) {
    const vector<int> &objective = it->first;
    const double weight = it->second->weight;
    if (LessObjective(mem1, mem2, objective))
      score += weight;
  }
  CHECK(score >= 0);
  return score;
}

double WeightedObjectives::Evaluate(const vector<uint8> &mem1,
				    const vector<uint8> &mem2) const {
  double score = 0.0;
  for (Weighted::const_iterator it = weighted.begin();
       it != weighted.end(); ++it) {
    const vector<int> &objective = it->first;
    const double weight = it->second->weight;
    score += weight * Order(mem1, mem2, objective);
  }
  return score;
}

static vector<uint8> GetValues(const vector<uint8> &mem,
			       const vector<int> &objective) {
  vector<uint8> out;
  out.resize(objective.size());
  for (int i = 0; i < objective.size(); i++) {
    CHECK(objective[i] < mem.size());
    out[i] = mem[objective[i]];
  }
  return out;
}

static vector< vector<uint8> >
GetUniqueValues(const vector< vector<uint8 > > &memories,
		const vector<int> &objective) {
  set< vector<uint8> > values;
  for (int i = 0; i < memories.size(); i++) {
    values.insert(GetValues(memories[i], objective));
  }
    
  vector< vector<uint8> > uvalues;
  uvalues.insert(uvalues.begin(), values.begin(), values.end());
  return uvalues;
}

// Find the index of the vector now within the values
// array, which is sorted and unique.
static inline int GetValueIndex(const vector< vector<uint8> > &values,
				const vector<uint8> &now) {
  return lower_bound(values.begin(), values.end(), now) - values.begin();
}

static inline double GetValueFrac(const vector< vector<uint8> > &values,
				  const vector<uint8> &now) {
  int idx = GetValueIndex(values, now);
  // -1, since it can never be the size itself?
  // and what should the value be if values is empty or singleton?
  return (double)idx / values.size();
}

double WeightedObjectives::GetNormalizedValue(const vector<uint8> &mem) 
  const {
  double sum = 0.0;

  for (Weighted::const_iterator it = weighted.begin();
       it != weighted.end(); ++it) {
    const vector<int> &obj = it->first;
    const Info &info = *it->second;
    
    vector<uint8> cur;
    cur.reserve(obj.size());
    for (int i = 0; i < obj.size(); i++) {
      cur.push_back(mem[obj[i]]);
    }

    sum += GetValueFrac(info.observations, cur);
  }

  sum /= (double)weighted.size();
  return sum;
}

void WeightedObjectives::WeightByExamples(const vector< vector<uint8> >
					  &memories) {
  for (Weighted::iterator it = weighted.begin();
       it != weighted.end(); ++it) {
    const vector<int> &obj = it->first;
    Info *info = it->second;
    // All the distinct values this objective takes on, in order.
    vector< vector<uint8> > values = GetUniqueValues(memories, obj);


    // Sum of deltas is just very last - very first.
    CHECK(memories.size() > 0);
    double score_end =
      GetValueFrac(values, GetValues(memories[memories.size() - 1], obj));
    double score_begin = 
      GetValueFrac(values, GetValues(memories[0], obj));
    CHECK(score_end >= 0 && score_end <= 1);
    CHECK(score_begin >= 0 && score_begin <= 1);
    double score = score_end - score_begin;

    /*
    int lastvaluefrac = 0.0;
    for (int i = 0; i < memories.size(); i++) {
      vector<uint8> now = GetValues(memories[i], obj);
      double valuefrac = GetValueFrac(values, now);
      CHECK(valuefrac >= 0);
      CHECK(valuefrac <= 1);
      score += (valuefrac - lastvaluefrac);
      lastvaluefrac = valuefrac;
    }
    */

    if (score <= 0.0) {
      printf("Bad objective lost more than gained: %f / [ %s ]\n",
	     score, ObjectiveToString(obj).c_str());
      info->weight = 0.0;
    } else {
      info->weight = score;
    }
  }
}

void WeightedObjectives::SaveSVG(const vector< vector<uint8> > &memories,
				 const string &filename) const {
  const double SPAN = 50;
  const double WIDTH = memories.size() * 2; // 1024.0
  const double HEIGHT = 768.0;

  // Add slop since other SVG does.
  string out = TextSVG::Header(WIDTH+12, HEIGHT+12);

  ArcFour rc("Zmake colors");

  size_t skipped = 0;
  int howmany = 500;
  for (Weighted::const_iterator it = weighted.begin();
       howmany-- && it != weighted.end(); ++it) {
    const vector<int> &obj = it->first;
    // const Info &info = *it->second;
    // All the distinct values this objective takes on, in order.
    const vector< vector<uint8> > values = GetUniqueValues(memories, obj);
    // printf("%lld distinct values for %s\n", values.size(),
    // ObjectiveToString(obj).c_str());

    const string color = RandomColor(&rc);
    const string startpolyline =
      StringPrintf("  <polyline fill=\"none\" "
		   // opacity=\"0.5\" "
		   "stroke=\"%s\""
		   " stroke-width=\"%d\" points=\"", 
		   // (info.weight <= 0) ? "#f00" : "#0f0",
		   color.c_str(),
		   1
		   // (info.weight <= 0) ? 3 : 1
		   );
    const string endpolyline = "\" />\n";
    out += "<g>\n";
    out += startpolyline;

    static const int MAXLEN = 256;
    int numleft = MAXLEN;
    // Fill in points as space separated x,y coords
    int lastvalueindex = -1;
    for (size_t i = 0; i < memories.size(); i++) {
      const vector<uint8> now = GetValues(memories[i], obj);
      const int valueindex = GetValueIndex(values, now);

      // Allow drawing horizontal lines without interstitial points.
      if (valueindex == lastvalueindex) {
	while (i < memories.size() - 1) {
	  const vector<uint8> next = GetValues(memories[i + 1], obj);
	  int nextvalueindex = GetValueIndex(values, next);
	  if (nextvalueindex != valueindex)
	    break;
	  i++;
	  skipped++;
	}
      }
      lastvalueindex = valueindex;

      // Fraction in [0, 1]
      double yf = (double)valueindex / values.size();
      double xf = (double)i / memories.size();
      out += Coords(WIDTH * xf, HEIGHT * (1.0 - yf)) + " ";
      if (numleft-- == 0) {
	out += endpolyline;
	out += startpolyline;
	out += Coords(WIDTH * xf, HEIGHT * (1.0 - yf)) + " ";
	numleft = MAXLEN;
      }
    }

    out += endpolyline;
    out += "</g>\n";
  }

  // XXX args?
  out += SVGTickmarks(WIDTH, memories.size() * 5, SPAN, 20.0, 12.0);

  out += TextSVG::Footer();
  Util::WriteFile(filename, out);

  printf("Wrote %zu objectives, skipping %zu points, to %s\n", 
	 weighted.size(), skipped, filename.c_str());
}
