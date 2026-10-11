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

enum { RGB_CHANNELS = 3 };

_Static_assert(WIDTH > 0, "WIDTH must be positive");
_Static_assert(HEIGHT > 0, "HEIGHT must be positive");
_Static_assert(SAMPLE_COUNT >= 0, "SAMPLE_COUNT must be nonnegative");

#if WIDTH > SIZE_MAX || HEIGHT > SIZE_MAX || SAMPLE_COUNT > SIZE_MAX
#error "Image dimensions and sample count must fit in size_t"
#endif

/*
 * bound must be positive; threshold must equal 2^32 modulo bound. Rejecting
 * values below threshold makes the accepted range divisible by bound, so every
 * possible index has the same probability.
 */
static uint32_t random_below(uint32_t bound, uint32_t threshold) {
  for (;;) {
    const uint32_t value = (uint32_t)RNG_FUN;

    if (value >= threshold) {
      return value % bound;
    }
  }
}

static int write_ppm(const char *path, const uint8_t *pixels, size_t width,
                     size_t height, size_t bytes) {
  FILE *file = fopen(path, "wb");
  if (file == NULL) {
    perror(path);
    return EXIT_FAILURE;
  }

  int failed = fprintf(file, "P6\n%zu %zu\n255\n", width, height) < 0;

  if (!failed && fwrite(pixels, 1, bytes, file) != bytes) {
    failed = 1;
  }

  /* Buffered writes can fail during closing, too. */
  if (fclose(file) != 0) {
    failed = 1;
  }

  if (failed) {
    fprintf(stderr, "Unable to write the complete image to %s\n", path);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  if (argc > 2) {
    fprintf(stderr, "Usage: %s [output.ppm]\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char *path = argc == 2 ? argv[1] : "image.ppm";
  const size_t width = (size_t)WIDTH;
  const size_t height = (size_t)HEIGHT;
  const size_t samples = (size_t)SAMPLE_COUNT;

  if (height == 0 || width == 0 || width > SIZE_MAX / height) {
    fputs("Invalid image dimensions\n", stderr);
    return EXIT_FAILURE;
  }

  const size_t pixel_count = width * height;
  if (pixel_count > SIZE_MAX / RGB_CHANNELS) {
    fputs("Image buffer size would overflow\n", stderr);
    return EXIT_FAILURE;
  }

  if (pixel_count > UINT32_MAX) {
    fputs("Image has too many pixels for bounded sampling\n", stderr);
    return EXIT_FAILURE;
  }

  uint8_t *pix_buffer = calloc(pixel_count, RGB_CHANNELS);
  if (pix_buffer == NULL) {
    fputs("Unable to allocate the pixel buffer\n", stderr);
    return EXIT_FAILURE;
  }

  rng_seed(IMAGE_SEED, IMAGE_STREAM);

  /* Unselected pixels remain black; later updates replace earlier colours. */
  for (size_t i = 0; i < samples; ++i) {
    const uint32_t threshold =
        (UINT32_C(0) - (uint32_t)pixel_count) % (uint32_t)pixel_count;
    const size_t pixel_ix = random_below((uint32_t)pixel_count, threshold);
    uint8_t *pixel = &pix_buffer[pixel_ix * RGB_CHANNELS];

    const uint32_t color = (uint32_t)RNG_FUN;

    /* PPM stores red, green, and blue bytes in that order. */
    pixel[0] = (uint8_t)(color >> 24);
    pixel[1] = (uint8_t)(color >> 16);
    pixel[2] = (uint8_t)(color >> 8);
  }

  // Write a PPM image
  const int status = write_ppm(path, pix_buffer, width, height, pixel_count * RGB_CHANNELS);
  free(pix_buffer);

  if (status == EXIT_SUCCESS) {
    printf("Wrote %zu x %zu RGB pixels to %s (%zu random pixel updates)\n",
           width, height, path, samples);
  }

  return status;
}
