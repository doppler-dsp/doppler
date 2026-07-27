/**
 * @file mpsk_receiver_r_ber.c
 * @brief Symbol-error-rate validation for the REAL-IF M-PSK receiver.
 *
 * The real-path twin of `mpsk_receiver_ber.c`: the same signal model, the same
 * measurement, the same operating points, through MpskReceiverR's R2C-halfband
 * front end instead of a complex-baseband one. Running both on one shared
 * harness (`mpsk_ber_common.h`) is what makes the two comparable — the
 * interesting number is not either rate alone but the gap between them, which
 * is the cost of the real front end and nothing else.
 *
 * Usage:
 *   validate_mpsk_receiver_r_ber           full report at every M
 *   validate_mpsk_receiver_r_ber --check   the same, asserting the loss bound
 *
 * ## Everything is measured at the design centre fc = 0.25
 *
 * The R2C halfband bakes in a `+fs/4` shift, so `fs/4` is where the front end
 * is symmetric and its image rejection is best — past -100 dB across roughly
 * 0.06..0.44, but only -7 dB at 0.01. This is also the realistic case: 40
 * MSa/s with a 10 MHz IF is exactly `fs/4`. Measuring off-centre and
 * attributing the result to the receiver is a mistake this project has already
 * made and retracted; `test_mpsk_receiver_r_core.c` test 5 pins the real
 * placement behaviour, and this validator does not re-litigate it.
 *
 * ## What the real front end actually costs — measured, over seeds
 *
 * Both paths meet the coherent bound comfortably at every M, and the real
 * path's only visible penalty is at 8PSK. Implementation loss in dB at each
 * M's SER=1e-3 anchor, five seeds each, `m_out = 8`:
 *
 *     M      complex          real            real - complex
 *     2      0.61             0.56            ~0
 *     4      0.60             0.53            ~0
 *     8      0.52 .. 0.62     0.83 .. 1.02    **~0.38 dB**
 *
 * So the R2C-halfband front end costs about **0.38 dB at 8PSK** and nothing
 * measurable below it. The EVM corroborates that independently and closely:
 * -15.42 dB complex against -15.07 dB real, a 0.35 dB gap against the 0.38 dB
 * the error rate implies. Two measurements that cannot fail the same way
 * agreeing to 0.03 dB is what makes this a property of the cascade rather than
 * an artifact of the harness.
 *
 * **This corrects an earlier estimate of ~1.5 dB / 4.3x the bound.** That
 * figure predates two things this validator now does: `m_out = 8` (the value
 * at which an I&D matched filter actually reaches the bound — `m_out = 4`
 * costs 3.11 dB by itself, and measuring through it attributes a filter
 * penalty to the front end), and a settled window derived from the receiver's
 * own handover rather than a fraction of the record. The 8PSK window here
 * opens around symbol 7400 of 24000; measuring from a quarter of the record
 * would start ~1400 symbols early, inside the decision-directed loop's
 * transient, and every one of those symbols is scored against a steady-state
 * bound.
 *
 * The loss allowance is therefore the SAME 2 dB the complex path is held to.
 * An asymmetric per-M allowance was drafted and dropped: with the worst
 * lower-limit across seeds at 0.86 dB, a uniform 2 dB already carries better
 * than 2x margin, and a special case for 8PSK would only have hidden a future
 * regression in the one place a regression is most likely.
 */
#include "mpsk_ber_common.h"
#include <string.h>

