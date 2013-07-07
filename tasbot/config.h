
#ifndef __CONFIG_H
#define __CONFIG_H

#include <getopt.h>
#include <stdlib.h>
#include <string>
#include <vector>
using std::string;
using std::vector;

#include "fceu/movie.h"

struct Config {
  int port;
  vector<int> helpers;
  string game, movie;
  size_t fastforward;
  MD5DATA romchecksum;
  Config(int argc, char *argv[]) {
    InitConfig(argc, argv);
  }
  int InitConfig(int argc, char *argv[]);
};

#endif
