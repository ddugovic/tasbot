
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "heap.h"

using namespace std;

typedef unsigned long long uint64;

struct TestValue : public Heapable {
  TestValue(uint64 i) : i(i) {}
  uint64 i;
};

static uint64 CrapHash(int a) {
  uint64 ret = ~a;
  ret *= 31337;
  ret ^= 0xDEADBEEF;
  ret = (ret >> 17) | (ret << (64 - 17));
  ret -= 911911911911;
  ret *= 65537;
  ret ^= 0xCAFEBABE;
  return ret;
}

int main () {
  static const int kNumValues = 1000;
  
  Heap<uint64, TestValue> heap;
  
  vector<TestValue> values;
  for (int i = 0; i < kNumValues; i++) {
    values.push_back(TestValue(CrapHash(i)));
  }

  for (int i = 0; i < values.size(); i++) {
    heap.Insert(values[i].i, &values[i]);
  }

  TestValue *last = heap.PopMinimumValue();
  while (!heap.Empty()) {
    TestValue *now = heap.PopMinimumValue();
    fprintf(stderr, "%llu %llu\n", last->i, now->i);
    if (now->i < last->i) {
      printf("FAIL: %llu %llu\n", last->i, now->i);
      return -1;
    }
    last = now;
  }

  printf("OK\n");
  return 0;
}
