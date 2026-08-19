/**
 * @file rx_battery.c
 * @brief The standard receiver battery, run on `MpskReceiver`.
 *
 * Two adapters and a loop, and neither adapter is here: they are
 * `native/tests/dp_rx_mpsk.h`, so that a second HARNESS constructs the same
 * receiver the same way. Everything that makes a number is in
 * `native/tests/dp_rx_test.h` — which is `docs/design/rx-test.md` goal 6:
 * "one harness, every receiver, parameterised by operating point, not forked
 * per object, so two receivers are comparable by construction rather than by
 * hoping two harnesses agree."
 *
 * What is left in this file is the LOOP and the gates it applies, which is
 * the battery itself.
 *
 * ## What `--check` gates, and what it does not
 *
 * It gates that every point which CLAIMS to be measurable produces a
 * defensible record — `dp_ber_report`'s four gates, which include the `sane`
 * check that catches an EVM beating the matched-filter bound. It does NOT gate
 * the loop numbers as values: a pull-in ceiling moves with the record length
 * allowed, so pinning one would pin the observation window rather than the
 * receiver (see `docs/design/mpsk.md` §8.1 and §3.3).
 *
 * A REFUSAL is not a failure. `dp_rx_run()` declining to report a number it
 * cannot defend is goal 1 working, and it is printed rather than counted.
 *
 * Usage:
 *   rx_battery            the full battery, printing the standard record
 *   rx_battery --check    the CI gate
 */
#include "dp_rx_mpsk.h"

#include <stdio.h>
#include <string.h>

static const dp_rx_iface_t *const RECEIVERS[] = { &DP_RX_MPSK };
#define RECEIVER_COUNT (sizeof RECEIVERS / sizeof RECEIVERS[0])

/* ── The battery ────────────────────────────────────────────────────────── */

int
main (int argc, char **argv)
{
  int             check   = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int             fail    = 0;
  dp_rx_witness_t witness = { 0, 0 };

  if (!check)
    printf ("The standard receiver battery — one harness, every receiver.\n"
            "Each row is one operating point; a refusal names itself.\n\n");

  for (size_t k = 0; k < RECEIVER_COUNT; k++)
    {
      /* PER RECEIVER, not per run: one tally across both would let a working
         receiver excuse a dead one, which is the aggregate this gate exists
         to notice. The witness above is genuinely run-level -- "was the sync
         detector ever asked" is a fact about the harness, not a receiver. */
      dp_rx_tally_t tally = { NULL, 0u, 0u };

      if (!check && k)
        printf ("\n");
      for (int i = 0; i < DP_RX_POINT_COUNT; i++)
        {
          const dp_rx_point_t *pt = dp_rx_point ((dp_rx_point_name_t)i);
          dp_rx_result_t       r;
          if (!pt)
            continue;
          r = dp_rx_run (RECEIVERS[k], pt);
          dp_rx_witness_add (&witness, &r);
          dp_rx_tally_add (&tally, &r);
          if (check)
            fail |= dp_rx_check (&r);
          else
            dp_rx_print (&r);
        }

      /* A refusal does not fail the per-point gate, so without this a
         receiver that refuses EVERY point exits 0 -- which is how a receiver
         pinning a tap whose lock statistic never clears its own threshold
         shipped green (doppler#791). */
      if (check)
        fail |= dp_rx_tally_check (&tally);
    }

  /* Run-level, because no single point can establish it — see
     dp_rx_witness_t. The standard set supplies both outcomes by construction:
     RX_FRAME_CONT's PN-127 misses nothing and `runburst`'s Barker-13 misses
     most, so a harness that stopped asking the detector fails here even
     though every per-point gate above stays green. */
  if (check)
    fail |= dp_rx_witness_check (&witness);

  if (check)
    printf ("rx_battery: %s\n", fail ? "FAILED" : "OK");
  return fail;
}
