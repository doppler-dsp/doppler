/**
 * @file awgn_core.c
 * @brief AWGN generator: xoshiro256++ RNG + Box-Muller transform.
 *
 * ### Algorithm
 *
 * Two paths share the same logical algorithm but differ in width:
 *
 *   For each output sample, consume two 64-bit RNG words:
 *     u1  = (top-24-bits(word0) + 1) × 2^-24   ∈ (0, 1]   (log-safe)
 *     idx = top-16-bits(word1)                  ∈ [0, 65535]
 *     r   = amplitude × sqrt(−2 × ln u1)        (Box-Muller radial)
 *     out = r × cos(idx) + j × r × sin(idx)     (96 dBc LUT phase)
 *
 * ### Scalar path
 *
 * One xoshiro256++ state (4 × uint64).  Two next() calls per sample.
 *
 * ### AVX-512 path
 *
 * Eight independent xoshiro256++ streams stored in vs[0..3][0..7]
 * (word × stream layout).  Each stream occupies one 64-bit lane of a
 * __m512i register, giving 8 independent outputs per SIMD step.
 *
 *   - Two xs8() calls produce 8 u1 and 8 idx values.
 *   - _mm512_cvtepi64_epi32 packs 8 × 64-bit → 8 × 32-bit (__m256i).
 *   - _ZGVdN8v_logf (libmvec AVX2) computes 8 logarithms in parallel.
 *   - _mm256_i32gather_ps reads sin and cos from the 65536-entry LUT.
 *   - Two _mm256_storeu_ps interleave re/im directly into the output.
 *
 * ### Sin/cos LUT
 *
 * 2^16 single-precision entries, identical layout to lo_core:
 *   lut[i]         = sin(2π·i / 65536)
 *   lut[i + LUT_QTR] = cos(2π·i / 65536)   (uint16 wrap, no branch)
 *
 * SFDR ≈ 96 dBc — sufficient for noise generation at any SNR of
 * practical interest.
 *
 * ### References
 *
 * xoshiro256++: Blackman & Vigna, ACM TOMS 47(4), 2021.
 * SplitMix64:   Steele et al., PLDI 2014.
 * _ZGVdN8v_logf: glibc libmvec, 8-wide single-precision log (AVX2).
 */
#include "awgn/awgn_core.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>

#ifdef __AVX512F__
#include <immintrin.h>
#endif

/* ------------------------------------------------------------------ */
/* Sin/cos LUT — 2^16 entries, same layout as lo_core.               */
/* ------------------------------------------------------------------ */

#define LUT_BITS 16u
#define LUT_SIZE (1u << LUT_BITS)
#define LUT_QTR  (LUT_SIZE >> 2u)

static float lut[LUT_SIZE];
static int   lut_ready = 0;

static void
lut_init (void)
{
  if (lut_ready)
    return;
  for (unsigned i = 0; i < LUT_SIZE; i++)
    lut[i] = sinf (2.0f * (float)M_PI * (float)i / (float)LUT_SIZE);
  lut_ready = 1;
}

/* ------------------------------------------------------------------ */
/* xoshiro256++ — scalar                                               */
/* ------------------------------------------------------------------ */

static inline uint64_t
rotl64 (uint64_t x, int k)
{
  return (x << k) | (x >> (64 - k));
}

static inline uint64_t
xoshiro_next (uint64_t s[4])
{
  uint64_t r = rotl64 (s[0] + s[3], 23) + s[0];
  uint64_t t = s[1] << 17;
  s[2] ^= s[0];
  s[3] ^= s[1];
  s[1] ^= s[2];
  s[0] ^= s[3];
  s[2] ^= t;
  s[3] = rotl64 (s[3], 45);
  return r;
}

