#ifndef RNG_DEMO_TIME_H
#define RNG_DEMO_TIME_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <time.h>
#endif

/* Monotonic timestamp in nanoseconds, with an unspecified epoch. */
static inline uint64_t demo_now_ns(void) {
#ifdef _WIN32
  static _Thread_local uint64_t frequency;
  LARGE_INTEGER value;

  if (frequency == 0) {
    if (!QueryPerformanceFrequency(&value) || value.QuadPart <= 0) {
      fputs("QueryPerformanceFrequency failed\n", stderr);
      exit(EXIT_FAILURE);
    }

    frequency = (uint64_t)value.QuadPart;
  }

  if (!QueryPerformanceCounter(&value)) {
    fputs("QueryPerformanceCounter failed\n", stderr);
    exit(EXIT_FAILURE);
  }

  const uint64_t ticks = (uint64_t)value.QuadPart;
  const uint64_t seconds = ticks / frequency;
  const uint64_t remainder = ticks % frequency;

  const uint64_t fractional_ns =
      (uint64_t)((long double)remainder * 1000000000.0L /
                 (long double)frequency);

  return seconds * UINT64_C(1000000000) + fractional_ns;
#else
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    perror("clock_gettime");
    exit(EXIT_FAILURE);
  }

  return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
#endif
}

#endif /* RNG_DEMO_TIME_H */