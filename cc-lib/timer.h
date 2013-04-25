
#include <time.h>

// Extremely simple timer. Only records one start-stop span,
// and only in seconds.
struct Timer {
  Timer() : starttime(0), stoptime(0) {
    Start();
  }

  void Start() {
    starttime = time(NULL);
  }

  void Stop() {
    stoptime = time(NULL);
  }

  time_t Seconds() {
    Stop();
    return stoptime - starttime;
  }

private:
  time_t starttime, stoptime;

};
