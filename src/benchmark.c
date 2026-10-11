#if !defined(_WIN32) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#include "demo_time.h"
#include "plt/plt.h"
#include "rng/rng.h"

#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef BENCH_CALLS
#define BENCH_CALLS 100000000
#endif

#ifndef BENCH_RUNS
#define BENCH_RUNS 20
#endif

#ifndef BENCH_WARMUP
#define BENCH_WARMUP (BENCH_CALLS / 10)
#endif

#ifndef BENCH_SEED
#define BENCH_SEED 42
#endif

#ifndef BENCH_STREAM
#define BENCH_STREAM 1
#endif

#ifndef BENCH_HIST_ROWS
#define BENCH_HIST_ROWS 6
#endif

#ifndef BENCH_HIST_COLS
#define BENCH_HIST_COLS 10
#endif

_Static_assert(BENCH_CALLS > 0, "BENCH_CALLS must be positive");
_Static_assert(BENCH_RUNS > 0, "BENCH_RUNS must be positive");
_Static_assert(BENCH_WARMUP >= 0, "BENCH_WARMUP must be nonnegative");
_Static_assert(BENCH_HIST_ROWS >= 0, "BENCH_HIST_ROWS must be nonnegative");
_Static_assert(BENCH_HIST_COLS >= 0, "BENCH_HIST_COLS must be nonnegative");

#if BENCH_CALLS > SIZE_MAX || BENCH_RUNS > SIZE_MAX || BENCH_WARMUP > SIZE_MAX
#error "Benchmark counts must fit in size_t"
#endif

typedef uint64_t (*bench_fn)(size_t n);
typedef void (*seed_fn)(void);

typedef struct {
  const char *name;
  bench_fn generate;
  seed_fn seed;
} benchmark;

typedef struct {
  uint64_t *elapsed;
  uint64_t checksum;
} benchmark_result;

/*
 * Generate seed and sampling functions with direct generator calls,
 * allowing inlining.
 *
 * Process four successive values per iteration using independent
 * accumulators to reduce loop overhead and checksum dependencies.
 * The RNG's state still advances sequentially. Handle the remaining
 * 0–3 values separately, making exactly n calls.
 *
 * Return a checksum consumed by the caller. Unsigned arithmetic wraps
 * by definition. Timing includes generation and checksum accumulation;
 * the checksum does not test randomness.
 */
#define DEFINE_BENCHMARK(name, next, reseed)                                   \
  static void seed_##name(void) { reseed; }                                    \
  static uint64_t bench_##name(size_t n) {                                     \
    uint64_t a = 0, b = 0, c = 0, d = 0;                                       \
    while (n >= 4) {                                                           \
      a += (uint64_t)(next);                                                   \
      b += (uint64_t)(next);                                                   \
      c += (uint64_t)(next);                                                   \
      d += (uint64_t)(next);                                                   \
      n -= 4;                                                                  \
    }                                                                          \
    while (n > 0) {                                                            \
      a += (uint64_t)(next);                                                   \
      --n;                                                                     \
    }                                                                          \
    return a + b + c + d;                                                      \
  }

DEFINE_BENCHMARK(rand, rand(), srand((unsigned int)BENCH_SEED))
#ifndef _WIN32
DEFINE_BENCHMARK(random, random(), srandom((unsigned int)BENCH_SEED))
#endif
DEFINE_BENCHMARK(rng_u, rng_u(), rng_seed(BENCH_SEED, BENCH_STREAM))

#undef DEFINE_BENCHMARK

static int compare_times(const void *lhs, const void *rhs) {
  const double a = *(const double *)lhs;
  const double b = *(const double *)rhs;

  return (a > b) - (a < b);
}