/** @brief Errors per point. ~7% relative standard error (`1/sqrt(r)`). */
#define TARGET_ERRORS 200uL

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int ms[3] = { 2, 4, 8 };
  /* The same allowance the complex path is held to. Worst 99% lower-limit
     loss across five seeds is 0.86 dB (8PSK), so this carries >2x margin at
     every M -- see the file docstring for the measured distribution and for
     why a per-M special case was dropped. */
  double allow_db[3] = { 2.0, 2.0, 2.0 };
  int    rc          = 0;

  printf ("MpskReceiverR symbol error rate vs the coherent bound\n");
  printf ("  real IF at the design centre fc = 0.25, I&D matched filter\n");
  printf ("  each M at its own SER=1e-3 anchor, %lu errors per point\n\n",
          TARGET_ERRORS);

  for (int mi = 0; mi < 3; mi++)
    {
      mpsk_ber_cfg_t    c;
      mpsk_ber_result_t r;
      double            esn0_db, loss_lo;
      char              label[48];

      c.real = 1;
      c.m    = ms[mi];
      /* m_out = 8 for TWO reasons, and the second is the one that bites at
         M = 8. (a) The matched filter: the rectangle is one symbol wide, so
         its filter is an m_out-tap sum spanning it, and a smaller m_out
         samples the same integral more coarsely. (b) The M-th-power
         discriminator: raising to the M-th power auto-convolves the spectrum
         M times, spreading energy over ~M*Rs, and whatever does not fit the
         update rate FOLDS BACK ONTO ITSELF. On a perfectly clean strobe
         `z^M` is a constant and there is nothing to fold; every departure
         from clean -- ISI, timing error, noise -- gets its energy splattered
         M-fold and aliased, so the nonlinearity's tolerance for a dirty input
         COLLAPSES as M grows.

         (a) alone is M-independent, so it predicts the same penalty at every
         M. Measured (complex path, halving m_out from 8 to 4): BPSK 1.7 dB,
         QPSK 1.6 dB -- and 8PSK 3.0 dB, which also lands the constellation
         0.87 dB from the fully-scattered EVM floor, i.e. barely
         distinguishable from noise. The extra ~1.3 dB at M = 8 is (b).

         This matters most at MPSK_RX_NDA_TAP_MF_ALL, where the M-th power
         genuinely runs on an oversampled pulse at m_out*Rs and needs
         `m_out >= M` outright -- and since m_out maxes out at 8, 8PSK at that
         tap is exactly critically sampled with no margin. The header's
         `|df| < F/(2M)` pull-in rule is the single-tone face of the same
         constraint. We use _STROBE, so the tone limit is `Rs/(2M)`.

         The cascade behind the halfband runs at twice the overall rate, so
         MpskReceiverR requires sps > 2*m_out: 32. */
      c.sps        = 32.0;
      c.m_out      = 8;
      c.fc         = 0.25;
      c.bn_timing  = 0.01;
      c.bn_carrier = 0.005;
      /* Inside the loop bandwidth — see the complex twin for why an offset
         outside Bn measures luck rather than the receiver. */
      c.foff         = 0.5 * c.bn_carrier / c.sps;
      c.acq_to_track = (c.m == 8);
      c.nda_tap      = MPSK_RX_NDA_TAP_STROBE;

      esn0_db = dp_ber_esn0_db_for_ser (c.m, DP_BER_TARGET_SER);
      r = mpsk_ber_measure (&c, esn0_db, TARGET_ERRORS, 4096u + (unsigned)mi);

      snprintf (label, sizeof label, "M=%d @%.1f dB", c.m, esn0_db);
      dp_ber_print (label, &r.rep);
      printf ("%-24s   %d burst(s), %d unsettled, clipped=%d\n\n", "",
              r.bursts, r.unsettled, r.clipped);

      if (!check)
        continue;

      loss_lo = esn0_db - dp_ber_esn0_db_for_ser (c.m, r.rep.ser.lo);
      if (r.clipped)
        {
          printf ("  M=%d FAIL: front end clipped\n", c.m);
          rc = 1;
        }
      else if (!r.rep.sane)
        {
          printf ("  M=%d FAIL: %s\n", c.m, r.rep.why);
          rc = 1;
        }
      else if (!r.rep.enough)
        {
          printf ("  M=%d FAIL: only %lu of %lu errors in %d bursts\n", c.m,
                  r.rep.ser.errors, TARGET_ERRORS, r.bursts);
          rc = 1;
        }
      else if (loss_lo > allow_db[mi])
        {
          printf ("  M=%d FAIL: loss %.2f dB (99%% lower limit) > %.1f dB\n",
                  c.m, loss_lo, allow_db[mi]);
          rc = 1;
        }
      else
        printf ("  M=%d OK: loss %.2f dB (point %.2f), bound %.1f dB\n\n", c.m,
                loss_lo, r.rep.loss_db, allow_db[mi]);
    }

  if (check)
    printf (rc ? "mpsk_receiver_r_ber FAILED\n"
               : "mpsk_receiver_r_ber PASSED\n");
  return rc;
}
