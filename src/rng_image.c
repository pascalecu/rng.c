#include "rng/rng.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Must produce uniformly distributed values over the full uint32_t range. */
#ifndef RNG_FUN
#define RNG_FUN rng_u()
#endif

#ifndef SAMPLE_COUNT
#define SAMPLE_COUNT (1u << 20)
#endif

#ifndef WIDTH
#define WIDTH 960
#endif

#ifndef HEIGHT
#define HEIGHT 540
#endif

#ifndef IMAGE_SEED
#define IMAGE_SEED 42
#endif

#ifndef IMAGE_STREAM
#define IMAGE_STREAM 1
#endif

#define DEPTH 3 // RGB

_Static_assert(WIDTH > 0, "WIDTH must be positive");
_Static_assert(HEIGHT > 0, "HEIGHT must be positive");
_Static_assert(SAMPLE_COUNT >= 0, "SAMPLE_COUNT must be nonnegative");

#if WIDTH > SIZE_MAX || HEIGHT > SIZE_MAX || SAMPLE_COUNT > SIZE_MAX
#error "Image dimensions and sample count must fit in size_t"
#endif

/* bound must be positive. Reject the incomplete interval to avoid bias. */
static uint32_t random_below(uint32_t bound) {
  const uint32_t threshold = (UINT32_C(0) - bound) % bound;

  for (;;) {
    const uint32_t value = (uint32_t)RNG_FUN;
    if (value >= threshold) {
      return value % bound;
    }
  }
}

int main(void) {
  const size_t width = (size_t)WIDTH;
  const size_t height = (size_t)HEIGHT;
  const size_t samples = (size_t)SAMPLE_COUNT;

  if (height == 0 || width == 0 || width > SIZE_MAX / height) {
    fputs("Invalid image dimensions\n", stderr);
    return EXIT_FAILURE;
  }

  const size_t pixel_count = width * height;
  if (pixel_count > SIZE_MAX / DEPTH) {
    fputs("Image buffer size would overflow\n", stderr);
    return EXIT_FAILURE;
  }

  if (pixel_count > UINT32_MAX) {
    fputs("Image has too many pixels for bounded sampling\n", stderr);
    return EXIT_FAILURE;
  }

  uint8_t *pix_buffer = calloc(pixel_count, DEPTH);
  if (pix_buffer == NULL) {
    fputs("Unable to allocate the pixel buffer\n", stderr);
    return EXIT_FAILURE;
  }

  rng_seed(IMAGE_SEED, IMAGE_STREAM);

  /* Unselected pixels remain black; later updates replace earlier colours. */
  for (size_t i = 0; i < samples; ++i) {
    const size_t pixel_ix = random_below((uint32_t)pixel_count);
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
