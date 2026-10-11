#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "demo_time.h"
#include "plt/plt.h"
#include "rng/rng.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SAMPLE_COUNT (10 * 1000 * 1000)
#define BEST_OF_N 4

int main(void) {
  rng_seed(time(NULL), 1);

  // Sample random numbers
  float *buffer = malloc((size_t)SAMPLE_COUNT * sizeof(*buffer));
  if (buffer == NULL) {
    fputs("Unable to allocate the sample buffer\n", stderr);
    return EXIT_FAILURE;
  }

  uint64_t best_time_ns = UINT64_MAX;
  for (size_t i = 0; i < BEST_OF_N; ++i) {
    uint64_t sample_start_ns = demo_now_ns();
    for (size_t j = 0; j < SAMPLE_COUNT; ++j) {
      buffer[j] = rng_norm();
    }
    uint64_t sample_time_ns = demo_now_ns() - sample_start_ns;
    if (sample_time_ns < best_time_ns) {
      best_time_ns = sample_time_ns;
    }
  }
  double ns_per_sample = (double)best_time_ns / SAMPLE_COUNT;

  // Stats for nerds
  double mean = 0.0f;
  double x_min = buffer[0], x_max = buffer[0];
  for (size_t i = 0; i < SAMPLE_COUNT; ++i) {
    mean += buffer[i];

    if (buffer[i] < x_min) {
      x_min = buffer[i];
    }
    if (buffer[i] > x_max) {
      x_max = buffer[i];
    }
  }
  mean /= SAMPLE_COUNT;

  double var = 0.0f;
  double kur = 0.0f;
  for (size_t i = 0; i < SAMPLE_COUNT; ++i) {
    double dev = (buffer[i] - mean);
    double dev_sq = dev * dev;
    var += dev_sq;
    kur += dev_sq * dev_sq;
  }
  var /= SAMPLE_COUNT;
  kur /= SAMPLE_COUNT;
  double std = sqrtf(var);

  plt_hist(buffer, SAMPLE_COUNT, 12, 72);
  printf("range [%.1f, %.1f] | mean: %.3f | std: %.3f | kur: %.3f\n", x_min,
         x_max, mean, std, kur - 3.0);
  printf("sample time: %.2f ns\n", ns_per_sample);

  free(buffer);
  return EXIT_SUCCESS;
}