/* SplitMix64: expands one 64-bit seed into 4 independent state words. */
static uint64_t
splitmix64 (uint64_t *x)
{
  uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

static void
seed_state (uint64_t s[4], uint64_t seed)
{
  uint64_t sm = seed;
  s[0]        = splitmix64 (&sm);
  s[1]        = splitmix64 (&sm);
  s[2]        = splitmix64 (&sm);
  s[3]        = splitmix64 (&sm);
}

/* ================================================================== */
/* Lifecycle                                                           */
/* ================================================================== */

awgn_state_t *
awgn_create (uint64_t seed, float amplitude)
{
  lut_init ();
  awgn_state_t *s = malloc (sizeof *s);
  if (!s)
    return NULL;
  s->seed      = seed;
  s->amplitude = amplitude;
  seed_state (s->s, seed);
  /* Initialise 8 independent AVX streams via offset seeds.
   * vs is transposed: vs[word][stream], so words for stream j are at
   * vs[0][j], vs[1][j], vs[2][j], vs[3][j] — not contiguous. */
  for (int j = 0; j < 8; j++)
    {
      uint64_t stream_seed = seed + (uint64_t)j * 0x9e3779b97f4a7c15ULL;
      uint64_t sm          = stream_seed;
      s->vs[0][j]          = splitmix64 (&sm);
      s->vs[1][j]          = splitmix64 (&sm);
      s->vs[2][j]          = splitmix64 (&sm);
      s->vs[3][j]          = splitmix64 (&sm);
    }
  return s;
}

void
awgn_destroy (awgn_state_t *state)
{
  free (state);
}

void
awgn_reset (awgn_state_t *state)
{
  seed_state (state->s, state->seed);
  for (int j = 0; j < 8; j++)
    {
      uint64_t stream_seed
          = state->seed + (uint64_t)j * 0x9e3779b97f4a7c15ULL;
      uint64_t sm     = stream_seed;
      state->vs[0][j] = splitmix64 (&sm);
      state->vs[1][j] = splitmix64 (&sm);
      state->vs[2][j] = splitmix64 (&sm);
      state->vs[3][j] = splitmix64 (&sm);
    }
}

/* ================================================================== */
/* Properties                                                          */
/* ================================================================== */

float
awgn_get_amplitude (const awgn_state_t *state)
{
  return state->amplitude;
}

void
awgn_set_amplitude (awgn_state_t *state, float val)
{
  state->amplitude = val;
}

void
awgn_reseed (awgn_state_t *state, uint64_t seed)
{
  state->seed = seed;
  awgn_reset (state);
}

size_t
awgn_generate_max_out (awgn_state_t *state)
{
  (void)state;
  return 65536;
}

/* ================================================================== */
/* Generate — scalar path                                              */
/* ================================================================== */

static void
generate_scalar (awgn_state_t *state, size_t n, float complex *out)
{
  const float amp = state->amplitude;
  uint64_t   *s   = state->s;

  for (size_t i = 0; i < n; i++)
    {
      uint64_t a   = xoshiro_next (s);
      uint64_t b   = xoshiro_next (s);
      float    u1  = (float)((uint32_t)(a >> 40) + 1u) * 0x1.0p-24f;
      uint16_t idx = (uint16_t)(b >> 48);
      float    r   = amp * sqrtf (-2.0f * logf (u1));
      out[i] = CMPLXF (r * lut[(uint16_t)(idx + (uint16_t)LUT_QTR)],
                       r * lut[idx]);
    }
}

/* ================================================================== */
/* Generate — AVX-512 path                                            */
/* ================================================================== */

#ifdef __AVX512F__

/*
 * _ZGVdN8v_logf: glibc libmvec 8-wide float32 log (AVX2).
 * Linked from libm on glibc systems.
 */
extern __m256 _ZGVdN8v_logf (__m256);

/*
 * 8-wide xoshiro256++ step using __m512i (8 independent 64-bit lanes).
 * vs[word][stream] is transposed into __m512i s[4] before the loop and
 * written back after.
 */
static inline __m512i
xoshiro8_next (__m512i s[4])
{
  __m512i sum = _mm512_add_epi64 (s[0], s[3]);
  __m512i r   = _mm512_add_epi64 (
      _mm512_or_si512 (_mm512_slli_epi64 (sum, 23),
                       _mm512_srli_epi64 (sum, 41)),
      s[0]);
  __m512i t = _mm512_slli_epi64 (s[1], 17);
  s[2] = _mm512_xor_si512 (s[2], s[0]);
  s[3] = _mm512_xor_si512 (s[3], s[1]);
  s[1] = _mm512_xor_si512 (s[1], s[2]);
  s[0] = _mm512_xor_si512 (s[0], s[3]);
  s[2] = _mm512_xor_si512 (s[2], t);
  s[3] = _mm512_or_si512 (_mm512_slli_epi64 (s[3], 45),
                           _mm512_srli_epi64 (s[3], 19));
  return r;
}

static void
generate_avx512 (awgn_state_t *state, size_t n, float complex *out)
{
  const float amp = state->amplitude;

  /* Load 8-stream state: vs[word][stream_lane] → __m512i s[word] */
  __m512i s[4];
  for (int w = 0; w < 4; w++)
    s[w] = _mm512_loadu_si512 (state->vs[w]);

  size_t i = 0;
  for (; i + 8 <= n; i += 8)
    {
      __m512i r0 = xoshiro8_next (s);
      __m512i r1 = xoshiro8_next (s);

      /* u1 = (top-24(r0) + 1) × 2^-24 ∈ (0, 1] */
      __m256i hi0 = _mm512_cvtepi64_epi32 (_mm512_srli_epi64 (r0, 40));
      hi0         = _mm256_add_epi32 (hi0, _mm256_set1_epi32 (1));
      __m256 u1   = _mm256_mul_ps (_mm256_cvtepi32_ps (hi0),
                                   _mm256_set1_ps (0x1.0p-24f));

      /* idx = top-16(r1), packed as 8 × 32-bit */
      __m256i sin_idx = _mm512_cvtepi64_epi32 (_mm512_srli_epi64 (r1, 48));
      __m256i cos_idx = _mm256_and_si256 (
          _mm256_add_epi32 (sin_idx, _mm256_set1_epi32 (LUT_QTR)),
          _mm256_set1_epi32 (0xFFFF));

      /* LUT gather: sin and cos in one 256-bit load each */
      __m256 vsin = _mm256_i32gather_ps (lut, sin_idx, 4);
      __m256 vcos = _mm256_i32gather_ps (lut, cos_idx, 4);

      /* radial = amp × sqrt(−2 × log(u1)) */
      __m256 rad = _mm256_mul_ps (
          _mm256_set1_ps (amp),
          _mm256_sqrt_ps (_mm256_mul_ps (_mm256_set1_ps (-2.0f),
                                         _ZGVdN8v_logf (u1))));

      /* Interleave re/im into contiguous complex output (8 × CF32 = 64 bytes) */
      __m256 re  = _mm256_mul_ps (rad, vcos);
      __m256 im  = _mm256_mul_ps (rad, vsin);
      __m256 ilo = _mm256_unpacklo_ps (re, im);
      __m256 ihi = _mm256_unpackhi_ps (re, im);
      _mm256_storeu_ps ((float *)(out + i),
                        _mm256_permute2f128_ps (ilo, ihi, 0x20));
      _mm256_storeu_ps ((float *)(out + i + 4),
                        _mm256_permute2f128_ps (ilo, ihi, 0x31));
    }

  /* Store vector state back. */
  for (int w = 0; w < 4; w++)
    _mm512_storeu_si512 (state->vs[w], s[w]);

  /* Scalar tail (< 8 remaining samples). */
  if (i < n)
    generate_scalar (state, n - i, out + i);
}

#endif /* __AVX512F__ */

/* ================================================================== */
/* Public dispatcher                                                   */
/* ================================================================== */

size_t
awgn_generate (awgn_state_t *state, size_t n, float complex *out)
{
#ifdef __AVX512F__
  generate_avx512 (state, n, out);
#else
  generate_scalar (state, n, out);
#endif
  return n;
}

int
awgn (uint64_t seed, float amplitude, size_t n, float complex *out)
{
  awgn_state_t *g = awgn_create (seed, amplitude);
  if (!g)
    return DP_ERR_MEMORY;
  awgn_generate (g, n, out);
  awgn_destroy (g);
  return DP_OK;
}
