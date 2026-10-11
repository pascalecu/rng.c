#include "rng/rng.h"
#include "greatest.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 &&
                   sizeof(float) == sizeof(uint32_t),
               "These tests require the sampler's binary32 float format");

/* Format context only when it changes, and failure details only on failure. */
static char context[192];
static char failure_text[512];
static size_t sample_index;
static pcg32_random_t saved_global;

static void set_context(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(context, sizeof(context), format, args);
  va_end(args);
  sample_index = SIZE_MAX;
}

static const char *failure(const char *expression) {
  if (sample_index == SIZE_MAX) {
    snprintf(failure_text, sizeof(failure_text), "%s%s%s", context,
             context[0] != '\0' ? ": " : "", expression);
  } else {
    snprintf(failure_text, sizeof(failure_text), "%s%sindex %zu: %s", context,
             context[0] != '\0' ? ": " : "", sample_index, expression);
  }
  return failure_text;
}

static void setup(void *unused) {
  (void)unused;
  saved_global = pcg32_global;
  context[0] = '\0';
  sample_index = SIZE_MAX;
}

static void teardown(void *unused) {
  (void)unused;
  pcg32_global = saved_global;
}

static int same_state(pcg32_random_t a, pcg32_random_t b) {
  return a.state == b.state && a.inc == b.inc;
}

static uint32_t float_bits(float value) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  return bits;
}

static int equal_state(const void *expected, const void *actual, void *unused) {
  (void)unused;
  return same_state(*(const pcg32_random_t *)expected,
                    *(const pcg32_random_t *)actual);
}

static int print_state(const void *value, void *unused) {
  (void)unused;
  const pcg32_random_t *state = value;
  return fprintf(GREATEST_STDOUT,
                 "{state=0x%016" PRIx64 ", inc=0x%016" PRIx64 "}", state->state,
                 state->inc);
}

static int equal_float(const void *expected, const void *actual, void *unused) {
  (void)unused;
  const float a = *(const float *)expected;
  const float b = *(const float *)actual;
  return isfinite(a) && isfinite(b) && float_bits(a) == float_bits(b);
}

static int print_float(const void *value, void *unused) {
  (void)unused;
  const float number = *(const float *)value;
  return fprintf(GREATEST_STDOUT, "%a (bits 0x%08" PRIx32 ")", (double)number,
                 float_bits(number));
}

static greatest_type_info state_type = {equal_state, print_state};
static greatest_type_info float_type = {equal_float, print_float};

/* Function arguments are evaluated once, including when an assertion fails. */
TEST check_u32(uint32_t expected, uint32_t actual, const char *expression) {
  ASSERT_EQ_FMTm(failure(expression), expected, actual, "0x%08" PRIx32);
  PASS();
}

TEST check_state(pcg32_random_t expected, pcg32_random_t actual,
                 const char *expression) {
  ASSERT_EQUAL_Tm(failure(expression), &expected, &actual, &state_type, NULL);
  PASS();
}

TEST check_float(float expected, float actual, const char *expression) {
  ASSERT_EQUAL_Tm(failure(expression), &expected, &actual, &float_type, NULL);
  PASS();
}

TEST check_near(double expected, double actual, double tolerance,
                const char *expression) {
  /* Comparisons alone can silently accept NaNs in a tolerance check. */
  const int valid = isfinite(expected) && isfinite(actual) &&
                    isfinite(tolerance) && tolerance >= 0.0 &&
                    fabs(actual - expected) <= tolerance;
  if (!valid) {
    fprintf(GREATEST_STDOUT, "\nExpected %.17g +/- %.17g, got %.17g\n",
            expected, tolerance, actual);
  }
  ASSERTm(failure(expression), valid);
  PASS();
}

