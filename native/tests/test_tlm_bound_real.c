/*
 * test_tlm_bound_real.c — the bound, checked against a REAL emitter.
 *
 * dp_tlm_block_bound() claims `probe_count * block_samples` is a genuine upper
 * bound on what a block can emit. That claim came from reading every emit site
 * across every object with a *_set_telemetry, and a code audit is exactly the
 * kind of evidence that is right until someone edits one of those files.
 *
 * test_tlm_capture.c exercises the machinery by emitting synthetically at
 * exactly the bound, which proves the capture but assumes the bound. This file
 * closes the loop the other way: drive an actual DSP object and measure what
 * it really emits.
 *
 * carrier_nda is the deliberate choice. Of the eleven instrumented objects it
 * is the ONLY unconditional per-input emitter -- its flush is not gated on a
 * strobe, so it fires four probes every single input sample, plus a fifth
 * (the embedded AGC's gain) every eighth. Anything that would break the bound
 * breaks it here first.
 *
 * The strong assertion is `count <= bound`, not `dropped == 0`. Zero drops
 * would also hold if the ring merely happened to be roomy; only the measured
 * emission count can falsify the audit.
 */
#include "carrier_nda/carrier_nda_core.h"
#include "dp_test.h"
#include "dp_tlm_capture/dp_tlm_capture_core.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* One QPSK-ish block with a carrier offset, so the loop actually tracks and
   every probe has something to say. Content does not matter to the bound --
   only that the object runs its real code path. */
static void
fill (float complex *x, size_t n, size_t sps, double foff)
{
  for (size_t i = 0; i < n; i++)
    {
      size_t sym = (i / sps) & 3u;
      double ph  = M_PI_4 + (double)sym * M_PI_2;
      double rot = 2.0 * M_PI * foff * (double)i;
      x[i]       = (float complex) (cos (ph + rot) + I * sin (ph + rot));
    }
}

int
main (void)
{
  const size_t SPS   = 8;
  const size_t BLOCK = 512;
  const int    NBLK  = 40;

  carrier_nda_state_t *c = carrier_nda_create (0.01, 0.707, 0.0, SPS, 4, 4);
  dp_tlm_t            *t = dp_tlm_create (1);
  DP_CHECK (c && t);
  if (!c || !t)
    return 1;

  /* decim = 1: every event on every probe, the densest the object can be. */
  DP_CHECK (carrier_nda_set_telemetry (c, t, "car", 1) == DP_OK);

  size_t probes = dp_tlm_probe_count (t);
  /* 4, all its own. Was 5 until gh-657 retired the embedded arm AGC and with
     it the forwarded "car.agc.gain_db". If this number moves, the bound moves
     with it -- which is the point of deriving it rather than hard-coding. */
  DP_CHECK (probes == 4);

  size_t bound = dp_tlm_block_bound (t, BLOCK);
  DP_CHECK (bound == probes * BLOCK);

  float complex *x = malloc (BLOCK * sizeof *x);
  float complex *y = malloc (BLOCK * sizeof *y);
  DP_CHECK (x && y);
  if (!x || !y)
    return 1;
  fill (x, BLOCK, SPS, 1.0 / 512.0);

  dp_tlm_capture_t *cap = dp_tlm_capture_open (t, BLOCK, NULL, NULL);
  DP_CHECK (cap != NULL);

  /* Measure the WORST block, not just the total: an object that averaged
     under the bound while overshooting on one block would still overflow a
     ring sized to the bound, and a total-only check would miss it. */
  size_t prev = 0, worst = 0;
  for (int blk = 0; blk < NBLK; blk++)
    {
      dp_tlm_set_now (t, (uint64_t)blk * BLOCK); /* boundary */
      size_t got = dp_tlm_capture_count (cap);
      if (blk > 0 && got - prev > worst)
        worst = got - prev;
      prev = got;
      (void)carrier_nda_steps (c, x, BLOCK, y, BLOCK);
    }
  DP_CHECK (dp_tlm_capture_close (cap) == DP_OK);

  size_t total = dp_tlm_capture_count (cap);
  size_t tail  = total - prev;
  if (tail > worst)
    worst = tail;

  printf ("carrier_nda: %zu probes, block %zu -> bound %zu,"
          " worst observed %zu (%.1f%% of bound), total %zu\n",
          probes, BLOCK, bound, worst, 100.0 * (double)worst / (double)bound,
          total);

  /* THE assertion: a real object, at its densest setting, never exceeded the
     bound in any single block. */
  DP_CHECK (worst <= bound);
  DP_CHECK (total <= (size_t)NBLK * bound);
  /* And, because the ring was sized to the bound, nothing was lost. */
  DP_CHECK (dp_tlm_capture_dropped (cap) == 0);
  DP_CHECK (dp_tlm_dropped (t) == 0);

  /* Tightness, which is what makes the inequality above mean something. The
     audit predicts this object emits 4 probes per input UNCONDITIONALLY and
     nothing else -- 2048 records for a 512-sample block, which is the bound
     exactly. The measurement agrees to the record, so the bound is not merely
     satisfied, it is understood.

     It used to predict 4.125 per input: 4 unconditional plus the embedded arm
     AGC's gain every AGC_DECIM_DEFAULT (8) inputs. gh-657 removed that AGC,
     which is precisely the kind of density change this equality exists to
     make someone look at -- it went red, and the audit was updated rather
     than the assertion relaxed.

     Two assertions, doing different jobs. The floor catches the ratio
     collapsing -- a silent object satisfies `worst <= bound` while proving
     nothing at all. The equality pins the prediction itself, and it is
     deliberately brittle: if this object's emission density moves again, this
     goes red, and that is the correct outcome. */
  DP_CHECK (total > 0);
  DP_CHECK (worst > bound / 2);
  DP_CHECK (worst == 4 * BLOCK); /* the audit's exact prediction */

  dp_tlm_capture_destroy (cap);
  dp_tlm_destroy (t);
  carrier_nda_destroy (c);
  free (x);
  free (y);

  DP_TEST_END ("test_tlm_bound_real");
}
