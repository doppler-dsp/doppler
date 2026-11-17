#include "doppler/carrier_acq/carrier_acq_core.h"
#include "doppler/carrier_acq/carrier_acq_private.h"
#include "doppler/dp_complex.h"
#include "doppler/psd/psd_core.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_stats_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* BPSK NRZ data (+-1, held sps samples/symbol) * a complex tone at
 * tone_hz, sampled at sample_rate_hz -- deterministic given seed. */
static float _Complex *
_make_signal (size_t n_symbols, size_t sps, double sample_rate_hz,
              double tone_hz, uint32_t seed, size_t *out_len)
{
  size_t          n  = n_symbols * sps;
  float _Complex *x  = malloc (n * sizeof (float _Complex));
  uint32_t        st = seed;
  for (size_t sym = 0; sym < n_symbols; sym++)
    {
      float bit = (dp_xs32 (&st) & 1u) ? 1.0f : -1.0f;
      for (size_t j = 0; j < sps; j++)
        {
          size_t i     = sym * sps + j;
          double phase = 2.0 * M_PI * tone_hz * (double)i / sample_rate_hz;
          x[i]         = bit * (cosf ((float)phase) + I * sinf ((float)phase));
        }
    }
  *out_len = n;
  return x;
}

static float _Complex *
_make_noise (size_t n, uint32_t seed)
{
  float _Complex *noise = malloc (n * sizeof (float _Complex));
  uint32_t        st    = seed;
  for (size_t i = 0; i < n; i++)
    {
      float a  = ((float)(dp_xs32 (&st) % 2001u) - 1000.0f) / 100000.0f;
      float b  = ((float)(dp_xs32 (&st) % 2001u) - 1000.0f) / 100000.0f;
      noise[i] = a + I * b;
    }
  return noise;
}

static const double SAMPLE_RATE_HZ = 8000.0;
static const double SYMBOL_RATE_HZ = 1000.0;
static const double TONE_HZ        = 123.0;
#define SPS 8
#define N_SYMBOLS 4000
#define MAX_N_BLOCKS 100000

/* A tone's template on the receiver's refine geometry (async-dsss-receiver
 * §4.7): 12 kSa/s, 64-point blocks zero-padded 8 times, the spectrum the
 * CA's own PSD kernel makes of one block of a constant. */
#define TONE_FS 12000.0
#define TONE_N 64u
#define TONE_PAD 8u
static dp_carrier_acq_state_t *
_tone_ca (double pfa, size_t dwell)
{
  /* NULL on any failure: the caller requires it. */
  dp_psd_kernel_t k;
  if (dp_psd_kernel_init (&k, TONE_N, DP_PSD_WINDOW_HANN, 0.0f, TONE_PAD, 1.0)
      != DP_OK)
    return NULL;
  float _Complex one[TONE_N];
  for (size_t i = 0; i < TONE_N; i++)
    one[i] = 1.0f;
  float *t = malloc (k.nfft * sizeof *t);
  if (!t)
    {
      dp_psd_kernel_free (&k);
      return NULL;
    }
  dp_psd_frame_power (&k, one, t);
  dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
      TONE_FS, 3000.0, TONE_FS / (double)TONE_N, TONE_PAD, DP_PSD_WINDOW_HANN,
      0.0f, t, k.nfft, pfa, 0.9, 1.0, false, 100);
  if (ca)
    ca->dwell_target = dwell;
  dp_psd_kernel_free (&k);
  free (t);
  return ca;
}

/* The derived threshold's realized false-alarm rate (async-dsss-receiver
 * §4.7): noise-only dwells of the squared stream (a complex Gaussian
 * squared, what the receiver's squared estimator sees with no signal),
 * 16 blocks each, through the built estimator at pfa 1e-2. The derivation
 * is conservative by construction (0.3-0.9 of the target in the design's
 * Monte Carlo), so the count's whole Wilson interval must lie at or under
 * the target, and its top must not collapse toward nothing, which is what
 * the fitted KAPPA rule does here (about 1e-4 at this pfa). 20000 dwells,
 * enough that the rule's realized 0.55% and an anticonservative 1.2% have
 * intervals on opposite sides of 1%. A probability is stated with its
 * interval: printed whatever the verdict. Sabotage: KAPPA in place of the
 * derived rule -> the top falls under pfa / 4; the median's spread dropped
 * (1.2% realized) -> the top rises past pfa. */
