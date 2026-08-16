/**
 * @file rx_battery.c
 * @brief The standard receiver battery, run on `MpskReceiver`.
 *
 * One adapter and a loop. Everything that makes a number is in
 * `native/tests/dp_rx_test.h`, so a second receiver design costs an adapter
 * and nothing else — which is `docs/design/rx-test.md` goal 6: "one harness,
 * every receiver, parameterised by operating point, not forked per object, so
 * two receivers are comparable by construction rather than by hoping two
 * harnesses agree."
 *
 * The adapter below is the whole fork. It is thin because both shipped M-PSK
 * receivers expose the same surface modulo an `_r` infix; a design that cannot
 * fill one of these entries in is telling you something real about its
 * observability rather than about the harness.
 *
 * ## What `--check` gates, and what it does not
 *
 * It gates that every point which CLAIMS to be measurable produces a
 * defensible record — `dp_ber_report`'s four gates, which include the `sane`
 * check that catches an EVM beating the matched-filter bound. It does NOT gate
 * the loop numbers as values: a pull-in ceiling moves with the record length
 * allowed, so pinning one would pin the observation window rather than the
 * receiver (see `docs/design/mpsk.md` §8.1 and the `nda_tap` history).
 *
 * A REFUSAL is not a failure. `dp_rx_run()` declining to report a number it
 * cannot defend is goal 1 working, and it is printed rather than counted.
 *
 * Usage:
 *   rx_battery            the full battery, printing the standard record
 *   rx_battery --check    the CI gate
 */
#include "dp_rx_test.h"

#include "mpsk_receiver/mpsk_receiver_core.h"

#include <stdio.h>
#include <string.h>

/* ── The adapter: the only receiver-specific code in the battery ─────────── */

static void *
rx_mpsk_create (const dp_rx_point_t *pt)
{
  /* Five construction parameters are 0 on purpose — that asks the object to
     DERIVE them (doppler#644, design/mpsk.md §8.1). The battery states the
     link; it does not re-derive what the receiver already knows. */
  return mpsk_receiver_create (pt->m, pt->sps, pt->m_out, MPSK_RX_PULSE_RRC,
                               pt->beta, pt->span, pt->bn_carrier,
                               0.0 /* zeta */, pt->bn_timing,
                               pt->acq_to_track, 0.0 /* lock_thresh */,
                               /* foff is cycles per SYMBOL (so one value
                                  means one thing at every rate); the ctor
                                  wants cycles per SAMPLE. Mixing them is an
                                  sps-sized error, and at sps=8 it asked the
                                  loop for 8x its design envelope. */
                               pt->fc - pt->foff / pt->sps,
                               0 /* differential */,
                               0 /* num_phases */, pt->nda_tap, 1 /* agc */,
                               0.0 /* bn_agc_ratio */);
}

static void
rx_mpsk_destroy (void *h)
{
  mpsk_receiver_destroy ((mpsk_receiver_state_t *)h);
}

static int
rx_mpsk_step (void *h, float complex x, float complex *y)
{
  return mpsk_receiver_step_ted ((mpsk_receiver_state_t *)h, x, y,
                                 RATESYNC_TED_GARDNER);
}

static double
rx_mpsk_norm_freq (const void *h)
{
  return mpsk_receiver_get_norm_freq ((const mpsk_receiver_state_t *)h);
}
static double
rx_mpsk_last_error (const void *h)
{
  return mpsk_receiver_get_last_error ((const mpsk_receiver_state_t *)h);
}
static double
rx_mpsk_lock (const void *h)
{
  return mpsk_receiver_get_lock ((const mpsk_receiver_state_t *)h);
}
static int
rx_mpsk_locked (const void *h)
{
  return mpsk_receiver_get_locked ((const mpsk_receiver_state_t *)h);
}
static long
rx_mpsk_lock_time (const void *h)
{
  return (long)mpsk_receiver_get_lock_time ((const mpsk_receiver_state_t *)h);
}
static int
rx_mpsk_clipped (const void *h)
{
  return mpsk_receiver_get_clipped ((const mpsk_receiver_state_t *)h);
}

static const dp_rx_iface_t RX_MPSK = {
  "MpskReceiver",   DP_RX_IN_COMPLEX,  rx_mpsk_create,     rx_mpsk_destroy,
  rx_mpsk_step,     rx_mpsk_norm_freq, rx_mpsk_last_error, rx_mpsk_lock,
  rx_mpsk_locked,   rx_mpsk_lock_time, rx_mpsk_clipped
};

/* ── The battery ────────────────────────────────────────────────────────── */

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int fail  = 0;

  if (!check)
    printf ("The standard receiver battery — one harness, every receiver.\n"
            "Each row is one operating point; a refusal names itself.\n\n");

  for (int i = 0; i < DP_RX_POINT_COUNT; i++)
    {
      const dp_rx_point_t *pt = dp_rx_point ((dp_rx_point_name_t)i);
      dp_rx_result_t       r;
      if (!pt)
        continue;
      r = dp_rx_run (&RX_MPSK, pt);
      if (check)
        fail |= dp_rx_check (&r);
      else
        dp_rx_print (&r);
    }

  if (check)
    printf ("rx_battery: %s\n", fail ? "FAILED" : "OK");
  return fail;
}