#define EXPECT(expression) ASSERTm(failure(#expression), (expression))
#define EXPECT_U32(expected, actual)                                           \
  CHECK_CALL(check_u32((expected), (actual), #actual))
#define EXPECT_STATE(expected, actual)                                         \
  CHECK_CALL(check_state((expected), (actual), #actual))
#define EXPECT_FLOAT(expected, actual)                                         \
  CHECK_CALL(check_float((expected), (actual), #actual))
#define EXPECT_NEAR(expected, actual, tolerance)                               \
  CHECK_CALL(check_near((expected), (actual), (tolerance), #actual))

TEST check_mixed_draws(pcg32_random_t *expected, pcg32_random_t *actual) {
  EXPECT_U32(pcg32_random_r(expected), pcg32_random_r(actual));
  EXPECT_FLOAT(rng_f_r(expected), rng_f_r(actual));
  EXPECT_FLOAT(rng_norm_r(expected), rng_norm_r(actual));
  EXPECT_STATE(*expected, *actual);
  PASS();
}

typedef struct {
  const char *name;
  uint64_t seed, stream;
  pcg32_random_t initial;
  uint32_t words[6];
} seed_case;

/* Published PCG demo plus independent fixtures from upstream pcg-c-basic. */
static const seed_case seed_cases[] = {
    {"published",
     42,
     54,
     {UINT64_C(0x185706b82c2e03f8), 109},
     {0xa15c02b7, 0x7b47f409, 0xba1d3330, 0x83d2f293, 0xbfa4784b, 0xcbed606e}},
    {"zero_zero",
     0,
     0,
     {UINT64_C(0x5851f42d4c957f2e), 1},
     {0xe4c14788, 0x379c6516, 0x5c4ab3bb, 0x601d23e0, 0x1c382b8c, 0xd1faab16}},
    {"zero_max",
     0,
     UINT64_MAX,
     {UINT64_C(0xa7ae0bd2b36a80d2), UINT64_MAX},
     {0x44726f5c, 0xcbd39f38, 0x9e250255, 0x463f303a, 0xf97183f8, 0x0a562dbc}},
    {"max_zero",
     UINT64_MAX,
     0,
     {1, 1},
     {0x00000000, 0xe4c14788, 0x379c6516, 0x5c4ab3bb, 0x601d23e0, 0x1c382b8c}},
    {"max_max",
     UINT64_MAX,
     UINT64_MAX,
     {UINT64_C(0x4f5c17a566d501a5), UINT64_MAX},
     {0x2675c047, 0x7779a837, 0xa145aa13, 0x5f6be726, 0x523c44c5, 0x75a406d6}},
    {"high_bits",
     UINT64_C(1) << 63,
     (UINT64_C(1) << 63) - 1,
     {UINT64_C(0x27ae0bd2b36a80d2), UINT64_MAX},
     {0x6f5c0472, 0x9738cbd3, 0x12559e25, 0x383a463f, 0x83f8fb71, 0x2dbc0ad6}},
};

TEST test_reference_sequence(const seed_case *entry) {
  set_context("seed=0x%016" PRIx64 ", stream=0x%016" PRIx64, entry->seed,
              entry->stream);
  pcg32_random_t local = {UINT64_MAX, UINT64_MAX};
  pcg32_global = local;
  pcg32_srandom_r(&local, entry->seed, entry->stream);
  rng_seed(entry->seed, entry->stream);

  EXPECT_STATE(entry->initial, local);
  EXPECT_STATE(entry->initial, pcg32_global);
  for (size_t i = 0; i < COUNT_OF(entry->words); ++i) {
    sample_index = i;
    EXPECT_U32(entry->words[i], pcg32_random_r(&local));
    EXPECT_U32(entry->words[i], rng_u());
  }
  EXPECT_STATE(local, pcg32_global);
  PASS();
}

TEST test_all_output_rotations(void) {
  /* Fixed upstream outputs for states with rotation counts 0 through 31. */
  const uint32_t expected[] = {
      0x0468adeb, 0x823476f5, 0xc11a0b7a, 0x608d0dbd, 0xb0469ade, 0x58234f6f,
      0xac11a4b7, 0xd608d2db, 0xeb046aad, 0xf5823576, 0x7ac11a8b, 0xbd608d4d,
      0xdeb046ba, 0x6f58235f, 0xb7ac11ac, 0xdbd608d6, 0xadeb046c, 0x76f58236,
      0x0b7ac11b, 0x8dbd608d, 0xdadeb046, 0x6f6f5823, 0xb4b7ac11, 0xdadbd608,
      0x6eadeb04, 0x3776f582, 0x1b8b7ac1, 0x8dcdbd60, 0x46fadeb0, 0x237f6f58,
      0x11bcb7ac, 0x08dedbd6,
  };

  for (size_t rotation = 0; rotation < 32; ++rotation) {
    sample_index = rotation;
    pcg32_random_t local = {
        ((uint64_t)rotation << 59) | UINT64_C(0x0023456789abcdef), 1};
    EXPECT_U32(expected[rotation], pcg32_random_r(&local));
  }
  PASS();
}

TEST test_state_wraparound(void) {
  pcg32_random_t local = {UINT64_MAX, UINT64_MAX};
  const uint32_t actual = pcg32_random_r(&local);

  const pcg32_random_t expected = {UINT64_C(0xa7ae0bd2b36a80d2), UINT64_MAX};
  EXPECT_U32(UINT32_C(0xfff00001), actual);
  EXPECT_STATE(expected, local);
  PASS();
}

TEST test_reseeding(void) {
  uint32_t integers[64];
  float uniforms[64];
  float normals[64];

  rng_seed(42, 1);
  for (size_t i = 0; i < 64; ++i) {
    integers[i] = rng_u();
    uniforms[i] = rng_f();
    normals[i] = rng_norm();
  }
  const pcg32_random_t final_state = pcg32_global;

  rng_seed(42, 1);
  for (size_t i = 0; i < 64; ++i) {
    sample_index = i;
    EXPECT_U32(integers[i], rng_u());
    EXPECT_FLOAT(uniforms[i], rng_f());
    EXPECT_FLOAT(normals[i], rng_norm());
  }
  EXPECT_STATE(final_state, pcg32_global);
  PASS();
}

TEST test_stream_selector(void) {
  const uint64_t streams[] = {0, 1, 54, UINT64_C(1) << 62,
                              (UINT64_C(1) << 63) - 1};

  for (size_t s = 0; s < COUNT_OF(streams); ++s) {
    set_context("stream=0x%016" PRIx64, streams[s]);
    pcg32_random_t a, b;
    pcg32_srandom_r(&a, 42, streams[s]);
    pcg32_srandom_r(&b, 42, streams[s] | (UINT64_C(1) << 63));

    /* The high bit is discarded when the stream selector is encoded. */
    EXPECT_STATE(a, b);
    for (size_t i = 0; i < 64; ++i) {
      sample_index = i;
      EXPECT_U32(pcg32_random_r(&a), pcg32_random_r(&b));
    }
  }
  PASS();
}

TEST test_different_streams(void) {
  const uint64_t streams[] = {0, 1, UINT64_C(1) << 62, (UINT64_C(1) << 63) - 1};
  pcg32_random_t states[COUNT_OF(streams)];

  for (size_t i = 0; i < COUNT_OF(streams); ++i) {
    set_context("stream=0x%016" PRIx64, streams[i]);
    pcg32_srandom_r(&states[i], 42, streams[i]);
    EXPECT((states[i].inc & 1u) != 0);
    for (size_t j = 0; j < i; ++j) {
      sample_index = j;
      EXPECT(states[i].inc != states[j].inc);
    }
  }
  PASS();
}

TEST test_reseeding_local_state(void) {
  pcg32_random_t reference, local;
  pcg32_srandom_r(&reference, 42, 1);
  pcg32_srandom_r(&local, UINT64_MAX, UINT64_MAX);
  for (size_t i = 0; i < 37; ++i) {
    (void)rng_norm_r(&local);
  }

  pcg32_srandom_r(&local, 42, 1);
  EXPECT_STATE(reference, local);
  for (size_t i = 0; i < 64; ++i) {
    sample_index = i;
    CHECK_CALL(check_mixed_draws(&reference, &local));
  }
  PASS();
}

TEST test_state_snapshot(void) {
  pcg32_random_t local;
  pcg32_srandom_r(&local, 42, 54);
  for (size_t i = 0; i < 37; ++i) {
    (void)rng_norm_r(&local);
  }

  pcg32_random_t snapshot = local;
  for (size_t i = 0; i < 64; ++i) {
    sample_index = i;
    CHECK_CALL(check_mixed_draws(&snapshot, &local));
  }
  PASS();
}

TEST test_interleaved_states(void) {
  pcg32_random_t first, second;
  pcg32_srandom_r(&first, 42, 1);
  pcg32_srandom_r(&second, UINT64_MAX, 54);
  pcg32_random_t first_reference = first;
  pcg32_random_t second_reference = second;

  for (size_t i = 0; i < 64; ++i) {
    sample_index = i;
    CHECK_CALL(check_mixed_draws(&first_reference, &first));
    const pcg32_random_t first_before = first;
    for (size_t repeat = 0; repeat <= i % 3; ++repeat) {
      CHECK_CALL(check_mixed_draws(&second_reference, &second));
    }
    EXPECT_STATE(first_before, first);
  }
  PASS();
}

TEST test_independent_state(void) {
  rng_seed(123, 7);
  const pcg32_random_t global_before = pcg32_global;

  pcg32_random_t local;
  pcg32_srandom_r(&local, 42, 1);
  const pcg32_random_t local_before = local;

  EXPECT_STATE(global_before, pcg32_global);
  for (size_t i = 0; i < 10000; ++i) {
    (void)pcg32_random_r(&local);
    (void)rng_f_r(&local);
    (void)rng_norm_r(&local);
  }
  EXPECT(!same_state(local, local_before));
  EXPECT_STATE(global_before, pcg32_global);
  PASS();
}

TEST test_global_wrappers(uint64_t seed, uint64_t stream) {
  set_context("seed=0x%016" PRIx64 ", stream=0x%016" PRIx64, seed, stream);
  pcg32_random_t local;
  pcg32_srandom_r(&local, seed, stream);
  rng_seed(seed, stream);

  for (size_t i = 0; i < 10000; ++i) {
    sample_index = i;
    EXPECT_U32(pcg32_random_r(&local), rng_u());
    EXPECT_FLOAT(rng_f_r(&local), rng_f());
    EXPECT_FLOAT(rng_norm_r(&local), rng_norm());
    EXPECT_STATE(local, pcg32_global);
  }
  PASS();
}

TEST test_uniform_values(void) {
  /* These valid states produce raw words 0 and UINT32_MAX, respectively. */
  pcg32_random_t zero = {0, 1};
  pcg32_random_t maximum = {UINT64_C(0x07fffe0000000000), 1};
  EXPECT_FLOAT(0.0f, rng_f_r(&zero));
  EXPECT_FLOAT(0x1.fffffep-1f, rng_f_r(&maximum));

  pcg32_random_t local;
  pcg32_srandom_r(&local, 42, 1);
  for (size_t i = 0; i < 100000; ++i) {
    sample_index = i;
    const pcg32_random_t before = local;
    const float value = rng_f_r(&local);
    const float scaled = value * 0x1.0p24f;

    EXPECT(value >= 0.0f && value < 1.0f);
    EXPECT(scaled == floorf(scaled));

    pcg32_random_t expected = before;
    (void)pcg32_random_r(&expected);
    EXPECT_STATE(expected, local);
  }
  PASS();
}

TEST test_uniform_discards_low_bits(void) {
  /* These states produce 0x12345600 through 0x123456ff as their first word. */
  for (uint32_t low = 0; low < 256; ++low) {
    sample_index = low;
    pcg32_random_t local = {
        UINT64_C(0x0091a29468a51a29) ^ ((uint64_t)low << 27), 1};
    pcg32_random_t raw = local;
    const uint32_t word = pcg32_random_r(&raw);
    EXPECT_U32(UINT32_C(0x12345600) | low, word);

    const float actual = rng_f_r(&local);
    EXPECT_FLOAT(0x1.23456p-4f, actual);
    EXPECT_STATE(raw, local);
  }
  PASS();
}

TEST test_uniform_distribution_smoke(void) {
  /* Fixed seed and broad bounds catch gross errors, not statistical quality. */
  const size_t count = 100000;
  size_t bins[16] = {0};
  double sum = 0.0;
  double squared_sum = 0.0;
  pcg32_random_t local;
  pcg32_srandom_r(&local, 42, 54);

  for (size_t i = 0; i < count; ++i) {
    sample_index = i;
    const double value = rng_f_r(&local);
    EXPECT(value >= 0.0 && value < 1.0);
    ++bins[(size_t)(value * 16.0)];
    sum += value;
    squared_sum += value * value;
  }

  const double mean = sum / (double)count;
  const double variance = squared_sum / (double)count - mean * mean;
  set_context("uniform moments, %zu samples", count);
  EXPECT_NEAR(0.5, mean, 0.005);
  EXPECT_NEAR(1.0 / 12.0, variance, 0.002);
  set_context("uniform bin counts");
  for (size_t i = 0; i < COUNT_OF(bins); ++i) {
    sample_index = i;
    EXPECT_NEAR((double)count / 16.0, (double)bins[i], 375.0);
  }
  PASS();
}

TEST check_layer_case(pcg32_random_t initial, float expected, size_t draws) {
  pcg32_random_t after = initial;
  for (size_t i = 0; i < draws; ++i) {
    (void)pcg32_random_r(&after);
  }

  EXPECT_FLOAT(expected, rng_norm_r(&initial));
  EXPECT_STATE(after, initial);
  PASS();
}

typedef struct {
  const char *name;
  pcg32_random_t initial, after;
  float value;
  size_t draws;
} normal_case;

/* Final states are frozen upstream PCG results, not calculated by the RNG
 * under test. Float fixtures avoid platform-sensitive rejection decisions. */
static const normal_case normal_cases[] = {
    {"slow_accept",
     {UINT64_C(0x35a6ef94402c5926), 3},
     {UINT64_C(0x8bf618ff38964f20), 3},
     0x1.19753p+0f,
     2},
    {"one_rejection",
     {UINT64_C(0xd0377236d96c40ac), 3},
     {UINT64_C(0x918fa3f4da004be1), 3},
     0x1.1db458p-1f,
     3},
    {"two_rejections",
     {UINT64_C(0x3550caabcb23a47e), 3},
     {UINT64_C(0x4013cbf651e29edd), 3},
     -0x1.f0c20cp-2f,
     5},
    {"positive_tail",
     {UINT64_C(0x07fffe0fff83ffe1), UINT64_C(0xf1841115dfe66673)},
     {UINT64_C(0xf1841115dfe66673), UINT64_C(0xf1841115dfe66673)},
     0x1.fffffcp+2f,
     2},
    {"negative_tail",
     {UINT64_C(0x07fffe0ff783fde1), UINT64_C(0xffd117a872e4c073)},
     {UINT64_C(0xffd117a872e4c073), UINT64_C(0xffd117a872e4c073)},
     -0x1.fffffcp+2f,
     2},
};

TEST test_normal_path(const normal_case *entry) {
  set_context("%s, %zu raw draws", entry->name, entry->draws);
  pcg32_random_t local = entry->initial;
  EXPECT_FLOAT(entry->value, rng_norm_r(&local));
  EXPECT_STATE(entry->after, local);
  PASS();
}

TEST test_normal_all_layers_and_signs(void) {
  const size_t layers = (size_t)1 << RNG_ZIG_LAY_BITS;

  for (size_t layer = 0; layer < layers; ++layer) {
    for (uint32_t sign = 0; sign < 2; ++sign) {
      set_context("layer=%zu, sign=%" PRIu32, layer, sign);
      /* First word: position 1, selected layer, selected sign, rotation 0. */
      const pcg32_random_t initial = {(UINT64_C(1) << 36) |
                                          ((uint64_t)layer << 28) |
                                          ((uint64_t)sign << 27),
                                      1};
      const float magnitude = lay_xs[layer] * 0x1p-23f;
      const float expected = sign != 0 ? -magnitude : magnitude;

      /* Only the last layer needs a vertical draw at this position. */
      const size_t draws = layer == layers - 1 ? 2 : 1;
      CHECK_CALL(check_layer_case(initial, expected, draws));
    }
  }
  PASS();
}

TEST test_normal_zero_at_last_layer(void) {
  const size_t layer = ((size_t)1 << RNG_ZIG_LAY_BITS) - 1;
  const uint64_t after[] = {UINT64_C(0xf48e9add30000001),
                            UINT64_C(0x5ef346d698000001)};

  for (uint32_t sign = 0; sign < 2; ++sign) {
    set_context("last layer, sign=%" PRIu32 ", one raw draw", sign);
    /* x equals the next layer's zero width, so no vertical draw is needed. */
    pcg32_random_t local = {((uint64_t)layer << 28) | ((uint64_t)sign << 27),
                            1};
    const pcg32_random_t expected = {after[sign], 1};
    EXPECT_FLOAT(sign != 0 ? -0.0f : 0.0f, rng_norm_r(&local));
    EXPECT_STATE(expected, local);
  }
  PASS();
}

TEST test_normal_density_boundary(void) {
  /* Last layer, position 1; rounding makes y equal the density, both 1. */
  pcg32_random_t local = {UINT64_C(0x0000001ff0000001),
                          UINT64_C(0xf1c77c25836a80d3)};
  const pcg32_random_t after = {UINT64_C(0x2ec92225836a80d3),
                                UINT64_C(0xf1c77c25836a80d3)};
  const size_t layer = ((size_t)1 << RNG_ZIG_LAY_BITS) - 1;
  const float expected = lay_xs[layer] * 0x1p-23f;

  EXPECT_FLOAT(1.0f, prob_dens(expected));
  EXPECT_FLOAT(expected, rng_norm_r(&local));
  EXPECT_STATE(after, local);
  PASS();
}

TEST test_normal_values(void) {
  pcg32_random_t positive_zero = {0, 1};
  pcg32_random_t negative_zero = {UINT64_C(1) << 27, 1};
  EXPECT_FLOAT(0.0f, rng_norm_r(&positive_zero));
  EXPECT_FLOAT(-0.0f, rng_norm_r(&negative_zero));

  pcg32_random_t local;
  pcg32_srandom_r(&local, 42, 1);

  for (size_t i = 0; i < 100000; ++i) {
    sample_index = i;
    const float value = rng_norm_r(&local);
    EXPECT(isfinite(value));
    EXPECT(fabsf(value) <= lay_xs[0]);
  }
  PASS();
}

TEST test_normal_distribution_smoke(uint64_t seed, uint64_t stream) {
  /* Broad moment checks across fixed seeds, allowing small libm differences. */
  set_context("seed=0x%016" PRIx64 ", stream=0x%016" PRIx64, seed, stream);
  const size_t count = 100000;
  pcg32_random_t local;
  pcg32_srandom_r(&local, seed, stream);
  double sum = 0.0;
  double squared_sum = 0.0;
  double third_sum = 0.0;
  double fourth_sum = 0.0;
  size_t negative = 0;

  for (size_t i = 0; i < count; ++i) {
    sample_index = i;
    const double value = rng_norm_r(&local);
    EXPECT(isfinite(value));
    const double squared = value * value;
    sum += value;
    squared_sum += squared;
    third_sum += squared * value;
    fourth_sum += squared * squared;
    negative += value < 0.0;
  }

  sample_index = SIZE_MAX;
  const double mean = sum / (double)count;
  const double variance = squared_sum / (double)count - mean * mean;
  EXPECT_NEAR(0.0, mean, 0.02);
  EXPECT_NEAR(1.0, variance, 0.04);
  EXPECT_NEAR(0.0, third_sum / (double)count, 0.08);
  EXPECT_NEAR(3.0, fourth_sum / (double)count, 0.3);
  EXPECT_NEAR(0.5, (double)negative / (double)count, 0.02);
  PASS();
}

TEST test_legacy_normal(void) {
  rng_seed(42, 1);

  for (size_t i = 0; i < 1000; ++i) {
    sample_index = i;
    pcg32_random_t expected = pcg32_global;
    for (size_t draw = 0; draw < 12; ++draw) {
      (void)pcg32_random_r(&expected);
    }

    const float value = rng_norm_old();
    EXPECT(isfinite(value));
    EXPECT(value >= -6.0f && value <= 6.0f);
    EXPECT_STATE(expected, pcg32_global);
  }
  PASS();
}

TEST test_legacy_normal_sequence(void) {
  const float expected[] = {
      0x1.9f2098p-3f,  -0x1.00f62cp-1f, -0x1.0a6978p-1f,
      -0x1.de1916p-1f, 0x1.19694p-5f,   -0x1.fe49ep-5f,
  };
  rng_seed(42, 1);

  for (size_t i = 0; i < COUNT_OF(expected); ++i) {
    sample_index = i;
    EXPECT_FLOAT(expected[i], rng_norm_old());
  }
  PASS();
}

TEST test_tables(void) {
  const size_t layers = (size_t)1 << RNG_ZIG_LAY_BITS;

  EXPECT_FLOAT(8.0f, lay_xs[0]);
  EXPECT_FLOAT(0.0f, lay_xs[layers]);
  EXPECT_FLOAT(0.0f, lay_ys[0]);
  EXPECT_FLOAT(1.0f, lay_ys[layers]);

  for (size_t i = 0; i <= layers; ++i) {
    sample_index = i;
    EXPECT(isfinite(lay_xs[i]) && isfinite(lay_ys[i]));
    EXPECT(lay_xs[i] >= 0.0f && lay_xs[i] <= 8.0f);
    EXPECT(lay_ys[i] >= 0.0f && lay_ys[i] <= 1.0f);

    if (i != 0) {
      EXPECT(lay_xs[i] <= lay_xs[i - 1]);
      EXPECT(lay_ys[i] >= lay_ys[i - 1]);
    }
  }
  PASS();
}

TEST test_table_density(void) {
  const size_t layers = (size_t)1 << RNG_ZIG_LAY_BITS;

  for (size_t i = 0; i <= layers; ++i) {
    sample_index = i;
    const double x = lay_xs[i];
    const double density = exp(-0.5 * x * x);
    EXPECT_NEAR(density, (double)lay_ys[i], 2e-7);
  }
  PASS();
}

TEST test_table_areas(void) {
  const size_t layers = (size_t)1 << RNG_ZIG_LAY_BITS;
  const double expected = (double)lay_xs[0] * lay_ys[1];

  for (size_t i = 0; i < layers; ++i) {
    sample_index = i;
    const double height = (double)lay_ys[i + 1] - lay_ys[i];
    const double area = (double)lay_xs[i] * height;
    EXPECT(area > 0.0);
    EXPECT_NEAR(expected, area, 5e-7);
  }
  PASS();
}

typedef struct {
  const char *name;
  uint64_t value;
} named_value;

static const named_value seeds[] = {
    {"zero", 0}, {"42", 42}, {"max", UINT64_MAX}};
static const named_value streams[] = {
    {"zero", 0}, {"one", 1}, {"max", UINT64_MAX}};

static void isolate_tests(void) {
  SET_SETUP(setup, NULL);
  SET_TEARDOWN(teardown, NULL);
}

SUITE(pcg_tests) {
  isolate_tests();
  for (size_t i = 0; i < COUNT_OF(seed_cases); ++i) {
    greatest_set_test_suffix(seed_cases[i].name);
    RUN_TESTp(test_reference_sequence, &seed_cases[i]);
  }
  RUN_TEST(test_all_output_rotations);
  RUN_TEST(test_state_wraparound);
  RUN_TEST(test_reseeding);
  RUN_TEST(test_reseeding_local_state);
  RUN_TEST(test_stream_selector);
  RUN_TEST(test_different_streams);
  RUN_TEST(test_state_snapshot);
  RUN_TEST(test_interleaved_states);
  RUN_TEST(test_independent_state);
  /* Every seed/stream combination, not just paired entries. */
  for (size_t s = 0; s < COUNT_OF(seeds); ++s) {
    for (size_t t = 0; t < COUNT_OF(streams); ++t) {
      char suffix[64];
      snprintf(suffix, sizeof(suffix), "seed_%s_stream_%s", seeds[s].name,
               streams[t].name);
      greatest_set_test_suffix(suffix);
      RUN_TESTp(test_global_wrappers, seeds[s].value, streams[t].value);
    }
  }
}

SUITE(uniform_tests) {
  isolate_tests();
  RUN_TEST(test_uniform_values);
  RUN_TEST(test_uniform_discards_low_bits);
}

SUITE(normal_tests) {
  isolate_tests();
  RUN_TEST(test_normal_values);
  RUN_TEST(test_normal_all_layers_and_signs);
  RUN_TEST(test_normal_zero_at_last_layer);
  RUN_TEST(test_normal_density_boundary);
  for (size_t i = 0; i < COUNT_OF(normal_cases); ++i) {
    greatest_set_test_suffix(normal_cases[i].name);
    RUN_TESTp(test_normal_path, &normal_cases[i]);
  }
  RUN_TEST(test_legacy_normal);
  RUN_TEST(test_legacy_normal_sequence);
  RUN_TEST(test_tables);
  RUN_TEST(test_table_density);
  RUN_TEST(test_table_areas);
}

SUITE(smoke_tests) {
  isolate_tests();
  RUN_TEST(test_uniform_distribution_smoke);
  for (size_t i = 0; i < COUNT_OF(seeds); ++i) {
    char suffix[64];
    snprintf(suffix, sizeof(suffix), "seed_%s_stream_%s", seeds[i].name,
             streams[i].name);
    greatest_set_test_suffix(suffix);
    RUN_TESTp(test_normal_distribution_smoke, seeds[i].value, streams[i].value);
  }
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
  GREATEST_MAIN_BEGIN();
  RUN_SUITE(pcg_tests);
  RUN_SUITE(uniform_tests);
  RUN_SUITE(normal_tests);
  RUN_SUITE(smoke_tests);
  GREATEST_PRINT_REPORT();

  struct greatest_report_t report;
  greatest_get_report(&report);
  if (!GREATEST_LIST_ONLY() && report.passed == 0 && report.failed == 0 &&
      report.skipped == 0) {
    fputs("No tests matched the requested filters\n", stderr);
    return EXIT_FAILURE;
  }
  return greatest_all_passed() ? EXIT_SUCCESS : EXIT_FAILURE;
}