static int
_test_derived_threshold_false_alarms (void)
{
  const double            pfa   = 1e-2;
  const size_t            dwell = 16, trials = 20000, n = dwell * TONE_N;
  dp_carrier_acq_state_t *ca = _tone_ca (pfa, dwell);
  DP_REQUIRE (ca != NULL);
  dp__carrier_acq_derive_threshold (ca);
  float _Complex *x  = malloc (n * sizeof *x);
  uint32_t        st = 0x5eed1234u;
  DP_REQUIRE (x != NULL);
  size_t fired = 0;
  for (size_t tr = 0; tr < trials; tr++)
    {
      for (size_t i = 0; i < n; i++)
        {
          const float _Complex z = dp_cgauss (&st);
          x[i]                   = z * z;
        }
      dp_carrier_acq_reset (ca);
      dp_carrier_acq_steps (ca, x, n);
      DP_REQUIRE (ca->n_blocks == dwell);
      fired += ca->ready;
    }
  double lo, hi; /* lo is printed: the interval, not a point */
  dp_wilson ((double)fired, (double)trials, &lo, &hi);
  printf ("derived threshold: %zu of %zu noise-only dwells fired, "
          "[%.4f, %.4f] at pfa %g (eta %.3f)\n",
          fired, trials, lo, hi, pfa,
          dp__carrier_acq_derived_threshold (ca, dwell));
  DP_CHECK_MSG (hi <= pfa, "the derived threshold holds its false-alarm "
                           "rate at or under the target");
  DP_CHECK_MSG (hi >= pfa / 4.0, "and is not the fitted rule's near-zero "
                                 "one");
  free (x);
  dp_carrier_acq_destroy (ca);
  return 0;
}

/* The rule travels (#2022 class): a blob from the derived estimator lands
 * on the derived rule in a target built with the default one, and a
 * flag that is neither rule is refused, the target untouched. Sabotage:
 * set_state leaving thr_derived alone -> the target stays on KAPPA. */
static int
_test_derived_threshold_round_trip (void)
{
  dp_carrier_acq_state_t *a = _tone_ca (1e-3, 16), *b = _tone_ca (1e-3, 16);
  DP_REQUIRE (a != NULL && b != NULL);
  dp__carrier_acq_derive_threshold (a);
  DP_REQUIRE (a->thr_derived == 1 && b->thr_derived == 0);
  const size_t   nb   = dp_carrier_acq_state_bytes (a);
  unsigned char *blob = malloc (nb), *bad = malloc (nb);
  DP_REQUIRE (blob && bad && nb == dp_carrier_acq_state_bytes (b));
  dp_carrier_acq_get_state (a, blob);
  memcpy (bad, blob, nb);
  /* The flag's byte: the blob's one difference from b's own. */
  dp_carrier_acq_state_t *c  = _tone_ca (1e-3, 16);
  unsigned char          *cb = malloc (nb);
  DP_REQUIRE (c != NULL && cb != NULL);
  dp_carrier_acq_get_state (c, cb);
  size_t at = SIZE_MAX, diffs = 0;
  for (size_t i = 0; i < nb; i++)
    if (blob[i] != cb[i])
      {
        at = i;
        diffs++;
      }
  DP_REQUIRE_MSG (diffs == 1, "the rule is one byte of the blob");
  bad[at] = 2;
  DP_CHECK (dp_carrier_acq_set_state (b, bad) == DP_ERR_INVALID);
  DP_CHECK_MSG (b->thr_derived == 0, "a refused blob leaves the rule");
  DP_CHECK (dp_carrier_acq_set_state (b, blob) == DP_OK);
  DP_CHECK_MSG (b->thr_derived == 1,
                "a restored estimator tests against the derived rule");
  free (cb);
  free (bad);
  free (blob);
  dp_carrier_acq_destroy (a);
  dp_carrier_acq_destroy (b);
  dp_carrier_acq_destroy (c);
  return 0;
}

