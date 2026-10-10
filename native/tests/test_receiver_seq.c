/*
 * test_receiver_seq.c -- the receiver example's dropped-frame count
 * survives a sequence reset (#2017).
 *
 * native/examples/receiver.c counts the frames missing between consecutive
 * header `sequence` numbers. It subtracted unconditionally, so a publisher
 * restarting at 0 -- or one repeated frame -- wrapped the uint64 counter
 * to about 1.8e19 dropped frames. The count lives in receiver_seq.h, and
 * receiver.c's main() keeps a receiver_seq_t and feeds it each frame, so
 * these tests drive the state the example runs, not a copy of its loop.
 *
 * Each stream below is red under a plausible wrong rule, proven by
 * sabotage: no first-frame anchor (the late join reads 999999), a backward
 * step counted as a restart from 0 (the restart to 3 reads 3), and a
 * high-water mark that only re-anchors forward (the gap after a restart
 * reads 2), as well as the original unconditional `seq - last - 1`.
 */
#include "dp_test.h"

#include "receiver_seq.h"
#include <stddef.h>
#include <stdint.h>

/* The dropped count after feeding `n` sequence numbers to a fresh state. */
static uint64_t
dropped_over (const uint64_t *seqs, size_t n)
{
  receiver_seq_t s = { 0 };
  for (size_t i = 0; i < n; i++)
    receiver_seq_feed (&s, seqs[i]);
  return s.dropped;
}

#define DROPPED(a) dropped_over ((a), sizeof (a) / sizeof *(a))

int
main (void)
{
  /* The rule between two frames: a forward jump counts the frames between;
     a repeat or a backward step counts none. */
  DP_CHECK (receiver_frames_missing (4, 5) == 0);
  DP_CHECK (receiver_frames_missing (4, 7) == 2);
  DP_CHECK (receiver_frames_missing (4, 4) == 0);
  DP_CHECK (receiver_frames_missing (1000, 0) == 0);

  /* A late join: the first frame anchors the count, so joining a stream
     at 1,000,000 has dropped nothing. */
  static const uint64_t late[] = { 1000000, 1000001, 1000002 };
  DP_CHECK (DROPPED (late) == 0);

  /* A restart to a NONZERO value adds nothing: 1000 -> 3 is the publisher
     coming back, not three frames lost. */
  static const uint64_t restart[] = { 998, 999, 1000, 3, 4 };
  DP_CHECK (DROPPED (restart) == 0);

  /* A gap after a restart still counts: 9 -> 12 drops two, the restart
     12 -> 0 none, and 1 -> 3 drops one more. */
  static const uint64_t gap_after[] = { 7, 8, 9, 12, 0, 1, 3 };
  DP_CHECK (DROPPED (gap_after) == 3);

  /* The original report: one real gap (2 -> 5), then a restart (5 -> 0). */
  static const uint64_t reported[] = { 0, 1, 2, 5, 0, 1, 2, 3 };
  DP_CHECK (DROPPED (reported) == 2);

  DP_TEST_END ("test_receiver_seq");
}