static void report(const benchmark *entry, const benchmark_result *result,
                   double *times, size_t runs) {
  double mean = 0.0;
  double m2 = 0.0;

  for (size_t run = 0; run < runs; ++run) {
    const double time = (double)result->elapsed[run] / (double)BENCH_CALLS;
    times[run] = time;

    /* Welford's algorithm for the mean and sample variance. */
    const double delta = time - mean;
    mean += delta / (double)(run + 1);
    m2 += delta * (time - mean);
  }

  qsort(times, runs, sizeof(*times), compare_times);

  const double median = runs % 2 != 0
                            ? times[runs / 2]
                            : (times[runs / 2 - 1] + times[runs / 2]) / 2.0;

  const double throughput = median > 0.0 ? 1000.0 / median : NAN;

  char stddev_text[32] = "N/A";
  char cv_text[32] = "N/A";

  if (runs > 1) {
    const double stddev = sqrt(fmax(m2, 0.0) / (double)(runs - 1));
    snprintf(stddev_text, sizeof(stddev_text), "%.3f", stddev);

    if (mean > 0.0) {
      snprintf(cv_text, sizeof(cv_text), "%.2f", 100.0 * stddev / mean);
    }
  }

  printf("%-8s %8.3f %8.3f %8s %7s %8.3f %8.3f %10.1f  %016" PRIx64 "\n",
         entry->name, median, mean, stddev_text, cv_text, times[0],
         times[runs - 1], throughput, result->checksum);
}

static void plot_times(const benchmark *entry, const benchmark_result *result,
                       size_t runs) {
  if (BENCH_HIST_ROWS == 0 || BENCH_HIST_COLS == 0) {
    return;
  }

  if (runs > SIZE_MAX / sizeof(float)) {
    fputs("Too many timings to plot\n", stderr);
    return;
  }

  float *values = malloc(runs * sizeof(*values));
  if (values == NULL) {
    fputs("Unable to allocate histogram samples\n", stderr);
    return;
  }

  uint64_t minimum = result->elapsed[0];
  uint64_t maximum = minimum;

  for (size_t run = 0; run < runs; ++run) {
    const uint64_t elapsed = result->elapsed[run];
    values[run] = (float)((double)elapsed / (double)BENCH_CALLS);

    if (elapsed < minimum) {
      minimum = elapsed;
    }
    if (elapsed > maximum) {
      maximum = elapsed;
    }
  }

  printf("\n%s: %.3f–%.3f ns/value, %zu runs\n", entry->name,
         (double)minimum / (double)BENCH_CALLS,
         (double)maximum / (double)BENCH_CALLS, runs);

  plt_hist(values, runs, BENCH_HIST_ROWS, BENCH_HIST_COLS);
  free(values);
}

