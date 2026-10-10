/*
 * receiver_seq.h -- the receiver example's dropped-frame count, as one unit
 * its main() and a test both call (#2017).
 */
#ifndef RECEIVER_SEQ_H
#define RECEIVER_SEQ_H

#include <stdint.h>

/* Frames missing between two consecutive frames' header `sequence`
   numbers: the ones skipped when the sequence moved forward. A sequence
   that repeats or goes BACKWARDS adds nothing. A SUB socket's delivery is
   at-most-once (docs/design/streaming.md, section 9), so no frame comes
   twice: a repeat or a backward step means the publisher restarted,
   counting from 0 again, or a second publisher shares the address. Neither
   is a dropped frame, and `seq - last - 1` in unsigned arithmetic would
   wrap either one to about 1.8e19 of them. */
static inline uint64_t
receiver_frames_missing (uint64_t last_seq, uint64_t seq)
{
  return seq > last_seq ? seq - last_seq - 1 : 0;
}

/* The running count over a stream: the state a receiver keeps between
   frames. Zero-initialise it (`receiver_seq_t s = { 0 };`). */
typedef struct
{
  uint64_t last;      /* the previous frame's sequence */
  int      have_last; /* 0 until the first frame: nothing to count from */
  uint64_t dropped;   /* frames skipped so far */
} receiver_seq_t;

/* Take one frame's sequence. The first frame anchors the count and adds
   nothing, so a receiver that joins mid-stream (first frame 1,000,000) has
   dropped nothing yet. Each later frame adds receiver_frames_missing from
   the previous one, and becomes the anchor whichever way it moved: after a
   restart the count follows the new numbering, so a gap there counts too.

   `dropped` is a LOWER bound across a restart. The frames a restarting
   publisher sent before it went down but after the last one received, and
   any it sent after coming up but before the first one received, leave no
   gap in either numbering, so nothing counts them. */
static inline void
receiver_seq_feed (receiver_seq_t *s, uint64_t seq)
{
  if (s->have_last)
    s->dropped += receiver_frames_missing (s->last, seq);
  s->last      = seq;
  s->have_last = 1;
}

#endif /* RECEIVER_SEQ_H */
