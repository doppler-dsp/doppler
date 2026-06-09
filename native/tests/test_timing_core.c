/*
 * test_timing_core.c — sample-clock pacing + stamping.
 *
 * Timing assertions use generous bounds (CI is not an RTOS): the meaningful
 * checks are that pacing actually waits ~n/fs, that the stamp arithmetic is
 * exact, and that underruns are counted / resync re-anchors.
 */
#include "timing/timing_core.h"

#include <inttypes.h>
#include <stdio.h>

#define CHECK(c, m)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(c))                                                               \
        {                                                                     \
          fprintf (stderr, "FAIL: %s\n", m);                                  \
          return 1;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/* The stamp is pure arithmetic off a fixed epoch: two stamps n samples apart
   differ by exactly round(n / fs * 1e9) ns, independent of wall time. */
static int
test_stamp_exact (void)
{
  dp_sample_clock_t c;
  dp_sample_clock_init (&c, 1000.0, 0); /* 1 kHz → 1 sample = 1 ms */
  uint64_t t0 = dp_sample_clock_stamp (&c);
  c.n         = 500; /* poke the count directly to test the math */
  uint64_t t1 = dp_sample_clock_stamp (&c);
  CHECK (t1 - t0 == 500000000ULL, "500 samples @1kHz == 0.5 s");
  c.n = 1000;
  CHECK (dp_sample_clock_stamp (&c) - t0 == 1000000000ULL, "1000 == 1 s");
  return 0;
}

/* Pacing N samples at fs takes ~N/fs seconds. fs=1e5, 20000 samples => 0.2 s.
   Lower bound proves it waited; upper bound is loose for loaded runners. */
static int
test_pace_waits (void)
{
  dp_sample_clock_t c;
  dp_sample_clock_init (&c, 100000.0, 0); /* 100 kS/s */
  uint64_t start = dp_mono_ns ();
  for (int i = 0; i < 4; i++)
    dp_sample_clock_pace (&c, 5000); /* 4 * 5000 / 1e5 = 0.2 s */
  double elapsed = (double)(dp_mono_ns () - start) / 1e9;
  CHECK (elapsed > 0.18, "paced run waited at least ~0.2 s");
  CHECK (elapsed < 0.6, "paced run was not absurdly slow");
  CHECK (c.underruns == 0, "no underruns on an idle pacer");
  CHECK (c.n == 20000, "cumulative sample count tracked");
  return 0;
}

/* An impossibly high rate makes every deadline already past → underruns,
   and the worst lateness is recorded. */
static int
test_underrun_counted (void)
{
  dp_sample_clock_t c;
  dp_sample_clock_init (&c, 1e12, 0); /* 1 TS/s: 1000 samples = 1 ns */
  for (int i = 0; i < 5; i++)
    dp_sample_clock_pace (&c, 1000);
  CHECK (c.underruns >= 1, "behind-real-time blocks count as underruns");
  CHECK (c.max_late_ns > 0, "worst lateness recorded");
  return 0;
}

/* With resync set, the epoch advances on underrun so the clock tracks "now"
   instead of accumulating an ever-growing backlog. */
static int
test_resync (void)
{
  dp_sample_clock_t c;
  dp_sample_clock_init (&c, 1e12, 1); /* resync on */
  uint64_t epoch0 = c.epoch_mono_ns;
  for (int i = 0; i < 5; i++)
    dp_sample_clock_pace (&c, 1000);
  CHECK (c.epoch_mono_ns > epoch0, "resync re-anchored the epoch forward");
  return 0;
}

/* reset() returns a clock to n=0 with cleared counters. */
static int
test_reset (void)
{
  dp_sample_clock_t c;
  dp_sample_clock_init (&c, 1e12, 0);
  dp_sample_clock_pace (&c, 1000);
  dp_sample_clock_reset (&c);
  CHECK (c.n == 0, "reset zeroes the sample count");
  CHECK (c.underruns == 0, "reset clears underruns");
  return 0;
}

int
main (void)
{
  if (test_stamp_exact ())
    return 1;
  if (test_pace_waits ())
    return 1;
  if (test_underrun_counted ())
    return 1;
  if (test_resync ())
    return 1;
  if (test_reset ())
    return 1;
  printf ("test_timing_core: all passed\n");
  return 0;
}
