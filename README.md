# rng.c

A C library for generating pseudorandom integers and floating-point values,
using [PCG32](https://www.pcg-random.org/) with uniform and approximately
standard normal sampling.

## Example usage

```c
#include "rng/rng.h"

#include <inttypes.h>
#include <stdio.h>

int main(void) {
  /* Seed and stream selector. Use fixed values for reproducible results. */
  rng_seed(42, 1);

  const uint32_t integer = rng_u();
  const uint32_t die = rng_bounded(6) + 1;
  const float uniform = rng_f();
  const float normal = rng_norm();

  printf("integer: %" PRIu32 "\n", integer);
  printf("die:     %" PRIu32 "\n", die);
  printf("uniform: %.6f\n", uniform);
  printf("normal:  %.6f\n", normal);

  return 0;
}
```

Core API:

| Operation | Global state | Caller-owned state |
| --- | --- | --- |
| Initialize with a seed and stream | `rng_seed(seed, stream)` | `pcg32_srandom_r(&rng, seed, stream)` |
| Generate a `uint32_t` | `rng_u()` | `pcg32_random_r(&rng)` |
| Generate a `uint32_t` in `[0, bound)` | `rng_bounded(bound)` | `rng_bounded_r(&rng, bound)` |
| Generate a uniform `float` in `[0, 1)` | `rng_f()` | `rng_f_r(&rng)` |
| Generate an approximately standard normal `float` | `rng_norm()` | `rng_norm_r(&rng)` |

Seeds and stream selectors are `uint64_t` values. Only the low 63 bits of the
stream selector are significant. A fixed seed and stream reproduce the raw PCG32
sequence. All sampling functions advance their generator's state, so mixing
integer and float calls changes which values later calls receive.

### Caller-owned state

For separate generator instances, initialize a `pcg32_random_t` and pass it to
the stateful functions:

```c
pcg32_random_t rng;
pcg32_srandom_r(&rng, 42, 1);

const uint32_t integer = pcg32_random_r(&rng);
const uint32_t die = rng_bounded_r(&rng, 6) + 1;
const float uniform = rng_f_r(&rng);
const float normal = rng_norm_r(&rng);
```

The convenience functions share one global generator state. Concurrent access to
that state, or to the same caller-owned state, requires synchronization.
Separate state objects can be used concurrently without changing the global
generator. Copy a state object to save and later resume its sequence.

### Sampling details

Bounded integer sampling requires a positive `uint32_t` bound. It uses rejection
sampling to avoid modulo bias and consumes one or more PCG32 draws, including
one draw when the bound is 1. For example, `rng_bounded(6) + 1` generates a
value from 1 through 6. Passing zero violates the API's precondition; it is not
a supported input, even when assertions are disabled.

Uniform sampling consumes one PCG32 draw and uses a grid of $2^{24}$ values on
binary32 implementations. It can return zero and never returns one.

Normal sampling uses rejection sampling with 256 layers and a variable number of
PCG32 draws. The current tables cap the magnitude at 8. The sampler assumes IEEE
754 binary32 `float`; exact normal sequences can depend on the math library and
floating-point settings.

`rng_norm_old()` remains available as a legacy approximation that sums twelve
uniform samples and subtracts six.

## Build

Requires a C17 compiler and GNU make. The Makefile supports Linux, macOS and
native Windows with a compatible toolchain such as
[llvm-mingw](https://github.com/mstorsjo/llvm-mingw).

```sh
make -j
```

To select a compiler:

```sh
make -j CC=clang
```

On native Windows:

```powershell
mingw32-make.exe -j CC=clang
```

Use `mingw32-make.exe` in place of `make` for the targets below on native
Windows.

Executables are written to `bin/`; object and dependency files are written to
`obj/`.

To use the library in another project, add `src/` to the include path and
compile:

- `src/rng/rng.c`
- `src/rng/rng_zig_layers.c`

Link with the math library where required, typically `-lm` on Linux.

For example, save the usage example as `example.c` and build it directly:

```sh
cc -std=c17 -O3 -Isrc example.c src/rng/rng.c src/rng/rng_zig_layers.c -lm -o example
```

## Tests

Run all 52 named regression tests:

```sh
make test
```

The tests use a checked-in copy of
[greatest](https://github.com/silentbicycle/greatest). No additional
installation or network access is required, and checks remain active with
`-DNDEBUG`.

Use `TEST_ARGS` to select a suite, filter test names, or list the available
cases:

```sh
make test TEST_ARGS="-v -s normal_tests"
make test TEST_ARGS="-v -s bounded_tests"
make test TEST_ARGS="-v -s smoke_tests"
make test TEST_ARGS="-v -t rejection"
make test TEST_ARGS="-l"
```

On native Windows, for example:

```powershell
mingw32-make.exe test TEST_ARGS="-v -s normal_tests"
```

Coverage includes upstream PCG reference vectors, seed boundaries, all output
rotations, state wraparound, stream selection, reseeding, snapshots, interleaved
states, and every tested seed/stream combination for the global wrappers.
Sampling tests cover bounded-integer limits, powers of two, forced rejection,
threshold equality, and global/local state agreement; uniform bounds and draw
counts; every normal layer and sign; rejection paths; tail limits; and table
density and area invariants.

Each named case reports independently and restores the global generator when it
finishes. Loop failures identify the input or sample; value and state
comparisons show expected and actual results. A filter that selects no tests
exits with failure; listing does not run tests.

Fixed-seed distribution smoke tests use broad tolerances to catch gross errors.
They do not establish statistical quality or independence.

Fixture sources, test details, and the greatest version and license are
documented in [tests/README.md](tests/README.md). The vendored header is
[tests/greatest.h](tests/greatest.h).

## Normal distribution demo

```sh
make run
```

The demo generates normal samples, draws a terminal histogram and reports the
range, mean, standard deviation, skewness and excess kurtosis.

It also reports the fastest sampling run. This measurement includes both
generation and writes to the sample buffer. An untimed warmup runs first, and
each timed run starts from the same seed and stream.

## Benchmark

```sh
make benchmark
```

The benchmark compares `rng_u()` with `rand()` and, on non-Windows builds,
`random()`.

It warms up the generators, repeats the measurements and rotates their execution
order between rounds. The summary reports:

- Median, mean, minimum and maximum time in nanoseconds per value.
- Sample standard deviation and coefficient of variation across runs.
- Throughput calculated from the median time.
- A checksum that keeps generated results observable.
- Histograms of run timings, each with its own scale.

Progress is written to standard error. A bar using full and half blocks appears
when standard error is connected to a terminal, and updates after each complete
round. The results table is written to standard output.

Timing includes checksum accumulation. These results measure generation speed
and timing variability; they do not measure randomness quality.

To save individual run timings as CSV:

```sh
./bin/benchmark timings.csv
```

On native Windows:

```powershell
.\bin\benchmark.exe timings.csv
```

The CSV records each round, execution position, generator, value count, elapsed
nanoseconds and time per value. The output file is overwritten if it exists.

Results depend on the processor, compiler, optimization flags, runtime library,
and system load.

## Random image demo

```sh
make image
make open
```

The image demo writes `image.ppm`, a binary RGB PPM image.

Pixels are selected with replacement: repeated selections overwrite earlier
colors and unselected pixels remain black.

To choose another output path:

```sh
./bin/rng_image noise.ppm
```

On native Windows:

```powershell
.\bin\rng_image.exe noise.ppm
```

## Configuration

Demo settings can be overridden with preprocessor definitions.

For example, to change the benchmark workload:

```sh
make clean
make benchmark CPPFLAGS="-DBENCH_CALLS=10000000 -DBENCH_RUNS=24"
```

Common settings:

| Program | Definitions |
| --- | --- |
| Normal demo | `SAMPLE_COUNT`, `BEST_OF_N`, `DEMO_SEED`, `DEMO_STREAM`, `DEMO_HIST_ROWS`, `DEMO_HIST_COLS` |
| Benchmark | `BENCH_CALLS`, `BENCH_RUNS`, `BENCH_WARMUP`, `BENCH_SEED`, `BENCH_STREAM`, `BENCH_HIST_ROWS`, `BENCH_HIST_COLS` |
| Benchmark progress | `BENCH_PROGRESS`, `BENCH_PROGRESS_WIDTH` |
| Image demo | `WIDTH`, `HEIGHT`, `SAMPLE_COUNT`, `IMAGE_SEED`, `IMAGE_STREAM` |

Set either histogram dimension to zero to disable that program's histogram. Set
`BENCH_PROGRESS=0` to disable the progress bar; its default width is 30 terminal
columns. `BENCH_PROGRESS_WIDTH` changes that width.

The image demo also accepts an `RNG_FUN` expression. It must produce uniformly
distributed values over the full `uint32_t` range for unbiased pixel selection.

Clean before changing the compiler, compiler flags or preprocessor definitions:
make does not automatically detect those command-line changes.

## Generated tables

Normal builds use the committed normal-distribution table. If it is missing, the
Makefile generates it automatically.

To regenerate it explicitly:

```sh
make regen
```

## Cleanup

Remove executables, objects and the default generated image:

```sh
make clean
```

To also remove the generated distribution table:

```sh
make clean-gen
```

## Blog article

Read more about how and why this library was created:

[Stop using
`rand()`](https://vanderoost.com/articles/2026/09/25/stop-using-rand/)

## Screencasts

Building the generator from scratch:

[![Coding RNG from scratch in
C](https://img.youtube.com/vi/-5NqTA-fDNQ/mqdefault.jpg)](https://youtu.be/-5NqTA-fDNQ)

Improving the random float generator:

[![Generating better random floats in
C](https://img.youtube.com/vi/qa83_KrkVIM/mqdefault.jpg)](https://youtu.be/qa83_KrkVIM)

## License

The library is licensed under the [MIT License](LICENSE). PCG attribution is
retained in the source files. The vendored greatest test runner uses the ISC
license; its notice is retained in [tests/greatest.h](tests/greatest.h).
