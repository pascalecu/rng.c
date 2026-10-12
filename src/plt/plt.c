#include "plt.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void plt_hist(float *data, size_t len, size_t rows, size_t cols) {
  if (data == NULL || len == 0 || rows == 0 || cols == 0) {
    return;
  }

  // Get the x range
  double x_min = (double)data[0], x_max = (double)data[0];

  for (size_t i = 0; i < len; ++i) {
    const double value = (double)data[i];

    if (!isfinite(value)) {
      fputs("Histogram data must be finite\n", stderr);
      return;
    }

    if (value < x_min) {
      x_min = value;
    }
    if (value > x_max) {
      x_max = value;
    }
  }

  const double x_range = x_max - x_min;

  // Keep track of bucket counts
  size_t *buckets = calloc(cols, sizeof(*buckets));
  if (buckets == NULL) {
    perror("calloc");
    return;
  }

  for (size_t i = 0; i < len; ++i) {
    size_t bucket_index = 0;

    if (x_range > 0.0) {
      const double fraction = ((double)data[i] - x_min) / x_range;
      const double position = fraction * (double)cols;

      /* Clamp before converting to an integer. */
      bucket_index = position >= (double)cols ? cols - 1 : (size_t)position;
    }

    buckets[bucket_index]++;
  }

  // Find the max count
  size_t bucket_max = 0;

  for (size_t i = 0; i < cols; ++i) {
    if (buckets[i] > bucket_max) {
      bucket_max = buckets[i];
    }
  }

  // clang-format off
  const char *blocks[] = {
    " ",
    "\u2581",
    "\u2582",
    "\u2583",
    "\u2584",
    "\u2585",
    "\u2586",
    "\u2587",
    "\u2588",
  };
  // clang-format on

  size_t block_count = sizeof(blocks) / sizeof(*blocks) - 1;

  // Scale heights
  for (size_t i = 0; i < cols; ++i) {
    buckets[i] = (buckets[i] * rows * block_count) / bucket_max;
  }

  // Plotting
  for (size_t i = 0; i < rows; ++i) {
    size_t height = (rows - 1 - i) * block_count;

    for (size_t j = 0; j < cols; ++j) {
      size_t block_ix = buckets[j] > height ? buckets[j] - height : 0;
      block_ix = block_ix > block_count ? block_count : block_ix;

      printf("%s", blocks[block_ix]);
    }

    printf("\n");
  }

  free(buckets);
}