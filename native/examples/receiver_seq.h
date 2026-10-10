/*
 * receiver_seq.h -- the receiver example's dropped-frame count, as one rule
 * a test can reach (#2017).
 */
#ifndef RECEIVER_SEQ_H
#define RECEIVER_SEQ_H

#include <stdint.h>

/* Frames missing between two consecutive frames' header `sequence`
   numbers: the ones skipped when the sequence moved forward. A sequence
   that repeats or goes BACKWARDS adds nothing. A restarted publisher counts
   from 0 again and a redelivery repeats a number; neither is a dropped
   frame, and `seq - last - 1` in unsigned arithmetic would wrap either one
   to about 1.8e19 of them. */
static inline uint64_t
receiver_frames_missing (uint64_t last_seq, uint64_t seq)
{
  return seq > last_seq ? seq - last_seq - 1 : 0;
}

#endif /* RECEIVER_SEQ_H */
