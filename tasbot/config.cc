
#include "config.h"

int Config::InitConfig(int argc, char *argv[]) {
  static struct option long_options[] = {
    {"fastforward", required_argument, NULL, 'f'},
    {"game", required_argument, NULL, 'g'},
  #ifdef MARIONET
    {"helper", required_argument, NULL, 'h'},
    {"master", required_argument, NULL, 'm'},
  #endif
    {"movie", required_argument, NULL, 'i'}
  };
  char ch;
  while ((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
    switch (ch) {
    case 'g':
      game = optarg;
      break;
    case 'i':
      movie = optarg;
      break;
    case 'f':
      fastforward = atoi(optarg);
      break;
  #ifdef MARIONET
    case 'h':
      port = atoi(optarg);
      if (!port) {
        fprintf(stderr, "Expected a port number after --helper.\n");
        abort();
      }
      break;
    case 'm':
      port = atoi(optarg);
      if (!port) {
        fprintf(stderr, "Expected port numbers after --master.\n");
        abort();
      }
      helpers.push_back(port);
      for ( ; optind < argc; optind++) {
        port = atoi(argv[optind]);
        if (!port) {
          break;
        }
        helpers.push_back(port);
      }
      break;
  #endif
    }
  }
  return 0;
}
