/* Tests for the Objective learning library. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tasbot.h"
#include "fceu/types.h"
#include "../cc-lib/util.h"
#include "../cc-lib/arcfour.h"
#include "objective.h"

#ifdef MARIONET
#include "SDL.h" 
#endif

// Expected lex order is 0, 4, 1.
// 3 is ruled out because it's non-monotonic and 2 never increases.
static const char *kMem0[] = {
  "12345",
  "10346",
  "12346",
  "13346",
  "11347",
  "11347",
  "20001",
  "20091",
  "20051",
};

static const char *kMem1[] = {
  "152",
  "160",
  "162",
  "171",
};

static const char *kMem2[] = {
  "100",
  "105",
  "180",
  "180",
  "200",
};

static const char *kMem3[] = {
  "000",
  "001",
  "011",
};

static const char *kMem4[] = {
  "72",
  "98",
  "91",
};

template<int M>
static vector< vector<uint8> > MakeMem(const char *(&mem)[M]) {
  int N = strlen(mem[0]);
  printf("%d memories of size %d:\n", M, N);
  vector< vector<uint8> > v;
  for (int i = 0; i < M; i++) {
    vector<uint8> row;
    printf("  ");
    for (int j = 0; j < N; j++) {
      printf("%c", mem[i][j]);
      row.push_back((uint8)mem[i][j]);
    }
    printf("\n");
    v.push_back(row);
  }
  return v;
}

static void pr(const vector<int> &ordering) {
  for (int i = 0; i < ordering.size(); i++) {
    printf("%d ", ordering[i]);
  }
  printf("\n");
}

static void ignore(const vector<int> &ordering) {}

static void FindCounterExample() {
  ArcFour rc("hello");
  for (int nmem = 1; nmem < 20; nmem++) {
    for (int size = 1; size < 20; size++) {
      for (int t = 0; t < 3000; t++) {
	vector< vector<uint8> > memories;
	for (int i = 0; i < nmem; i++) {
	  vector<uint8> mem;
	  for (int j = 0; j < size; j++) {
	    mem.push_back(rc.Byte());
	  }
	  memories.push_back(mem);
	}
	Objective obj(memories);
	obj.EnumerateFullAll(ignore, 1, 0);
      }
    }
  }
}

int main(int argc, char *argv[]) {
  fprintf(stderr, "Testing objectives.\n");

  {
    vector< vector<uint8> > memories = MakeMem(kMem0);
    printf("Create.\n");
    Objective obj(memories);
    printf("Enumerate.\n");
    obj.EnumerateFullAll(pr, -1, 0);
  }

  {
    vector< vector<uint8> > memories = MakeMem(kMem1);
    Objective obj(memories);
    obj.EnumerateFullAll(pr, -1, 0);
  }

  {
    vector< vector<uint8> > memories = MakeMem(kMem2);
    Objective obj(memories);
    obj.EnumerateFullAll(pr, -1, 0);
  }

  {
    vector< vector<uint8> > memories = MakeMem(kMem3);
    Objective obj(memories);
    obj.EnumerateFullAll(pr, -1, 0);
  }

  {
    vector< vector<uint8> > memories = MakeMem(kMem4);
    Objective obj(memories);
    obj.EnumerateFullAll(pr, -1, 0);
  }
  
  FindCounterExample();

  return 0;
}
