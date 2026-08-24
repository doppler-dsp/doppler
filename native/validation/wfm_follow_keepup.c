/**
 * @file wfm_follow_keepup.c
 * @brief Does a following reader keep up, and what does the poll slice cost?
 *
 * docs/design/end-of-capture.md §7 lists three quantities the contract
 * asserts and nothing measures. Two of them are here; the third (a bounded
 * grace) is a policy nobody has had to choose yet.
 *
 * This is also the disk bullet of doppler#975 -- *"the disk reader's
 * short-read behaviour against a writer still appending"* -- which io-
 * termination §5 has carried as prose across all three transports.
 *
 * ## What is measured, and why these two
 *
 * **BACKLOG** -- whole samples still readable the instant a read returns.
 * This is "does the reader keep up" stated as a number. Near zero means the
 * consumer drains as fast as the producer fills. A backlog that CLIMBS with
 * the run is the reader losing the race, with the file silently absorbing
 * the difference -- the one failure mode the disk transport hides, because
 * unlike a ring it never drops and never signals.
 *
 * **WAIT** -- wall time a read spent parked before it had anything. On a
 * starved reader this is the poll slice, and it is what caps throughput at
 * one block per slice. It is the cost of the slice made visible.
 *
 * Neither is instrumented in the reader itself. That was tried and reverted:
 * `dp_tlm_core.h` pulls in `buffer/buffer.h`, which defines `_GNU_SOURCE`
 * at its own line 50 -- too late for any translation unit that reached a
 * libc header first -- so putting it in a public header exports an include-
 * ORDER constraint to every consumer. A harness owning both ends can
 * compute both quantities directly, which is what this does.
 *
 * ## Reading the table
 *
 * `fill` is the producer's duty cycle: the fraction of real time it spends
 * producing. At fill << 1 the reader should be idle-waiting with ~0 backlog.
 * The number to watch is `backlog_end` against `backlog_max` -- a reader
 * that keeps up returns both near zero whatever the fill.
 *
 * Usage:
 *   wfm_follow_keepup           full sweep, prints the table
 *   wfm_follow_keepup --check   fast CI gate: the reader keeps up, and a
 *                               starved read waits rather than reporting EOF
 */
#include "wfm_reader/wfm_reader_core.h"
#include "wfm_writer/wfm_writer_core.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PATH "validate_wfm_follow.blue"
#define FS 2.4e6
#define STYPE 3 /* ci16 */
#define BLK 4096

