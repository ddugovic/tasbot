
#ifndef __LEARNFUN_H
#define __LEARNFUN_H

#include <map>
#include <vector>

#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "emulator.h"
#include "simplefm2.h"
#include "objective.h"
#include "weighted-objectives.h"

#ifdef MARIONET
#include "marionet.pb.h"
#include "netutil.h"
#endif

static void SaveMemory(vector< vector<uint8> > *memories);

static void PrintAndSave(const vector<int> &ordering);

// With e.g. an divisor of 3, generate slices covering
// the first third, middle third, and last third.
static void GenerateNthSlices(int divisor, int num, 
			      const vector< vector<uint8> > &memories,
			      Objective *obj);

static void GenerateOccasional(int stride, int offsets, int num,
			       const vector< vector<uint8> > &memories,
			       Objective *obj);

static void MakeObjectives(const string &game, const vector< vector<uint8> > &memories);

#endif
