/**
 * @file test_wlan_core.c
 * @brief Unit tests for the 802.11 non-HT builders.
 *
 * What this file claims, and what it does not. The standard's literal tables
 * (the RATE bits, the L-STF and L-LTF values and their scales, the
 * constellation labels where they differ from the library's, and the second
 * interleaver permutation) are dp_wlan_tables_standard(), and section 9 holds
 * the RATE bits, both training sequences, the constellation encodings and the
 * 2/3 and 3/4 puncture masks against their printed form (docs/design/wlan.md
 * section 9), and the second permutation by its worked 16-QAM example and by a
 * re-derivation of the printed equation. The tests of
 * the machinery run on a SYNTHETIC table built here, so what they pin does not
 * depend on a transcription; they claim no conformance with any 802.11 text.
 * What they pin is what is derivable: the counts and lengths from the
 * checked numbers, the field offsets of the layout, the chain's own
 * inverses, the CRC by its catalogue check value, the scrambler by its
 * recurrence, the BCC by its generators' impulse response, and the pilot
 * polarity by an independent recurrence.
 *
 * Sections:
 *   1. Rates: the counts the checked numbers give, and the symbol count
 *   2. The BCC: generators, the impulse response, the puncture patterns
 *   3. The scrambler: the recurrence, the two spellings, the 127 states
 *   4. The minimal MAC and the FCS
 *   5. The SIGNAL and Data bits
 *   6. Interleaving and mapping
 *   7. The points, and the refusals that change nothing
 *   8. The PPDU: lengths, periodicity, prefixes, pilots, the data tones
 */
#include "doppler/dp_interleave.h"
#include "doppler/mpsk/mpsk_core.h"
#include "doppler/pn/pn_core.h"
#include "doppler/wlan/wlan_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include "wlan_ppdu_m1_oracle.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TWOPI 6.283185307179586

/* Eq. (17-16) and (17-17) as posted on #3109, written here with no code of
 * the library's: the second interleaver permutation,
 *   s = max (N_BPSC / 2, 1)
 *   j = s * floor (i / s) + (i + N_CBPS - floor (16 i / N_CBPS)) mod s.
 * A RE-DERIVATION from the printed equation, not a printed table: a misread
 * equation would be shared with the table under test. `flip` is the control,
 * the same formula with the sign of the floor term reversed (the slip an
 * audit named), which must disagree with the table somewhere. */
static size_t
ref_perm2 (size_t i, size_t n_cbps, size_t n_bpsc, int flip)
{
  const size_t s = n_bpsc / 2 > 1 ? n_bpsc / 2 : 1;
  const size_t f = 16 * i / n_cbps;
  const size_t t = flip ? i + n_cbps + f : i + n_cbps - f;
  return s * (i / s) + t % s;
}

/* Figure 17-9 as posted on #3109: the bits SENT from each period of the
 * encoder's outputs A0 B0 A1 B1 (A2 B2), as printed ("A0" is the first
 * encoder's output for source bit 0). The mask a builder carries has bit
 * 2 * i + (B ? 1 : 0) set for each one sent. */
static uint32_t
mask_from_sent (const char *const *sent, size_t n)
{
  uint32_t m = 0;
  for (size_t q = 0; q < n; q++)
    m |= 1u << (2u * (unsigned)(sent[q][1] - '0') + (sent[q][0] == 'B'));
  return m;
}

/* ── Synthetic tables: NOT the standard's ────────────────────────────── */

static float _Complex g_stf[53], g_ltf[53];
static dp_wlan_signal_rate_t g_rates[16];
static uint16_t              g_relabel_rev[64];

/* A permutation of one symbol that is not the standard's: rotate within
 * groups of s = max(n_bpsc/2, 1) positions. */
static size_t
synth_perm2 (size_t i, size_t n_cbps, size_t n_bpsc)
{
  (void)n_cbps;
  const size_t s = n_bpsc / 2 > 1 ? n_bpsc / 2 : 1;
  return s * (i / s) + (i + 1) % s;
}

/* Not a permutation: every input to position 0. */
static size_t
bad_perm2 (size_t i, size_t n_cbps, size_t n_bpsc)
{
  (void)i;
  (void)n_cbps;
  (void)n_bpsc;
  return 0;
}

static dp_wlan_tables_t
synth_tables (void)
{
  for (int k = -26; k <= 26; k++)
    {
      g_stf[k + 26] = 0.0f;
      g_ltf[k + 26] = 0.0f;
      /* twelve tones at multiples of 4, so the time signal has a period of
       * 16 samples: derivable, whatever the values are */
      if (k != 0 && k % 4 == 0 && abs (k) <= 24)
        g_stf[k + 26] = (float)(1 + (k > 0)) + (float)(1 - (k < 0)) * I;
      if (k != 0)
        g_ltf[k + 26] = (k & 1) ? 1.0f : -1.0f;
    }
  /* an entry per rate combination, bits 0..15 in order: synthetic */
  static const unsigned bpsc[4] = { 1, 2, 4, 6 };
  int                   n       = 0;
  for (int c = 0; c < 4; c++)
    for (int m = 0; m < 4; m++)
      {
        g_rates[n].rate.n_bpsc = bpsc[m];
        g_rates[n].rate.code   = (dp_wlan_code_t)c;
        g_rates[n].bits        = (uint8_t)n;
        n++;
      }
  dp_wlan_tables_t t;
  memset (&t, 0, sizeof t);
  t.stf            = g_stf;
  t.ltf            = g_ltf;
  t.scale_stf      = 1.0 / sqrt (12.0);
  t.scale_ltf      = 1.0 / sqrt (52.0);
  t.scale_data     = 1.0 / sqrt (52.0);
  t.signal_rates   = g_rates;
  t.n_signal_rates = 16;
  t.perm2          = synth_perm2;
  return t;
}

/* The scrambling sequence by the polynomial's recurrence, an independent
 * formulation from the shift register: s[n] = s[n-7] xor s[n-4], seeded with
 * the first seven bits. */
static void
recurrence (unsigned seq7, size_t n, uint8_t *out)
{
  for (int i = 0; i < 7 && (size_t)i < n; i++)
    out[i] = (uint8_t)((seq7 >> (6 - i)) & 1u);
  for (size_t i = 7; i < n; i++)
    out[i] = out[i - 7] ^ out[i - 4];
}

static int
close_c (float _Complex a, float _Complex b, double tol)
{
  return cabsf (a - b) <= tol;
}

/* Worst |a[i] - gain * b[i]| over [first, first + len), relative to the
   largest |a| in that range. */
static double
worst_rel (const float _Complex *a, const float _Complex *b, double gain,
           size_t first, size_t len)
{
  double worst = 0, peak = 0;
  for (size_t i = first; i < first + len; i++)
    {
      const double _Complex d
          = (double _Complex)a[i] - gain * (double _Complex)b[i];
      worst = fmax (worst, cabs (d));
      peak  = fmax (peak, cabs ((double _Complex)a[i]));
    }
  return worst / peak;
}

/* Render a PPDU of 'len' octets psdu[i] = i * 37 + 11 at the given rate,
 * window and oversampling; returns the samples (caller frees) and their
 * count in *n, or NULL. */
static float _Complex *
render_ppdu (const dp_wlan_rate_t *r, size_t len, uint8_t state,
             unsigned window, unsigned m, size_t *n)
{
  uint8_t psdu[128];
  for (size_t i = 0; i < len; i++)
    psdu[i] = (uint8_t)(i * 37 + 11);
  const size_t n_sym = dp_wlan_rate_n_sym (r, len);
  *n                 = dp_wlan_ppdu_len (n_sym, window, m);
  float _Complex *x  = malloc (*n * sizeof *x);
  if (!x)
    return NULL;
  if (dp_wlan_ppdu (dp_wlan_tables_standard (), r, psdu, len, state, window, m,
                    x, *n)
      != *n)
    {
      free (x);
      return NULL;
    }
  return x;
}

/* The relative l2 error of x against an oracle of interleaved real and
 * imaginary parts given as IEEE-754 bit patterns. */
static double
oracle_l2 (const float _Complex *x, size_t n, const uint32_t *oracle)
{
  double num = 0, den = 0;
  for (size_t i = 0; i < n; i++)
    {
      float re, im;
      memcpy (&re, &oracle[2 * i], 4);
      memcpy (&im, &oracle[2 * i + 1], 4);
      const double dr = (double)crealf (x[i]) - re;
      const double di = (double)cimagf (x[i]) - im;
      num += dr * dr + di * di;
      den += (double)re * re + (double)im * im;
    }
  return sqrt (num / den);
}

/* The tolerance of every decimation and cross-build comparison below, set by
 * the error those checks must catch. A layout error (a length, a prefix, a
 * scale, a grid offset) moves samples by O(1): the controls below are at least
 * 1e-2. The tolerance only has to sit above float32 rounding, and its source
 * is the accuracy the FFT's own test pins: test_fft_core.c accepts the
 * library's float32 transform against a double reference to TOL32 = 5e-4
 * ABSOLUTE in each of the real and imaginary parts, on inputs of unit scale.
 * A complex sample therefore differs by at most sqrt (2) * 5e-4; the two
 * PPDUs compared come from two transforms, so 2 * sqrt (2) * 5e-4 = 1.41e-3
 * per sample at unit rms, and a relative l2 (a root mean of per-sample
 * squares) cannot exceed it: 1.5e-3. The expected size of the rounding is far
 * smaller, a small multiple of u log2 N with u = 2^-24 (Higham, Accuracy and
 * Stability of Numerical Algorithms, the FFT section), about 1e-6; that is an
 * expectation, not what is asserted. */
#define WLAN_FFT_TOL 1.5e-3

/* Mean power of x[first .. first + len). */
static double
mean_power (const float _Complex *x, size_t first, size_t len)
{
  double p = 0;
  for (size_t i = 0; i < len; i++)
    p += (double)(crealf (x[first + i]) * crealf (x[first + i])
                  + cimagf (x[first + i]) * cimagf (x[first + i]));
  return p / (double)len;
}

/* The relative l2 error of the decimated PPDU against the M = 1 one:
 * sqrt (sum |x[M i] - y[i]|^2 / sum |y[i]|^2) over i < n_cmp, skipping the
 * samples i in [b, b + w) of each boundary b in bounds. */
static double
decimation_error (const float _Complex *xm, unsigned m,
                  const float _Complex *y, size_t n_cmp, const size_t *bounds,
                  size_t n_bounds, size_t w)
{
  double num = 0, den = 0;
  for (size_t i = 0; i < n_cmp; i++)
    {
      int skip = 0;
      for (size_t k = 0; k < n_bounds; k++)
        if (i >= bounds[k] && i < bounds[k] + w)
          skip = 1;
      if (skip)
        continue;
      const double d = cabs ((double _Complex)xm[(size_t)m * i] - y[i]);
      num += d * d;
      den += (double)(crealf (y[i]) * crealf (y[i])
                      + cimagf (y[i]) * cimagf (y[i]));
    }
  return sqrt (num / den);
}

/* ── The layout as it was before dp_wlan_layout took an oversample argument ──
 * A copy of the 20 MHz construction (64-point transform, literal lengths; it
 * equals c86bfb247's dp_wlan_layout), kept so the M = 1 output of the new code
 * is compared with the old construction in the SAME build, bit for bit: a
 * committed float is bound to the compiler and the FFT dispatch that wrote it,
 * an in-build oracle is not. This copy is a second layout in the test, and the
 * first commit of the next PR in the stack removes it: the proof is in the
 * history and in that PR's CI, and no copy stays on main to be mirrored. */
