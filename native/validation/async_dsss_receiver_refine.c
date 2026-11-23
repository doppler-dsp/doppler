/**
 * @file async_dsss_receiver_refine.c
 * @brief The refine at the 415.1-B-1 point: how often it decides on noise,
 *        and what it hands over.
 *
 * The refine (docs/design/async-dsss-receiver.md §4.7) runs one
 * CarrierAcquisition estimator, the data template's, on the despread
 * stream of a seed. The hand-over takes its estimate when it decided, and
 * the seed's own Doppler when it gave up. At the use case's configuration
 * the refine is NOT sequential (refine_sequential false, the binding's
 * default for the receiver and the pool): the estimator tests ONCE, at its
 * dwell target, so the design's per-dwell false-alarm rate is its pfa.
 *
 * Stimulus: the use-case scene, native/tests/dp_air_test.h, at the 415.1
 * point (11 MSa/s, 3,000 sym/s, the 275-in-1,000 code-only window), C/N0
 * at the antenna. Receiver: the specification's configuration, seeded by
 * hand (dp_async_dsss_receiver_seed()) as the search would seed it.
 *
 * NOISE. The scene with no emitter, the noise at the design C/N0's
 * reference. Each dwell: a reset, a seed at a drawn code phase and a drawn
 * Doppler in +/-1,500 Hz, then the receiver fed until it leaves refining.
 * Whether the estimator decided (its `ready`), and which estimate the
 * hand-over took (get_handover_source()). The rate with its Wilson 95%
 * interval against pfa.
 *
 * HAND-OVER. The emitter on the air, static or on a +/-R_env ramp
 * (dp_s415_r_env(), the residual's rate the receiver meets; the
 * sign by seed), at the floor (4 dB) and 7 dB. Each dwell is seeded on the
 * truth's code phase at that sample, as the receiver's code NCO would read
 * it (half a receive sample before the continuous phase, the dwell-start
 * convention of rx.phase.code), and on the truth's Doppler plus an offset
 * from a grid of OFFSETS across one search row; the emitters' own offsets
 * are RF_OFFSETS points evenly inside the use case's +/-1,500 Hz
 * (DP_S415_SPAN_HZ). Recorded against the
 * channel's own offset at the hand-over sample: the error of what was
 * handed over, and which estimate it took.
 *
 * Asserted, the same in the sweep and --check: every dwell leaves
 * refining (a hand-over or a give-up, never stuck); and on noise neither
 * decision's rate (the data template's, the fine check's) is shown to
 * exceed its design: each interval starts at or under pfa. Printed: the rate,
 * and the hand-over errors as median, 90th percentile and worst.
 * --check: 2.8 GHz, 300 noise dwells and the static 7 dB hand-over grid
 * at one noise seed.
 *
 * Usage:  validate_async_dsss_receiver_refine [--check] [--emit]
 *             [--dwells N]
 */
#include "async_dsss_receiver_common.h"
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "dp_air_test.h"
#include "dp_dsss_test.h"
#include "dp_s415_test.h"
#include "dp_stats_test.h"
#include "dp_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RF_PUSH 4096u
#define RF_DWELLS 2000u
#define RF_CHECK_DWELLS 300u
#define RF_OFFSETS 20u
#define RF_NOISE_SEEDS 4u
#define RF_MAX_S 3.0 /* a refine longer than this is stuck */

static dp_async_dsss_receiver_state_t *
rf_rx (const uint8_t *code, double fc)
{
  return dp_s415_rx (code, fc, true);
}

/* Feed `rx` from `air` until it leaves refining; the samples fed, or 0 if
   it was still refining after RF_MAX_S. */
static uint64_t
rf_until_decided (dp_async_dsss_receiver_state_t *rx, dp_air_t *air,
                  float complex *x, float complex *out, size_t ocap)
{
  const uint64_t max = (uint64_t)(RF_MAX_S * DP_S415_FS);
  uint64_t       fed = 0;
  while (rx->state == ASYNC_DSSS_RX_REFINING && fed < max)
    {
      DP_REQUIRE (dp_air_next (air, x, RF_PUSH));
      (void)dp_async_dsss_receiver_steps (rx, x, RF_PUSH, out, ocap);
      fed += RF_PUSH;
    }
  return rx->state == ASYNC_DSSS_RX_REFINING ? 0 : fed;
}

static void
rate_row (const char *what, size_t k, size_t n, double design, int emit)
{
  double lo, hi;
  dp_wilson ((double)k, (double)n, &lo, &hi);
  if (!emit)
    printf ("    %-34s %4zu of %zu = %.2e [%.2e, %.2e], design %.1e\n", what,
            k, n, (double)k / (double)n, lo, hi, design);
  DP_CHECK_MSG (lo <= design, "no false-alarm rate is shown to exceed its "
                              "design");
}