/* The telemetry pairs each decision with the statistic it thresholds,
 * per tested block: five records a block in sequential mode, ".ready" 0
 * while ".stat" stays at or under ".thr", and 1 on the first block where
 * it is above -- the block the estimator decides on -- whose ".hz" is the
 * decision's residual_hz and ".n" its n_blocks. A threshold or statistic
 * published from anywhere but the decision breaks the pairing. The level
 * (0.4 against unit-power noise, pfa 1e-6) decides after 46 blocks; the
 * estimate itself is noise-limited there and is not what this pins. */
#define AMP 0.4f
static int
_test_telemetry_pairs_decision_with_statistic (void)
{
  size_t          n;
  float _Complex *x
      = _make_signal (N_SYMBOLS, SPS, SAMPLE_RATE_HZ, TONE_HZ, 777u, &n);
  uint32_t st = 99u;
  for (size_t i = 0; i < n; i++) /* weak enough to take several blocks */
    x[i] = AMP * x[i] + (float _Complex)dp_cgauss (&st);
  dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
      SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
      0, 1e-6, 0.9, 2.0, /*sequential=*/true, MAX_N_BLOCKS);
  dp_tlm_t *t = dp_tlm_create (1u << 16);
  DP_REQUIRE (ca != NULL && t != NULL);
  DP_REQUIRE (dp_carrier_acq_set_telemetry (ca, t, "ca", 1) == DP_OK);
  DP_CHECK (dp_carrier_acq_set_telemetry (ca, t, "ca", 0) == DP_ERR_INVALID);
  dp_carrier_acq_steps (ca, x, n);
  DP_REQUIRE (ca->ready);
  DP_REQUIRE (ca->n_blocks > 2); /* the pairing is seen over several */

  const int id_stat  = dp_tlm_probe_id (t, "ca.stat");
  const int id_thr   = dp_tlm_probe_id (t, "ca.thr");
  const int id_hz    = dp_tlm_probe_id (t, "ca.hz");
  const int id_n     = dp_tlm_probe_id (t, "ca.n");
  const int id_ready = dp_tlm_probe_id (t, "ca.ready");
  DP_REQUIRE (id_stat >= 0 && id_thr >= 0 && id_hz >= 0 && id_n >= 0
              && id_ready >= 0);
  const size_t  cap = dp_tlm_read_max_out (t);
  dp_tlm_rec_t *r   = malloc ((cap ? cap : 1) * sizeof *r);
  DP_REQUIRE (r != NULL);
  const size_t got = dp_tlm_read (t, 0, r, cap);
  DP_CHECK (got == 5 * ca->n_blocks); /* every block tested */
  size_t blocks = 0, decided_at = 0;
  for (size_t i = 0; i + 5 <= got; i += 5)
    {
      DP_REQUIRE (r[i].probe == id_stat && r[i + 1].probe == id_thr
                  && r[i + 2].probe == id_hz && r[i + 3].probe == id_n
                  && r[i + 4].probe == id_ready);
      blocks++;
      DP_CHECK ((size_t)r[i + 3].value == blocks);
      const int above = r[i].value > r[i + 1].value;
      DP_CHECK ((int)r[i + 4].value == above);
      if (above && !decided_at)
        {
          decided_at = blocks;
          DP_CHECK (fabs (r[i + 2].value - ca->residual_hz) < 1e-3);
        }
    }
  DP_CHECK (decided_at == ca->n_blocks);
  printf ("  telemetry: %zu tested blocks, decided at %zu, %.1f Hz\n", blocks,
          decided_at, ca->residual_hz);
  free (r);
  free (x);
  dp_carrier_acq_destroy (ca);
  dp_tlm_destroy (t);
  return 0;
}

/* A give-up is not a detection: a dwell that reaches its cap without
 * crossing the threshold leaves ready False, n_blocks at the cap, and
 * further steps() a no-op -- what the `ready` property says. Noise only,
 * at the fixed dwell of 4 blocks. */
