/*
 * graceful_shutdown_demo.c — stopping a generator from C.
 *
 * `doppler_wfmgen()` is the wfmgen tool as an ordinary C call, for a
 * program that wants the generator's behaviour without spawning a process.
 * A continuous run has no natural end, so something has to stop it:
 * `dp_interrupt()` asks every blocking wait and every generate loop in the
 * process to finish.
 *
 * That call is all a supervisor thread, a test harness, or an embedded
 * host needs — no signal required, and the generator behaves identically
 * whether the flag was set by a handler or by a call.
 *
 * Stopping cleanly matters for the output, not just the process: the
 * capture's header carries its final sample count and is written when the
 * file is closed. A run that is killed leaves a file no reader can open;
 * a run that is interrupted leaves a shorter capture that is still valid.
 *
 * Usage:
 *   ./build/native/examples/graceful_shutdown_demo
 *
 * See docs/design/io-termination.md for the wider contract.
 */

#include "dp_interrupt.h"
#include "wfm/wfmgen.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Short on purpose. Unpaced the generator writes on the order of a
   gigabyte a second, so the run is bounded by TIME rather than by pacing
   it down: the CPU stays pegged, which is the condition worth stopping
   under, and the capture stays a manageable size. */
#define RUN_MS 50

#define OUT_PATH "graceful_shutdown_demo.cf32"

/* Stamped the instant the stop is requested, so the latency measured is
   request-to-return and not the run length subtracted from a total. */
static struct timespec t_request;

static void *
stopper (void *arg)
{
  (void)arg;
  struct timespec nap = { RUN_MS / 1000, (RUN_MS % 1000) * 1000L * 1000L };
  nanosleep (&nap, NULL);

  printf ("  stopping it with dp_interrupt()\n");
  fflush (stdout);
  clock_gettime (CLOCK_MONOTONIC, &t_request);
  dp_interrupt ();
  return NULL;
}

int
main (void)
{
  printf ("doppler — stopping a generator from C\n");
  printf ("  writing : %s\n", OUT_PATH);
  printf ("  running : %d ms, unpaced (CPU pegged)\n\n", RUN_MS);

  /* The flag is process-wide and stays set until cleared, so start from a
     known state. */
  dp_resume ();

  pthread_t th;
  if (pthread_create (&th, NULL, stopper, NULL) != 0)
    {
      fprintf (stderr, "cannot start the stopper thread\n");
      return 1;
    }

  /* Unpaced: the generator runs flat out and the CPU stays pegged for the
     whole run. That is the demanding case for a stop -- the loop is busy
     generating rather than idling in a wait, so a stop that is prompt here
     is prompt anywhere. */
  char           *args[] = { (char *)"wfmgen", (char *)"--continuous",
                             (char *)"--output", (char *)OUT_PATH, NULL };
  int             rc     = doppler_wfmgen (4, args);
  struct timespec t_returned;
  clock_gettime (CLOCK_MONOTONIC, &t_returned);

  pthread_join (th, NULL);
  dp_resume ();

  if (rc != 0)
    {
      fprintf (stderr, "\ndoppler_wfmgen returned %d\n", rc);
      return 1;
    }

  double stop_ms = (double)(t_returned.tv_sec - t_request.tv_sec) * 1e3
                   + (double)(t_returned.tv_nsec - t_request.tv_nsec) / 1e6;

  printf ("\nstopped cleanly, exit 0 — %.2f ms from the request to the "
          "generator returning,\nwith the CPU pegged the whole time\n",
          stop_ms);

  /* A stop that takes as long as the run is not a stop. The bound is
     generous next to the ~100 ms a blocking wait may take to notice. */
  if (stop_ms > 2000.0)
    {
      fprintf (stderr, "the generator took %.0f ms to stop\n", stop_ms);
      return 1;
    }

  /* The capture must also be VALID, not merely present. The header
     carries the final sample count and is written when the file is
     closed, so a killed process leaves a file no reader can open while an
     interrupted one leaves a shorter capture that is still good. */
  FILE *f = fopen (OUT_PATH, "rb");
  if (!f)
    {
      fprintf (stderr, "no capture was written\n");
      return 1;
    }
  fseek (f, 0, SEEK_END);
  long bytes = ftell (f);
  fclose (f);
  remove (OUT_PATH);

  if (bytes <= 0)
    {
      fprintf (stderr, "the capture is empty\n");
      return 1;
    }
  printf ("  %.1f MB written in %d ms, and the file was closed properly\n",
          (double)bytes / 1e6, RUN_MS);
  return 0;
}