/* NOISE: `n` dwells at carrier `fc`. */
static int
noise_rows (const uint8_t *code, double fc, size_t n, int emit)
{
  const double cn0 = dp_s415_cn0 (DP_S415_FLOOR_DB);
  dp_air_t     air;
  DP_REQUIRE (dp_air_init (&air, NULL, 0, DP_S415_FS, cn0, 4242u, RF_PUSH));
  dp_async_dsss_receiver_state_t *rx = rf_rx (code, fc);
  DP_REQUIRE (rx != NULL);
  const double   pfa  = rx->pfa;
  const size_t   ocap = 1u << 14;
  float complex *x    = dp_xmalloc (RF_PUSH * sizeof *x);
  float complex *out  = dp_xmalloc (ocap * sizeof *out);
  uint32_t       st   = trial_rng ((uint32_t)(fc / 1e8));
  size_t         data = 0, stuck = 0;
  size_t         src[4] = { 0, 0, 0, 0 };
  if (emit)
    printf ("# noise\nfc_ghz,dwell,data_ready,ho_src,samples\n");
  for (size_t d = 0; d < n; d++)
    {
      dp_async_dsss_receiver_reset (rx);
      const double ph = 1023.0 * dp_uni (&st);
      const double dop
          = -DP_S415_SPAN_HZ + 2.0 * DP_S415_SPAN_HZ * dp_uni (&st);
      DP_REQUIRE (dp_async_dsss_receiver_seed (rx, ph, dop, cn0) == DP_OK);
      const uint64_t fed = rf_until_decided (rx, &air, x, out, ocap);
      stuck += fed == 0;
      const int dr = rx->ca->ready;
      data += (size_t)dr;
      const int hs = dp_async_dsss_receiver_get_handover_source (rx);
      if (hs >= 0 && hs < 4)
        src[hs]++;
      if (emit)
        printf ("%.1f,%zu,%d,%d,%llu\n", fc / 1e9, d, dr, hs,
                (unsigned long long)fed);
    }
  DP_CHECK_MSG (stuck == 0, "every noise dwell leaves refining");
  if (!emit)
    printf ("  noise at %.1f GHz: %zu dwells (hand-over from the seed %zu, "
            "data %zu, fine %zu)\n",
            fc / 1e9, n, src[ASYNC_DSSS_RX_HO_SEED],
            src[ASYNC_DSSS_RX_HO_DATA], src[ASYNC_DSSS_RX_HO_FINE]);
  rate_row ("data template decides", data, n, pfa, emit);
  /* The fine check is a second decision on the same dwell, at the same
     pfa (adr_fine_thr_db(): the largest of the fine sums' noise bins at
     pfa): how often it passes on noise is held to it too. */
  rate_row ("fine estimate passes its check", src[ASYNC_DSSS_RX_HO_FINE], n,
            pfa, emit);
  free (x);
  free (out);
  dp_async_dsss_receiver_destroy (rx);
  dp_air_free (&air);
  return 0;
}

static int
cmp_dbl (const void *a, const void *b)
{
  const double x = *(const double *)a, y = *(const double *)b;
  return (x > y) - (x < y);
}

static void
err_col (const char *what, double *v, size_t n)
{
  size_t m = 0;
  for (size_t i = 0; i < n; i++)
    if (!isnan (v[i]))
      v[m++] = fabs (v[i]);
  if (!m)
    {
      printf ("      %-14s none decided\n", what);
      return;
    }
  qsort (v, m, sizeof *v, cmp_dbl);
  printf ("      %-14s %3zu decided, |err| %.1f / %.1f / %.1f Hz\n", what, m,
          v[m / 2], v[(size_t)(0.9 * (double)m)], v[m - 1]);
}

/* One hand-over dwell: the emitter at the 415.1 point, the receiver seeded
   on the truth's code phase and the truth's Doppler plus `off`. Returns the
   error of what was handed over against the channel's offset at the hand-over
   sample (NAN if it stuck) and the source. */
static int
one_handover (const uint8_t *code, double fc, double cn0, double rate,
              uint32_t s, size_t o, double off_frac, double *err, int *src)
{
  const double                    chip = dp_s415_chip_rate (fc);
  const size_t                    ocap = 1u << 14;
  float complex                  *x    = dp_xmalloc (RF_PUSH * sizeof *x);
  float complex                  *out  = dp_xmalloc (ocap * sizeof *out);
  dp_async_dsss_receiver_state_t *rx   = rf_rx (code, fc);
  DP_REQUIRE (rx != NULL);
  const double     off = rx->acq->search->doppler_res_hz * off_frac;
  dp_air_emitter_t e   = dp_s415_emitter (
      code, fc,
      -DP_S415_SPAN_HZ
          + 2.0 * DP_S415_SPAN_HZ * ((double)o + 0.5) / (double)RF_OFFSETS,
      rate == 0.0 ? 0.0 : (s & 1u ? rate : -rate), cn0, 0.0, 100u + s);
  e.frame_off = (size_t)(37u * (unsigned)o + 101u * s) % DP_S415_FRAME_SYM;
  dp_air_t air;
  DP_REQUIRE (dp_air_init (&air, &e, 1, DP_S415_FS, cn0,
                           9001u + 31u * s + (uint32_t)o, RF_PUSH));
  /* A block of signal first, so the refine starts mid-run; the seed's
     code phase is the scene's next sample's, as the receiver's code NCO
     reads it (half a receive sample before the continuous phase). */
  DP_REQUIRE (dp_air_next (&air, x, RF_PUSH));
  const uint64_t       at0 = air.n_out;
  const dp_air_truth_t t0  = dp_air_truth (&air, 0, at0);
  const double         ph
      = fmod (t0.code_chips - 0.5 * chip / DP_S415_FS + 1023.0, 1023.0);
  DP_REQUIRE (dp_async_dsss_receiver_seed (rx, ph, t0.car_hz + off, cn0)
              == DP_OK);
  const uint64_t fed = rf_until_decided (rx, &air, x, out, ocap);
  const uint64_t at  = at0 + (rx->samples_in - rx->state_samples);
  *err               = fed ? dp_async_dsss_receiver_get_doppler_hz (rx)
                                 - dp_air_truth (&air, 0, at).car_hz
                           : NAN;
  *src               = dp_async_dsss_receiver_get_handover_source (rx);
  dp_air_free (&air);
  dp_async_dsss_receiver_destroy (rx);
  free (x);
  free (out);
  return 0;
}

