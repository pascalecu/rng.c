/*
 * The PCG32 seeding routine below is derived from PCG Random Number Generation for C
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

#include "rng.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

pcg32_random_t pcg32_global = {
    .state = UINT64_C(0x853c49e6748fea9b),
    .inc = UINT64_C(0xda3e39cb94b95bdb),
};

void pcg32_srandom_r(pcg32_random_t *rng, uint64_t initstate,
                     uint64_t initseq) {
  assert(rng != NULL);

  rng->state = UINT64_C(0);
  rng->inc = (initseq << 1u) | UINT64_C(1);

  /* Mix the stream selector, then the seed, into the initial state. */
  (void)pcg32_random_r(rng);
  rng->state += initstate;
  (void)pcg32_random_r(rng);
}

void rng_seed(uint64_t seed, uint64_t seq) {
  pcg32_srandom_r(&pcg32_global, seed, seq);
}