#if !defined(_WIN32) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#include "demo_time.h"
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

_Static_assert(BENCH_CALLS > 0, "BENCH_CALLS must be positive");
_Static_assert(BENCH_RUNS > 0, "BENCH_RUNS must be positive");
_Static_assert(BENCH_WARMUP >= 0, "BENCH_WARMUP must be nonnegative");

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

static void bench(const benchmark *entry, double *times, size_t runs) {
  entry->seed();
  uint64_t checksum = entry->generate((size_t)BENCH_WARMUP);
  double mean = 0.0;
  double m2 = 0.0;

  for (size_t run = 0; run < runs; ++run) {
    entry->seed();

    const uint64_t start = demo_now_ns();
    const uint64_t result = entry->generate((size_t)BENCH_CALLS);
    const uint64_t end = demo_now_ns();

    checksum += result;

    const double time = (double)(end - start) / (double)BENCH_CALLS;
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

  /* A zero duration is below the timer's resolution. */
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
         times[runs - 1], throughput, checksum);
}

int main(void) {
  const benchmark benchmarks[] = {
      {"rand", bench_rand, seed_rand},
#ifndef _WIN32
      {"random", bench_random, seed_random},
#endif
      {"rng_u", bench_rng_u, seed_rng_u},
  };

  const size_t runs = (size_t)BENCH_RUNS;

  if (runs > SIZE_MAX / sizeof(double)) {
    fputs("Too many benchmark runs\n", stderr);
    return EXIT_FAILURE;
  }

  double *times = malloc(runs * sizeof(*times));
  if (times == NULL) {
    fputs("Unable to allocate benchmark timings\n", stderr);
    return EXIT_FAILURE;
  }

  printf("%zu values/run | %zu runs | %zu warmup values\n", (size_t)BENCH_CALLS,
         runs, (size_t)BENCH_WARMUP);
  printf("seed %" PRIu64 " | PCG stream %" PRIu64 "\n", (uint64_t)BENCH_SEED,
         (uint64_t)BENCH_STREAM);

  puts("Times: ns/value, including checksum accumulation.");
  puts("Stddev: sample standard deviation across runs; CV: stddev/mean (%).");
  puts("Mvalues/s: median throughput in million values/second.\n");

  printf("%-8s %8s %8s %8s %7s %8s %8s %10s  %s\n", "RNG", "Median", "Mean",
         "Stddev", "CV(%)", "Min", "Max", "Mvalues/s", "Checksum");

  for (size_t i = 0; i < sizeof(benchmarks) / sizeof(benchmarks[0]); ++i) {
    bench(&benchmarks[i], times, runs);
  }

  free(times);
  return EXIT_SUCCESS;
}