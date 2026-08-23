/*
 * timing_core.c — sample-clock pacing + timestamping (POSIX).
 *
 * See timing_core.h for the model. The schedule is anchored at init: each
 * block's deadline is recomputed as ``epoch_mono + n/fs`` from the cumulative
 * sample count, so sleep jitter never accumulates into drift.
 *
 * The deadline offset is computed in ``long double`` as ``n / fs * 1e9`` (in
 * that order, to keep the magnitudes small) so it stays exact for the sample
 * counts a long-running stream reaches.
 *
 * track()/stamp_at() are the receive-side counterpart of pace()/stamp():
 * instead of sleeping toward a deadline it owns, a receiver reconciles its
 * clock's epoch against ground truth read off an arriving stream header, then
 * stamps arbitrary historical sample indices (e.g. several per-record
 * detections spanning different offsets within one buffered input) off that
 * same drift-free timeline.
 */
#define _POSIX_C_SOURCE 200809L

#include "timing/timing_core.h"
#include "dp_interrupt.h"

#include <errno.h>
#include <stdlib.h>
#include <time.h>

uint64_t
dp_mono_ns (void)
{
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
}

uint64_t
dp_real_ns (void)
{
  struct timespec t;
  clock_gettime (CLOCK_REALTIME, &t);
  return (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
}

/* ns offset of sample index n from the epoch: round(n / fs * 1e9). */
static uint64_t
offset_ns (uint64_t n, double fs)
{
  /* Defensive guard (gh-178 review #4): the generated SampleClock binding no
     longer rejects fs <= 0 (the retired hand binding raised "fs must be > 0").
     A non-positive or NaN fs makes n / fs infinite/NaN, and casting that to
     uint64_t is undefined behaviour — manifesting as inf deadlines (pace never
     wakes) and garbage timestamps. A real clock cannot advance without a rate,
     so the offset is simply zero: pace returns immediately, stamp stays at the
     epoch. No raise — jm bindings stay unchecked; this just refuses the UB. */
  if (!(fs > 0.0))
    return 0;
  long double secs = (long double)n / (long double)fs;
  return (uint64_t)(secs * 1e9L + 0.5L);
}

/* Sleep until the absolute monotonic instant @p target (ns). Drift-free: the
   target is fixed, so a signal-interrupted sleep simply resumes toward it --
   UNLESS the caller has been interrupted, in which case it gives up.

   That exception is load-bearing. Resuming toward a fixed deadline is the
   right pacing behaviour and it is also a wait that no signal can end: a
   handler sets a flag, and the loop that would read it is on the far side of
   this sleep. Paced against a low sample rate the sleep is seconds long, so
   `wfmgen --continuous --realtime` ignored Ctrl+C entirely while the same
   run without --realtime exited in 3 ms. Same defect as the ring's spin and
   the blocking recv, in a third place; see docs/design/io-termination.md. */
static void
sleep_until_mono_ns (uint64_t target)
{
  /* SLICED, and checked between slices -- not one sleep to the deadline.
     The obvious form checks the flag only where clock_nanosleep returns
     EINTR, which means it works when a SIGNAL happens to land and not at
     all when a caller (or another thread) simply sets the flag: no signal,
     no EINTR, no check, and the sleep serves its full interval. Worse,
     dp_interrupt_on_signal installs with SA_RESTART, under which Linux
     restarts an absolute clock_nanosleep by itself -- so even the signal
     case is not guaranteed to surface.

     Slicing makes the bound a property of the code rather than of whether
     a signal arrived, and reuses the knob that already exists for exactly
     this: dp_interrupt_latency_ms() is the worst-case delay between
     dp_interrupt() and a wait returning, whatever the wait is waiting on. */
  for (;;)
    {
      if (dp_interrupted ())
        return;

      uint64_t now = dp_mono_ns ();
      if (now >= target)
        return;

      uint64_t rem   = target - now;
      uint64_t slice = (uint64_t)dp_interrupt_latency_ms () * 1000000ULL;
      uint64_t nap   = rem < slice ? rem : slice;

      /* An absolute deadline per slice keeps pacing drift-free: the target
         never moves, so a short or interrupted nap costs nothing. */
      uint64_t        wake = now + nap;
      struct timespec ts;
      ts.tv_sec  = (time_t)(wake / 1000000000ULL);
      ts.tv_nsec = (long)(wake % 1000000000ULL);

#if defined(__linux__)
      while (clock_nanosleep (CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL)
             == EINTR)
        if (dp_interrupted ())
          return;
#else
      /* Portable fallback (e.g. macOS, which lacks clock_nanosleep): sleep
         the slice as an interval, re-reading the clock on the next pass. */
      struct timespec dur;
      dur.tv_sec  = (time_t)(nap / 1000000000ULL);
      dur.tv_nsec = (long)(nap % 1000000000ULL);
      nanosleep (&dur, NULL);
#endif
    }
}

void
dp_sample_clock_init (dp_sample_clock_t *c, double fs, int resync)
{
  c->fs            = fs;
  c->resync        = resync;
  c->n             = 0;
  c->underruns     = 0;
  c->max_late_ns   = 0;
  c->epoch_mono_ns = dp_mono_ns ();
  c->epoch_real_ns = dp_real_ns ();
  c->has_anchor    = 0;
}

double
dp_sample_clock_pace (dp_sample_clock_t *c, size_t count)
{
  c->n += (uint64_t)count;
  uint64_t deadline = c->epoch_mono_ns + offset_ns (c->n, c->fs);
  uint64_t now      = dp_mono_ns ();
  double   slack    = ((double)deadline - (double)now) / 1e9;
  if (now < deadline)
    {
      sleep_until_mono_ns (deadline);
    }
  else
    {
      uint64_t late = now - deadline;
      c->underruns++;
      if (late > c->max_late_ns)
        c->max_late_ns = late;
      if (c->resync)
        c->epoch_mono_ns += late; /* re-anchor forward, dropping the slip */
    }
  return slack;
}

uint64_t
dp_sample_clock_stamp_at (const dp_sample_clock_t *c, uint64_t n)
{
  return c->epoch_real_ns + offset_ns (n, c->fs);
}

uint64_t
dp_sample_clock_stamp (const dp_sample_clock_t *c)
{
  return dp_sample_clock_stamp_at (c, c->n);
}

int
dp_sample_clock_track (dp_sample_clock_t *c, uint64_t observed_timestamp_ns,
                       uint64_t n_at_observation, uint64_t tolerance_ns)
{
  /* Stale/out-of-order/redelivered header -- never walk the epoch
     backward (this codebase's own PUSH/PULL delivery is at-least-once and
     may redeliver to a different worker after a crash). */
  if (n_at_observation < c->n)
    return 0;

  if (!c->has_anchor)
    {
      c->epoch_real_ns
          = observed_timestamp_ns - offset_ns (n_at_observation, c->fs);
      c->n          = n_at_observation;
      c->has_anchor = 1;
      return 1;
    }

  uint64_t predicted = dp_sample_clock_stamp_at (c, n_at_observation);
  uint64_t diff      = (predicted > observed_timestamp_ns)
                           ? predicted - observed_timestamp_ns
                           : observed_timestamp_ns - predicted;
  c->n = n_at_observation; /* always advance to the latest known position */
  if (diff > tolerance_ns)
    {
      c->epoch_real_ns
          = observed_timestamp_ns - offset_ns (n_at_observation, c->fs);
      return 1;
    }
  return 0;
}

void
dp_sample_clock_reset (dp_sample_clock_t *c)
{
  dp_sample_clock_init (c, c->fs, c->resync);
}

void
dp_sample_clock_resync (dp_sample_clock_t *c)
{
  uint64_t want = c->epoch_mono_ns + offset_ns (c->n, c->fs);
  uint64_t now  = dp_mono_ns ();
  if (now > want)
    c->epoch_mono_ns += now - want; /* absorb current lateness */
}

/* ── stats snapshot for the generated SampleClock handle ─────────────────────
 */

void
dp_sample_clock_stats (const dp_sample_clock_t *c, dp_sample_clock_t *out)
{
  *out = *c;
}

/* ── opaque heap clock for the generated realtime composer stream ────────────
 */

dp_sample_clock_t *
dp_sample_clock_create (double fs, int resync)
{
  dp_sample_clock_t *c = (dp_sample_clock_t *)malloc (sizeof *c);
  if (c)
    dp_sample_clock_init (c, fs, resync);
  return c;
}

void
dp_sample_clock_destroy (dp_sample_clock_t *c)
{
  free (c);
}
