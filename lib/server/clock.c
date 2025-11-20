#include "server.h"


#include <stdint.h>

uint64_t current_millis() {
#if defined(_WIN32)
    #include <windows.h>
    return GetTickCount64();
#else
  #include <time.h>

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
#endif
}