/* HAND-OVER: the offset grid at carrier `fc`, Es/N0 `esn0`, ramp
   magnitude `rate`. */
static int
handover_rows (const uint8_t *code, double fc, double esn0, double rate,
               size_t seeds, int emit)
{
  const double cn0 = dp_s415_cn0 (esn0);
  const size_t n   = RF_OFFSETS * seeds;
  double      *eb  = dp_xmalloc (n * sizeof *eb);
  size_t       k = 0, stuck = 0, sb[4] = { 0 };
  for (uint32_t s = 1; s <= seeds; s++)
    for (size_t o = 0; o < RF_OFFSETS; o++, k++)
      {
        const double frac
            = -1.0 + 2.0 * ((double)o + 0.5) / (double)RF_OFFSETS;
        int hb = 0;
        DP_REQUIRE (one_handover (code, fc, cn0, rate, s, o, frac, &eb[k], &hb)
                    == 0);
        stuck += isnan (eb[k]);
        if (hb >= 0 && hb < 4)
          sb[hb]++;
        if (emit)
          printf ("%.1f,%.1f,%.0f,%u,%.3f,%.2f,%d\n", fc / 1e9, esn0,
                  rate == 0.0 ? 0.0 : (s & 1u ? rate : -rate), s, frac, eb[k],
                  hb);
      }
  DP_CHECK_MSG (stuck == 0, "every hand-over dwell leaves refining");
  if (!emit)
    {
      printf ("    %.1f GHz %4.1f dB %s: %zu dwells (fine %zu, data %zu, seed "
              "%zu)\n",
              fc / 1e9, esn0, rate == 0.0 ? "static" : "+/-R_env", n,
              sb[ASYNC_DSSS_RX_HO_FINE], sb[ASYNC_DSSS_RX_HO_DATA],
              sb[ASYNC_DSSS_RX_HO_SEED]);
      err_col ("handed over", eb, n);
    }
  free (eb);
  return 0;
}

int
main (int argc, char **argv)
{
  int      check = 0, emit = 0;
  unsigned dwells = RF_DWELLS;
  for (int i = 1; i < argc; i++)
    {
      check |= strcmp (argv[i], "--check") == 0;
      emit |= strcmp (argv[i], "--emit") == 0;
      if (strcmp (argv[i], "--dwells") == 0 && i + 1 < argc)
        dwells = (unsigned)strtoul (argv[++i], NULL, 10);
    }
  if (check)
    dwells = RF_CHECK_DWELLS;
  DP_REQUIRE (dwells >= 1);
  uint8_t code[SF];
  dp_dsss_gold_1023 (code);
  static const double fcs[2]   = { DP_S415_FC_LO, DP_S415_FC_HI };
  static const double esn0s[2] = { 4.0, 7.0 };
  const double        rates[2] = { 0.0, dp_s415_r_env () };
  if (!emit)
    printf ("refine: 415.1-B-1, 11 MSa/s, 3000 sym/s; refine_sequential "
            "false (one test an estimator); C/N0 at the antenna\n");
  for (int f = check ? 1 : 0; f < 2; f++)
    DP_REQUIRE (noise_rows (code, fcs[f], dwells, emit) == 0);
  if (emit)
    printf ("# handover\nfc_ghz,esn0_db,rate_hz_s,seed,offset_rows,"
            "err_hz,ho_src\n");
  else
    printf ("  hand-over: %u offsets across one search row, the error "
            "against the channel's offset at the hand-over sample\n",
            RF_OFFSETS);
  for (int f = check ? 1 : 0; f < 2; f++)
    for (int e = check ? 1 : 0; e < 2; e++)
      for (int r = 0; r < (check ? 1 : 2); r++)
        DP_REQUIRE (handover_rows (code, fcs[f], esn0s[e], rates[r],
                                   check ? 1u : RF_NOISE_SEEDS, emit)
                    == 0);
  if (emit)
    DP_TEST_EMIT_END ("validate_async_dsss_receiver_refine");
  DP_TEST_END ("validate_async_dsss_receiver_refine");
}