static int
pre_oversample_layout (dp_ofdm_state_t *o, const dp_wlan_tables_t *t,
                       size_t n_sym, unsigned window)
{
  if (!t || !t->stf || !t->ltf || t->scale_stf <= 0.0 || t->scale_ltf <= 0.0
      || t->scale_data <= 0.0 || n_sym == 0 || n_sym > WLAN_N_SYM_MAX)
    return -1;
  uint8_t null_map[WLAN_N_FFT];
  uint8_t data_map[WLAN_N_FFT];
  memset (null_map, 0, sizeof null_map);
  memset (data_map, 0, sizeof data_map);
  static const int32_t pk[4] = WLAN_PILOT_K;
  /* every subcarrier but DC may carry data; the four pilots, added below as
   * fixed entries, displace data on their subcarriers, leaving 48 */
  for (int32_t k = -26; k <= 26; k++)
    if (k != 0)
      data_map[(size_t)(k + (int32_t)WLAN_N_FFT / 2)] = 1;
  /* L-STF: ten short periods read as 160 samples of the 64-point transform,
   * no prefix; L-LTF: 128 samples (two long periods) behind a 32-sample
   * prefix; SIGNAL and Data share one class so the pilot polarity runs on. */
  const int stf = dp_ofdm_add_class (o, WLAN_N_FFT, null_map, WLAN_N_FFT,
                                     WLAN_STF_LEN, 0, window, t->scale_stf, 1);
  const int ltf = dp_ofdm_add_class (o, WLAN_N_FFT, null_map, WLAN_N_FFT, 128,
                                     32, window, t->scale_ltf, 1);
  const int sym
      = dp_ofdm_add_class (o, WLAN_N_FFT, data_map, WLAN_N_FFT, WLAN_N_FFT,
                           WLAN_GI, window, t->scale_data, 1);
  if (stf < 0 || ltf < 0 || sym < 0)
    return -1;
  for (int32_t k = -26; k <= 26; k++)
    {
      const float _Complex vs = t->stf[k + 26], vl = t->ltf[k + 26];
      if (vs != 0.0f
          && dp_ofdm_add_fixed (o, (uint32_t)stf, OFDM_ALL_PHASES, k, vs) != 0)
        return -1;
      if (vl != 0.0f
          && dp_ofdm_add_fixed (o, (uint32_t)ltf, OFDM_ALL_PHASES, k, vl) != 0)
        return -1;
    }
  static const double pv[4] = WLAN_PILOT_V;
  for (int i = 0; i < 4; i++)
    if (dp_ofdm_add_fixed (o, (uint32_t)sym, OFDM_ALL_PHASES, pk[i], pv[i])
        != 0)
      return -1;
  /* the pilot polarity: the scrambler's sequence from the all-ones state,
   * with 0 read as +1 and 1 as -1 */
  uint8_t seq[WLAN_POLARITY_LEN];
  float _Complex pol[WLAN_POLARITY_LEN];
  dp_wlan_scrambler_sequence (127, WLAN_POLARITY_LEN, seq);
  for (size_t i = 0; i < WLAN_POLARITY_LEN; i++)
    pol[i] = seq[i] ? -1.0f : 1.0f;
  if (dp_ofdm_add_polarity (o, (uint32_t)sym, pol, WLAN_POLARITY_LEN) != 0)
    return -1;
  if (dp_ofdm_add_run (o, (uint32_t)stf, 1, OFDM_CP_DEFAULT, 0, 0) < 0
      || dp_ofdm_add_run (o, (uint32_t)ltf, 1, OFDM_CP_DEFAULT, 0, 0) < 0
      || dp_ofdm_add_run (o, (uint32_t)sym, 1, OFDM_CP_DEFAULT, 0, 0) < 0
      || dp_ofdm_add_run (o, (uint32_t)sym, (uint32_t)n_sym, OFDM_CP_DEFAULT,
                          1, 0)
             < 0)
    return -1;
  return dp_ofdm_check (o);
}