static int write_csv(const char *path, const benchmark *entries,
                     const benchmark_result *results, size_t count,
                     size_t runs) {
  FILE *file = fopen(path, "w");
  if (file == NULL) {
    perror(path);
    return EXIT_FAILURE;
  }

  int failed = fprintf(file, "round,position,generator,calls,elapsed_ns,"
                             "ns_per_value\n") < 0;

  for (size_t run = 0; run < runs && !failed; ++run) {
    const size_t first = run % count;

    for (size_t position = 0; position < count; ++position) {
      const size_t i = (first + position) % count;
      const uint64_t elapsed = results[i].elapsed[run];

      if (fprintf(file, "%zu,%zu,%s,%zu,%" PRIu64 ",%.9f\n", run + 1,
                  position + 1, entries[i].name, (size_t)BENCH_CALLS, elapsed,
                  (double)elapsed / (double)BENCH_CALLS) < 0) {
        failed = 1;
        break;
      }
    }
  }

  if (fclose(file) != 0) {
    failed = 1;
  }

  if (failed) {
    fprintf(stderr, "Unable to finish writing %s\n", path);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

static void print_progress(size_t completed, size_t total) {
  const size_t width = 30;
  const double fraction = (double)completed / (double)total;
  const size_t half_blocks = (size_t)(fraction * (double)(width * 2));
  const size_t full_blocks = half_blocks / 2;
  const int has_half = half_blocks % 2 != 0;

  fputs("\r[", stderr);

  for (size_t i = 0; i < width; ++i) {
    if (i < full_blocks) {
      fputs("\u2588", stderr);
    } else if (i == full_blocks && has_half) {
      fputs("\u258c", stderr);
    } else {
      fputc(' ', stderr);
    }
  }

  fprintf(stderr, "] %5.1f%% (%zu/%zu rounds)",
          fraction * 100.0, completed, total);

  if (completed == total) {
    fputc('\n', stderr);
  }

  fflush(stderr);
}

int main(int argc, char **argv) {
  const benchmark benchmarks[] = {
      {"rand", bench_rand, seed_rand},
#ifndef _WIN32
      {"random", bench_random, seed_random},
#endif
      {"rng_u", bench_rng_u, seed_rng_u},
  };

  const size_t count = sizeof(benchmarks) / sizeof(benchmarks[0]);
  const size_t runs = (size_t)BENCH_RUNS;
  benchmark_result results[sizeof(benchmarks) / sizeof(benchmarks[0])] = {0};

  if (argc > 2) {
    fprintf(stderr, "Usage: %s [timings.csv]\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (runs > SIZE_MAX / sizeof(uint64_t) / count ||
      runs > SIZE_MAX / sizeof(double)) {
    fputs("Too many benchmark runs\n", stderr);
    return EXIT_FAILURE;
  }

  uint64_t *elapsed = malloc(count * runs * sizeof(*elapsed));
  double *times = malloc(runs * sizeof(*times));

  if (elapsed == NULL || times == NULL) {
    fputs("Unable to allocate benchmark timings\n", stderr);
    free(elapsed);
    free(times);
    return EXIT_FAILURE;
  }

  fprintf(stderr, "Warming up %zu generators...\n", count);
  fflush(stderr);

  for (size_t i = 0; i < count; ++i) {
    results[i].elapsed = elapsed + i * runs;
    benchmarks[i].seed();
    results[i].checksum = benchmarks[i].generate((size_t)BENCH_WARMUP);
  }

  fprintf(stderr, "Measuring %zu rounds...\n", runs);
  fflush(stderr);
  print_progress(0, runs);

  /* Rotate the first generator to distribute execution positions. */
  for (size_t run = 0; run < runs; ++run) {
    const size_t first = run % count;

    for (size_t position = 0; position < count; ++position) {
      const size_t i = (first + position) % count;
      const benchmark *entry = &benchmarks[i];

      entry->seed();

      const uint64_t start = demo_now_ns();
      const uint64_t checksum = entry->generate((size_t)BENCH_CALLS);
      const uint64_t end = demo_now_ns();

      results[i].elapsed[run] = end - start;
      results[i].checksum += checksum;
    }

    /* Update progress only after every generator has finished the round. */
    print_progress(run + 1, runs);
  }

  printf("%zu values/run | %zu runs | %zu warmup values\n", (size_t)BENCH_CALLS,
         runs, (size_t)BENCH_WARMUP);
  printf("PCG seed %" PRIu64 " | stream %" PRIu64 " | libc seed %u\n",
         (uint64_t)BENCH_SEED, (uint64_t)BENCH_STREAM,
         (unsigned int)BENCH_SEED);

  puts("Generator order rotates between rounds.");
  puts("Times: ns/value, including checksum accumulation.");
  puts("Stddev: sample standard deviation across runs; CV: stddev/mean (%).");
  puts("Mvalues/s: median throughput in million values/second.\n");

  printf("%-8s %8s %8s %8s %7s %8s %8s %10s  %s\n", "RNG", "Median", "Mean",
         "Stddev", "CV(%)", "Min", "Max", "Mvalues/s", "Checksum");

  for (size_t i = 0; i < count; ++i) {
    report(&benchmarks[i], &results[i], times, runs);
  }

  if (BENCH_HIST_ROWS != 0 && BENCH_HIST_COLS != 0) {
    puts(
        "\nTiming histograms: each has its own horizontal and vertical scale.");

    for (size_t i = 0; i < count; ++i) {
      plot_times(&benchmarks[i], &results[i], runs);
    }
  }

  int status = EXIT_SUCCESS;

  if (argc == 2) {
    status = write_csv(argv[1], benchmarks, results, count, runs);

    if (status == EXIT_SUCCESS) {
      printf("\nRaw timings saved to %s\n", argv[1]);
    }
  }

  free(times);
  free(elapsed);
  return status;
}