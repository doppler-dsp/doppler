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
      pt->bn_carrier, 0.0 /* zeta */, pt->bn_timing, 0.0 /* lock_thresh */,
      /* foff is cycles per SYMBOL (so one value
         means one thing at every rate); the ctor
         wants cycles per SAMPLE. Mixing them is an
         sps-sized error, and at sps=8 it asked the
         loop for 8x its design envelope. */
      pt->fc - pt->foff / pt->sps, 0 /* differential */, 0 /* num_phases */,
      1 /* agc */, 0.0 /* bn_agc_ratio */);
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

/* There is no second adapter here any more. `ContinuousMpskReceiver` was
 * one -- a view over this same core that pinned the gating -- and it is gone
 * (doppler#877), because with the handover deleted it pinned nothing and was
 * a duplicate of `MpskReceiver`. Its row cost the battery no coverage: the
 * harness recorded, at the time, that every named point already set
 * `acq_to_track = 0`, so the two receivers constructed identically and the
 * second row proved the ADAPTER rather than any behavioural difference.
 *
 * The adapter claim is worth re-proving on a receiver that genuinely differs,
 * and the real-input face is the candidate the battery does not yet cover --
 * see doppler#802.
 */

#endif /* DP_RX_MPSK_H */
