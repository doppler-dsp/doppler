/*
 * test_receiver_seq.c -- the receiver example's dropped-frame count
 * survives a sequence reset (#2017).
 *
 * native/examples/receiver.c counts the frames missing between consecutive
 * header `sequence` numbers. It subtracted unconditionally, so a publisher
 * restarting at 0 -- or one repeated frame -- wrapped the uint64 counter
 * to about 1.8e19 dropped frames. Proven by sabotage: restoring the
 * unconditional `seq - last - 1` turns the reset and repeat checks red.
 */
#include "dp_test.h"

#include "receiver_seq.h"
#include <stdint.h>

int
main (void)
{
  /* A forward jump counts the frames between; the next frame counts none. */
  DP_CHECK (receiver_frames_missing (4, 5) == 0);
  DP_CHECK (receiver_frames_missing (4, 7) == 2);

  /* A repeat and a backwards jump (a restarted publisher) count none. */
  DP_CHECK (receiver_frames_missing (4, 4) == 0);
  DP_CHECK (receiver_frames_missing (1000, 0) == 0);

  /* The example's loop over a stream with one real gap (2 -> 5) and a
     publisher restart (5 -> 0): two dropped frames, not ~1.8e19. */
  {
    static const uint64_t seqs[]  = { 0, 1, 2, 5, 0, 1, 2, 3 };
    uint64_t              dropped = 0, last = 0;
    for (size_t i = 0; i < sizeof seqs / sizeof *seqs; i++)
      {
        if (i > 0)
          dropped += receiver_frames_missing (last, seqs[i]);
        last = seqs[i];
      }
    DP_CHECK (dropped == 2);
  }

  DP_TEST_END ("test_receiver_seq");
}
