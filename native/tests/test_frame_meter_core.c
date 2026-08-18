/*
 * test_frame_meter_core.c — the frame accumulator's COUNTING RULES.
 *
 * Nothing here is arithmetic worth testing; what is worth testing is what
 * counts as an error, because every one of those decisions is a convention
 * that goes wrong silently and shows up as a receiver result:
 *
 *   - a frame whose sync was never detected IS a frame error (you did not
 *     deliver it), so an FER that scored only the frames it managed to find
 *     would improve as the receiver got WORSE at finding them;
 *   - a frame carrying no CRC is NOT an error when its sync was found, or
 *     every unprotected frame fails and the number measures the frame format
 *     rather than the receiver;
 *   - the two failure modes stay separately countable, because "the sync is
 *     too short at this Es/N0" and "the demodulator makes bit errors" are
 *     different repairs.
 *
 * The interval itself is `ber_confidence`, already pinned where it lives; what
 * is checked here is that this meter hands it the right two numbers.
 */
#define _GNU_SOURCE
#include "dp_state_test.h"
#include "dp_test.h"
#include "frame_meter/frame_meter_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main (void)
{
  frame_meter_state_t *m = frame_meter_create (10, 0.99);
  DP_CHECK (m != NULL);
  if (!m)
    return 1;

  /* ── what counts as delivered ─────────────────────────────────────────── */
  {
    frame_meter_add (m, 1, 1);  /* found, checked        -> delivered */
    frame_meter_add (m, 1, -1); /* found, no CRC carried -> delivered */
    frame_meter_add (m, 1, 0);  /* found, failed CRC     -> ERROR     */
    frame_meter_add (m, 0, -1); /* never found           -> ERROR     */
    frame_meter_add (m, 0, 1);  /* never found; a CRC verdict here is
                                   meaningless and must not rescue it */

    DP_REQUIRE_MSG (frame_meter_get_frames (m) == 5, "every add is a frame");
    DP_REQUIRE_MSG (frame_meter_get_sync_detected (m) == 3, "3 found");
    DP_REQUIRE_MSG (frame_meter_get_crc_passed (m) == 1, "1 checked");
    DP_REQUIRE_MSG (frame_meter_get_errors (m) == 3,
                    "a failed CRC and two misses are three frame errors");

    /* The counts feed the two rates, and each rate takes its own denominator:
       FER over every frame attempted, sync-miss over the same -- both are
       error rates over attempts, which is what makes them comparable. */
    ber_interval_t fer = frame_meter_fer (m);
    DP_REQUIRE_MSG (fer.errors == 3 && fer.symbols == 5, "fer counts");
    ber_interval_t sm = frame_meter_sync_miss (m);
    DP_REQUIRE_MSG (sm.errors == 2 && sm.symbols == 5, "sync-miss counts");
    DP_REQUIRE_MSG (fer.lo <= fer.p_hat && fer.p_hat <= fer.hi,
                    "the interval brackets its own estimate");
    DP_REQUIRE_MSG (fer.conf == 0.99, "the configured confidence is used");

    /* A no-CRC record can still be measured: the sync rate is the whole
       measurement then, which is exactly the RX_FRAME_ACQ case (preamble
       only, nothing to demodulate). */
    frame_meter_reset (m);
    for (int i = 0; i < 100; i++)
      frame_meter_add (m, i % 4 != 0, -1);
    DP_REQUIRE_MSG (frame_meter_get_errors (m) == 25,
                    "with no CRC, errors are exactly the misses");
    DP_REQUIRE_MSG (frame_meter_sync_miss (m).errors == 25, "and so is the "
                                                            "miss count");
  }

  /* ── the stopping rule ────────────────────────────────────────────────── */
  {
    frame_meter_reset (m);
    DP_REQUIRE_MSG (!frame_meter_get_enough (m), "nothing counted yet");
    for (int i = 0; i < 9; i++)
      frame_meter_add (m, 1, 0);
    DP_REQUIRE_MSG (!frame_meter_get_enough (m), "9 of 10 errors is not yet");
    frame_meter_add (m, 1, 0);
    DP_REQUIRE_MSG (frame_meter_get_enough (m), "10 errors is enough");

    /* `enough` is about ERRORS, not frames -- that is the whole point of the
       stopping rule, and it is what makes ber_confidence's interval the right
       one. A meter that stopped on frames would have precision that depended
       on the rate it was measuring. */
    frame_meter_reset (m);
    for (int i = 0; i < 10000; i++)
      frame_meter_add (m, 1, 1);
    DP_REQUIRE_MSG (!frame_meter_get_enough (m),
                    "10 000 clean frames is never 'enough' -- no errors");
  }

  /* ── reset clears the counters and keeps the configuration ────────────── */
  {
    frame_meter_reset (m);
    DP_REQUIRE_MSG (frame_meter_get_frames (m) == 0
                        && frame_meter_get_errors (m) == 0
                        && frame_meter_get_sync_detected (m) == 0
                        && frame_meter_get_crc_passed (m) == 0,
                    "reset clears every counter");
    frame_meter_add (m, 1, 0);
    DP_REQUIRE_MSG (frame_meter_fer (m).conf == 0.99,
                    "and leaves the configuration alone");
  }

  /* ── the state triplet: a record resumes across a process boundary ────── */
  {
    frame_meter_reset (m);
    for (int i = 0; i < 37; i++)
      frame_meter_add (m, i % 3 != 0, i % 5 ? 1 : 0);

    size_t   n    = frame_meter_state_bytes (m);
    uint8_t *blob = malloc (n);
    DP_CHECK (blob != NULL);
    frame_meter_get_state (m, blob);

    frame_meter_state_t *r = frame_meter_create (10, 0.99);
    DP_CHECK (r != NULL);
    DP_REQUIRE_MSG (frame_meter_set_state (r, blob) == DP_OK, "restore");
    DP_REQUIRE_MSG (frame_meter_get_frames (r) == frame_meter_get_frames (m)
                        && frame_meter_get_errors (r)
                               == frame_meter_get_errors (m)
                        && frame_meter_get_sync_detected (r)
                               == frame_meter_get_sync_detected (m)
                        && frame_meter_get_crc_passed (r)
                               == frame_meter_get_crc_passed (m),
                    "every counter survives the round trip");

    /* Continuing the restored meter must match continuing the original --
       the point of the triplet is that a record can be split across
       processes and still add up to one measurement. */
    frame_meter_add (m, 1, 0);
    frame_meter_add (r, 1, 0);
    DP_REQUIRE_MSG (frame_meter_get_errors (r) == frame_meter_get_errors (m),
                    "and accumulation continues identically");

    ((uint8_t *)blob)[0] ^= 0xFFu;
    DP_REQUIRE_MSG (frame_meter_set_state (r, blob) == DP_ERR_INVALID,
                    "a clobbered envelope is REJECTED, never reinterpreted");
    free (blob);
    frame_meter_destroy (r);
  }

  frame_meter_destroy (m);
  DP_TEST_END ("frame_meter_core");
}
