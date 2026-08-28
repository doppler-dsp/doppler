/**
 * wfmgen_demo.c — the composed scene from C: who owns what, and when it ends.
 *
 * `wfmgen` the tool, and `doppler.wfm` the Python face, both hand you a
 * finished array. This is the layer underneath, where the caller is the one
 * holding the buffer. Four things the Python face genuinely hides:
 *
 *   1. YOU own the output. wfm_compose_execute() never allocates; it fills
 *      what you give it and tells you how much it used.
 *   2. The stream ends with a SHORT READ, not an error. One execute() is a
 *      chunk, not the answer, so composing means draining in a loop.
 *   3. The segment list is BORROWED. wfm_compose_segments() points into the
 *      composer, and destroy() takes it with it.
 *   4. A declaration is deterministic. The same scene composed twice is
 *      byte-identical, which is what makes a capture reproducible.
 *
 * The scene: two segments, the first summing two clean tones, each followed
 * by a gap. Clean sources (snr >= WFM_SYNTH_SNR_CLEAN) carry no AWGN, so the
 * gaps are exact zeros — which is what lets the checks below be equalities
 * rather than thresholds.
 *
 * Every check is explicit and returns non-zero on failure. `assert()` is
 * deliberately not used: examples build Release, where NDEBUG would compile
 * the checks out and leave a demo that validates nothing while still
 * exiting 0 — the exact shape `make test-examples-c` exists to prevent.
 *
 * Build:
 *   make build
 *   ./build/native/examples/wfmgen_demo
 */

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wfm/wfm_compose.h>
#include <wfm_synth/wfm_synth_core.h>

#define FS 1.0e6 /* sample rate, Hz — one per segment */

/* Segment geometry, stated once and reused by the checks below so the
   expected length is DERIVED from the declaration rather than typed twice. */
#define SEG0_ON 4096u
#define SEG0_OFF 1024u
#define SEG1_ON 2048u
#define SEG1_OFF 512u

#define TOTAL (SEG0_ON + SEG0_OFF + SEG1_ON + SEG1_OFF)

/* Deliberately not a divisor of any span: the drain loop has to be a loop,
   and a chunk that lined up with the geometry would hide a caller that
   assumed one execute() returns everything. */
#define CHUNK 700u

static int failures = 0;

/** @brief Report one named check; the first failure sets the exit status. */
static void
check (int ok, const char *what)
{
  printf ("  [%s] %s\n", ok ? "ok" : "FAIL", what);
  if (!ok)
    failures++;
}

/** @brief Mean power of a span, in linear units. */
static double
power (const float complex *x, size_t n)
{
  double acc = 0.0;
  for (size_t i = 0; i < n; i++)
    acc += (double)crealf (x[i]) * crealf (x[i])
           + (double)cimagf (x[i]) * cimagf (x[i]);
  return n ? acc / (double)n : 0.0;
}

/** @brief Is every sample in the span exactly zero? */
static int
all_zero (const float complex *x, size_t n)
{
  for (size_t i = 0; i < n; i++)
    if (crealf (x[i]) != 0.0f || cimagf (x[i]) != 0.0f)
      return 0;
  return 1;
}

/** @brief One clean tone at `freq`, offset from the segment's centre. */
static wfm_source_t
clean_tone (double freq)
{
  /* `= { 0 }` then named fields, never a positional initialiser list:
     wfm_source_t carries 30-odd members and a positional list silently
     shifts the moment one is inserted. */
  wfm_source_t src = { 0 };
  src.type         = WFM_SYNTH_TONE;
  src.freq         = freq;
  src.snr          = WFM_SYNTH_SNR_CLEAN; /* >= 100 dB: AWGN skipped */
  src.snr_mode     = 1;                   /* fs */
  src.seed         = 1u;
  return src;
}

/** @brief Compose the two-segment scene into `out`, draining in CHUNK steps.
 *
 *  @return Total samples written, or 0 if the composer could not be built.
 */
