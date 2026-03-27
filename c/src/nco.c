#include "dp/nco.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/* ------------------------------------------------------------------
 * Sine LUT — 2^16 = 65536 single-precision entries.
 *
 * The 32-bit phase accumulator is mapped to a LUT index by taking
 * the top 16 bits:
 *
 *   idx      = (uint16_t)(phase >> 16)
 *   sin(θ)   = nco_lut[idx]
 *   cos(θ)   = nco_lut[(uint16_t)(idx + NCO_LUT_QTR)]
 *
 * Adding NCO_LUT_QTR (= 16384 = N/4) shifts by π/2 in phase, which
 * maps sin → cos.  The uint16_t cast lets the index wrap at 65536
 * without a branch.
 *
 * Memory: 65536 × 4 bytes = 256 KB (fits in L2 on all modern CPUs).
 * SFDR: ~96 dBc (16-bit phase truncation → 6 × 16 dB rule).
 * ------------------------------------------------------------------ */
#define NCO_LUT_BITS 16u
#define NCO_LUT_SIZE (1u << NCO_LUT_BITS) /* 65536               */
#define NCO_LUT_QTR  (NCO_LUT_SIZE >> 2u) /* 16384  (π/2 offset) */

static float nco_lut[NCO_LUT_SIZE];
static int   nco_lut_ready = 0;

static void
nco_lut_init (void)
{
  if (nco_lut_ready)
    return;
  for (unsigned i = 0; i < NCO_LUT_SIZE; i++)
    nco_lut[i] = sinf (2.0f * (float)M_PI * (float)i
                       / (float)NCO_LUT_SIZE);
  nco_lut_ready = 1;
}

/* ------------------------------------------------------------------
 * Overflow / carry detection.
 *
 * On GCC / Clang: __builtin_add_overflow maps to a single ADD + SETB
 * (x86) or ADDS + CSET (AArch64) with no branch.
 *
 * Fallback: the comparison idiom `new < old` is equally branchless
 * on all targets that understand unsigned arithmetic; modern compilers
 * generate the same carry-flag instruction sequence as the builtin.
 *
 * We prefer the builtin where available because:
 *   1. Intent is unambiguous — no need to rely on pattern matching.
 *   2. It is immune to a hypothetical future compiler that reorders
 *      the addition and the comparison before emitting a branch.
 * ------------------------------------------------------------------ */
#if defined(__GNUC__) || defined(__clang__)
#define NCO_ADD_OVF(a, b, res)                                                 \
  ((uint8_t) __builtin_add_overflow ((uint32_t)(a), (uint32_t)(b),            \
                                     (uint32_t *)(res)))
#else
static inline uint8_t
nco_add_ovf_ (uint32_t a, uint32_t b, uint32_t *res)
{
  *res = a + b;
  return (uint8_t)(*res < a);
}
#define NCO_ADD_OVF(a, b, res) nco_add_ovf_ ((a), (b), (res))
#endif

/* ------------------------------------------------------------------
 * Normalised frequency → uint32 phase increment.
 *
 * Uses double arithmetic to avoid rounding at the float→uint32
 * boundary.  Folding via floor() maps negative frequencies correctly
 * (e.g. −0.25 → 0.75 → 3×2^30) and keeps the cast in [0, 2^32).
 * ------------------------------------------------------------------ */
static uint32_t
norm_to_inc (float norm_freq)
{
  double d = (double)norm_freq;
  d       -= floor (d); /* fold into [0, 1) */
  return (uint32_t)(d * 4294967296.0);
}

/* ------------------------------------------------------------------
 * NCO state
 * ------------------------------------------------------------------ */
struct dp_nco
{
  uint32_t phase;
  uint32_t phase_inc;
  float    norm_freq;
};

/* ------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------ */

dp_nco_t *
dp_nco_create (float norm_freq)
{
  nco_lut_init ();
  dp_nco_t *nco = malloc (sizeof *nco);
  if (!nco)
    return NULL;
  nco->phase     = 0;
  nco->phase_inc = norm_to_inc (norm_freq);
  nco->norm_freq = norm_freq;
  return nco;
}

void
dp_nco_set_freq (dp_nco_t *nco, float norm_freq)
{
  nco->phase_inc = norm_to_inc (norm_freq);
  nco->norm_freq = norm_freq;
}

float
dp_nco_get_freq (const dp_nco_t *nco)
{
  return nco->norm_freq;
}

