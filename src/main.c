#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "demo_time.h"
#include "plt/plt.h"
#include "rng/rng.h"
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

  const double ns_per_sample = (double)best_time_ns / (double)count;

  // Stats for nerds
  double mean = 0.0f;
  double x_min = buffer[0], x_max = buffer[0];
  for (size_t i = 0; i < count; ++i) {
    mean += buffer[i];

    if (buffer[i] < x_min) {
      x_min = buffer[i];
    }
    if (buffer[i] > x_max) {
      x_max = buffer[i];
    }
  }
  mean /= (double)count;

  double var = 0.0f;
  double kur = 0.0f;
  for (size_t i = 0; i < count; ++i) {
    double dev = (buffer[i] - mean);
    double dev_sq = dev * dev;
    var += dev_sq;
    kur += dev_sq * dev_sq;
  }
  var /= (double)count;
  kur /= (double)count;
  double std = sqrtf(var);

  if (DEMO_HIST_ROWS > 0 && DEMO_HIST_COLS > 0) {
    plt_hist(buffer, count, DEMO_HIST_ROWS, DEMO_HIST_COLS);
  }
  printf("range [%.1f, %.1f] | mean: %.3f | std: %.3f | kur: %.3f\n", x_min,
         x_max, mean, std, kur - 3.0);
  printf("sample time: %.2f ns\n", ns_per_sample);

  free(buffer);
  return EXIT_SUCCESS;
}
