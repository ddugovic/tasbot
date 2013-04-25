
#include "objective.h"
#include "tasbot.h"

// Self-check output.
#define DEBUG_OBJECTIVE 1
#define VERBOSE_OBJECTIVE 0

#define VPRINTF if (VERBOSE_OBJECTIVE) printf

Objective::Objective(const vector< vector<uint8> > &mm) :
  memories(mm) {
  CHECK(!memories.empty());
  VPRINTF("Each memory is size %ld and there are %ld memories.\n",
	  memories[0].size(), memories.size());
}

struct CompareByHash {
  uint64 CrapHash(int a) {
    uint64 ret = ~a;
    for (int i = 0; i < (a & 3) + 1; i++) {
      ret = (ret >> i) | (ret << (64 - i));
      ret *= 31337;
      ret += (seed << 7) | (seed >> (64 - 7));
      ret ^= 0xDEADBEEF;
      ret = (ret >> 17) | (ret << (64 - 17));
      ret -= 911911911911;
      ret *= 65537;
      ret ^= 0xCAFEBABE;
    }
    return ret;
  }

  bool operator ()(int a, int b) {
    return CrapHash(a) < CrapHash(b);
  }

  CompareByHash(int seed) : seed(seed) {}
  uint64 seed;
};

static void Shuffle(vector<int> *v, int seed) {
  CompareByHash c(seed);
  std::sort(v->begin(), v->end(), c);
}

static bool EqualOnPrefix(const vector<uint8> &mem1, 
			  const vector<uint8> &mem2,
			  const vector<int> &prefix) {
  for (int i = 0; i < prefix.size(); i++) {
    int p = prefix[i];
    // printf("  eop %d: %d vs %d\n", p, mem1[i], mem2[i]);
    if (mem1[p] != mem2[p]) {
      VPRINTF("Disequal at %d so not equal on prefix\n", p);
      return false;
    }
  }
  return true;
}

static bool LessEqual(const vector<uint8> &mem1, 
		      const vector<uint8> &mem2,
		      const vector<int> &order) {
  for (int i = 0; i < order.size(); i++) {
    int p = order[i];
    VPRINTF("  %d: %d vs %d ", p, mem1[p], mem2[p]);
    if (mem1[p] > mem2[p])
      return false;
    if (mem1[p] < mem2[p]) {
      VPRINTF(" ok\n");
      return true;
    }
  }
  VPRINTF(" end\n");
  return true;
}

void Objective::EnumeratePartial(const vector<int> &look,
				 vector<int> *prefix,
				 const vector<int> &left,
				 vector<int> *remain,
				 vector<int> *candidates) {
  // First step is to remove any candidates from left that
  // are not interesting here. For c to be interesting, there
  // must be some i,j within look where i < j and memory[i][c] <
  // memory[j][c] and memory[i] == memory[j] for the prefix,
  // and memory[i][c] is always <= memory[j][c] for all i,j
  // in look where memory[i] == memory[j] for the prefix.
  // We only need to check consecutive memories; a distant
  // counterexample means that there is an adjacent
  // counterexample somewhere in between.

  // Cache this.
  // indices lo in look where look[lo] and look[lo + 1]
  // are equal on the prefix
  vector<int> lequal;
  lequal.reserve(look.size() - 1);
  VPRINTF("Equal on prefix:");
  for (int lo = 0; lo < look.size() - 1; lo++) {
    int i = look[lo], j = look[lo + 1];
    if (EqualOnPrefix(memories[i], memories[j], *prefix)) {
      lequal.push_back(lo);
      VPRINTF(" %d-%d", i, j);
    }
  }
  VPRINTF("\n");

  for (int le = 0; le < left.size(); le++) {
    int c = left[le];
    bool less = false;

    // PERF I don't think this is actually necessary. Since this
    // function returns ALL candidates, the candidates are also all in
    // the remainder list. So, ignore anything that's already in the
    // prefix. (But it should get ignored below since it will
    // obviously be equal whenever EqualOnPrefix and it's in the
    // prefix.)
    for (int i = 0; i < prefix->size(); i++) {
      if ((*prefix)[i] == c) {
	VPRINTF("  skip %d in prefix\n", c);
	goto skip;
      }
    }

    for (int li = 0; li < lequal.size(); li++) {
      int lo = lequal[li];
      int i = look[lo], j = look[lo + 1];
      VPRINTF("  at lo %d. i=%d, j=%d\n", lo, i, j);
      if (memories[i][c] > memories[j][c]) {
	// It may be legal later, but not a candidate.
	remain->push_back(c);
	VPRINTF("  skip %d because memories #%d and #%d have %d->%d\n",
		c, i, j, memories[i][c], memories[j][c]);
	goto skip;
      }

      less = less || memories[i][c] < memories[j][c];
    }

    if (less) {
      candidates->push_back(c);
      remain->push_back(c);
    } else {
      // Always equal. Filtered out and can never become
      // interesting.
      VPRINTF("  %d is always equal; filtered.\n", c);
    }

    skip:;
  }
}

