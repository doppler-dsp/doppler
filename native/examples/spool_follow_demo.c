/*
 * spool_follow_demo.c — spooling an endless stream to disk while reading
 * it back, and stopping both halves cleanly with Ctrl+C.
 *
 * This is the topology docs/design/end-of-capture.md is written for:
 *
 *     a stream we do not control       we own both of these
 *     ────────────────────────    ┌──────────────────────────────┐
 *       (here: a signal generator)┼─► writer ──► capture.blue    │
 *                                 │                  │           │
 *                                 │                  ▼           │
 *                                 │            follow reader ──► "DSP"
 *                                 └──────────────────────────────┘
 *
 * The stream never ends on its own, so only you can stop it. Press Ctrl+C
 * and watch the ending propagate THROUGH the file rather than around it:
 *
 *   1. the handler sets one flag
 *   2. the WRITER stops pulling, flushes, and closes -- and close() is
 *      what patches the BLUE header's data_size. That patch is the MARKER.
 *   3. the READER keeps draining while whole samples remain, sees the
 *      marker, drains to it, and reports end-of-capture
 *
 * Step 3 is the part worth watching. The reader does NOT exit on the flag:
 * if it did, it would discard a tail that is already safely on disk, by an
 * amount that varies per run because it is a race between two loops. It
 * exits because the capture ended, which is a fact the writer wrote down.
 *
 * The demo self-validates: it asserts every sample the writer accepted
 * came back out of the reader. Exit 0 means that held.
 *
 * Usage:
 *   ./build/native/examples/spool_follow_demo        # Ctrl+C when bored
 *   ./build/native/examples/spool_follow_demo 3      # or stop itself at 3s
 */

#include "dp_interrupt.h"
#include "wfm_reader/wfm_reader_core.h"
#include "wfm_writer/wfm_writer_core.h"

#include <complex.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define PATH "spool_follow_demo.blue"

/* How long the run lasts when nobody says. An example must TERMINATE: the
   `make test-examples-c` gate runs every one of them with no arguments and
   no terminal, so a demo that waits for a real Ctrl+C is a demo that hangs
   until the gate's 120 s timeout and reports FAIL. This one did, on every
   machine and in the glibc 2.28 job, and it was the unexplained half of
   `make gates` being red.

   Long enough to be a real run at the writer's 4096-samples-per-20 ms pace
   (~15 blocks, ~61k samples) and short enough that twelve examples in a row
   still finish quickly. Pass a number of seconds to override, or 0 to wait
   for an actual Ctrl+C -- which is the interactive demonstration, and stays
   one keystroke away. */
#define DEFAULT_RUN_S 0.3
#define FS 2.4e6
#define BLK 4096
#define STYPE 3 /* ci16 */

static size_t n_written = 0; /* what the writer accepted */
static size_t n_read    = 0; /* what the reader got back */

static void
nap_ms (long ms)
{
  struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
  nanosleep (&ts, NULL);
}

/* Stand-in for a stream we do not control: it produces on its own schedule
   and has no end. Paced so the reader is FASTER than the source, which is
   the interesting case -- the reader spends its life waiting. */
static void *
writer_thread (void *arg)
{
  FILE *fp = (FILE *)arg;
  /* total_samples = 0: an unbounded run declares no length, so data_size
     starts as a placeholder and close() is what makes it real. */
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_BLUE, STYPE, 0, FS, 0.0, 0, 0.0);
  if (!w)
    return NULL;
  /* Land the 512-byte header immediately. Until it is on disk the file is
     empty, and an empty file auto-detects as RAW -- which has no header and
     therefore no end-of-capture marker, so a reader that opened during that
     window would wait for a marker that can never arrive. */
  wfm_writer_flush (w);

  float _Complex blk[BLK];
  size_t k = 0;
  while (!dp_interrupted ())
    {
      for (size_t i = 0; i < BLK; i++, k++)
        blk[i] = (float)(0.7 * sin (0.01 * (double)k))
                 + (float)(0.7 * cos (0.013 * (double)k)) * I;
      n_written += wfm_writer_write (w, blk, BLK);
      /* Flush per block: it makes the samples OBSERVABLE to the reader
         without ending the capture, and because write() emits whole
         samples it also leaves the file on a sample boundary. */
      wfm_writer_flush (w);
      nap_ms (20);
    }

  /* The stop reached us. Finish the capture properly -- this close() is
     what the reader is waiting for. */
  wfm_writer_close (w);
  printf ("writer : stopped, capture closed after %zu samples\n", n_written);
  return NULL;
}

