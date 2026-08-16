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

/* ── The adapter: the only receiver-specific code in the battery ───────────
 */

static void *
rx_mpsk_create (const dp_rx_point_t *pt)
{
  /* Five construction parameters are 0 on purpose — that asks the object to
     DERIVE them (doppler#644, design/mpsk.md §8.1). The battery states the
     link; it does not re-derive what the receiver already knows. */
  return mpsk_receiver_create (
      pt->m, pt->sps, pt->m_out, MPSK_RX_PULSE_RRC, pt->beta, pt->span,
      pt->bn_carrier, 0.0 /* zeta */, pt->bn_timing, pt->acq_to_track,
      0.0 /* lock_thresh */,
      /* foff is cycles per SYMBOL (so one value
         means one thing at every rate); the ctor
         wants cycles per SAMPLE. Mixing them is an
         sps-sized error, and at sps=8 it asked the
         loop for 8x its design envelope. */
      pt->fc - pt->foff / pt->sps, 0 /* differential */, 0 /* num_phases */,
      pt->nda_tap, 1 /* agc */, 0.0 /* bn_agc_ratio */);
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
/* Read back rather than restated. `create()` above passes 0 and asks the
   receiver to DERIVE its damping; the ramp law is a function of it, so the
   harness asks the object what it settled on instead of carrying a second
   copy of the default that would go stale the moment the receiver changed. */
static double
rx_mpsk_zeta (const void *h)
{
  return mpsk_receiver_get_zeta ((const mpsk_receiver_state_t *)h);
}

static const dp_rx_iface_t RX_MPSK
    = { "MpskReceiver", DP_RX_IN_COMPLEX,  rx_mpsk_create,     rx_mpsk_destroy,
        rx_mpsk_step,   rx_mpsk_norm_freq, rx_mpsk_last_error, rx_mpsk_lock,
        rx_mpsk_locked, rx_mpsk_lock_time, rx_mpsk_clipped,    rx_mpsk_zeta };

/* ── The second adapter, and the whole point of goal 6 ──────────────────────
 *
 * `ContinuousMpskReceiver` is the continuous flavor: a view over the same
 * core that PINS the gating (`acq_to_track = 0`, `nda_tap = strobe`, `agc`
 * on) and the five derived parameters. Everything past construction is
 * shared verbatim, so this adapter is one function long and the other ten
 * entries are reused unchanged -- which is "a second receiver design costs
 * an adapter and nothing else" being cashed rather than asserted.
 *
 * It deliberately IGNORES `pt->acq_to_track` and `pt->nda_tap`. That is not
 * the adapter taking a liberty: those are the knobs the flavor exists to
 * remove, so a point that sets them is asking for a receiver this one is
 * not.
 *
 * AND AT EVERY POINT IN THE CURRENT SET THE TWO ROWS COINCIDE EXACTLY, which
 * is worth stating rather than leaving to be noticed. Every named point sets
 * `acq_to_track = 0` and `nda_tap = strobe`, so what the flavor pins is what
 * the point already asked for and the two receivers construct identically.
 * The second row therefore proves the ADAPTER — that a second receiver design
 * costs one function and reuses the other ten entries verbatim — and not a
 * behavioural difference, because at these points there is none to prove. The
 * battery does not exercise the handover on either receiver; a point that
 * turns `acq_to_track` on is what would separate them, and it is not here yet
 * (doppler#790).
 */
static void *
rx_cont_create (const dp_rx_point_t *pt)
{
  return mpsk_receiver_create_continuous (
      pt->m, pt->sps, MPSK_RX_PULSE_RRC, pt->beta, pt->span, pt->bn_carrier,
      pt->bn_timing, pt->fc - pt->foff / pt->sps, 0 /* differential */);
}

static const dp_rx_iface_t RX_CONT
    = { "ContinuousMpskReceiver", DP_RX_IN_COMPLEX, rx_cont_create,
        rx_mpsk_destroy,          rx_mpsk_step,     rx_mpsk_norm_freq,
        rx_mpsk_last_error,       rx_mpsk_lock,     rx_mpsk_locked,
        rx_mpsk_lock_time,        rx_mpsk_clipped };

static const dp_rx_iface_t *const RECEIVERS[] = { &RX_MPSK, &RX_CONT };
#define RECEIVER_COUNT (sizeof RECEIVERS / sizeof RECEIVERS[0])

/* ── The battery ────────────────────────────────────────────────────────── */

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int fail  = 0;

  if (!check)
    printf ("The standard receiver battery — one harness, every receiver.\n"
            "Each row is one operating point; a refusal names itself.\n\n");

  for (size_t k = 0; k < RECEIVER_COUNT; k++)
    {
      if (!check && k)
        printf ("\n");
      for (int i = 0; i < DP_RX_POINT_COUNT; i++)
        {
          const dp_rx_point_t *pt = dp_rx_point ((dp_rx_point_name_t)i);
          dp_rx_result_t       r;
          if (!pt)
            continue;
          r = dp_rx_run (RECEIVERS[k], pt);
          if (check)
            fail |= dp_rx_check (&r);
          else
            dp_rx_print (&r);
        }

      /* A per-point refusal is a result and is not counted (see the header),
         so nothing here gates a receiver that refuses EVERY point -- and that
         is a real hole: the tap regression that pinned `mf_in` on the
         continuous flavor printed nine "no burst settled" lines and exited 0,
         because each line individually was the harness declining to defend a
         number. Nine of them is not nine refusals, it is a receiver that does
         not work. Closing it needs a RUN-level gate rather than a per-point
         one, which is what doppler#794 is adding to this same loop as
         `dp_rx_witness_t`; the counter that proved the shape is handed over
         there with its sabotage evidence rather than landed here, where it
         would only conflict. */
    }

  if (check)
    printf ("rx_battery: %s\n", fail ? "FAILED" : "OK");
  return fail;
}