static int
_test_give_up_leaves_ready_false (void)
{
  dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
      SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
      0, 1e-6, 0.9, 2.0, /*sequential=*/false, MAX_N_BLOCKS);
  DP_REQUIRE (ca != NULL);
  ca->dwell_target   = 4;
  const size_t    n  = 64 * ca->psd->k.n;
  float _Complex *x  = malloc (n * sizeof *x);
  uint32_t        st = 4242u;
  DP_REQUIRE (x != NULL);
  for (size_t i = 0; i < n; i++)
    x[i] = dp_cgauss (&st);
  dp_carrier_acq_steps (ca, x, n);
  DP_CHECK (ca->n_blocks == ca->dwell_target);
  DP_CHECK (!ca->ready);
  DP_CHECK (ca->residual_hz == 0.0);
  dp_carrier_acq_steps (ca, x, n); /* a no-op now */
  DP_CHECK (ca->n_blocks == ca->dwell_target && !ca->ready);
  free (x);
  dp_carrier_acq_destroy (ca);
  return 0;
}

int
main (void)
{
  DP_CHECK (_test_give_up_leaves_ready_false () == 0);
  DP_CHECK (_test_telemetry_pairs_decision_with_statistic () == 0);
  DP_CHECK (_test_derived_threshold_false_alarms () == 0);
  DP_CHECK (_test_derived_threshold_round_trip () == 0);
  /* ── sequential=true: fires early, accurate residual_hz ── */
  {
    size_t          n;
    float _Complex *x
        = _make_signal (N_SYMBOLS, SPS, SAMPLE_RATE_HZ, TONE_HZ, 12345u, &n);
    dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, /*sequential=*/true, MAX_N_BLOCKS);
    DP_CHECK (ca != NULL);
    if (ca)
      {
        dp_carrier_acq_steps (ca, x, n);
        DP_CHECK (ca->ready);
        if (ca->ready)
          DP_CHECK (fabs (ca->residual_hz - TONE_HZ) < 5.0);
        DP_CHECK (ca->n_blocks <= ca->max_n_blocks);
        dp_carrier_acq_destroy (ca);
      }
    free (x);
  }

  /* ── sequential=false: waits the full fixed dwell, same accuracy ── */
  {
    size_t          n;
    float _Complex *x
        = _make_signal (N_SYMBOLS, SPS, SAMPLE_RATE_HZ, TONE_HZ, 12345u, &n);
    dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, /*sequential=*/false, MAX_N_BLOCKS);
    DP_CHECK (ca != NULL);
    if (ca)
      {
        dp_carrier_acq_steps (ca, x, n);
        DP_CHECK (ca->ready);
        if (ca->ready)
          {
            DP_CHECK (fabs (ca->residual_hz - TONE_HZ) < 5.0);
            DP_CHECK (ca->n_blocks == ca->dwell_target);
          }
        dp_carrier_acq_destroy (ca);
      }
    free (x);
  }

  /* ── carry-buffer split-call: tiny, non-block-aligned chunks give the
     same result as one big call ── */
  {
    size_t          n;
    float _Complex *x
        = _make_signal (N_SYMBOLS, SPS, SAMPLE_RATE_HZ, TONE_HZ, 12345u, &n);
    dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, true, MAX_N_BLOCKS);
    DP_CHECK (ca != NULL);
    if (ca)
      {
        const size_t chunk = 3; /* not a multiple of n_fft or sps */
        for (size_t off = 0; off < n && !ca->ready; off += chunk)
          {
            size_t take = (n - off < chunk) ? n - off : chunk;
            dp_carrier_acq_steps (ca, x + off, take);
          }
        DP_CHECK (ca->ready);
        if (ca->ready)
          DP_CHECK (fabs (ca->residual_hz - TONE_HZ) < 5.0);
        dp_carrier_acq_destroy (ca);
      }
    free (x);
  }

  /* ── pure noise, non-sequential mode: never confidently detects, gives
     up at dwell_target (the fixed, design_snr-driven wait point) ── */
  {
    dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, /*sequential=*/false, MAX_N_BLOCKS);
    DP_CHECK (ca != NULL);
    if (ca)
      {
        size_t nfft_frame
            = (size_t)llround (SAMPLE_RATE_HZ / (SYMBOL_RATE_HZ / 10.0));
        size_t          n     = (ca->dwell_target + 2) * nfft_frame;
        float _Complex *noise = _make_noise (n, 999u);
        dp_carrier_acq_steps (ca, noise, n);
        DP_CHECK (!ca->ready);
        DP_CHECK (ca->n_blocks == ca->dwell_target);
        free (noise);
        dp_carrier_acq_destroy (ca);
      }
  }

  /* ── pure noise, sequential mode: tests EVERY block (not bounded by
     dwell_target, an optimistic design_snr point estimate that can be
     as small as 1) -- gives up only at its OWN, separate max_n_blocks
     cap. A small explicit max_n_blocks keeps this fast. ── */
  {
    const size_t            small_cap = 5;
    dp_carrier_acq_state_t *ca        = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, /*sequential=*/true, small_cap);
    DP_CHECK (ca != NULL);
    if (ca)
      {
        DP_CHECK (ca->max_n_blocks == small_cap);
        size_t nfft_frame
            = (size_t)llround (SAMPLE_RATE_HZ / (SYMBOL_RATE_HZ / 10.0));
        size_t          n     = (small_cap + 2) * nfft_frame;
        float _Complex *noise = _make_noise (n, 999u);
        dp_carrier_acq_steps (ca, noise, n);
        DP_CHECK (!ca->ready);
        DP_CHECK (ca->n_blocks == small_cap);
        free (noise);
        dp_carrier_acq_destroy (ca);
      }
  }

  /* ── template override: a caller-supplied shape is used verbatim,
     runs to completion without crashing ── */
  {
    dp_carrier_acq_state_t *probe = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, true, MAX_N_BLOCKS);
    DP_CHECK (probe != NULL);
    if (probe)
      {
        size_t nfft = probe->nfft;
        dp_carrier_acq_destroy (probe);

        float *tmpl = malloc (nfft * sizeof (float));
        /* A single sharp spike at DC (bin nfft/2, DC-centred) instead of
           the default sinc^2 -- a deliberately DIFFERENT known shape. */
        for (size_t k = 0; k < nfft; k++)
          tmpl[k] = (k == nfft / 2) ? 1.0f : 0.0f;

        size_t          n;
        float _Complex *x = _make_signal (N_SYMBOLS, SPS, SAMPLE_RATE_HZ,
                                          TONE_HZ, 12345u, &n);
        dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
            SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f,
            tmpl, nfft, 1e-3, 0.9, 2.0, true, MAX_N_BLOCKS);
        DP_CHECK (ca != NULL);
        if (ca)
          {
            DP_CHECK (ca->nfft == nfft);
            dp_carrier_acq_steps (ca, x, n);
            DP_CHECK (ca->n_blocks > 0);
            dp_carrier_acq_destroy (ca);
          }
        free (x);
        free (tmpl);
      }
  }

  /* ── the blob is a function of the object (#2076): only carry_len of the
     carry buffer's psd->k.n samples are state, so get_state writes zeros
     after them, whatever an earlier block left in the buffer. A carry of
     n - 1 samples, then two more (one closes the block, one starts a new
     carry), leaves n - 1 real samples (carry_buf[1..n-1]) in the tail to
     leak: proven by sabotage, writing the whole buffer again turns this red.
     The blob is poisoned first, so dropping only the zeroing goes red too,
     not just on a heap that happens to hand back zeros. ── */
  {
    dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, true, MAX_N_BLOCKS);
    DP_CHECK (ca != NULL);
    if (ca)
      {
        const size_t    n     = ca->psd->k.n;
        float _Complex *noise = _make_noise (n + 1, 2076u);
        dp_carrier_acq_steps (ca, noise, n - 1);
        dp_carrier_acq_steps (ca, noise + (n - 1), 2);
        DP_CHECK (ca->carry_len == 1);
        const size_t   bytes = dp_carrier_acq_state_bytes (ca);
        unsigned char *blob  = malloc (bytes);
        memset (blob, 0xA5, bytes);
        dp_carrier_acq_get_state (ca, blob);
        /* The carry region is the blob's last n samples. */
        const unsigned char *carry
            = blob + bytes - n * sizeof (float _Complex);
        DP_CHECK (memcmp (carry, ca->carry_buf, sizeof (float _Complex)) == 0);
        size_t nonzero = 0;
        for (size_t i = sizeof (float _Complex);
             i < n * sizeof (float _Complex); i++)
          nonzero += carry[i] != 0;
        DP_CHECK (nonzero == 0);
        free (blob);
        free (noise);
        dp_carrier_acq_destroy (ca);
      }
  }

  /* ── invalid args rejected ── */
  DP_CHECK (dp_carrier_acq_create (0.0, 0.0, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f,
                                   NULL, 0, 1e-3, 0.9, 2.0, true, MAX_N_BLOCKS)
            == NULL);
  /* the rectangular window, which PSD takes but carrier_acq never offered,
     is refused, and the three offered windows build (#2185) */
  DP_CHECK (dp_carrier_acq_create (SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4,
                                   DP_PSD_WINDOW_RECT, 0.0f, NULL, 0, 1e-3,
                                   0.9, 2.0, true, MAX_N_BLOCKS)
            == NULL);
  for (int win = DP_PSD_WINDOW_HANN; win <= DP_PSD_WINDOW_BLACKMAN_HARRIS;
       win++)
    {
      dp_carrier_acq_state_t *ok = dp_carrier_acq_create (
          SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, win, 6.0f, NULL, 0, 1e-3,
          0.9, 2.0, true, MAX_N_BLOCKS);
      DP_CHECK (ok != NULL);
      dp_carrier_acq_destroy (ok);
    }

  /* ── a coarse resolution still builds: the block is floored at 3 ──
   * fs/resolution of 2.22, 1 and 0.25 asked for blocks of 2, 1 and 0.  The
   * floor was 2, whose Hann (the default) was then the symmetric [0, 0]:
   * PSD refused that window, so create returned NULL, and
   * AsyncDsssReceiver, which wraps this create in dp_xnn with refine_n_fft
   * as the block, aborted (#1959).  PSD's Hann is periodic now, [0, 1] at
   * 2 (#2053); at the floor of 3 every window has gain, and the averaged
   * spectrum is a real estimate -- finite, above the -200 dB floor. */
  {
    const double res_hz[]
        = { 0.45 * SAMPLE_RATE_HZ, SAMPLE_RATE_HZ, 4.0 * SAMPLE_RATE_HZ };
    size_t          n;
    float _Complex *x
        = _make_signal (64, SPS, SAMPLE_RATE_HZ, TONE_HZ, 777u, &n);
    for (size_t r = 0; r < sizeof res_hz / sizeof res_hz[0]; r++)
      for (int win = DP_PSD_WINDOW_HANN; win <= DP_PSD_WINDOW_BLACKMAN_HARRIS;
           win++)
        {
          dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
              SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, res_hz[r], 4, win, 6.0f, NULL, 0,
              1e-3, 0.9, 2.0, false, 16);
          DP_CHECK (ca != NULL);
          if (!ca)
            continue;
          DP_CHECK (ca->psd->k.n == 3);
          dp_carrier_acq_steps (ca, x, n);
          float        db[16];
          const size_t got = dp_psd_psd_db (ca->psd, db, ca->psd->k.nfft);
          DP_CHECK (got == ca->psd->k.nfft);
          float peak = -1e30f;
          for (size_t i = 0; i < got; i++)
            {
              DP_CHECK (isfinite (db[i]));
              if (db[i] > peak)
                peak = db[i];
            }
          DP_CHECK (peak > -150.0f);
          dp_carrier_acq_destroy (ca);
        }
    free (x);
  }

  /* ── state roundtrip + envelope reject; resumed instance continues
     bit-for-bit identically to the un-interrupted one ── */
  {
    size_t          n;
    float _Complex *x
        = _make_signal (N_SYMBOLS, SPS, SAMPLE_RATE_HZ, TONE_HZ, 777u, &n);
    dp_carrier_acq_state_t *a = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, true, MAX_N_BLOCKS);
    dp_carrier_acq_state_t *b = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, true, MAX_N_BLOCKS);
    DP_CHECK (a != NULL && b != NULL);
    if (a && b)
      {
        size_t half = n / 2 + 3; /* mid-stream, not block-aligned */
        if (half > n)
          half = n;
        dp_carrier_acq_steps (a, x, half);
        DP_STATE_ROUNDTRIP_TEST (dp_carrier_acq, a, b);
        DP_CHECK (b->n_blocks == a->n_blocks);
        DP_CHECK (b->ready == a->ready);

        dp_carrier_acq_steps (a, x + half, n - half);
        dp_carrier_acq_steps (b, x + half, n - half);
        DP_CHECK (a->ready == b->ready);
        DP_CHECK (a->n_blocks == b->n_blocks);
        if (a->ready && b->ready)
          DP_CHECK (fabs (a->residual_hz - b->residual_hz) < 1e-9);
      }
    if (a)
      dp_carrier_acq_destroy (a);
    if (b)
      dp_carrier_acq_destroy (b);
    free (x);
  }

  /* ── state-serialization round trip (mid-dwell split) + envelope reject +
     reset ── the standard's "resume bit-for-bit" contract for a stateful
     object; also exercises dp_carrier_acq_reset(). ── */
  {
    size_t          n;
    float _Complex *x
        = _make_signal (N_SYMBOLS, SPS, SAMPLE_RATE_HZ, TONE_HZ, 999u, &n);
    dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, /*sequential=*/true, MAX_N_BLOCKS);
    DP_CHECK (ca != NULL);
    if (ca)
      {
        size_t half = n / 2;
        dp_carrier_acq_steps (ca, x, half);

        size_t nb   = dp_carrier_acq_state_bytes (ca);
        void  *blob = malloc (nb);
        dp_carrier_acq_get_state (ca, blob);

        dp_carrier_acq_state_t *cr = dp_carrier_acq_create (
            SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f,
            NULL, 0, 1e-3, 0.9, 2.0, /*sequential=*/true, MAX_N_BLOCKS);
        DP_CHECK (cr != NULL);
        if (cr)
          {
            DP_CHECK (dp_carrier_acq_set_state (cr, blob) == DP_OK);
            dp_carrier_acq_steps (ca, x + half, n - half);
            dp_carrier_acq_steps (cr, x + half, n - half);
            DP_CHECK (ca->ready == cr->ready);
            DP_CHECK (ca->n_blocks == cr->n_blocks);
            if (ca->ready && cr->ready)
              DP_CHECK (fabs (ca->residual_hz - cr->residual_hz) < 1e-9);

            /* a corrupted envelope must be rejected, not reinterpreted */
            ((char *)blob)[0] ^= (char)0xFF;
            DP_CHECK (dp_carrier_acq_set_state (cr, blob) == DP_ERR_INVALID);
            dp_carrier_acq_destroy (cr);
          }
        free (blob);
        dp_carrier_acq_reset (ca);
        dp_carrier_acq_destroy (ca);
      }
    free (x);
  }

  /* ── a carry is 0..n-1 samples, so set_state refuses a blob claiming
     carry_len == n: steps never leaves one, and get_state never writes it.
     The field is the extra header's last (nfft, dwell_target, max_n_blocks,
     n_blocks, ready + 7 pad, residual_hz, carry_len), and its offset is
     checked against the live value before the blob is patched. ── */
  {
    dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, true, MAX_N_BLOCKS);
    dp_carrier_acq_state_t *cr = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, true, MAX_N_BLOCKS);
    DP_CHECK (ca != NULL && cr != NULL);
    if (ca && cr)
      {
        const size_t    n     = ca->psd->k.n;
        float _Complex *noise = _make_noise (n, 2077u);
        dp_carrier_acq_steps (ca, noise, n - 1);
        DP_CHECK (ca->carry_len == n - 1);
        const size_t   bytes = dp_carrier_acq_state_bytes (ca);
        unsigned char *blob  = malloc (bytes);
        dp_carrier_acq_get_state (ca, blob);
        const size_t at = sizeof (dp_state_hdr_t) + 6 * sizeof (uint64_t);
        uint64_t     v;
        memcpy (&v, blob + at, sizeof v);
        DP_CHECK (v == n - 1);
        DP_CHECK (dp_carrier_acq_set_state (cr, blob) == DP_OK);
        v = n;
        memcpy (blob + at, &v, sizeof v);
        DP_CHECK (dp_carrier_acq_set_state (cr, blob) == DP_ERR_INVALID);
        free (blob);
        free (noise);
      }
    dp_carrier_acq_destroy (ca);
    dp_carrier_acq_destroy (cr);
  }

  /* ── a refused detector child changes nothing, the PSD included ──────
     'CAQR' nests the PSD's blob and then the 1-D detector's 'CORL'. A
     restore that wrote the PSD before reading the detector's child left a
     PSD restored behind a refusal; both children are now checked first
     (#2752 step 3.3). The detector's child is forged at its magic, and the
     target, fed other noise, must read back exactly as it was. ── */
  {
    dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, true, MAX_N_BLOCKS);
    dp_carrier_acq_state_t *cr = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, true, MAX_N_BLOCKS);
    DP_REQUIRE (ca != NULL && cr != NULL);
    const size_t    n  = ca->psd->k.n;
    float _Complex *na = _make_noise (3 * n + 5, 3301u);
    float _Complex *nb = _make_noise (2 * n + 9, 3302u);
    dp_carrier_acq_steps (ca, na, 3 * n + 5);
    dp_carrier_acq_steps (cr, nb, 2 * n + 9);
    const size_t   bytes = dp_carrier_acq_state_bytes (ca);
    unsigned char *blob = malloc (bytes), *pre = malloc (bytes),
                  *post = malloc (bytes);
    DP_REQUIRE (blob && pre && post);
    dp_carrier_acq_get_state (ca, blob);
    dp_carrier_acq_get_state (cr, pre);
    DP_REQUIRE (memcmp (blob, pre, bytes) != 0); /* the two differ */
    /* The extra header is seven 8-byte fields; the PSD's child follows. */
    const size_t det = sizeof (dp_state_hdr_t) + 7 * sizeof (uint64_t)
                       + dp_psd_state_bytes (ca->psd);
    DP_REQUIRE (det + sizeof (dp_state_hdr_t) < bytes);
    blob[det] ^= 0x5A;
    DP_CHECK (dp_carrier_acq_set_state (cr, blob) == DP_ERR_INVALID);
    dp_carrier_acq_get_state (cr, post);
    DP_CHECK (memcmp (pre, post, bytes) == 0);
    blob[det] ^= 0x5A; /* the positive control: unforged, it loads */
    DP_CHECK (dp_carrier_acq_set_state (cr, blob) == DP_OK);
    free (blob);
    free (pre);
    free (post);
    free (na);
    free (nb);
    dp_carrier_acq_destroy (ca);
    dp_carrier_acq_destroy (cr);
  }

  /* ── a version-2 AccTrace blob, nested in its PSD, is refused (#2094) ──
     The trace's mean is a sum in version 3, so a version-2 blob is refused
     through the PSD, and atomically: the PSD is the first thing restored and
     the acquisition's own fields come last, so the refusal leaves it exactly
     as advanced. It is judged before the first block completes (once
     `ready`, steps ignores its input), so the advance is the carry. */
  {
    size_t          n;
    float _Complex *x
        = _make_signal (N_SYMBOLS, SPS, SAMPLE_RATE_HZ, TONE_HZ, 2094u, &n);
    dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, DP_PSD_WINDOW_HANN, 0.0f, NULL,
        0, 1e-3, 0.9, 2.0, /*sequential=*/false, MAX_N_BLOCKS);
    DP_REQUIRE (ca != NULL && x != NULL);
    dp_carrier_acq_steps (ca, x, 5);
    DP_STATE_NESTED_VERSION_TEST (dp_carrier_acq, ca,
                                  dp_carrier_acq_steps (ca, x + 5, 7),
                                  ACC_TRACE_STATE_MAGIC, 2u, 1, 1);
    dp_carrier_acq_destroy (ca);
    free (x);
  }

  DP_TEST_END ("test_carrier_acq_core");
}