static void CheckOrdering(const vector<int> &look,
			  const vector< vector<uint8> > &memories,
			  const vector<int> &ordering) {
  VPRINTF("CheckOrdering [");
  for (int i = 0; i < ordering.size(); i++) {
    VPRINTF("%d ", ordering[i]);
  }
  VPRINTF("]...\n");

  for (int lo = 0; lo < look.size() - 1; lo++) {
    int ii = look[lo], jj = look[lo + 1];
    const vector<uint8> &mem1 = memories[ii];
    const vector<uint8> &mem2 = memories[jj];

    #if VERBOSE_OBJECTIVE
    if (mem1 == mem2) {
      VPRINTF("Memories exactly the same? %d %d\n", ii, jj);
      continue;
    } else if (EqualOnPrefix(mem1, mem2, ordering)) {
      // printf("equal. %d %d\n", ii, jj);
      // abort();
      continue;
    } else {
      VPRINTF("mem #%d vs #%d\n", ii, jj);
    }
    #endif

    if (!LessEqual(mem1, mem2, ordering)) {
      printf("On these memories (note this ignores look):\n");
      for (int i = 0; i < memories.size(); i++) {
	const vector<uint8> &mem = memories[i];
	for (int j = 0; j < mem.size(); j++) {
	  printf("%3d ", mem[j]);
	}
	printf("\n");
      }

      printf("Illegal ordering: [");
      for (int i = 0; i < ordering.size(); i++) {
	printf("%d ", ordering[i]);
      }
      printf("] at memories #%d and #%d:\n", ii, jj);

      for (int i = 0; i < ordering.size(); i++) {
	int p = ordering[i];
	printf ("%d is %d vs %d\n", p, mem1[p], mem2[p]);
      }

      abort();
    }
    
  }
  
}

void Objective::EnumeratePartialRec(const vector<int> &look,
				    vector<int> *prefix,
				    const vector<int> &left,
				    void (*f)(const vector<int> &ordering),
				    int *limit, int seed) {
#if VERBOSE_OBJECTIVE
  VPRINTF("EPR: [");
  for (int i = 0; i < prefix->size(); i++) {
    VPRINTF("%d ", (*prefix)[i]);
  }
  VPRINTF("] left: [");
  for (int i = 0; i < left.size(); i++) {
    VPRINTF("%d ", left[i]);
  }
  VPRINTF("]\n");
#endif

#if 0
  int seed = *limit + prefix->size();
  if (!look.empty()) {
    seed += (look[0] << 3);
    // seed ^= (look[look.size() - 1] << 1);
  }
  seed ^= look.size();
  if (prefix->size() < 2) VPRINTF("seed %ld\n", seed);
#endif

  vector<int> candidates, remain;
  EnumeratePartial(look, prefix, left, &remain, &candidates);

  if (seed != 0) {
    seed += *limit + prefix->size();
    if (!look.empty()) seed += (look[0] << 3);
    seed ^= look.size();
    // if (prefix->size() < 2) printf("seed %ld\n", seed);
    Shuffle(&candidates, seed);
  }

#if VERBOSE_OBJECTIVE
  if (true || prefix->size() < 2) {
    VPRINTF("Candidates: ");
    for (int i = 0; i < candidates.size(); i++)
      VPRINTF("%d ", candidates[i]);
    VPRINTF("\n");
  }
#endif

  // If this is a maximal prefix, output it. Otherwise, extend.
  if (candidates.empty()) {
#   ifdef DEBUG_OBJECTIVE
    CheckOrdering(look, memories, *prefix);
    // printf("Checked:\n");
#   endif
    (*f)(*prefix);
    if (*limit > 0) --*limit;
  } else {
    prefix->resize(prefix->size() + 1);
    for (int i = 0; i < candidates.size(); i++) {
      (*prefix)[prefix->size() - 1] = candidates[i];
      EnumeratePartialRec(look, prefix, remain, f, limit, seed);
      if (*limit == 0) {
	prefix->resize(prefix->size() - 1);
	return;
      }
    }
    prefix->resize(prefix->size() - 1);
  }
}

void Objective::EnumerateFull(const vector<int> &look,
			      void (*f)(const vector<int> &ordering),
			      int limit, int seed) {
  vector<int> prefix, left;
  for (int i = 0; i < memories[0].size(); i++) {
    left.push_back(i);
  }
  EnumeratePartialRec(look, &prefix, left, f, &limit, seed);
}

void Objective::EnumerateFullAll(void (*f)(const vector<int> &ordering),
				 int limit, int seed) {
  vector<int> look;
  for (int i = 0; i < memories.size(); i++) {
    if (i > 0 && memories[i] == memories[i - 1]) {
      VPRINTF("Duplicate memory at %d-%d\n", i - 1, i);
      // PERF don't include it!
      // look.push_back(i);
    } else {
      look.push_back(i);
    }
  }
  EnumerateFull(look, f, limit, seed);
}