static void *
reader_thread (void *arg)
{
  (void)arg;
  /* Open only once the capture really is BLUE. `create` SUCCEEDS on an
     empty file -- it falls back to raw, which is the right answer for an
     unrecognised file and the wrong one here. */
  wfm_reader_state_t *r = NULL;
  for (int tries = 0; tries < 500; tries++)
    {
      r = wfm_reader_create (PATH, STYPE, 0);
      if (r && wfm_reader_get_file_type (r) == WFM_FT_BLUE)
        break;
      if (r)
        wfm_reader_destroy (r);
      r = NULL;
      nap_ms (10);
    }
  if (!r)
    {
      printf ("reader : capture never became readable\n");
      return NULL;
    }

  /* How this reader learns a stop was requested. Injected rather than
     hard-wired: a capture reader has no business depending on the process
     interrupt primitive, so the caller says what "stop" means. */
  wfm_reader_set_stop_fn (r, dp_interrupted);
  /* Budgets left at 0 = wait forever, which is the right answer for a
     stream with no end: any finite budget would fire during an ordinary
     quiet patch and report an ending that has not happened. */

  float _Complex blk[BLK];
  double acc = 0.0;
  size_t got;
  while ((got = wfm_reader_read_follow (r, BLK, blk, BLK)) > 0)
    {
      /* Stand-in for the DSP: mean power over the block. */
      for (size_t i = 0; i < got; i++)
        acc += (double)(crealf (blk[i]) * crealf (blk[i])
                        + cimagf (blk[i]) * cimagf (blk[i]));
      n_read += got;
    }

  const char *why = "?";
  switch (wfm_reader_get_ending (r))
    {
    case WFM_FOLLOW_EOF:
      why = "eof (the writer closed and said so)";
      break;
    case WFM_FOLLOW_TIMEOUT:
      why = "timeout";
      break;
    case WFM_FOLLOW_INTERRUPTED:
      why = "interrupted (tail may be short)";
      break;
    default:
      why = "none";
      break;
    }
  printf ("reader : stopped, %zu samples, mean power %.4f, ending = %s\n",
          n_read, n_read ? acc / (double)n_read : 0.0, why);
  wfm_reader_destroy (r);
  return NULL;
}

int
main (int argc, char **argv)
{
  /* Default is a bounded run, NOT "wait forever": see DEFAULT_RUN_S. An
     explicit 0 asks for the interactive form. */
  double limit_s = (argc > 1) ? atof (argv[1]) : DEFAULT_RUN_S;

  /* Install EARLY -- before the threads exist. A signal arriving before
     the handler does terminates the process. */
  dp_interrupt_on_signal (SIGINT);

  FILE *fp = fopen (PATH, "wb+");
  if (!fp)
    {
      perror ("fopen");
      return 1;
    }

  setvbuf (stdout, NULL, _IOLBF, 0);
  if (limit_s > 0)
    printf ("spooling to %s -- stopping after %.2f s (Ctrl+C stops it "
            "sooner; pass 0 to wait for one)\n",
            PATH, limit_s);
  else
    printf ("spooling to %s -- press Ctrl+C to stop\n", PATH);

  pthread_t wt, rt;
  pthread_create (&wt, NULL, writer_thread, fp);
  pthread_create (&rt, NULL, reader_thread, NULL);

  if (limit_s > 0)
    {
      nap_ms ((long)(limit_s * 1000.0));
      dp_interrupt (); /* exactly what the Ctrl+C handler does */
    }

  pthread_join (wt, NULL);
  pthread_join (rt, NULL);
  fclose (fp);
  remove (PATH);

  /* The claim this demo exists to make: a stop is not a discard. */
  if (n_read != n_written)
    {
      printf ("FAIL   : wrote %zu, read %zu -- the tail was lost\n", n_written,
              n_read);
      return 1;
    }
  printf ("ok     : every one of the %zu samples written came back out\n",
          n_written);
  return 0;
}