uint32_t
dp_nco_get_phase (const dp_nco_t *nco)
{
  return nco->phase;
}

uint32_t
dp_nco_get_phase_inc (const dp_nco_t *nco)
{
  return nco->phase_inc;
}

void
dp_nco_reset (dp_nco_t *nco)
{
  nco->phase = 0;
}

void
dp_nco_destroy (dp_nco_t *nco)
{
  free (nco);
}

/* ------------------------------------------------------------------
 * Execute — free-running
 *
 * AVX-512F path: processes 16 complex samples per iteration using
 * gather loads from the sine LUT and permutex2var interleaving.
 *
 * Per iteration:
 *   1. Build per-lane phases: ph + lane*inc  (mullo_epi32)
 *   2. Extract top-16-bit LUT indices for sin and cos (srli + add)
 *   3. Gather 16 sin and 16 cos values from the float LUT
 *   4. Interleave into I/Q pairs via unpacklo/hi + permutex2var
 *   5. Two 512-bit stores (samples 0-7, samples 8-15)
 *
 * Scalar fallback handles the n%16 tail and non-AVX-512 builds.
 * ------------------------------------------------------------------ */

#ifdef __AVX512F__
#include <immintrin.h>

void
dp_nco_execute_cf32 (dp_nco_t *nco, dp_cf32_t *out, size_t n)
{
  uint32_t ph  = nco->phase;
  uint32_t inc = nco->phase_inc;

  /* Permutation index vectors — constant across all iterations.
   *
   * After _mm512_unpacklo_ps(vcos, vsin) and _mm512_unpackhi_ps:
   *   lo 128-bit lanes: [c0,s0,c1,s1] [c4,s4,c5,s5] [c8,..] [c12,..]
   *   hi 128-bit lanes: [c2,s2,c3,s3] [c6,s6,c7,s7] [c10,..] [c14,..]
   *
   * perm0: gathers lo/hi lanes 0,1 → output samples 0-7
   * perm1: gathers lo/hi lanes 2,3 → output samples 8-15
   *
   * Index bit 4 selects source: 0 = lo (a), 1 = hi (b).          */
  static const int32_t perm0_arr[16] = {
    0,  1,  2,  3,  16, 17, 18, 19,
    4,  5,  6,  7,  20, 21, 22, 23
  };
  static const int32_t perm1_arr[16] = {
    8,  9,  10, 11, 24, 25, 26, 27,
    12, 13, 14, 15, 28, 29, 30, 31
  };
  __m512i vperm0 = _mm512_loadu_si512 (perm0_arr);
  __m512i vperm1 = _mm512_loadu_si512 (perm1_arr);

  /* Lane-offset vector {0,1,...,15} and broadcast increment */
  __m512i vsamp = _mm512_setr_epi32 (
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
  __m512i vinc  = _mm512_set1_epi32 ((int32_t)inc);

  size_t i = 0;
  for (; i + 16 <= n; i += 16, ph += 16u * inc)
    {
      /* Phase for each of the 16 lanes */
      __m512i vph = _mm512_add_epi32 (
        _mm512_set1_epi32 ((int32_t)ph),
        _mm512_mullo_epi32 (vsamp, vinc));

      /* Top 16 bits → LUT index [0, 65535] */
      __m512i vsin_idx = _mm512_srli_epi32 (vph, 16);
      __m512i vcos_idx = _mm512_and_epi32 (
        _mm512_add_epi32 (vsin_idx, _mm512_set1_epi32 (NCO_LUT_QTR)),
        _mm512_set1_epi32 (0xFFFF));

      /* Gather from the float LUT (scale=4: byte_offset = idx*4) */
      __m512 vsin = _mm512_i32gather_ps (vsin_idx, nco_lut, 4);
      __m512 vcos = _mm512_i32gather_ps (vcos_idx, nco_lut, 4);

      /* Interleave cos (I) and sin (Q) into contiguous IQ pairs */
      __m512 lo = _mm512_unpacklo_ps (vcos, vsin);
      __m512 hi = _mm512_unpackhi_ps (vcos, vsin);

      _mm512_storeu_ps ((float *)(out + i),
                        _mm512_permutex2var_ps (lo, vperm0, hi));
      _mm512_storeu_ps ((float *)(out + i + 8),
                        _mm512_permutex2var_ps (lo, vperm1, hi));
    }

  /* Scalar tail (n % 16 remaining samples) */
  for (; i < n; i++)
    {
      uint16_t idx = (uint16_t)(ph >> (32u - NCO_LUT_BITS));
      out[i].i     = nco_lut[(uint16_t)(idx + (uint16_t)NCO_LUT_QTR)];
      out[i].q     = nco_lut[idx];
      ph          += inc;
    }

  nco->phase = ph;
}

#else /* scalar fallback for non-AVX-512 builds */

void
dp_nco_execute_cf32 (dp_nco_t *nco, dp_cf32_t *out, size_t n)
{
  uint32_t ph  = nco->phase;
  uint32_t inc = nco->phase_inc;

  for (size_t i = 0; i < n; i++)
    {
      uint16_t idx = (uint16_t)(ph >> (32u - NCO_LUT_BITS));
      out[i].i     = nco_lut[(uint16_t)(idx + (uint16_t)NCO_LUT_QTR)];
      out[i].q     = nco_lut[idx];
      ph          += inc;
    }

  nco->phase = ph;
}

#endif /* __AVX512F__ */

/* ------------------------------------------------------------------
 * Execute — with per-sample phase-increment control port
 * ------------------------------------------------------------------ */

void
dp_nco_execute_cf32_ctrl (dp_nco_t    *nco,
                          const float *ctrl,
                          dp_cf32_t   *out,
                          size_t       n)
{
  uint32_t ph  = nco->phase;
  uint32_t inc = nco->phase_inc;

  for (size_t i = 0; i < n; i++)
    {
      double   d        = (double)ctrl[i];
      d                -= floor (d);
      uint32_t ctrl_inc = (uint32_t)(d * 4294967296.0);
      uint16_t idx      = (uint16_t)(ph >> (32u - NCO_LUT_BITS));
      out[i].i          = nco_lut[(uint16_t)(idx + (uint16_t)NCO_LUT_QTR)];
      out[i].q          = nco_lut[idx];
      ph               += inc + ctrl_inc;
    }

  nco->phase = ph;
}

/* ------------------------------------------------------------------
 * Execute — raw uint32 phase output
 * ------------------------------------------------------------------ */

void
dp_nco_execute_u32 (dp_nco_t *nco, uint32_t *out, size_t n)
{
  uint32_t ph  = nco->phase;
  uint32_t inc = nco->phase_inc;

  for (size_t i = 0; i < n; i++)
    {
      out[i] = ph;
      ph    += inc;
    }

  nco->phase = ph;
}

void
dp_nco_execute_u32_ctrl (dp_nco_t    *nco,
                         const float *ctrl,
                         uint32_t    *out,
                         size_t       n)
{
  uint32_t ph  = nco->phase;
  uint32_t inc = nco->phase_inc;

  for (size_t i = 0; i < n; i++)
    {
      double   d        = (double)ctrl[i];
      d                -= floor (d);
      uint32_t ctrl_inc = (uint32_t)(d * 4294967296.0);
      out[i]            = ph;
      ph               += inc + ctrl_inc;
    }

  nco->phase = ph;
}

/* ------------------------------------------------------------------
 * Execute — raw uint32 phase + per-sample overflow / carry bit
 * ------------------------------------------------------------------ */

void
dp_nco_execute_u32_ovf (dp_nco_t *nco,
                        uint32_t *out,
                        uint8_t  *carry,
                        size_t    n)
{
  uint32_t ph  = nco->phase;
  uint32_t inc = nco->phase_inc;

  for (size_t i = 0; i < n; i++)
    {
      out[i]   = ph;
      carry[i] = NCO_ADD_OVF (ph, inc, &ph);
    }

  nco->phase = ph;
}

void
dp_nco_execute_u32_ovf_ctrl (dp_nco_t    *nco,
                              const float *ctrl,
                              uint32_t    *out,
                              uint8_t     *carry,
                              size_t       n)
{
  uint32_t ph  = nco->phase;
  uint32_t inc = nco->phase_inc;

  for (size_t i = 0; i < n; i++)
    {
      double   d        = (double)ctrl[i];
      d                -= floor (d);
      uint32_t ctrl_inc = (uint32_t)(d * 4294967296.0);
      uint32_t eff_inc  = inc + ctrl_inc;
      out[i]            = ph;
      carry[i]          = NCO_ADD_OVF (ph, eff_inc, &ph);
    }

  nco->phase = ph;
}
