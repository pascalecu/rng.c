#ifndef RNG_H
#define RNG_H

/*
 * The PCG32 generator below is derived from PCG Random Number Generation for C
 * (https://github.com/imneme/pcg-c):
 *
 *   Copyright 2014-2019 Melissa O'Neill <oneill@pcg-random.org> and the PCG Project
 *   contributors.
 *
 * That code is offered under (Apache-2.0 OR MIT); it is used here under the MIT option,
 * the same license as the rest of this library. See https://www.pcg-random.org for
 * details on the generation scheme.
 *
 * SPDX-License-Identifier: MIT
 */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define RNG_ZIG_LAY_BITS 8

typedef struct {
  uint64_t state;
  uint64_t inc;
} pcg32_random_t;

extern pcg32_random_t pcg32_global;

extern const float lay_xs[];
extern const float lay_ys[];

static inline float prob_dens(float x) { return expf(-0.5f * x * x); }
static inline float prob_dens_inv(float y) { return sqrtf(-2.0f * logf(y)); }

/**
 * Initialize a caller-owned PCG32 generator.
 *
 * @param rng       Generator to initialize; must not be NULL.
 * @param initstate Seed value; all 64 bits are significant.
 * @param initseq   Stream selector; only the low 63 bits are significant.
 *
 * The same initialization arguments reproduce the same integer sequence.
 * Initializing this generator does not affect the global generator.
 *
 * Separate generator objects can be used concurrently. Access to the same
 * object requires synchronization.
 */
void pcg32_srandom_r(pcg32_random_t *rng, uint64_t initstate, uint64_t initseq);

/**
 * Initialize the shared global generator.
 *
 * @param seed Seed value; all 64 bits are significant.
 * @param seq  Stream selector; only the low 63 bits are significant.
 *
 * Reusing the same arguments reproduces the same integer sequence. All rng_*
 * sampling functions advance this shared state, so changing their call order
 * changes subsequent results.
 *
 * Calls that access the global generator require synchronization when used
 * concurrently.
 */
void rng_seed(uint64_t seed, uint64_t seq);

/**
 * Generate a uniformly distributed integer in [0, UINT32_MAX].
 *
 * @param rng Initialized generator; must not be NULL.
 * @return The next 32-bit value.
 *
 * Advances only the supplied generator by one step.
 */
static inline uint32_t pcg32_random_r(pcg32_random_t *rng) {
  uint64_t oldstate = rng->state;
  // LCG step (pcg_setseq_64_step_r)
  rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
  // XSH RR output (pcg_output_xsh_rr_64_32), fed the old state so the
  // step and the output function can overlap
  uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
  uint32_t rot = oldstate >> 59u;
  return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

/**
 * Generate a uniformly distributed integer in [0, UINT32_MAX].
 *
 * Advances the global generator by one step.
 */
static inline uint32_t rng_u(void) { return pcg32_random_r(&pcg32_global); }

/**
 * Generate a uniform value in [0, 1).
 *
 * On binary floating-point implementations with at least 24 bits of
 * precision, the result is one of 2^24 equally spaced values:
 * k / 2^24, where 0 ≤ k < 2^24.
 *
 * Zero is possible; one is excluded. This samples a uniform grid,
 * rather than every representable float in the interval.
 *
 * Advances the global generator by one step.
 */
static inline float rng_f(void) {
  return (pcg32_random_r(&pcg32_global) >> 8) * 0x1.0p-24f;
}

typedef union {
  float f;
  uint32_t u;
} Bits;

#define RNG_ZIG_LAY_MASK ((1u << RNG_ZIG_LAY_BITS) - 1)

/**
 * Generate an approximately standard normal value.
 *
 * Uses rejection sampling with the generated layer tables. The current tables
 * cap the sampled magnitude at 8, so this does not reproduce the unbounded
 * tails of an exact normal distribution.
 *
 * Consumes a variable number of values from the global generator. Exact
 * floating-point results can depend on the compiler and math library.
 *
 * The current sign-bit implementation assumes IEEE 754 binary32 floats.
 */
static inline float rng_norm(void) {
  Bits x;
  for (;;) {
    uint32_t roll = pcg32_random_r(&pcg32_global);

    uint32_t lay_ix = (roll >> 1) & RNG_ZIG_LAY_MASK;
    x.f = (roll >> 9) * 0x1.0p-23f * lay_xs[lay_ix];

    if (x.f > lay_xs[lay_ix + 1]) {
      float lay_h = lay_ys[lay_ix + 1] - lay_ys[lay_ix];
      float y = lay_ys[lay_ix] + rng_f() * lay_h;

      if (y > prob_dens(x.f)) {
        continue; // Reject
      }
    }

    x.u |= roll << 31;
    return x.f;
  }
}

/**
 * Generate a rough normal approximation by summing 12 uniform values.
 *
 * The result has bounded support near [-6, 6] and follows a different
 * distribution from rng_norm().
 *
 * Consumes exactly 12 values from the global generator. Retained for comparison
 * with the rejection sampler.
 */
static inline float rng_norm_old(void) {
  float result = -6.0f;

  for (size_t i = 0; i < 12; ++i) {
    result += rng_f();
  }

  return result;
}

#endif // RNG_H
