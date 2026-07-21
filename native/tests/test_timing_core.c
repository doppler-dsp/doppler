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
   Lower bound proves it waited; upper bound is loose for loaded runners. We do
   NOT assert zero underruns: on a non-realtime OS (notably the macOS CI path,
   which sleeps via nanosleep rather than clock_nanosleep ABSTIME) an idle
   pacer can legitimately fall behind once under scheduler load — that's
   exactly what the underrun counter records. The absolute schedule
   self-corrects, so the total elapsed still tracks ~0.2 s. */
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

/* stamp_at() on an arbitrary historical n matches stamp() at the same n --
   the whole point is that stamp() is just stamp_at(c, c->n). */
static int
test_stamp_at_matches_stamp (void)
{
  dp_sample_clock_t c;
  dp_sample_clock_init (&c, 1000.0, 0);
  c.n = 500;
  CHECK (dp_sample_clock_stamp_at (&c, 500) == dp_sample_clock_stamp (&c),
         "stamp_at(c, c->n) == stamp(c)");
  CHECK (dp_sample_clock_stamp_at (&c, 250)
             == dp_sample_clock_stamp_at (&c, 500) - 250000000ULL,
         "stamp_at is pure arithmetic at an arbitrary (past) n");
  return 0;
}

/* The first track() call always adopts the observation outright, regardless
   of tolerance -- a fresh clock has no real anchor to compare against. */
static int
test_track_first_call_adopts (void)
{
  dp_sample_clock_t c;
  dp_sample_clock_init (&c, 1000.0, 0);
  uint64_t observed = 1700000000000000000ULL; /* an arbitrary "real" ts */
  CHECK (dp_sample_clock_track (&c, observed, 100, 1000000ULL) != 0,
         "first track() call adopts");
  CHECK (c.has_anchor != 0, "has_anchor set after first track()");
  CHECK (dp_sample_clock_stamp_at (&c, 100) == observed,
         "adopted epoch reproduces the observed timestamp exactly");
  return 0;
}

/* Once anchored, an observation consistent with the clock's own model
   (within tolerance) does NOT re-anchor -- the whole point of tolerance is
   to absorb ordinary jitter without needlessly perturbing the epoch. */
static int
test_track_tolerance_absorbs_jitter (void)
{
  dp_sample_clock_t c;
  dp_sample_clock_init (&c, 1000.0, 0);
  uint64_t observed0 = 1700000000000000000ULL;
  dp_sample_clock_track (&c, observed0, 0, 1000000ULL);
  uint64_t epoch_after_first = c.epoch_real_ns;

  /* Predicted stamp at n=1000 is observed0 + 1s exactly; perturb it by a
     small amount well inside the 1ms tolerance. */
  uint64_t predicted = dp_sample_clock_stamp_at (&c, 1000);
  int resynced = dp_sample_clock_track (&c, predicted + 5000 /* 5us */, 1000,
                                        1000000ULL);
  CHECK (resynced == 0, "small jitter inside tolerance does not resync");
  CHECK (c.epoch_real_ns == epoch_after_first,
         "epoch unchanged when within tolerance");
  CHECK (c.n == 1000, "n still advances to the latest observation");
  return 0;
}

/* A discrepancy beyond tolerance DOES resync -- a genuine discontinuity
   (dropped samples, a real source restart) must not be silently absorbed. */
static int
test_track_resyncs_beyond_tolerance (void)
{
  dp_sample_clock_t c;
  dp_sample_clock_init (&c, 1000.0, 0);
  uint64_t observed0 = 1700000000000000000ULL;
  dp_sample_clock_track (&c, observed0, 0, 1000000ULL);

  uint64_t predicted = dp_sample_clock_stamp_at (&c, 1000);
  uint64_t skewed    = predicted + 50000000ULL; /* 50ms, past the 1ms tol */
  int      resynced  = dp_sample_clock_track (&c, skewed, 1000, 1000000ULL);
  CHECK (resynced != 0, "discrepancy beyond tolerance resyncs");
  CHECK (dp_sample_clock_stamp_at (&c, 1000) == skewed,
         "re-anchored epoch reproduces the new observation exactly");
  return 0;
}

/* A stale/out-of-order/redelivered observation (n_at_observation < c->n) is
   rejected outright -- the epoch must never walk backward. */
static int
test_track_rejects_stale (void)
{
  dp_sample_clock_t c;
  dp_sample_clock_init (&c, 1000.0, 0);
  dp_sample_clock_track (&c, 1700000000000000000ULL, 5000, 1000000ULL);
  uint64_t epoch_before = c.epoch_real_ns;
  uint64_t n_before     = c.n;

  int accepted
      = dp_sample_clock_track (&c, 1600000000000000000ULL, 100, 1000000ULL);
  CHECK (accepted == 0, "stale (older-n) observation is rejected");
  CHECK (c.epoch_real_ns == epoch_before, "epoch unchanged by a stale obs");
  CHECK (c.n == n_before, "n unchanged by a stale obs");
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
  if (test_stamp_at_matches_stamp ())
    return 1;
  if (test_track_first_call_adopts ())
    return 1;
  if (test_track_tolerance_absorbs_jitter ())
    return 1;
  if (test_track_resyncs_beyond_tolerance ())
    return 1;
  if (test_track_rejects_stale ())
    return 1;
  printf ("test_timing_core: all passed\n");
  return 0;
}
