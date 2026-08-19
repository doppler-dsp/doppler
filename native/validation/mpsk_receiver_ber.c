/**
 * @file mpsk_receiver_ber.c
 * @brief Symbol-error-rate validation for the complex-baseband M-PSK receiver.
 *
 * Drives MpskReceiver with a rectangular (I&D-matched) BPSK/QPSK/8PSK signal
 * at a carrier offset and measures how far its symbol error rate sits from the
 * coherent bound. The receiver acquires the carrier non-data-aided (M-th
 * power) and recovers symbol timing (Gardner); nothing is given to it but the
 * samples.
 *
 * Usage:
 *   validate_mpsk_receiver_ber           full report at every M
 *   validate_mpsk_receiver_ber --check   the same, asserting the loss bound
 *
 * ## What changed, and why the old numbers are not comparable
 *
 * This validator used to measure a fixed 40 000 symbols, score them over a
 * `nout/4 .. 7*nout/8` window, and take the **minimum error count over a
 * +-30 lag and M-fold rotation search**. Every one of those is a documented
 * way to produce a confident wrong number:
 *
 *   - a window pinned to a FRACTION of the record measures the acquisition
 *     transient whenever settling is longer than the fraction (here the
 *     settling budget is 3000 symbols against a 10 000-symbol quarter, so it
 *     was inside it for the slower geometries);
 *   - a `min over (lag, rotation)` is an optimisation over the answer, not a
 *     measurement of the receiver. It false-PASSES on a lucky alignment and
 *     false-FLOORS when the true lag falls outside the span — this project has
 *     shipped both, including a committed "~12 dB floor" that was really 5 dB;
 *   - a fixed symbol count makes the precision depend on the very rate being
 *     measured. 40 000 symbols at SER 1e-3 is ~40 errors, ~16% relative, which
 *     reads as receiver variation and is not.
 *
 * All three are now the harness's job (`native/tests/dp_ber_test.h`): the
 * window comes from the receiver's own carrier lock and handover plus the
 * analytic loop budget, the alignment is DETECTED against a marker under a
 * false-alarm gate rather than searched, and the run stops on a fixed ERROR
 * count so the interval width is `1/sqrt(r)` regardless of the rate. The
 * result carries a 99% confidence interval and is cross-checked against EVM,
 * the blind M2M4 Es/N0 and theory before it is called a result at all.
 *
 * ## The operating point
 *
 * Each M is measured at its own **SER = 1e-3** anchor (6.8 / 10.3 / 15.7 dB),
 * so every constellation is asked the same question at the same place on its
 * curve. The gate is stated as an **implementation loss in dB** — the Es/N0
 * theory would need to produce the measured rate, subtracted from the Es/N0
 * actually applied — because a loss in dB is comparable across M and across
 * operating points while a ratio of rates is not.
 *
 * The assertion is on the interval's LOWER limit, so counting noise cannot
 * flake it: we fail only when the receiver is worse than the bound with 99%
 * confidence, not when a draw happened to land badly.
 */
#include "mpsk_ber_common.h"
#include <string.h>

/** @brief Tolerated implementation loss vs the coherent bound, dB. */
#define LOSS_DB 2.0

/** @brief Errors per point. ~7% relative standard error (`1/sqrt(r)`). */
#define TARGET_ERRORS 200uL

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int ms[3] = { 2, 4, 8 };
  int rc    = 0;

  printf ("MpskReceiver symbol error rate vs the coherent bound\n");
  printf ("  complex baseband, rectangular pulse, I&D matched filter\n");
  printf ("  each M at its own SER=1e-3 anchor, %lu errors per point\n\n",
          TARGET_ERRORS);

  for (int mi = 0; mi < 3; mi++)
    {
      mpsk_ber_cfg_t    c;
      mpsk_ber_result_t r;
      double            esn0_db, loss_lo;
      char              label[48];

      c.real  = 0;
      c.m     = ms[mi];
      c.sps   = 8.0;
      c.m_out = 8;
      c.fc    = 0.0;
      /* An offset INSIDE the loop's acquisition bound, so the carrier loop
         has real work to do and the measurement says something about it.
         Seeded exactly on truth the loop never leaves its initial state and
         any conclusion drawn about it is void; asserted OUTSIDE the bound the
         test measures luck.

         The bound carries the `m`: the NDA discriminator is an M-th power, so
         it is `bn_carrier / m` per symbol. `0.5 * bn / sps` stood here, which
         is u = 0.5*m -- the same number at every order while asking a 4x
         harder question at 8PSK than at BPSK, and landing the 8PSK row of
         this very sweep at u = 4.0, the measured limit with no margin. */
      c.bn_timing  = 0.01;
      c.bn_carrier = 0.005;
      c.foff = dp_test_freq_offset_inside_bw (c.bn_carrier, c.m, 1.0) / c.sps;
      /* 8PSK used to hand over to a decision-directed discriminator here,
         on the reasoning that its decision margin is only +-pi/8 and the
         M-th-power discriminator's own phase noise eats into it. This
         harness is where that reasoning was measured and did not survive:
         turning the handover off moved 8PSK from 0.44 dB to 0.53 dB of loss
         -- 0.09 dB, against a settling window it MORE THAN DOUBLED, since
         the handover fired around symbol 8500. The receiver no longer has
         one (doppler#877), so the M=8 row below is the NDA discriminator's
         own number and should read ~0.53 dB. */

      esn0_db = dp_ber_esn0_db_for_ser (c.m, DP_BER_TARGET_SER);
      r = mpsk_ber_measure (&c, esn0_db, TARGET_ERRORS, 2024u + (unsigned)mi);

      snprintf (label, sizeof label, "M=%d @%.1f dB", c.m, esn0_db);
      dp_ber_print (label, &r.rep);
      printf ("%-24s   %d burst(s), %d unsettled, clipped=%d\n\n", "",
              r.bursts, r.unsettled, r.clipped);

      if (!check)
        continue;

      /* The gate: the implementation loss implied by the interval's LOWER
         limit (the receiver's best defensible rate) must clear LOSS_DB. Using
         the limit rather than the point estimate is what stops counting noise
         flaking this. */
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
      else if (loss_lo > LOSS_DB)
        {
          printf ("  M=%d FAIL: loss %.2f dB (99%% lower limit) > %.1f dB\n",
                  c.m, loss_lo, LOSS_DB);
          rc = 1;
        }
      else
        printf ("  M=%d OK: loss %.2f dB (point %.2f), bound %.1f dB\n\n", c.m,
                loss_lo, r.rep.loss_db, LOSS_DB);
    }

  if (check)
    printf (rc ? "mpsk_receiver_ber FAILED\n" : "mpsk_receiver_ber PASSED\n");
  return rc;
}
