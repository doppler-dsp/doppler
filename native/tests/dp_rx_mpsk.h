/**
 * @file dp_rx_mpsk.h
 * @brief The `MpskReceiver` adapters for the receiver instrument.
 *
 * `dp_rx_test.h` forks per receiver design at exactly one place —
 * @ref dp_rx_iface_t — and this is that fork filled in for the two shipped
 * M-PSK flavors. It lives in a header rather than in a harness because the
 * SECOND harness is what proves goal 6 ("one harness, every receiver"): a
 * copy of these twelve entries in each `native/validation/*.c` that wants a
 * receiver is how two harnesses come to construct the same object
 * differently and stop being comparable, which is the failure the instrument
 * exists to prevent one level up.
 *
 * Nothing here measures anything. Every number comes from `dp_rx_test.h`.
 *
 * @see docs/design/rx-test.md goal 6
 */
#ifndef DP_RX_MPSK_H
#define DP_RX_MPSK_H

#include "dp_rx_test.h"

#include "mpsk_receiver/mpsk_receiver_core.h"

/* ── The adapter: the only receiver-specific code in a battery ──────────── */

static void *
dp_rx_mpsk_create (const dp_rx_point_t *pt)
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
dp_rx_mpsk_destroy (void *h)
{
  mpsk_receiver_destroy ((mpsk_receiver_state_t *)h);
}

static int
dp_rx_mpsk_step (void *h, float complex x, float complex *y)
{
  return mpsk_receiver_step_ted ((mpsk_receiver_state_t *)h, x, y,
                                 RATESYNC_TED_GARDNER);
}

static double
dp_rx_mpsk_norm_freq (const void *h)
{
  return mpsk_receiver_get_norm_freq ((const mpsk_receiver_state_t *)h);
}
static double
dp_rx_mpsk_last_error (const void *h)
{
  return mpsk_receiver_get_last_error ((const mpsk_receiver_state_t *)h);
}
static double
dp_rx_mpsk_lock (const void *h)
{
  return mpsk_receiver_get_lock ((const mpsk_receiver_state_t *)h);
}
static int
dp_rx_mpsk_locked (const void *h)
{
  return mpsk_receiver_get_locked ((const mpsk_receiver_state_t *)h);
}
static long
dp_rx_mpsk_lock_time (const void *h)
{
  return (long)mpsk_receiver_get_lock_time ((const mpsk_receiver_state_t *)h);
}
static int
dp_rx_mpsk_clipped (const void *h)
{
  return mpsk_receiver_get_clipped ((const mpsk_receiver_state_t *)h);
}
/* Read back rather than restated. `create()` above passes 0 and asks the
   receiver to DERIVE its damping; the ramp law is a function of it, so the
   harness asks the object what it settled on instead of carrying a second
   copy of the default that would go stale the moment the receiver changed. */
static double
dp_rx_mpsk_zeta (const void *h)
{
  return mpsk_receiver_get_zeta ((const mpsk_receiver_state_t *)h);
}

/** @brief `MpskReceiver`, the general flavor: every knob a point sets. */
static const dp_rx_iface_t DP_RX_MPSK
    = { "MpskReceiver",        DP_RX_IN_COMPLEX,   dp_rx_mpsk_create,
        dp_rx_mpsk_destroy,    dp_rx_mpsk_step,    dp_rx_mpsk_norm_freq,
        dp_rx_mpsk_last_error, dp_rx_mpsk_lock,    dp_rx_mpsk_locked,
        dp_rx_mpsk_lock_time,  dp_rx_mpsk_clipped, dp_rx_mpsk_zeta };

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
dp_rx_cont_create (const dp_rx_point_t *pt)
{
  return mpsk_receiver_create_continuous (
      pt->m, pt->sps, MPSK_RX_PULSE_RRC, pt->beta, pt->span, pt->bn_carrier,
      pt->bn_timing, pt->fc - pt->foff / pt->sps, 0 /* differential */);
}

/** @brief `ContinuousMpskReceiver`: the same core with the gating pinned. */
static const dp_rx_iface_t DP_RX_CONT
    = { "ContinuousMpskReceiver", DP_RX_IN_COMPLEX,   dp_rx_cont_create,
        dp_rx_mpsk_destroy,       dp_rx_mpsk_step,    dp_rx_mpsk_norm_freq,
        dp_rx_mpsk_last_error,    dp_rx_mpsk_lock,    dp_rx_mpsk_locked,
        dp_rx_mpsk_lock_time,     dp_rx_mpsk_clipped, dp_rx_mpsk_zeta };

#endif /* DP_RX_MPSK_H */
