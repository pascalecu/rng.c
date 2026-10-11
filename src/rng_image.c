#include "rng/rng.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define RNG_FUN rng_u()

#define SAMPLE_COUNT (1 << 20)

#define WIDTH 960
#define HEIGHT 540
#define DEPTH 3 // RGB

int main(void) {
  const size_t width = (size_t)WIDTH;
  const size_t height = (size_t)HEIGHT;

  if (height == 0 || width == 0 || width > SIZE_MAX / height) {
    fputs("Invalid image dimensions\n", stderr);
    return EXIT_FAILURE;
  }

  const size_t pixel_count = width * height;
  if (pixel_count > SIZE_MAX / DEPTH) {
    fputs("Image buffer size would overflow\n", stderr);
    return EXIT_FAILURE;
  }

  uint8_t *pix_buffer = calloc(pixel_count, DEPTH);
  if (pix_buffer == NULL) {
    fputs("Unable to allocate the pixel buffer\n", stderr);
    return EXIT_FAILURE;
  }

  // Generate random pixels
  double rand_max_inv = 1.0 / (UINT32_MAX + 1.0);
  for (size_t i = 0; i < SAMPLE_COUNT; ++i) {
    uint32_t roll_a = RNG_FUN << 1;

    size_t pixel_ix = roll_a * rand_max_inv * pixel_count;
    uint8_t *pixel = &pix_buffer[pixel_ix * DEPTH];

    const uint32_t color = (uint32_t)RNG_FUN;

    /* PPM stores red, green, and blue bytes in that order. */
    pixel[0] = (uint8_t)(color >> 24);
    pixel[1] = (uint8_t)(color >> 16);
    pixel[2] = (uint8_t)(color >> 8);
  }

  // Write a PPM image
  FILE *fp = fopen("image.ppm", "wb");
  if (fp == NULL) {
    fprintf(stderr, "shit hit the fan :/");
    free(pix_buffer);
    return EXIT_FAILURE;
  }
  fprintf(fp, "P6\n");
  fprintf(fp, "%zu %zu\n", width, height);
  fprintf(fp, "255\n");
  size_t pixels_written = fwrite(pix_buffer, DEPTH, pixel_count, fp);
  printf("Wrote %zu pixels\n", pixels_written);
  fclose(fp);

  free(pix_buffer);
  return EXIT_SUCCESS;
}
