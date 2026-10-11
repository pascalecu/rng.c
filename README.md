# rng.c

A C library for generating pseudorandom integers and floating-point values,
based on [PCG](https://www.pcg-random.org/).

## Example usage

```c
#include "rng/rng.h"

#include <inttypes.h>
#include <stdio.h>

int main(void) {
  /* Seed and stream selector. Use fixed values for reproducible results. */
  rng_seed(42, 1);

  uint32_t integer = rng_u();
  float uniform = rng_f();
  float normal = rng_norm();

  printf("integer: %" PRIu32 "\n", integer);
  printf("uniform: %.6f\n", uniform);
  printf("normal:  %.6f\n", normal);

  return 0;
}
```

Available functions:

| Function | Result |
| --- | --- |
| `rng_seed(seed, stream)` | Initialize the generator using two `uint64_t` values |
| `rng_u()` | A pseudorandom 32-bit unsigned integer |
| `rng_f()` | A uniform floating-point value in `[0, 1)` |
| `rng_norm()` | An approximately standard normal value, with mean 0 and standard deviation 1 |

The convenience functions share one global generator state. Concurrent access
requires synchronization.

## Build

Requires a C17 compiler and GNU make. The Makefile supports Linux, macOS and
native Windows with a compatible toolchain such as
[WinLibs](https://winlibs.com/) or
[llvm-mingw](https://github.com/mstorsjo/llvm-mingw). This is untested on MSVC,
but it shouldn't be too hard to adapt this to your needs.

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

Executables are written to `bin/`; object and dependency files are written to
`obj/`.

To use the library in another project, add `src/` to the include path and
compile:

- `src/rng/rng.c`
- `src/rng/rng_zig_layers.c`

Link with the math library where required, typically `-lm` on Linux.

## Normal distribution demo

```sh
make run
```

The demo generates normal samples, draws a terminal histogram and reports the
range, mean, standard deviation, skewness and excess kurtosis.

It also reports the fastest sampling run. This measurement includes both
generation and writes to the sample buffer.

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

## Configuration

Demo settings can be overridden with preprocessor definitions.

For example, to change the benchmark workload:

```sh
make clean
make benchmark CPPFLAGS='-DBENCH_CALLS=10000000 -DBENCH_RUNS=24'
```

Common settings:

| Program | Definitions |
| --- | --- |
| Normal demo | `SAMPLE_COUNT`, `BEST_OF_N`, `DEMO_SEED`, `DEMO_STREAM`, `DEMO_HIST_ROWS`, `DEMO_HIST_COLS` |
| Benchmark | `BENCH_CALLS`, `BENCH_RUNS`, `BENCH_WARMUP`, `BENCH_SEED`, `BENCH_STREAM`, `BENCH_HIST_ROWS`, `BENCH_HIST_COLS` |
| Image demo | `WIDTH`, `HEIGHT`, `SAMPLE_COUNT`, `IMAGE_SEED`, `IMAGE_STREAM` |

Set either histogram dimension to zero to disable that program's histogram.

Clean before changing command-line compiler flags or definitions: make does not
automatically detect those changes.

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