static double
now_s (void)
{
  struct timespec ts;
  clock_gettime (CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static void
nap_ms (long ms)
{
  struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
  nanosleep (&ts, NULL);
}

typedef struct
{
  size_t n_written, n_read;
  double backlog_max, backlog_end, wait_ms_max, elapsed_s;
} result_t;

/* One run of `rounds`. Each round the producer writes `w_blocks` and the
   consumer is allowed `r_reads` reads -- a RATE RATIO, which is the whole
   point. Draining to empty every round would make backlog identically zero
   whatever the producer does, and a metric that cannot show the bad case
   measures nothing.

   Single-threaded and interleaved rather than two threads: a harness that
   measures backlog wants the two loops in a KNOWN order, not a race. */
static result_t
run_once (int rounds, int w_blocks, int r_reads, long gap_ms,
          uint32_t slice_budget_ms)
{
  result_t out = { 0, 0, 0.0, 0.0, 0.0, 0.0 };
  FILE    *fp  = fopen (PATH, "wb+");
  if (!fp)
    return out;
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_BLUE, STYPE, 0, FS, 0.0, 0, 0.0);
  if (!w)
    {
      fclose (fp);
      return out;
    }
  wfm_writer_flush (w); /* header down, so the reader detects BLUE */

  wfm_reader_state_t *r = wfm_reader_create (PATH, STYPE, 0);
  if (!r)
    {
      wfm_writer_close (w);
      fclose (fp);
      return out;
    }
  wfm_reader_set_follow_timeout_ms (r, slice_budget_ms);

  float _Complex src[BLK], dst[BLK];
  for (size_t i = 0; i < BLK; i++)
    src[i] = (float)(0.7 * sin (0.01 * (double)i));

  double t0 = now_s ();
  for (int k = 0; k < rounds; k++)
    {
      for (int b = 0; b < w_blocks; b++)
        out.n_written += wfm_writer_write (w, src, BLK);
      wfm_writer_flush (w);
      if (gap_ms)
        nap_ms (gap_ms);

      for (int i = 0; i < r_reads; i++)
        {
          double t      = now_s ();
          size_t g      = wfm_reader_read_follow (r, BLK, dst, BLK);
          double waited = (now_s () - t) * 1e3;
          out.n_read += g;
          if (g == 0 && waited > out.wait_ms_max)
            out.wait_ms_max = waited; /* a starved read: the slice's cost */
        }
      double backlog = (double)(out.n_written - out.n_read);
      if (backlog > out.backlog_max)
        out.backlog_max = backlog;
    }
  out.backlog_end = (double)(out.n_written - out.n_read);
  out.elapsed_s   = now_s () - t0;

  wfm_reader_destroy (r);
  wfm_writer_close (w);
  fclose (fp);
  remove (PATH);
  return out;
}

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);

  if (check)
    {
      /* Two claims, both cheap. (1) the reader keeps up: nothing is left
         behind. (2) a starved read WAITS rather than reporting the capture
         over -- the defect this whole design exists to remove. */
      result_t r = run_once (8, 1, 2, 2, 40);
      if (r.n_read != r.n_written || r.backlog_end != 0.0)
        {
          printf ("FAIL: wrote %zu read %zu backlog_end %.0f\n", r.n_written,
                  r.n_read, r.backlog_end);
          return 1;
        }
      if (r.wait_ms_max < 20.0)
        {
          printf ("FAIL: a starved read returned in %.1f ms -- it did not"
                  " wait out its budget\n",
                  r.wait_ms_max);
          return 1;
        }
      printf ("wfm_follow_keepup --check: OK (%zu samples, backlog 0,"
              " starved read waited %.0f ms)\n",
              r.n_read, r.wait_ms_max);
      return 0;
    }

  printf ("Following reader, keep-up and poll cost\n");
  printf ("  block = %d samples, budget = 40 ms\n\n", BLK);
  printf ("  %-6s %-6s %-8s %10s %12s %12s %10s\n", "w/rnd", "r/rnd",
          "verdict", "written", "backlog_max", "backlog_end", "wait_ms");
  printf ("  %-6s %-6s %-8s %10s %12s %12s %10s\n", "-----", "-----",
          "-------", "----------", "------------", "------------",
          "----------");

  /* The reader is provisioned at, above, and BELOW the producer's rate.
     The last two rows are the ones that make the metric trustworthy: if
     backlog did not grow there, it could not detect a reader falling
     behind anywhere. */
  const int W[] = { 1, 1, 2, 4 };
  const int R[] = { 2, 1, 1, 1 };
  for (size_t i = 0; i < sizeof W / sizeof W[0]; i++)
    {
      result_t r = run_once (16, W[i], R[i], 1, 40);
      printf ("  %-6d %-6d %-8s %10zu %12.0f %12.0f %10.1f\n", W[i], R[i],
              R[i] >= W[i] ? "keeps up" : "behind", r.n_written, r.backlog_max,
              r.backlog_end, r.wait_ms_max);
    }
  printf ("\n  A reader provisioned at or above the producer's rate ends"
          " at backlog 0.\n");
  printf ("  Under-provisioned, backlog grows without bound -- the file"
          " absorbs it\n  silently, which is the disk transport's one"
          " hidden failure mode.\n");
  printf ("  wait_ms is the poll slice's cost, paid only when starved.\n");
  return 0;
}
