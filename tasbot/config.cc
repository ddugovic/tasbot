
#include <stdlib.h>

#include "config.h"


Config *global_config = NULL;
void InitConfig() {
  global_config = new Config;
}