/* A fixture that cannot be built stops the test (DP_REQUIRE returns int). */
#define NEED(c)                                                               \
  do                                                                          \
    {                                                                         \
      if (!(c))                                                               \
        {                                                                     \
          DP_TEST_ERR ("FAIL fixture: %s\n", #c);                             \
          exit (1);                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/* A PPDU through pre_oversample_layout(), as dp_wlan_ppdu() built it. */
static float _Complex *
render_pre (const dp_wlan_rate_t *r, size_t len, uint8_t state,
            unsigned window, size_t *n)
{
  uint8_t psdu[128];
  for (size_t i = 0; i < len; i++)
    psdu[i] = (uint8_t)(i * 37 + 11);
  const dp_wlan_tables_t *t     = dp_wlan_tables_standard ();
  const size_t            n_sym = dp_wlan_rate_n_sym (r, len);
  const size_t            n_pts = WLAN_N_SD + (size_t)WLAN_N_SD * n_sym;
  *n                            = 160 + 160 + 80 + 80 * n_sym + window;
  float _Complex *pts           = malloc (n_pts * sizeof *pts);
  float _Complex *x             = malloc (*n * sizeof *x);
  NEED (pts != NULL && x != NULL);
  NEED (dp_wlan_points (t, r, psdu, len, state, pts, n_pts) == n_pts);
  dp_ofdm_state_t *o = dp_ofdm_create ();
  NEED (pre_oversample_layout (o, t, n_sym, window) == 0);
  const size_t a = dp_ofdm_modulate (o, pts, n_pts, x, *n);
  NEED (a > 0);
  NEED (a + dp_ofdm_finish (o, o->tail_len, x + a, *n - a) == *n);
  dp_ofdm_destroy (o);
  free (pts);
  return x;
}

int
main (void)
{
  /* 1 ── Rates ──────────────────────────────────────────────────────────── */
  {
    static const struct
    {
      unsigned       bpsc;
      dp_wlan_code_t code;
      unsigned       cbps, dbps;
    } rates[] = {
      { 1, WLAN_CODE_1_2, 48, 24 },   { 1, WLAN_CODE_3_4, 48, 36 },
      { 2, WLAN_CODE_1_2, 96, 48 },   { 2, WLAN_CODE_3_4, 96, 72 },
      { 4, WLAN_CODE_1_2, 192, 96 },  { 4, WLAN_CODE_3_4, 192, 144 },
      { 6, WLAN_CODE_2_3, 288, 192 }, { 6, WLAN_CODE_3_4, 288, 216 },
    };
    for (size_t i = 0; i < sizeof rates / sizeof *rates; i++)
      {
        const dp_wlan_rate_t r = { rates[i].bpsc, rates[i].code };
        DP_CHECK (dp_wlan_rate_ok (&r));
        DP_CHECK (dp_wlan_n_cbps (&r) == rates[i].cbps);
        DP_CHECK (dp_wlan_n_dbps (&r) == rates[i].dbps);
      }
    const dp_wlan_rate_t bad1 = { 3, WLAN_CODE_1_2 }, bad2 = { 1, 7 };
    DP_CHECK (!dp_wlan_rate_ok (&bad1) && !dp_wlan_rate_ok (&bad2));
    DP_CHECK (!dp_wlan_rate_ok (NULL));
    DP_CHECK (dp_wlan_n_cbps (&bad1) == 0 && dp_wlan_n_dbps (&bad2) == 0);

    /* The old refusal "data bits per symbol not a whole number" cannot fire:
     * 48 * n_bpsc * num / den is whole for all sixteen (constellation, code
     * rate) pairs, so every one is built. The fractions are typed here, and
     * the divisibility is the test's own arithmetic. */
    {
      static const unsigned frac[4][2]
          = { { 1, 2 }, { 2, 3 }, { 3, 4 }, { 5, 6 } };
      static const unsigned nb[4] = { 1, 2, 4, 6 };
      for (int c = 0; c < 4; c++)
        for (int b = 0; b < 4; b++)
          {
            const dp_wlan_rate_t r = { nb[b], (dp_wlan_code_t)c };
            DP_CHECK ((48u * nb[b] * frac[c][0]) % frac[c][1] == 0);
            DP_CHECK (dp_wlan_rate_ok (&r));
            DP_CHECK (dp_wlan_n_dbps (&r) * frac[c][1]
                      == dp_wlan_n_cbps (&r) * frac[c][0]);
          }
      /* control: the same remainder test does fail for a code rate that
       * would not divide (a 1/5 code on 48 BPSK carriers is 9.6 bits) */
      DP_CHECK ((48u * 1u * 1u) % 5u != 0);
    }

    /* WLAN_N_SYM_MAX is the largest PSDU at the slowest rate:
     * ceil ((16 + 8 * 4095 + 6) / 24) = 1366, and the layout takes it and
     * refuses one more. */
    {
      const dp_wlan_rate_t slow = { 1, WLAN_CODE_1_2 };
      DP_CHECK (dp_wlan_rate_n_sym (&slow, WLAN_PSDU_MAX) == WLAN_N_SYM_MAX);
      DP_CHECK (WLAN_N_SYM_MAX == (16u + 8u * 4095u + 6u + 23u) / 24u);
      dp_ofdm_state_t *lo = dp_ofdm_create (), *hi = dp_ofdm_create ();
      DP_CHECK (
          dp_wlan_layout (lo, dp_wlan_tables_standard (), WLAN_N_SYM_MAX, 0, 1)
          == 0);
      DP_CHECK (dp_wlan_layout (hi, dp_wlan_tables_standard (),
                                WLAN_N_SYM_MAX + 1, 0, 1)
                == -1);
      dp_ofdm_destroy (lo);
      dp_ofdm_destroy (hi);
    }

    /* 100 octets at 16-QAM 3/4: (16 + 800 + 6) / 144 = 5.7 -> 6 symbols */
    const dp_wlan_rate_t r36 = { 4, WLAN_CODE_3_4 };
    DP_CHECK (dp_wlan_rate_n_sym (&r36, 100) == 6);
    DP_CHECK (dp_wlan_rate_n_sym (&r36, 1) == 1); /* 30 bits in 144 */
    DP_CHECK (dp_wlan_rate_n_sym (&r36, 0) == 0);
    DP_CHECK (dp_wlan_rate_n_sym (&r36, WLAN_PSDU_MAX) > 0);
    DP_CHECK (dp_wlan_rate_n_sym (&r36, WLAN_PSDU_MAX + 1) == 0);
    /* the symbol count is exactly enough, and one fewer is not */
    for (size_t len = 1; len < 400; len += 7)
      {
        const size_t n = dp_wlan_rate_n_sym (&r36, len);
        DP_CHECK (n * 144 >= 22 + 8 * len);
        DP_CHECK ((n - 1) * 144 < 22 + 8 * len);
      }
    DP_CHECK (dp_wlan_ppdu_len (6, 0, 1) == 160 + 160 + 80 + 480);
    DP_CHECK (dp_wlan_ppdu_len (6, 2, 1) == 160 + 160 + 80 + 480 + 2);

    /* The data rate, Table 17-6's first column. The expected values are
     * typed from the printed table; the arithmetic N_DBPS / 4 (the 4 us
     * symbol) is a second, independent statement that must agree. */
    {
      static const struct
      {
        unsigned       bpsc;
        dp_wlan_code_t code;
        unsigned       mbps;
      } printed[] = {
        { 1, WLAN_CODE_1_2, 6 },  { 1, WLAN_CODE_3_4, 9 },
        { 2, WLAN_CODE_1_2, 12 }, { 2, WLAN_CODE_3_4, 18 },
        { 4, WLAN_CODE_1_2, 24 }, { 4, WLAN_CODE_3_4, 36 },
        { 6, WLAN_CODE_2_3, 48 }, { 6, WLAN_CODE_3_4, 54 },
      };
      const dp_wlan_tables_t *std = dp_wlan_tables_standard ();
      unsigned                seen[8];
      for (size_t i = 0; i < 8; i++)
        {
          const dp_wlan_rate_t r = { printed[i].bpsc, printed[i].code };
          unsigned             m = 777;
          DP_CHECK (dp_wlan_mbps (std, &r, &m) == 1);
          DP_CHECK (m == printed[i].mbps);
          DP_CHECK (m * 4u == dp_wlan_n_dbps (&r));
          seen[i] = m;
          /* the inverse returns the pair */
          dp_wlan_rate_t back = { 0, 0 };
          DP_CHECK (dp_wlan_rate_from_mbps (std, m, &back) == 1);
          DP_CHECK (back.n_bpsc == r.n_bpsc && back.code == r.code);
        }
      for (size_t i = 0; i < 8; i++) /* eight rates, eight distinct values */
        for (size_t j = i + 1; j < 8; j++)
          DP_CHECK (seen[i] != seen[j]);

      /* all sixteen pairs: exactly eight have a row, the rest refuse and
       * leave the output as the caller set it */
      static const unsigned nb[4] = { 1, 2, 4, 6 };
      int                   rows  = 0;
      for (int c = 0; c < 4; c++)
        for (int b = 0; b < 4; b++)
          {
            const dp_wlan_rate_t r  = { nb[b], (dp_wlan_code_t)c };
            unsigned             m  = 777;
            const int            ok = dp_wlan_mbps (std, &r, &m);
            rows += ok;
            DP_CHECK (ok ? m != 777 : m == 777);
          }
      DP_CHECK (rows == 8);

      /* refusals: a rate with no row, zero, and the neighbours of each
       * printed value; the pair is left as set */
      static const unsigned none[] = { 0, 1, 5, 7, 8, 10, 35, 37, 55, 1000 };
      for (size_t i = 0; i < sizeof none / sizeof *none; i++)
        {
          dp_wlan_rate_t r = { 99, 99 };
          DP_CHECK (dp_wlan_rate_from_mbps (std, none[i], &r) == 0);
          DP_CHECK (r.n_bpsc == 99 && r.code == 99);
        }

      /* an empty table, and a table with no rates given (the synthetic one) */
      const dp_wlan_tables_t syn = synth_tables ();
      const dp_wlan_rate_t   r36 = { 4, WLAN_CODE_3_4 };
      unsigned               m   = 777;
      dp_wlan_rate_t         rr  = { 99, 99 };
      DP_CHECK (dp_wlan_mbps (dp_wlan_tables_empty (), &r36, &m) == 0);
      DP_CHECK (dp_wlan_mbps (&syn, &r36, &m) == 0);
      DP_CHECK (m == 777);
      DP_CHECK (dp_wlan_rate_from_mbps (dp_wlan_tables_empty (), 36, &rr)
                == 0);
      DP_CHECK (dp_wlan_rate_from_mbps (&syn, 36, &rr) == 0);
      /* 0 is "not given" in a row, never a rate that matches it */
      DP_CHECK (dp_wlan_rate_from_mbps (&syn, 0, &rr) == 0);
      DP_CHECK (rr.n_bpsc == 99 && rr.code == 99);

      /* negative control: the cross-check against N_DBPS / 4 is red for a
       * table whose 36 Mb/s row says 37 */
      dp_wlan_signal_rate_t wrong[8];
      memcpy (wrong, std->signal_rates, sizeof wrong);
      wrong[5].mbps        = 37;
      dp_wlan_tables_t bad = *std;
      bad.signal_rates     = wrong;
      int agrees           = 1;
      for (size_t i = 0; i < 8; i++)
        {
          unsigned got = 0;
          if (!dp_wlan_mbps (&bad, &wrong[i].rate, &got)
              || got * 4u != dp_wlan_n_dbps (&wrong[i].rate))
            agrees = 0;
        }
      DP_CHECK (agrees == 0);
    }

    /* the object */
    dp_wlan_state_t *w = dp_wlan_create (2, 2, 0, 1);
    DP_REQUIRE (w != NULL);
    DP_CHECK (w->n_bpsc == 4 && w->n_cbps == 192 && w->n_dbps == 144);
    DP_CHECK (w->rate_mbps == 36);
    DP_CHECK (dp_wlan_n_sym (w, 100) == 6);
    dp_wlan_reset (w);
    dp_wlan_destroy (w);
    dp_wlan_destroy (NULL);
    {
      dp_wlan_state_t *x = dp_wlan_create (0, 1, 0, 1); /* BPSK 2/3: no row */
      DP_REQUIRE (x != NULL);
      DP_CHECK (x->rate_mbps == 0);
      dp_wlan_destroy (x);
    }
    DP_CHECK (dp_wlan_create (-1, 0, 0, 1) == NULL);
    DP_CHECK (dp_wlan_create (4, 0, 0, 1) == NULL);
    DP_CHECK (dp_wlan_create (0, 4, 0, 1) == NULL);
    /* every constellation at every rate that gives whole bits: all do here */
    for (int m = 0; m < 4; m++)
      for (int c = 0; c < 4; c++)
        {
          dp_wlan_state_t *x = dp_wlan_create (m, c, 0, 1);
          DP_CHECK (x != NULL);
          dp_wlan_destroy (x);
        }
  }

  /* 2 ── The BCC ────────────────────────────────────────────────────────── */
  {
    conv_code_t c;
    DP_CHECK (dp_wlan_bcc (WLAN_CODE_1_2, &c));
    DP_CHECK (c.k == 7 && c.n == 2 && c.poly[0] == 0133 && c.poly[1] == 0171
              && c.invert == 0 && c.punct_period == 0);
    DP_CHECK (!dp_wlan_bcc ((dp_wlan_code_t)9, &c));

    /* the impulse response of the generators, newest input at the left of the
     * polynomial: a one then zeros gives each output the polynomial's bits in
     * order, here interleaved A B: (1,1)(0,1)(1,1)(1,1)(0,0)(1,0)(1,1) */
    uint8_t       in[16]   = { 1 }, out[64];
    const uint8_t want[14] = { 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1 };
    DP_CHECK (dp_wlan_encode (WLAN_CODE_1_2, in, 16, out, sizeof out) == 32);
    DP_CHECK (memcmp (out, want, sizeof want) == 0);
    for (int i = 14; i < 32; i++)
      DP_CHECK (out[i] == 0); /* then the register is empty */

    /* the punctured stream is the unpunctured one with the deleted positions
     * removed: lengths, and the pattern, from the masks' meaning */
    uint8_t  bits[120], full[240], p[240];
    uint32_t rng = 12345u;
    for (int i = 0; i < 120; i++)
      {
        (void)dp_xs32 (&rng);
        bits[i] = (uint8_t)((rng >> 16) & 1u);
      }
    DP_CHECK (dp_wlan_encode (WLAN_CODE_1_2, bits, 120, full, sizeof full)
              == 240);
    static const struct
    {
      dp_wlan_code_t code;
      size_t         period, kept;
      uint32_t       mask;
    } pat[] = { { WLAN_CODE_2_3, 2, 3, 0x7 },
                { WLAN_CODE_3_4, 3, 4, 0x27 },
                { WLAN_CODE_5_6, 5, 6, 0x267 } };
    for (size_t q = 0; q < 3; q++)
      {
        const size_t n = dp_wlan_encode (pat[q].code, bits, 120, p, sizeof p);
        DP_CHECK (n == 120 / pat[q].period * pat[q].kept);
        size_t m  = 0;
        int    ok = 1;
        for (size_t i = 0; i < 120; i++)
          for (int j = 0; j < 2; j++)
            if ((pat[q].mask >> ((i % pat[q].period) * 2 + (size_t)j)) & 1u)
              ok &= (m < n && p[m++] == full[2 * i + (size_t)j]);
        DP_CHECK (ok && m == n);
      }
    DP_CHECK (dp_wlan_encode (WLAN_CODE_1_2, bits, 120, out, 10) == 0);

    /* The masks against Figure 17-9 as printed (posted on #3109): r = 2/3
     * sends A0 B0 A1 of A0 B0 A1 B1; r = 3/4 sends A0 B0 A1 B2 of A0 B0 A1
     * B1 A2 B2. 5/6 is Clause 19's and is not held. */
    static const char *const sent23[]  = { "A0", "B0", "A1" };
    static const char *const sent34[]  = { "A0", "B0", "A1", "B2" };
    static const char *const wrong34[] = { "A0", "B0", "A1", "B1" };
    conv_code_t              c23, c34;
    DP_CHECK (dp_wlan_bcc (WLAN_CODE_2_3, &c23) == 1);
    DP_CHECK (dp_wlan_bcc (WLAN_CODE_3_4, &c34) == 1);
    DP_CHECK (c23.punct_period == 2 && c34.punct_period == 3);
    DP_CHECK (c23.punct_mask == mask_from_sent (sent23, 3));
    DP_CHECK (c34.punct_mask == mask_from_sent (sent34, 4));
    /* CONTROL: a pattern that is not the printed one is not the mask */
    DP_CHECK (c34.punct_mask != mask_from_sent (wrong34, 4));
  }

  /* 3 ── The scrambler ──────────────────────────────────────────────────── */
  {
    /* Annex I's start, two spellings: the sequence 0110110 is 54, and the
     * register state that produces it is 1011101 */
    DP_CHECK (dp_wlan_scrambler_state_from_sequence (54) == 93);
    uint8_t seq[300], ref[300];
    DP_CHECK (dp_wlan_scrambler_sequence (93, 300, seq) == 300);
    recurrence (54, 300, ref);
    DP_CHECK (memcmp (seq, ref, 300) == 0);
    static const uint8_t first[11] = { 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0 };
    DP_CHECK (memcmp (seq, first, 11) == 0); /* 0110110 0000... */

    /* every state matches the recurrence seeded with its own first seven
     * bits, has period 127, and the 127 prefixes are distinct */
    int seen[128] = { 0 };
    for (unsigned s = 1; s < 128; s++)
      {
        uint8_t a[260], b[260];
        DP_CHECK (dp_wlan_scrambler_sequence ((uint8_t)s, 260, a) == 260);
        unsigned v = 0;
        for (int i = 0; i < 7; i++)
          v = (v << 1) | a[i];
        recurrence (v, 260, b);
        DP_CHECK (memcmp (a, b, 260) == 0);
        DP_CHECK (memcmp (a, a + 127, 127) == 0); /* period 127 */
        DP_CHECK (!seen[v]);
        seen[v] = 1;
        DP_CHECK (dp_wlan_scrambler_state_from_sequence ((uint8_t)v)
                  == (uint8_t)s);
      }
    /* the sequence is the library's `pn`: for every state some seed of the
     * Galois register with polynomial 0x48 (x^7 + x^4 + 1) reproduces it */
    for (unsigned s = 1; s < 128; s++)
      {
        uint8_t a[254], b[254];
        DP_CHECK (dp_wlan_scrambler_sequence ((uint8_t)s, 254, a) == 254);
        int found = 0;
        for (uint64_t seed = 1; seed < 128 && !found; seed++)
          {
            dp_pn_state_t *p = dp_pn_create (0x48, seed, 7, PN_GALOIS);
            DP_REQUIRE (p != NULL);
            dp_pn_generate (p, 254, b, 254);
            dp_pn_destroy (p);
            found = (memcmp (a, b, 254) == 0);
          }
        DP_CHECK (found);
      }
    unsigned ones = 0;
    uint8_t  one[127];
    dp_wlan_scrambler_sequence (127, 127, one);
    for (int i = 0; i < 127; i++)
      ones += one[i];
    DP_CHECK (ones == 64); /* a maximal-length sequence */
    DP_CHECK (dp_wlan_scrambler_sequence (0, 4, seq) == 0);
    DP_CHECK (dp_wlan_scrambler_sequence (128, 4, seq) == 0);
    DP_CHECK (dp_wlan_scrambler_state_from_sequence (0) == 0);
    DP_CHECK (dp_wlan_scrambler_state_from_sequence (128) == 0);
  }

  /* 4 ── The minimal MAC and the FCS ────────────────────────────────────── */
  {
    DP_CHECK (dp_wlan_mpdu_len (WLAN_MAC_NONE, 0, 100) == 100);
    DP_CHECK (dp_wlan_mpdu_len (WLAN_MAC_FCS, 0, 100) == 104);
    DP_CHECK (dp_wlan_mpdu_len (WLAN_MAC_DATA, 24, 100) == 128);
    DP_CHECK (dp_wlan_mpdu_len (WLAN_MAC_NONE, 0, 0) == 0);
    DP_CHECK (dp_wlan_mpdu_len (WLAN_MAC_FCS, 0, WLAN_PSDU_MAX - 3) == 0);
    DP_CHECK (dp_wlan_mpdu_len (WLAN_MAC_FCS, 0, WLAN_PSDU_MAX - 4)
              == WLAN_PSDU_MAX);
    DP_CHECK (dp_wlan_mpdu_len ((dp_wlan_mac_t)7, 0, 10) == 0);

    /* the FCS is CRC-32/ISO-HDLC, whose catalogue check value over the nine
     * ASCII digits is CBF43926, least significant octet first */
    const uint8_t digits[9] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    uint8_t       out[64];
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_FCS, NULL, 0, digits, 9, out, sizeof out)
              == 13);
    DP_CHECK (memcmp (out, digits, 9) == 0);
    DP_CHECK (out[9] == 0x26 && out[10] == 0x39 && out[11] == 0xF4
              && out[12] == 0xCB);

    /* appended that way, the CRC over the whole frame is one constant,
     * whatever the frame: the property the wire order exists to give */
    const dp_crc_t model = DP_CRC_32_ISO_HDLC;
    uint8_t        f1[40], f2[40];
    uint32_t       rng = 99u;
    for (int i = 0; i < 20; i++)
      {
        (void)dp_xs32 (&rng);
        f1[i] = (uint8_t)(rng >> 24);
        (void)dp_xs32 (&rng);
        f2[i] = (uint8_t)(rng >> 24);
      }
    uint8_t m1[32], m2[32];
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_FCS, NULL, 0, f2, 20, m2, sizeof m2)
              == 24);
    for (int i = 0; i < 20; i++)
      {
        (void)dp_xs32 (&rng);
        f1[i] = (uint8_t)(rng >> 24);
      }
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_FCS, NULL, 0, f1, 20, m1, sizeof m1)
              == 24);
    DP_CHECK (dp_crc_bytes (&model, m1, 24) == dp_crc_bytes (&model, m2, 24));

    /* DATA mode is the header then the body then the FCS: the FCS mode of the
     * concatenation */
    uint8_t hdr[24], body[50], cat[74], d1[80], d2[80];
    for (int i = 0; i < 24; i++)
      hdr[i] = (uint8_t)(i * 7 + 1);
    for (int i = 0; i < 50; i++)
      body[i] = (uint8_t)(i * 3 + 2);
    memcpy (cat, hdr, 24);
    memcpy (cat + 24, body, 50);
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_DATA, hdr, 24, body, 50, d1, sizeof d1)
              == 78);
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_FCS, NULL, 0, cat, 74, d2, sizeof d2)
              == 78);
    DP_CHECK (memcmp (d1, d2, 78) == 0);
    /* NONE is the caller's octets, untouched */
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_NONE, NULL, 0, body, 50, d1, sizeof d1)
              == 50);
    DP_CHECK (memcmp (d1, body, 50) == 0);
    /* a refusal leaves the buffer alone */
    memset (d1, 0xAB, sizeof d1);
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_DATA, hdr, 24, body, 50, d1, 77) == 0);
    DP_CHECK (dp_wlan_mpdu (WLAN_MAC_NONE, NULL, 0, body, 0, d1, 80) == 0);
    for (int i = 0; i < 80; i++)
      DP_CHECK (d1[i] == 0xAB);
  }

  /* 5 ── The SIGNAL and Data bits ───────────────────────────────────────── */
  {
    const dp_wlan_tables_t t = synth_tables ();
    const dp_wlan_rate_t   r = { 4, WLAN_CODE_3_4 };
    uint8_t                sig[WLAN_SIG_BITS];
    for (size_t len = 1; len <= WLAN_PSDU_MAX; len += 311)
      {
        DP_CHECK (dp_wlan_signal_bits (&t, &r, len, sig) == WLAN_SIG_BITS);
        unsigned rate_bits = 0, length = 0, parity = 0;
        for (int i = 0; i < 4; i++)
          rate_bits = (rate_bits << 1) | sig[i];
        for (int i = 0; i < 12; i++)
          length |= (unsigned)sig[5 + i] << i;
        for (int i = 0; i < 18; i++)
          parity ^= sig[i];
        /* the entry of this rate in the synthetic table: 2*4 + 2 */
        DP_CHECK (rate_bits == 10u);
        DP_CHECK (sig[4] == 0);   /* reserved */
        DP_CHECK (length == len); /* LENGTH, least significant first */
        DP_CHECK (parity == 0);   /* even parity over the first 17 */
        for (int i = 18; i < 24; i++)
          DP_CHECK (sig[i] == 0); /* tail */
      }
    DP_CHECK (dp_wlan_signal_bits (&t, &r, 0, sig) == 0);
    DP_CHECK (dp_wlan_signal_bits (&t, &r, WLAN_PSDU_MAX + 1, sig) == 0);
    DP_CHECK (dp_wlan_signal_bits (dp_wlan_tables_empty (), &r, 10, sig) == 0);
    dp_wlan_tables_t none = t;
    none.n_signal_rates   = 0;
    DP_CHECK (dp_wlan_signal_bits (&none, &r, 10, sig) == 0);

    /* the Data bits: unscramble, and SERVICE is zero, the PSDU is back, the
     * tail was reset, the pad is zero */
    uint8_t psdu[100];
    for (int i = 0; i < 100; i++)
      psdu[i] = (uint8_t)(i * 37 + 11);
    const size_t n_sym = 6, total = n_sym * 144;
    uint8_t      bits[6 * 144], seq[6 * 144];
    DP_CHECK (dp_wlan_data_bits (&r, psdu, 100, 93, bits, sizeof bits)
              == total);
    dp_wlan_scrambler_sequence (93, total, seq);
    for (size_t i = 0; i < 16; i++)
      DP_CHECK ((bits[i] ^ seq[i]) == 0); /* SERVICE */
    for (size_t i = 0; i < 100; i++)
      for (int b = 0; b < 8; b++)
        DP_CHECK ((bits[16 + 8 * i + (size_t)b] ^ seq[16 + 8 * i + (size_t)b])
                  == ((psdu[i] >> b) & 1)); /* each octet LSB first */
    for (size_t i = 16 + 800; i < 16 + 800 + 6; i++)
      DP_CHECK (bits[i] == 0); /* the tail, after scrambling */
    for (size_t i = 16 + 806; i < total; i++)
      DP_CHECK ((bits[i] ^ seq[i]) == 0); /* the pad */
    DP_CHECK (dp_wlan_data_bits (&r, psdu, 100, 93, bits, total - 1) == 0);
    DP_CHECK (dp_wlan_data_bits (&r, psdu, 100, 0, bits, sizeof bits) == 0);
    DP_CHECK (dp_wlan_data_bits (&r, psdu, 0, 93, bits, sizeof bits) == 0);
    /* a different state gives different scrambled bits */
    uint8_t bits2[6 * 144];
    DP_CHECK (dp_wlan_data_bits (&r, psdu, 100, 5, bits2, sizeof bits2)
              == total);
    DP_CHECK (memcmp (bits, bits2, total) != 0);
  }

  /* 6 ── Interleaving and mapping ───────────────────────────────────────── */
  {
    dp_wlan_tables_t      t       = synth_tables ();
    static const unsigned bpsc[4] = { 1, 2, 4, 6 };
    for (int m = 0; m < 4; m++)
      {
        const unsigned nb = bpsc[m];
        const size_t   nc = 48u * nb, n = 3 * nc;
        uint8_t        in[3 * 288], mid[3 * 288], back[3 * 288];
        for (size_t i = 0; i < n; i++)
          in[i] = (uint8_t)((i * 2654435761u >> 7) & 1u);
        DP_CHECK (dp_wlan_interleave (&t, nb, in, n, mid) == n);
        /* the permutation is the first one then the second, checked bit by
         * bit against the library's index functions */
        int ok = 1;
        for (size_t s = 0; s < 3; s++)
          for (size_t k = 0; k < nc; k++)
            {
              const size_t i = (k % 16) * (nc / 16) + k / 16;
              const size_t j = synth_perm2 (i, nc, nb);
              ok &= (mid[s * nc + j] == in[s * nc + k]);
            }
        DP_CHECK (ok);
        /* and it inverts: undo the second then the first */
        for (size_t s = 0; s < 3; s++)
          for (size_t k = 0; k < nc; k++)
            {
              const size_t i   = (k % 16) * (nc / 16) + k / 16;
              const size_t j   = synth_perm2 (i, nc, nb);
              back[s * nc + k] = mid[s * nc + j];
            }
        DP_CHECK (memcmp (back, in, n) == 0);
        DP_CHECK (dp_wlan_interleave (&t, nb, in, n - 1, mid) == 0);
        DP_CHECK (dp_wlan_interleave (&t, nb, in, 0, mid) == 0);
      }
    DP_CHECK (
        dp_wlan_interleave (&t, 3, (uint8_t[48]){ 0 }, 48, (uint8_t[48]){ 0 })
        == 0);
    DP_CHECK (dp_wlan_interleave (dp_wlan_tables_empty (), 1,
                                  (uint8_t[48]){ 0 }, 48, (uint8_t[48]){ 0 })
              == 0);     /* no second permutation: refused */
    t.perm2 = bad_perm2; /* not a permutation: refused, out untouched */
    uint8_t in48[48] = { 1 }, out48[48];
    memset (out48, 0x5A, sizeof out48);
    DP_CHECK (dp_wlan_interleave (&t, 1, in48, 48, out48) == 0);
    for (int i = 0; i < 48; i++)
      DP_CHECK (out48[i] == 0x5A);
    t.perm2 = synth_perm2;

    /* the mapper: every label gives a point; the points have unit average
     * power; the first bit is bit 0 of the label */
    for (int m = 0; m < 4; m++)
      {
        const unsigned nb   = bpsc[m];
        const size_t   nlab = (size_t)1 << nb;
        uint8_t        bits[6 * 64];
        float _Complex pts[64];
        for (size_t v = 0; v < nlab; v++)
          for (unsigned b = 0; b < nb; b++)
            bits[v * nb + b] = (uint8_t)((v >> b) & 1u);
        DP_CHECK (dp_wlan_map (&t, nb, bits, nlab * nb, pts) == nlab);
        double power = 0.0;
        for (size_t v = 0; v < nlab; v++)
          power += (double)crealf (pts[v]) * crealf (pts[v])
                   + (double)cimagf (pts[v]) * cimagf (pts[v]);
        DP_CHECK_NEAR (power / (double)nlab, 1.0, 1e-5);
        /* a relabeling table permutes the points the labels give */
        for (size_t v = 0; v < nlab; v++)
          g_relabel_rev[v] = (uint16_t)(nlab - 1 - v);
        dp_wlan_tables_t tr = t;
        tr.relabel[nb]      = g_relabel_rev;
        float _Complex rp[64];
        DP_CHECK (dp_wlan_map (&tr, nb, bits, nlab * nb, rp) == nlab);
        int ok = 1;
        for (size_t v = 0; v < nlab; v++)
          ok &= close_c (rp[v], pts[nlab - 1 - v], 1e-6);
        DP_CHECK (ok);
      }
    /* the first bit is bit 0 of the library's label: the bits (1, 0) are
     * label 1 and (0, 1) label 2; in 16-QAM (1,0,0,0) is 1 and (0,0,1,0) is
     * 4 */
    {
      float _Complex got[1], lib[1];
      const uint8_t q10[2] = { 1, 0 }, q01[2] = { 0, 1 };
      const uint8_t l1 = 1, l2 = 2;
      DP_CHECK (dp_wlan_map (&t, 2, q10, 2, got) == 1);
      dp_mpsk_map (&l1, 1, lib, 4);
      DP_CHECK (close_c (got[0], lib[0], 1e-6));
      DP_CHECK (dp_wlan_map (&t, 2, q01, 2, got) == 1);
      dp_mpsk_map (&l2, 1, lib, 4);
      DP_CHECK (close_c (got[0], lib[0], 1e-6));
      const uint8_t  b1000[4] = { 1, 0, 0, 0 }, b0010[4] = { 0, 0, 1, 0 };
      const uint16_t s1 = 1, s4 = 4;
      DP_CHECK (dp_wlan_map (&t, 4, b1000, 4, got) == 1);
      dp_qam_map (&s1, 1, lib, 16);
      DP_CHECK (close_c (got[0], lib[0], 1e-6));
      DP_CHECK (dp_wlan_map (&t, 4, b0010, 4, got) == 1);
      dp_qam_map (&s4, 1, lib, 16);
      DP_CHECK (close_c (got[0], lib[0], 1e-6));
    }
    uint8_t one_bit[2] = { 0, 1 };
    float _Complex two[2];
    DP_CHECK (dp_wlan_map (&t, 1, one_bit, 2, two) == 2);
    DP_CHECK (close_c (two[0], -two[1], 1e-6));           /* BPSK: antipodal */
    DP_CHECK (dp_wlan_map (&t, 4, one_bit, 2, two) == 0); /* a partial group */
    DP_CHECK (dp_wlan_map (&t, 5, one_bit, 2, two) == 0);
    DP_CHECK (dp_wlan_map (&t, 1, one_bit, 0, two) == 0);
  }

  /* 7 ── The points, and refusals that change nothing ───────────────────── */
  {
    const dp_wlan_tables_t t = synth_tables ();
    uint8_t                psdu[100];
    for (int i = 0; i < 100; i++)
      psdu[i] = (uint8_t)(i * 29 + 3);
    static const dp_wlan_rate_t rs[] = { { 1, WLAN_CODE_1_2 },
                                         { 2, WLAN_CODE_3_4 },
                                         { 4, WLAN_CODE_3_4 },
                                         { 6, WLAN_CODE_2_3 } };
    for (size_t q = 0; q < 4; q++)
      {
        const size_t n_sym = dp_wlan_rate_n_sym (&rs[q], 100);
        const size_t n_pts = 48 + 48 * n_sym;
        float _Complex pts[48 + 48 * 400];
        DP_CHECK (dp_wlan_points (&t, &rs[q], psdu, 100, 93, pts, n_pts)
                  == n_pts);
        /* the SIGNAL field's 48: the stages composed here, by hand */
        uint8_t sig[WLAN_SIG_BITS], coded[48], inter[48];
        float _Complex hand[48];
        DP_CHECK (dp_wlan_signal_bits (&t, &rs[q], 100, sig) == 24);
        DP_CHECK (dp_wlan_encode (WLAN_CODE_1_2, sig, 24, coded, 48) == 48);
        DP_CHECK (dp_wlan_interleave (&t, 1, coded, 48, inter) == 48);
        DP_CHECK (dp_wlan_map (&t, 1, inter, 48, hand) == 48);
        int ok = 1;
        for (int i = 0; i < 48; i++)
          ok &= close_c (pts[i], hand[i], 1e-6);
        DP_CHECK (ok);
        /* the SIGNAL field is BPSK: every point real, unit magnitude */
        for (int i = 0; i < 48; i++)
          DP_CHECK (fabsf (cimagf (pts[i])) < 1e-6
                    && fabsf (cabsf (pts[i]) - 1.0f) < 1e-6);
        /* a smaller capacity is refused */
        DP_CHECK (dp_wlan_points (&t, &rs[q], psdu, 100, 93, pts, n_pts - 1)
                  == 0);
      }
    /* an empty table refuses and writes nothing */
    float _Complex poison[600];
    for (int i = 0; i < 600; i++)
      poison[i] = 7.0f + 7.0f * I;
    const dp_wlan_rate_t r = { 4, WLAN_CODE_3_4 };
    DP_CHECK (dp_wlan_points (dp_wlan_tables_empty (), &r, psdu, 100, 93,
                              poison, 600)
              == 0);
    DP_CHECK (dp_wlan_ppdu (dp_wlan_tables_empty (), &r, psdu, 100, 93, 0, 1,
                            poison, 600)
              == 0);
    for (int i = 0; i < 48; i++)
      DP_CHECK (poison[i] == 7.0f + 7.0f * I);
    dp_ofdm_state_t *o = dp_ofdm_create ();
    DP_CHECK (dp_wlan_layout (o, dp_wlan_tables_empty (), 6, 0, 1) == -1);
    dp_ofdm_destroy (o);
  }

  /* 8 ── The PPDU ───────────────────────────────────────────────────────── */
  {
    const dp_wlan_tables_t t = synth_tables ();
    const dp_wlan_rate_t   r = { 4, WLAN_CODE_3_4 };
    uint8_t                psdu[100];
    for (int i = 0; i < 100; i++)
      psdu[i] = (uint8_t)(i * 41 + 7);
    const size_t    n_sym = dp_wlan_rate_n_sym (&r, 100); /* 6 */
    const size_t    n     = dp_wlan_ppdu_len (n_sym, 0, 1);
    float _Complex *x     = malloc (n * sizeof *x);
    DP_REQUIRE (x != NULL);
    DP_CHECK (dp_wlan_ppdu (&t, &r, psdu, 100, 93, 0, 1, x, n) == n);
    DP_CHECK (n == 160 + 160 + 80 + 480);
    DP_CHECK (dp_wlan_ppdu (&t, &r, psdu, 100, 93, 0, 1, x, n - 1) == 0);

    /* L-STF: tones at multiples of four give a period of 16 samples */
    for (size_t i = 0; i + 16 < 160; i++)
      DP_CHECK (close_c (x[i], x[i + 16], 1e-5));
    DP_CHECK (cabsf (x[0]) > 0.0f);
    /* L-LTF: a 32-sample prefix that is the last 32 of the body, two equal
     * 64-sample periods */
    for (int i = 0; i < 32; i++)
      DP_CHECK (close_c (x[160 + i], x[160 + 32 + 96 + i], 1e-6));
    for (int i = 0; i < 64; i++)
      DP_CHECK (close_c (x[192 + i], x[192 + 64 + i], 1e-6));
    /* every later OFDM symbol: a 16-sample prefix copied from the end */
    for (size_t s = 0; s < 1 + n_sym; s++)
      {
        const size_t b = 320 + 80 * s;
        for (int i = 0; i < 16; i++)
          DP_CHECK (
              close_c (x[b + (size_t)i], x[b + 16 + 48 + (size_t)i], 1e-6));
      }

    /* the Data symbols, demodulated by a naive DFT: the pilots are the
     * checked values times the polarity from an independent recurrence, and
     * the data tones are the points the builder returns */
    uint8_t seq[127];
    dp_wlan_scrambler_sequence (127, 127, seq); /* the all-ones start */
    float _Complex pts[48 + 48 * 6];
    DP_REQUIRE (dp_wlan_points (&t, &r, psdu, 100, 93, pts, 48 + 48 * 6)
                == 48 + 48 * 6);
    /* the checked pilots, as literals and not the header's macros: -21, -7
     * and 7 carry +1 and 21 carries -1 (17.3.5.9, 17.3.5.10) */
    static const int    pk[4]     = { -21, -7, 7, 21 };
    static const double pv[4]     = { 1.0, 1.0, 1.0, -1.0 };
    int                 pilots_ok = 1, tones_ok = 1;
    for (size_t s = 0; s < 1 + n_sym; s++)
      {
        const float _Complex *body = x + 320 + 80 * s + 16;
        const double          pol  = seq[s] ? -1.0 : 1.0;
        size_t                di   = 0;
        for (int k = -26; k <= 26; k++)
          {
            float _Complex acc = 0.0f;
            for (int m = 0; m < 64; m++)
              acc += body[m] * cexpf ((float)(-TWOPI * k * m / 64.0) * I);
            acc /= (float)(64.0 * t.scale_data);
            int ip = -1;
            for (int i = 0; i < 4; i++)
              if (k == pk[i])
                ip = i;
            if (ip >= 0)
              pilots_ok &= close_c (acc, (float)(pv[ip] * pol), 1e-4);
            else if (k == 0)
              pilots_ok &= (cabsf (acc) < 1e-4); /* DC carries nothing */
            else
              tones_ok &= close_c (acc, pts[48 * s + di++], 1e-4);
          }
        tones_ok &= (di == 48);
      }
    DP_CHECK (pilots_ok);
    DP_CHECK (tones_ok);

    /* a window adds its tail and leaves the interior alone */
    float _Complex *xw = malloc ((n + 4) * sizeof *xw);
    DP_REQUIRE (xw != NULL);
    DP_CHECK (dp_wlan_ppdu (&t, &r, psdu, 100, 93, 4, 1, xw, n + 4) == n + 4);
    for (size_t i = 200; i < n - 200; i++)
      if (i % 80 > 8 && i % 80 < 72)
        DP_CHECK (close_c (xw[i], x[i], 1e-4));
    free (xw);
    free (x);
  }

  /* ── 9. The standard's tables, each against its printed form ──────────
   * Every expectation below is typed here in the form the standard prints
   * it (IEEE Std 802.11-2024, as transcribed on #3109), not derived from
   * the table under test. */
  {
    const dp_wlan_tables_t *T = dp_wlan_tables_standard ();

    /* Table 17-6: the RATE bits R1 R2 R3 R4 for each (N_BPSC, code). */
    static const struct
    {
      unsigned       n_bpsc;
      dp_wlan_code_t code;
      const char    *bits;
    } rate_rows[] = {
      { 1, WLAN_CODE_1_2, "1101" }, { 1, WLAN_CODE_3_4, "1111" },
      { 2, WLAN_CODE_1_2, "0101" }, { 2, WLAN_CODE_3_4, "0111" },
      { 4, WLAN_CODE_1_2, "1001" }, { 4, WLAN_CODE_3_4, "1011" },
      { 6, WLAN_CODE_2_3, "0001" }, { 6, WLAN_CODE_3_4, "0011" },
    };
    for (size_t i = 0; i < sizeof rate_rows / sizeof *rate_rows; i++)
      {
        dp_wlan_rate_t r = { rate_rows[i].n_bpsc, rate_rows[i].code };
        uint8_t        sig[WLAN_SIG_BITS];
        DP_CHECK (dp_wlan_signal_bits (T, &r, 100, sig) == WLAN_SIG_BITS);
        for (int b = 0; b < 4; b++)
          DP_CHECK (sig[b] == (uint8_t)(rate_rows[i].bits[b] - '0'));
      }
    /* A pair Clause 17 has no RATE for is refused, not guessed. */
    {
      dp_wlan_rate_t r56 = { 6, WLAN_CODE_5_6 }, odd = { 1, WLAN_CODE_2_3 };
      uint8_t        sig[WLAN_SIG_BITS];
      DP_CHECK (dp_wlan_signal_bits (T, &r56, 100, sig) == 0);
      DP_CHECK (dp_wlan_signal_bits (T, &odd, 100, sig) == 0);
    }

    /* Eq. (17-6) and (17-8), k = -26..26, as printed: the L-STF's sign of
     * (1+j) per tone, scaled by sqrt(13/6), and the L-LTF's +-1. */
    static const int stf_sign[53]
        = { 0,  0, 1, 0, 0, 0, -1, 0, 0, 0, 1, 0, 0,  0, -1, 0, 0,  0,
            -1, 0, 0, 0, 1, 0, 0,  0, 0, 0, 0, 0, -1, 0, 0,  0, -1, 0,
            0,  0, 1, 0, 0, 0, 1,  0, 0, 0, 1, 0, 0,  0, 1,  0, 0 };
    static const int ltf_val[53]
        = { 1,  1,  -1, -1, 1,  1, -1, 1,  -1, 1, 1,  1,  1,  1, 1,  -1, -1, 1,
            1,  -1, 1,  -1, 1,  1, 1,  1,  0,  1, -1, -1, 1,  1, -1, 1,  -1, 1,
            -1, -1, -1, -1, -1, 1, 1,  -1, -1, 1, -1, 1,  -1, 1, 1,  1,  1 };
    const double sq = sqrt (13.0 / 6.0);
    for (int i = 0; i < 53; i++)
      {
        DP_CHECK (fabs (crealf (T->stf[i]) - stf_sign[i] * sq) < 1e-6);
        DP_CHECK (fabs (cimagf (T->stf[i]) - stf_sign[i] * sq) < 1e-6);
        DP_CHECK (crealf (T->ltf[i]) == (float)ltf_val[i]);
        DP_CHECK (cimagf (T->ltf[i]) == 0.0f);
      }
    DP_CHECK (T->stf[26] == 0.0f && T->ltf[26] == 0.0f); /* DC */
    /* One scale for every field, this library's convention (unit mean power
       over the 52 occupied tones, #3148); not the standard's printed 1/64. */
    DP_CHECK (T->scale_stf == T->scale_ltf && T->scale_ltf == T->scale_data);
    DP_CHECK (fabs (T->scale_data - 1.0 / sqrt (52.0)) < 1e-15);

    /* Eq. (17-16), (17-17): bijective on every Clause 17 (N_CBPS, N_BPSC),
     * the identity for BPSK (s = 1), and the standard's own worked example
     * (Annex I.1.6.2: k = 17 goes to i = 13, then j = 12, at 192 / 4). */
    static const unsigned pairs[4][2]
        = { { 48, 1 }, { 96, 2 }, { 192, 4 }, { 288, 6 } };
    for (int p = 0; p < 4; p++)
      {
        const size_t n = pairs[p][0], b = pairs[p][1];
        uint8_t     *seen = calloc (n, 1);
        for (size_t i = 0; i < n; i++)
          {
            const size_t j = T->perm2 (i, n, b);
            DP_REQUIRE (j < n);
            DP_CHECK (!seen[j]);
            seen[j] = 1;
            if (b == 1)
              DP_CHECK (j == i);
          }
        free (seen);
      }
    {
      /* the library's own first permutation (not arithmetic written here),
       * then the table's second */
      const size_t i1 = dp_interleave_index (17, 192 / 16, 16);
      DP_CHECK (i1 == 13);
      DP_CHECK (T->perm2 (i1, 192, 4) == 12);
    }
    /* The table against Eq. (17-16), (17-17) re-derived above, for EVERY
     * index of all four Clause 17 pairs (so 64-QAM's s = 3, where bijectivity
     * cannot tell a sign slip from the equation). */
    for (int p = 0; p < 4; p++)
      for (size_t i = 0; i < pairs[p][0]; i++)
        DP_CHECK (T->perm2 (i, pairs[p][0], pairs[p][1])
                  == ref_perm2 (i, pairs[p][0], pairs[p][1], 0));
    /* CONTROL: the sign-flipped formula disagrees with the table at 64-QAM
     * (it cannot at s = 1 or 2, where -x and x are the same mod s) */
    {
      size_t differ = 0;
      for (size_t i = 0; i < 288; i++)
        differ += T->perm2 (i, 288, 6) != ref_perm2 (i, 288, 6, 1);
      DP_CHECK (differ > 0);
    }

    /* Figure 17-10, Tables 17-14 to 17-18: the bit encodings as the
     * standard prints them (B0 is the first bit of the group), each point
     * d = (I + jQ) * K_MOD, against dp_wlan_map() through the relabels. */
    static const int lv16[4] = { -3, -1, 3, 1 }; /* B0B1: 00 01 10 11 */
    static const int lv64[8]
        = { -7, -5, -1, -3, 7, 5, 1, 3 }; /* B0B1B2 as MSB-first */
    static const unsigned nbs[4] = { 1, 2, 4, 6 };
    for (int ni = 0; ni < 4; ni++)
      {
        const unsigned nb       = nbs[ni];
        const unsigned n_groups = 1u << nb;
        for (unsigned v = 0; v < n_groups; v++)
          {
            uint8_t bits[6];
            for (unsigned b = 0; b < nb; b++)
              bits[b] = (uint8_t)((v >> b) & 1u); /* B0 first */
            double li = 0, lq = 0, K = 1;
            if (nb == 1)
              li = bits[0] ? 1 : -1;
            else if (nb == 2)
              {
                K  = 1 / sqrt (2.0);
                li = bits[0] ? 1 : -1;
                lq = bits[1] ? 1 : -1;
              }
            else if (nb == 4)
              {
                K  = 1 / sqrt (10.0);
                li = lv16[bits[0] * 2 + bits[1]];
                lq = lv16[bits[2] * 2 + bits[3]];
              }
            else
              {
                K  = 1 / sqrt (42.0);
                li = lv64[bits[0] * 4 + bits[1] * 2 + bits[2]];
                lq = lv64[bits[3] * 4 + bits[4] * 2 + bits[5]];
              }
            float _Complex pt;
            DP_CHECK (dp_wlan_map (T, nb, bits, nb, &pt) == 1);
            DP_CHECK (fabs (crealf (pt) - li * K) < 1e-5);
            DP_CHECK (fabs (cimagf (pt) - lq * K) < 1e-5);
          }
      }
  }

  /* ── 10. Each field's average power is 1 ─────────────────────────────────
   * This library's convention, the owner's ruling on #3148: every field leaves
   * the modulator at unit average power over its useful samples, whose scale
   * is 1/sqrt(52) (design page, goal 5). It is a convention, not the
   * standard's: Annex I prints the same math with the transform's own 1/64
   * (section 10b). Pinned here so a change to the modulator's transform
   * normalization cannot move it silently. No window, so the fields do not
   * overlap. */
  {
    const dp_wlan_tables_t *T = dp_wlan_tables_standard ();
    static uint8_t          psdu[1000];
    uint32_t                seed = 4242;
    for (int i = 0; i < 1000; i++)
      psdu[i] = (uint8_t)dp_xs32 (&seed);
    static const dp_wlan_rate_t rates[4] = { { 1, WLAN_CODE_1_2 },
                                             { 2, WLAN_CODE_1_2 },
                                             { 4, WLAN_CODE_3_4 },
                                             { 6, WLAN_CODE_3_4 } };
    for (int ri = 0; ri < 4; ri++)
      {
        const size_t    n_sym = dp_wlan_rate_n_sym (&rates[ri], 1000);
        const size_t    n     = 400 + 80 * n_sym;
        float _Complex *x     = malloc (n * sizeof *x);
        DP_REQUIRE (x != NULL);
        DP_CHECK (dp_wlan_ppdu (T, &rates[ri], psdu, 1000, 93, 0, 1, x, n)
                  == n);
        /* L-STF: 160 samples, no prefix; L-LTF: 128 useful after a 32
         * prefix; SIGNAL and Data: 64 useful after a 16 prefix. */
        const struct
        {
          size_t first, len;
        } f[3] = { { 0, 160 }, { 192, 128 }, { 336, 64 } };
        for (int k = 0; k < 3; k++)
          {
            double pw = 0;
            for (size_t m = 0; m < f[k].len; m++)
              pw += (double)(crealf (x[f[k].first + m])
                                 * crealf (x[f[k].first + m])
                             + cimagf (x[f[k].first + m])
                                   * cimagf (x[f[k].first + m]));
            pw /= (double)f[k].len;
            DP_CHECK (fabs (pw - 1.0) < 1e-4); /* training and SIGNAL: exact */
          }
        /* Data: BPSK and QPSK points all have unit power, so each symbol is
         * exact; 16- and 64-QAM average to 1 over symbols, not per symbol. */
        double total = 0;
        for (size_t s = 0; s < n_sym; s++)
          {
            double       pw   = 0;
            const size_t base = 400 + 80 * s + 16;
            for (size_t m = 0; m < 64; m++)
              pw += (double)(crealf (x[base + m]) * crealf (x[base + m])
                             + cimagf (x[base + m]) * cimagf (x[base + m]));
            pw /= 64.0;
            total += pw;
            if (rates[ri].n_bpsc <= 2)
              DP_CHECK (fabs (pw - 1.0) < 1e-4);
          }
        DP_CHECK (fabs (total / (double)n_sym - 1.0)
                  < 0.03); /* 1000 octets: the QAM mean is statistical */
        free (x);
      }
  }

  /* ── 10c. Oversampling: fs = M x 20 MHz by a larger transform ─────────── */
  {
    const dp_wlan_rate_t r36 = { 4, WLAN_CODE_3_4 }, r6 = { 1, WLAN_CODE_1_2 };
    size_t               n1;

    /* M = 1 is bit-identical to the construction before the argument
     * existed, compared in this build against the test's own copy of it
     * (pre_oversample_layout): the same compiler and FFT dispatch rendered
     * both. */
    {
      float _Complex *a = render_ppdu (&r36, 100, 93, 4, 1, &n1);
      float _Complex *p = render_pre (&r36, 100, 93, 4, &n1);
      DP_REQUIRE (a != NULL && n1 == 884);
      DP_CHECK (memcmp (a, p, n1 * sizeof *a) == 0);
      /* control: one bit of one sample is a difference the check sees */
      a[7] = nextafterf (crealf (a[7]), 1e9f) + I * cimagf (a[7]);
      DP_CHECK (memcmp (a, p, n1 * sizeof *a) != 0);
      free (a);
      free (p);
      a = render_ppdu (&r6, 10, 5, 0, 1, &n1);
      p = render_pre (&r6, 10, 5, 0, &n1);
      DP_REQUIRE (a != NULL && n1 == 800);
      DP_CHECK (memcmp (a, p, n1 * sizeof *a) == 0);
      free (a);
      free (p);
    }
    /* And to the committed bytes of wlan_ppdu_m1_oracle.h, produced by gcc at
     * c25c061d5, to WLAN_FFT_TOL: a different compiler's rounding is not
     * bit-equal (the clang build differs), and the same tolerance and source
     * as the decimation checks apply. */
    {
      float _Complex *a = render_ppdu (&r36, 100, 93, 4, 1, &n1);
      DP_REQUIRE (a != NULL);
      DP_CHECK (oracle_l2 (a, n1, ORACLE_36_W4) < WLAN_FFT_TOL);
      free (a);
      a = render_ppdu (&r6, 10, 5, 0, 1, &n1);
      DP_REQUIRE (a != NULL);
      DP_CHECK (oracle_l2 (a, n1, ORACLE_6_W0) < WLAN_FFT_TOL);
      /* control: a 5% change of 100 samples (l2 1.8e-2) is seen */
      for (size_t i = 100; i < 200; i++)
        a[i] *= 1.05f;
      DP_CHECK (oracle_l2 (a, n1, ORACLE_6_W0) > WLAN_FFT_TOL);
      free (a);
    }

    /* lengths are M times the 20 MHz value, the window is not scaled */
    DP_CHECK (dp_wlan_ppdu_len (6, 0, 4) == 4 * (160 + 160 + 80 + 480));
    DP_CHECK (dp_wlan_ppdu_len (6, 3, 4) == 4 * (160 + 160 + 80 + 480) + 3);
    /* the range: 1 to 128, and nothing is built outside it */
    DP_CHECK (dp_wlan_ppdu_len (6, 0, 0) == 0);
    /* and n_sym has dp_wlan_layout's range, 1 to WLAN_N_SYM_MAX */
    DP_CHECK (dp_wlan_ppdu_len (0, 0, 1) == 0);
    DP_CHECK (dp_wlan_ppdu_len (WLAN_N_SYM_MAX, 0, 1) > 0);
    DP_CHECK (dp_wlan_ppdu_len (WLAN_N_SYM_MAX + 1, 0, 1) == 0);
    DP_CHECK (dp_wlan_ppdu_len (6, 0, WLAN_OVERSAMPLE_MAX) > 0);
    DP_CHECK (dp_wlan_ppdu_len (6, 0, WLAN_OVERSAMPLE_MAX + 1) == 0);
    {
      dp_ofdm_state_t *o0 = dp_ofdm_create (), *o1 = dp_ofdm_create ();
      DP_CHECK (dp_wlan_layout (o0, dp_wlan_tables_standard (), 6, 0, 0)
                == -1);
      DP_CHECK (dp_wlan_layout (o1, dp_wlan_tables_standard (), 6, 0,
                                WLAN_OVERSAMPLE_MAX + 1)
                == -1);
      dp_ofdm_destroy (o0);
      dp_ofdm_destroy (o1);
      uint8_t psdu[10] = { 0 };
      float _Complex poison[16];
      for (int i = 0; i < 16; i++)
        poison[i] = 123.0f;
      DP_CHECK (dp_wlan_ppdu (dp_wlan_tables_standard (), &r6, psdu, 10, 5, 0,
                              0, poison, 16)
                == 0);
      DP_CHECK (dp_wlan_ppdu (dp_wlan_tables_standard (), &r6, psdu, 10, 5, 0,
                              WLAN_OVERSAMPLE_MAX + 1, poison, 16)
                == 0);
      for (int i = 0; i < 16; i++)
        DP_CHECK (crealf (poison[i]) == 123.0f);
    }

    /* The check, predicted from the arithmetic: with window 0 the M-times
     * PPDU decimated by M equals the M = 1 PPDU, because it is one equation
     * on a finer grid; the difference is float32 rounding in two transforms,
     * and WLAN_FFT_TOL (above) is asserted at every M tested, powers of two or
     * not. The controls are O(1). */
    static const unsigned ms[] = { 2, 3, 5, 11, 16, 128 };
    for (size_t k = 0; k < sizeof ms / sizeof *ms; k++)
      {
        const unsigned m = ms[k];
        /* the largest M on the short PPDU, the others on the 36 Mb/s one */
        const dp_wlan_rate_t *r   = m == 128 ? &r6 : &r36;
        const size_t          len = m == 128 ? 10 : 100;
        size_t                n_m;
        float _Complex       *y  = render_ppdu (r, len, 93, 0, 1, &n1);
        float _Complex       *xm = render_ppdu (r, len, 93, 0, m, &n_m);
        DP_REQUIRE (y != NULL && xm != NULL);
        DP_CHECK (n_m == (size_t)m * n1);
        DP_CHECK (decimation_error (xm, m, y, n1, NULL, 0, 0) < WLAN_FFT_TOL);
        /* control: a shift of the fine grid by half a coarse sample is far
         * over it */
        DP_CHECK (decimation_error (xm + m / 2, m, y, n1 - 1, NULL, 0, 0)
                  > 1e-2);
        free (y);
        free (xm);
      }

    /* With a window the two PPDUs compared are M x with window M w and
     * M = 1 with window w. Inside a transition they differ (the raised
     * cosine is sampled at (M j + 1/2) / (2 M w) against (j + 1/2) / (2 w),
     * times pi: a grid offset of (M - 1) / 2 fine samples), so
     * the equality is asserted outside the w samples that rise from each
     * boundary, counted on the M = 1 grid: the start of the L-STF, the
     * L-LTF, SIGNAL and each of the six Data symbols. */
    {
      const unsigned      w = 4, m = 4;
      static const size_t bounds[9]
          = { 0, 160, 320, 400, 480, 560, 640, 720, 800 };
      size_t          n_m;
      float _Complex *y  = render_ppdu (&r36, 100, 93, w, 1, &n1);
      float _Complex *xm = render_ppdu (&r36, 100, 93, m * w, m, &n_m);
      DP_REQUIRE (y != NULL && xm != NULL && n1 == 884);
      DP_CHECK (n_m == (size_t)m * 880 + m * w);
      DP_CHECK (decimation_error (xm, m, y, 880, bounds, 9, w) < WLAN_FFT_TOL);
      /* controls: without skipping the transitions the two differ, and so
       * they do when the skip is one sample short of the transition, so the
       * excluded span is tight */
      DP_CHECK (decimation_error (xm, m, y, 880, NULL, 0, 0) > 1e-2);
      DP_CHECK (decimation_error (xm, m, y, 880, bounds, 9, w - 1)
                > WLAN_FFT_TOL);
      free (y);
      free (xm);
    }

    /* The convention holds at every M tested: the L-STF and the L-LTF's long
     * symbols and the SIGNAL body have mean power one, and the first Data
     * symbol's body has the power it has at M = 1 (the same waveform). */
    {
      float _Complex *y = render_ppdu (&r36, 100, 93, 0, 1, &n1);
      DP_REQUIRE (y != NULL);
      const double p_data1 = mean_power (y, (400 + 16), 64);
      free (y);
      for (size_t k = 0; k < sizeof ms / sizeof *ms; k++)
        {
          const unsigned  m = ms[k];
          size_t          n_m;
          float _Complex *xm = render_ppdu (&r36, 100, 93, 0, m, &n_m);
          DP_REQUIRE (xm != NULL);
          DP_CHECK (fabs (mean_power (xm, 0, 160 * m) - 1.0) < 1e-4);
          DP_CHECK (fabs (mean_power (xm, 192 * m, 128 * m) - 1.0) < 1e-4);
          DP_CHECK (fabs (mean_power (xm, (320 + 16) * m, 64 * m) - 1.0)
                    < 1e-4);
          DP_CHECK (
              fabs (mean_power (xm, (400 + 16) * m, 64 * m) / p_data1 - 1.0)
              < 1e-4);
          free (xm);
        }
    }

    /* The spectrum of an oversampled PPDU: the 52 occupied subcarriers lie
     * within +-26/(64 M) of fs (0.1016 at M = 4), and outside them only the
     * symbol boundaries' skirts remain. A window shortens them: the power
     * at four out-of-band frequencies is lower with a window than without,
     * and without one it is not zero. */
    {
      const unsigned      m        = 4;
      static const double f_oob[4] = { 0.15, 0.2, 0.3, 0.4 };
      double              oob[2];
      for (int wi = 0; wi < 2; wi++)
        {
          size_t          n_m;
          float _Complex *xm
              = render_ppdu (&r36, 100, 93, wi ? m * 4 : 0, m, &n_m);
          DP_REQUIRE (xm != NULL);
          double acc = 0;
          for (int q = 0; q < 4; q++)
            {
              double re = 0, im = 0;
              for (size_t i = 0; i < n_m; i++)
                {
                  const double ph = -2.0 * M_PI * f_oob[q] * (double)i;
                  re += crealf (xm[i]) * cos (ph) - cimagf (xm[i]) * sin (ph);
                  im += crealf (xm[i]) * sin (ph) + cimagf (xm[i]) * cos (ph);
                }
              acc += (re * re + im * im) / (double)n_m;
            }
          oob[wi] = acc;
          free (xm);
        }
      DP_CHECK (oob[0] > 0.0);
      DP_CHECK (oob[1] < oob[0]);
    }
  }

  /* ── 10d. The object renders a PPDU: window and oversample are its
   * configuration, ppdu_why() is dp_wlan_ppdu() with a reason ───────────── */
  {
    /* the constructor's ranges: window 0 to 80 M, M 1 to 128 */
    static const struct
    {
      uint32_t window, m;
      int      ok;
    } cr[]
        = { { 0, 1, 1 },       { 80, 1, 1 },     { 81, 1, 0 },  { 160, 2, 1 },
            { 161, 2, 0 },     { 0, 0, 0 },      { 0, 128, 1 }, { 0, 129, 0 },
            { 10240, 128, 1 }, { 10241, 128, 0 } };
    for (size_t i = 0; i < sizeof cr / sizeof *cr; i++)
      {
        dp_wlan_state_t *x = dp_wlan_create (2, 2, cr[i].window, cr[i].m);
        DP_CHECK ((x != NULL) == cr[i].ok);
        if (x)
          {
            DP_CHECK (x->window == cr[i].window && x->oversample == cr[i].m);
            dp_wlan_destroy (x);
          }
      }
    /* the largest window the constructor accepts is one the modulator takes:
     * 80 M samples at each boundary */
    {
      const dp_wlan_rate_t r36 = { 4, WLAN_CODE_3_4 };
      size_t               n;
      float _Complex      *x = render_ppdu (&r36, 100, 93, 80, 1, &n);
      DP_CHECK (x != NULL);
      free (x);
      x = render_ppdu (&r36, 100, 93, 160, 2, &n);
      DP_CHECK (x != NULL);
      free (x);
    }

    /* each refusal names the argument that failed, and the predicate and the
       constructor agree; a row has one argument bad, the others good */
    {
      static const struct
      {
        int         c, r;
        uint32_t    w, m;
        const char *why;
      } why[] = {
        { 4, 0, 0, 1, "constellation must be 0 to 3" },
        { -1, 0, 0, 1, "constellation must be 0 to 3" },
        { 0, 4, 0, 1, "code_rate must be 0 to 3" },
        { 0, 0, 0, 0, "oversample must be 1 to 128" },
        { 0, 0, 0, 129, "oversample must be 1 to 128" },
        { 0, 0, 81, 1, "window must be 0 to 80 * oversample" },
        { 0, 0, 161, 2, "window must be 0 to 80 * oversample" },
        { 0, 0, 20000, 129, "oversample must be 1 to 128" }, /* both bad */
      };
      for (size_t i = 0; i < sizeof why / sizeof *why; i++)
        {
          const char *p
              = dp_wlan_params_why (why[i].c, why[i].r, why[i].w, why[i].m);
          DP_CHECK (p != NULL && strcmp (p, why[i].why) == 0);
          const char *got = NULL;
          DP_CHECK (
              dp_wlan_create_why (why[i].c, why[i].r, why[i].w, why[i].m, &got)
              == NULL);
          DP_CHECK (got != NULL && strcmp (got, why[i].why) == 0);
        }
      /* the minimal pairs: accepted, no reason, and why left as it was */
      const char      *sentinel = "untouched";
      const char      *got      = sentinel;
      dp_wlan_state_t *ok       = dp_wlan_create_why (0, 0, 80, 1, &got);
      DP_CHECK (ok != NULL && got == sentinel);
      DP_CHECK (dp_wlan_params_why (0, 0, 80, 1) == NULL);
      dp_wlan_destroy (ok);
    }

    /* a pair with no SIGNAL RATE bits cannot be sent: capacity 0 and the rate
       is named, whatever else is wrong */
    {
      dp_wlan_state_t *x = dp_wlan_create (0, 1, 0, 1); /* BPSK 2/3 */
      DP_REQUIRE (x != NULL && x->rate_mbps == 0);
      DP_CHECK (dp_wlan_ppdu_why_max_out (x, 100) == 0);
      uint8_t octets[4]     = { 0 };
      float _Complex out[1] = { 0 };
      const char *why       = NULL;
      DP_CHECK (dp_wlan_ppdu_why (x, octets, 4, 93, out, 1, &why) == -1);
      DP_CHECK (why != NULL && strstr (why, "no SIGNAL RATE bits") != NULL);
      why = NULL;
      DP_CHECK (dp_wlan_ppdu_why (x, octets, 4, 0, out, 1, &why) == -1);
      DP_CHECK (why != NULL && strstr (why, "no SIGNAL RATE bits") != NULL);
      dp_wlan_destroy (x);
    }

    /* the PSDU length edges at M = 1: 1 octet, and 4095 at 6 Mb/s, which is
       WLAN_N_SYM_MAX symbols, the layout's largest PPDU */
    {
      dp_wlan_state_t *x = dp_wlan_create (0, 0, 0, 1); /* BPSK 1/2 */
      DP_REQUIRE (x != NULL);
      static const size_t edges[] = { 1, 4095 };
      for (size_t i = 0; i < 2; i++)
        {
          const size_t    n_sym = dp_wlan_n_sym (x, edges[i]);
          const size_t    cap   = dp_wlan_ppdu_why_max_out (x, edges[i]);
          uint8_t        *oct   = calloc (edges[i], 1);
          float _Complex *out   = malloc (cap * sizeof *out);
          DP_REQUIRE (oct && out);
          DP_CHECK (n_sym == (i ? WLAN_N_SYM_MAX : 2)); /* 30 bits over 24 */
          DP_CHECK (cap == 160 + 160 + 80 + 80 * n_sym);
          DP_CHECK (dp_wlan_ppdu_why (x, oct, edges[i], 93, out, cap, NULL)
                    == (int64_t)cap);
          free (oct);
          free (out);
        }
      dp_wlan_destroy (x);
    }

    uint8_t psdu[100];
    for (size_t i = 0; i < 100; i++)
      psdu[i] = (uint8_t)(i * 37 + 11);
    for (uint32_t m = 1; m <= 2; m++)
      {
        dp_wlan_state_t *w = dp_wlan_create (2, 2, 4 * m, m);
        DP_REQUIRE (w != NULL);
        const dp_wlan_rate_t r   = { 4, WLAN_CODE_3_4 };
        const size_t         cap = dp_wlan_ppdu_why_max_out (w, 100);
        DP_CHECK (cap == dp_wlan_ppdu_len (6, 4 * m, m));
        float _Complex *a = malloc (cap * sizeof *a);
        float _Complex *b = malloc (cap * sizeof *b);
        DP_REQUIRE (a != NULL && b != NULL);
        const char *why = "untouched";
        DP_CHECK (dp_wlan_ppdu_why (w, psdu, 100, 93, a, cap, &why)
                  == (int64_t)cap);
        DP_CHECK (strcmp (why, "untouched") == 0); /* set only on refusal */
        DP_CHECK (dp_wlan_ppdu (dp_wlan_tables_standard (), &r, psdu, 100, 93,
                                4 * m, m, b, cap)
                  == cap);
        DP_CHECK (memcmp (a, b, cap * sizeof *a) == 0);
        /* refusals: -1, a sentence, the output untouched */
        static const struct
        {
          size_t      n_psdu;
          uint32_t    st;
          size_t      room;
          const char *why;
        } refuse[] = {
          { 0, 93, 0, "the PSDU is 1 to 4095 octets" },
          { 4096, 93, 0, "the PSDU is 1 to 4095 octets" },
          { 100, 0, 0, "the scrambler state is 1 to 127" },
          { 100, 128, 0, "the scrambler state is 1 to 127" },
          { 100, 93, 1, "the output buffer is shorter than the PPDU" },
        };
        for (size_t i = 0; i < sizeof refuse / sizeof *refuse; i++)
          {
            for (size_t k = 0; k < cap; k++)
              a[k] = 123.0f;
            why               = NULL;
            const size_t room = refuse[i].room ? cap - 1 : cap;
            DP_CHECK (dp_wlan_ppdu_why (w, psdu, refuse[i].n_psdu,
                                        refuse[i].st, a, room, &why)
                      == -1);
            DP_CHECK (why != NULL && strcmp (why, refuse[i].why) == 0);
            int untouched = 1;
            for (size_t k = 0; k < cap; k++)
              untouched &= (crealf (a[k]) == 123.0f);
            DP_CHECK (untouched);
          }
        DP_CHECK (dp_wlan_ppdu_why_max_out (w, 0) == 0);
        DP_CHECK (dp_wlan_ppdu_why_max_out (w, 4096) == 0);
        free (a);
        free (b);
        dp_wlan_destroy (w);
      }
  }

  /* ── 10b. The scale is a configuration, and one gain covers every field ──
   * Annex I's printed time-domain samples are the unnormalized inverse
   * transform with the 1/64 convention; the library's are that transform times
   * 1/sqrt(52), so the printed samples equal the library's times
   * sqrt(52)/64. Held without any printed value: the same PPDU rendered with
   * a table whose three scales are 1/64 equals the standard-table render
   * times sqrt(52)/64, field by field. The control: a table with ONE scale
   * left at 1/sqrt(52) fails exactly that field, so the check can fail. */
  {
    const dp_wlan_tables_t *T   = dp_wlan_tables_standard ();
    dp_wlan_tables_t        t64 = *T, one_off = *T;
    t64.scale_stf = t64.scale_ltf = t64.scale_data = 1.0 / 64.0;
    one_off.scale_stf = one_off.scale_data = 1.0 / 64.0; /* ltf stays */
    static uint8_t psdu[200];
    uint32_t       seed = 777;
    for (int i = 0; i < 200; i++)
      psdu[i] = (uint8_t)dp_xs32 (&seed);
    const dp_wlan_rate_t rate  = { 4, WLAN_CODE_3_4 };
    const size_t         n_sym = dp_wlan_rate_n_sym (&rate, 200);
    const size_t         n     = 400 + 80 * n_sym;
    float _Complex      *std   = malloc (n * sizeof *std);
    float _Complex      *ann   = malloc (n * sizeof *ann);
    float _Complex      *off   = malloc (n * sizeof *off);
    DP_REQUIRE (std && ann && off);
    DP_CHECK (dp_wlan_ppdu (T, &rate, psdu, 200, 93, 0, 1, std, n) == n);
    DP_CHECK (dp_wlan_ppdu (&t64, &rate, psdu, 200, 93, 0, 1, ann, n) == n);
    DP_CHECK (dp_wlan_ppdu (&one_off, &rate, psdu, 200, 93, 0, 1, off, n)
              == n);
    const double g = sqrt (52.0) / 64.0;
    const struct
    {
      const char *name;
      size_t      first, len;
    } fld[4] = { { "L-STF", 0, 160 },
                 { "L-LTF", 160, 160 },
                 { "SIGNAL", 320, 80 },
                 { "Data", 400, 80 * n_sym } };
    for (int k = 0; k < 4; k++)
      {
        DP_CHECK_MSG (worst_rel (ann, std, g, fld[k].first, fld[k].len) < 1e-5,
                      fld[k].name);
        /* the control: only the L-LTF's scale differs, and only it fails */
        const int differs
            = worst_rel (off, std, g, fld[k].first, fld[k].len) > 1e-3;
        DP_CHECK_MSG (differs == (k == 1),
                      "a table with one scale changed fails that field alone");
      }
    free (std);
    free (ann);
    free (off);
  }

  /* ── 11. The scrambler seed is a linear map of the prefix ───────────────
   * dp_wlan_scrambler_sequence() reaches the pn seed by XORing seven
   * literals. Pinned for all 127 states against the search it replaced:
   * the seed whose first seven chips are the state's prefix. */
  for (unsigned st = 1; st < 128; st++)
    {
      uint8_t got[70], want[70];
      DP_CHECK (dp_wlan_scrambler_sequence ((uint8_t)st, 70, got) == 70);
      int found = 0;
      for (uint64_t sd = 1; sd < 128 && !found; sd++)
        {
          dp_pn_state_t *p = dp_pn_create (0x48u, sd, 7, PN_GALOIS);
          dp_pn_generate (p, 70, want, 70);
          dp_pn_destroy (p);
          found = memcmp (got, want, 70) == 0;
        }
      DP_CHECK (found);
      /* the 127 states give 127 distinct prefixes: the map is a bijection */
      for (unsigned o = 1; o < st; o++)
        {
          uint8_t other[7];
          dp_wlan_scrambler_sequence ((uint8_t)o, 7, other);
          DP_CHECK (memcmp (got, other, 7) != 0);
        }
    }

  DP_TEST_END ("test_wlan_core");
}
