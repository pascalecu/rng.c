#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#else
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "demo_time.h"
#include "plt/plt.h"
#include "rng/rng.h"
#include <math.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef SAMPLE_COUNT
#define SAMPLE_COUNT 10000000
#endif

#ifndef BEST_OF_N
#define BEST_OF_N 4
#endif

#ifndef DEMO_SEED
#define DEMO_SEED 42
#endif

#ifndef DEMO_STREAM
#define DEMO_STREAM 1
#endif

#ifndef DEMO_HIST_ROWS
#define DEMO_HIST_ROWS 12
#endif

#ifndef DEMO_HIST_COLS
#define DEMO_HIST_COLS 72
#endif

_Static_assert(SAMPLE_COUNT > 0, "SAMPLE_COUNT must be positive");
_Static_assert(BEST_OF_N > 0, "BEST_OF_N must be positive");
_Static_assert(DEMO_HIST_ROWS >= 0, "DEMO_HIST_ROWS must be nonnegative");
_Static_assert(DEMO_HIST_COLS >= 0, "DEMO_HIST_COLS must be nonnegative");

#if SAMPLE_COUNT > SIZE_MAX || BEST_OF_N > SIZE_MAX
#error "Sample and run counts must fit in size_t"
#endif

static void fill_samples(float *buffer, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    buffer[i] = rng_norm();
  }
}

int main(void) {
#ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
#endif

  const size_t count = (size_t)SAMPLE_COUNT;
  const size_t runs = (size_t)BEST_OF_N;

  if (count > SIZE_MAX / sizeof(float)) {
    fputs("Sample buffer size would overflow\n", stderr);
    return EXIT_FAILURE;
  }

  float *buffer = malloc(count * sizeof(*buffer));
  if (buffer == NULL) {
    fputs("Unable to allocate the sample buffer\n", stderr);
    return EXIT_FAILURE;
  }

  rng_seed(DEMO_SEED, DEMO_STREAM);
  fill_samples(buffer, count);

  uint64_t best_time_ns = UINT64_MAX;

  for (size_t run = 0; run < runs; ++run) {
    /* Repeat the same sequence so each run performs the same work. */
    rng_seed(DEMO_SEED, DEMO_STREAM);
    const uint64_t start = demo_now_ns();
    fill_samples(buffer, count);
    const uint64_t elapsed = demo_now_ns() - start;
    if (elapsed < best_time_ns) {
      best_time_ns = elapsed;
    }
  }

  if (DEMO_HIST_ROWS > 0 && DEMO_HIST_COLS > 0) {
    plt_hist(buffer, count, DEMO_HIST_ROWS, DEMO_HIST_COLS);
  }

  double sum = 0.0;
  double minimum = buffer[0];
  double maximum = buffer[0];

  // Stats for nerds
  for (size_t i = 0; i < count; ++i) {
    const double value = buffer[i];
    sum += value;

    if (value < minimum) {
      minimum = value;
    }
    if (value > maximum) {
      maximum = value;
    }
  }

  const double mean = sum / (double)count;
  double second_sum = 0.0;
  double third_sum = 0.0;
  double fourth_sum = 0.0;

  for (size_t i = 0; i < count; ++i) {
    const double deviation = (double)buffer[i] - mean;
    const double squared = deviation * deviation;

    second_sum += squared;
    third_sum += squared * deviation;
    fourth_sum += squared * squared;
  }

  const double variance = second_sum / (double)count;
  const double stddev = sqrt(variance);
  const double third_moment = third_sum / (double)count;
  const double fourth_moment = fourth_sum / (double)count;

  const double skewness =
      variance > 0.0 ? third_moment / (variance * stddev) : NAN;
  const double excess_kurtosis =
      variance > 0.0 ? fourth_moment / (variance * variance) - 3.0 : NAN;

  printf("range: [%.3f, %.3f]\n", minimum, maximum);
  printf("mean: %.6f | stddev: %.6f\n", mean, stddev);
  printf("skewness: %.6f | excess kurtosis: %.6f\n", skewness, excess_kurtosis);
  puts("Standard normal targets: mean 0, stddev 1, skewness 0, excess kurtosis "
       "0.");

  const double ns_per_sample = (double)best_time_ns / (double)count;
  const double throughput = ns_per_sample > 0.0 ? 1000.0 / ns_per_sample : NAN;

  printf("best of %zu: %.3f ns/sample | %.1f million samples/s\n", runs,
         ns_per_sample, throughput);

  free(buffer);
  return EXIT_SUCCESS;
}