static size_t
compose_scene (float complex *out, size_t max, size_t *last_read,
               size_t *after_end)
{
  wfm_source_t seg0_srcs[2] = { clean_tone (+120e3), clean_tone (-80e3) };
  wfm_source_t seg1_srcs[1] = { clean_tone (+40e3) };

  wfm_segment_t segs[2] = { { 0 }, { 0 } };

  segs[0].sources     = seg0_srcs;
  segs[0].n_sources   = 2u; /* summed at the same time, one segment */
  segs[0].fs          = FS;
  segs[0].num_samples = SEG0_ON;
  segs[0].off_samples = SEG0_OFF;
  segs[0].gap_noise   = 0; /* auto; clean scene -> the gap is exact zeros */

  segs[1].sources     = seg1_srcs;
  segs[1].n_sources   = 1u;
  segs[1].fs          = FS;
  segs[1].num_samples = SEG1_ON;
  segs[1].off_samples = SEG1_OFF;
  segs[1].gap_noise   = 0;

  /* create() COPIES the segment list, so the arrays above may go out of
     scope after this call — the composer does not alias them. */
  wfm_compose_state_t *c = wfm_compose_create (segs, 2u, 0, 0);
  if (!c)
    return 0;

  /* The drain loop. execute() fills at most `want` and returns what it
     used; a short return means the sequence finished. */
  size_t total = 0, got = 0;
  while (total < max)
    {
      size_t want = max - total < CHUNK ? max - total : CHUNK;
      got         = wfm_compose_execute (c, out + total, want);
      if (got == 0)
        break;
      total += got;
    }
  *last_read = got;

  /* Past the end it keeps returning 0 rather than erroring or restarting. */
  float complex spare[8];
  *after_end = wfm_compose_execute (c, spare, 8u);

  /* The borrowed view: valid HERE, dangling after destroy() below. */
  size_t               n_segs = 0;
  int                  repeat = -1, continuous = -1;
  const wfm_segment_t *view
      = wfm_compose_segments (c, &n_segs, &repeat, &continuous);
  check (view != NULL && n_segs == 2u,
         "wfm_compose_segments borrows the 2-segment list back");
  check (view != NULL && view[0].fs == FS && view[0].num_samples == SEG0_ON,
         "the borrowed segment reports the geometry it was given");
  check (repeat == 0 && continuous == 0,
         "repeat/continuous read back as declared (a finite scene)");

  wfm_compose_destroy (c); /* `view` is dangling from this point on. */
  return total;
}

int
main (void)
{
  printf ("=== doppler wfmgen composition demo (the C caller's view) ===\n\n");

  /* 1. The caller owns the buffer, and sizes it from the declaration. */
  printf ("--- 1. You own the output buffer ---\n");
  const size_t   cap = TOTAL;
  float complex *buf = calloc (cap, sizeof *buf);
  if (!buf)
    {
      fprintf (stderr, "wfmgen_demo: out of memory\n");
      return 1;
    }
  printf ("  scene: %u on + %u off, then %u on + %u off = %u samples\n",
          SEG0_ON, SEG0_OFF, SEG1_ON, SEG1_OFF, TOTAL);
  printf ("  draining in %u-sample chunks (not a divisor of any span)\n\n",
          CHUNK);

  size_t last_read = 0, after_end = 1;
  size_t total = compose_scene (buf, cap, &last_read, &after_end);
  if (total == 0)
    {
      fprintf (stderr, "wfmgen_demo: wfm_compose_create failed\n");
      free (buf);
      return 1;
    }

  /* 2. Length is the declaration's, exactly. */
  printf ("\n--- 2. The composed length is the geometry, exactly ---\n");
  printf ("  wrote %zu sample(s), expected %u\n", total, TOTAL);
  check (total == (size_t)TOTAL, "total == on+off summed over both segments");

  /* 3. The end is a short read, then zero — not an error. */
  printf ("\n--- 3. The end is signalled by a SHORT read ---\n");
  printf ("  last execute() returned %zu of a %u-sample request\n", last_read,
          CHUNK);
  check (last_read < (size_t)CHUNK,
         "the final execute() is short, which is how the end is announced");
  check (after_end == 0,
         "execute() past the end returns 0, and keeps doing so");

  /* 4. A clean scene's gaps are exact zeros; its on-time is not. */
  printf ("\n--- 4. Clean sources make the gaps exact zeros ---\n");
  const float complex *seg0_on  = buf;
  const float complex *seg0_gap = buf + SEG0_ON;
  const float complex *seg1_on  = buf + SEG0_ON + SEG0_OFF;
  const double         p_on     = power (seg0_on, SEG0_ON);
  printf ("  segment 0 on-time mean power: %.4f\n", p_on);
  check (p_on > 0.1, "the on-time carries real signal power");
  check (all_zero (seg0_gap, SEG0_OFF),
         "segment 0's gap is exactly zero (clean scene, gap_noise=auto)");
  check (all_zero (buf + SEG0_ON + SEG0_OFF + SEG1_ON, SEG1_OFF),
         "segment 1's trailing gap is exactly zero too");
  check (power (seg1_on, SEG1_ON) > 0.1,
         "segment 1's on-time carries signal power as well");

  /* Two summed unit-power tones land near 2.0, and that is the point of
     n_sources: they ADD, they are not averaged. Checked as a band rather
     than an equality — the tones beat against each other over a finite
     span, so the mean is near 2, not exactly 2. */
  check (p_on > 1.5 && p_on < 2.5,
         "two summed unit-power tones give ~2x the power of one");

  /* 5. The same declaration composes byte-identically. */
  printf ("\n--- 5. A declaration is reproducible ---\n");
  float complex *again = calloc (cap, sizeof *again);
  if (!again)
    {
      fprintf (stderr, "wfmgen_demo: out of memory\n");
      free (buf);
      return 1;
    }
  size_t l2 = 0, a2 = 1;
  size_t total2 = compose_scene (again, cap, &l2, &a2);
  check (total2 == total, "the second pass composes the same length");
  check (memcmp (buf, again, cap * sizeof *buf) == 0,
         "and is byte-identical — same seed, same samples");

  free (again);
  free (buf);

  printf ("\n=== %s ===\n", failures ? "FAILED" : "all checks passed");
  return failures ? 1 : 0;
}
