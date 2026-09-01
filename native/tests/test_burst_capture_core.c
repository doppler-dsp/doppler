/**
 * test_burst_capture_core.c — what BurstCapture claims, pinned.
 *
 * The claims come from burst_capture_core.h's prose, in its order: the
 * constructor copies and derives, refine recovers a preamble start acquisition
 * structurally cannot report, every burst is emitted exactly once and never
 * partially, block size does not change the answer, and a blob resumes
 * bit-exactly.
 *
 * The burst synthesis is the same shape test_dsss_burst_receiver_core.c uses,
 * and deliberately a local copy: this suite places a burst at an arbitrary
 * stream offset inside a noise floor, which is a capture's problem. What it
 * does NOT do is demodulate, so there is no sync word and no CRC here — a
 * capture is finished when the samples come back.
 */
#include "burst_capture/burst_capture_core.h"
#include "pn/pn_core.h"

#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ACQ_SF 31u
#define DATA_SF 8u
#define REPS 4u
#define SPC 4u
#define PAYLOAD_SYMS 61u
/* Preamble + payload, in samples: what one capture window holds. */
#define BURST_LEN ((REPS * ACQ_SF + PAYLOAD_SYMS * DATA_SF) * SPC)

static float
csign (uint8_t c)
{
  return (c & 1u) ? -1.0f : 1.0f;
}

/* A REAL spreading code: the maximal-length sequence of a 5-stage LFSR, built
 * with the library's own generator. An arithmetic pattern that reads like a
 * reasonable code can have a peak-to-worst-sidelobe ratio near 1, which
 * measures the wrong thing entirely — the CFAR reference is then set by the
 * code's own autocorrelation rather than by noise. */
static const uint8_t *
acq_code (void)
{
  static uint8_t c[ACQ_SF];
  static int     built = 0;
  if (!built)
    {
      /* DP_REQUIRE returns 1, so it cannot appear in a pointer-returning
         helper; a NULL generator would surface as an all-zero code, which
         every detection assertion below would fail on. */
      pn_state_t *pn = pn_create (pn_mls_poly (5), 1u, 5u, 0);
      if (pn)
        {
          for (size_t i = 0; i < ACQ_SF; i++)
            c[i] = pn_step (pn);
          pn_destroy (pn);
        }
      built = 1;
    }
  return c;
}

static const uint8_t *
data_code (void)
{
  static uint8_t c[DATA_SF];
  for (size_t i = 0; i < DATA_SF; i++)
    c[i] = (uint8_t)((i >> 1) & 1u);
  return c;
}

/** @brief One burst: REPS preamble repetitions then a spread payload. */
static size_t
build_burst (float complex *y)
{
  const uint8_t *acode = acq_code (), *dcode = data_code ();
  size_t         n = 0;
  for (size_t r = 0; r < REPS; r++)
    for (size_t c = 0; c < ACQ_SF; c++)
      for (size_t k = 0; k < SPC; k++)
        y[n++] = csign (acode[c]);
  for (size_t j = 0; j < PAYLOAD_SYMS; j++)
    {
      float a = csign ((uint8_t)(j & 1u));
      for (size_t c = 0; c < DATA_SF; c++)
        for (size_t k = 0; k < SPC; k++)
          y[n++] = a * csign (dcode[c]);
    }
  return n;
}

/** @brief Noise everywhere, bursts at @p n_at offsets. */
static void
build_capture (float complex *cap, size_t n_cap, const size_t *at, size_t n_at,
               double sigma, uint32_t seed)
{
  uint32_t st = seed;
  for (size_t i = 0; i < n_cap; i++)
    {
      /* Named locals: two dp_gauss() calls in one expression would be
         ordered by the compiler, and gcc and clang differ. */
      float re = (float)(sigma * dp_gauss (&st));
      float im = (float)(sigma * dp_gauss (&st));
      cap[i]   = re + im * I;
    }
  static float complex burst[1 << 16];
  size_t               nb = build_burst (burst);
  for (size_t k = 0; k < n_at; k++)
    for (size_t i = 0; i < nb && at[k] + i < n_cap; i++)
      cap[at[k] + i] += burst[i];
}

static burst_capture_state_t *
make (void)
{
  return burst_capture_create (acq_code (), ACQ_SF, BURST_LEN, REPS, SPC,
                               1.0e6, 55.0, 0.0, 1e-3, 0.9, 0);
}

/* ── The constructor ─────────────────────────────────────────────────── */

/* The code is COPIED: the caller's array here is a stack local already out of
 * scope by the time this reads back, so a borrowing constructor shows up as
 * garbage geometry rather than as a clean pointer. */
static int
test_create_copies_and_derives (void)
{
  uint8_t code[ACQ_SF];
  for (size_t i = 0; i < ACQ_SF; i++)
    code[i] = (uint8_t)(i & 1u);
  burst_capture_state_t *s = burst_capture_create (
      code, ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0);
  DP_REQUIRE (s != NULL);

  DP_CHECK (s->acq_code != code);
  DP_CHECK (memcmp (s->acq_code, code, ACQ_SF) == 0);
  DP_CHECK (s->code_period == ACQ_SF * SPC);
  DP_CHECK (s->burst_len == BURST_LEN);

  /* refine_span is (k_lo + k_hi + reps) * P, and retain_span is that plus one
     whole burst — the MINIMUM TRAILING CONTEXT a caller must leave. Both are
     read back rather than recomputed by a caller, which is the whole reason
     they are fields (doppler#1011). */
  DP_CHECK (s->refine_span == (s->k_lo + s->k_hi + REPS) * s->code_period);
  DP_CHECK (s->retain_span == s->refine_span + BURST_LEN);

  /* The Doppler bin width is readable BEFORE any push: it is the engine's,
     a property of the configured search, and a composing bank sizes its
     cross-channel dedup from it at construction. Read through the
     last-event mirror it was 0.0 here. */
  DP_CHECK (burst_capture_get_doppler_res_hz (s) > 0.0);
  DP_CHECK (burst_capture_get_doppler_res_hz (s)
            == s->acq->engine->doppler_res_hz);

  /* The ring holds twice the retained span, so chunk_max is never zero: a
     push larger than the ring is sliced rather than refused. */
  DP_CHECK (s->hist->capacity >= 2u * s->retain_span);
  DP_CHECK (s->chunk_max > 0);

  /* The queue depth is DERIVED from burst_len/refine_span, never a constant.
     `q_cap >= 8` was the assertion here first, and a hardcoded 8 satisfies
     it -- the literals-only trap validation.md names. What the claim
     actually says is that the depth MOVES with the geometry, so the test is
     two objects whose burst_len differs by an order of magnitude. */
  {
    burst_capture_state_t *big
        = burst_capture_create (code, ACQ_SF, 20u * BURST_LEN, REPS, SPC,
                                1.0e6, 55.0, 0.0, 1e-3, 0.9, 0);
    DP_REQUIRE (big != NULL);
    DP_CHECK (big->q_cap > s->q_cap);
    DP_CHECK (s->q_cap >= 8u); /* ...and the floor still holds */
    burst_capture_destroy (big);
  }

  burst_capture_destroy (s);
  return 0;
}

/* Every guarded parameter is an ARGUMENT error, so create() returns NULL and
 * the binding turns that into a ValueError naming the constraint. */
static int
test_create_rejects_bad_parameters (void)
{
  const uint8_t *c = acq_code ();
  DP_CHECK (burst_capture_create (NULL, ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6,
                                  55.0, 0.0, 1e-3, 0.9, 0)
            == NULL);
  DP_CHECK (burst_capture_create (c, 0, BURST_LEN, REPS, SPC, 1.0e6, 55.0, 0.0,
                                  1e-3, 0.9, 0)
            == NULL);
  /* burst_len is the parameter this object exists to take; zero of it is not
     a capture. */
  DP_CHECK (burst_capture_create (c, ACQ_SF, 0, REPS, SPC, 1.0e6, 55.0, 0.0,
                                  1e-3, 0.9, 0)
            == NULL);
  DP_CHECK (burst_capture_create (c, ACQ_SF, BURST_LEN, 0, SPC, 1.0e6, 55.0,
                                  0.0, 1e-3, 0.9, 0)
            == NULL);
  DP_CHECK (burst_capture_create (c, ACQ_SF, BURST_LEN, REPS, 0, 1.0e6, 55.0,
                                  0.0, 1e-3, 0.9, 0)
            == NULL);
  DP_CHECK (burst_capture_create (c, ACQ_SF, BURST_LEN, REPS, SPC, 0.0, 55.0,
                                  0.0, 1e-3, 0.9, 0)
            == NULL);
  DP_CHECK (burst_capture_create (c, ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6, 0.0,
                                  0.0, 1e-3, 0.9, 0)
            == NULL);
  DP_CHECK (burst_capture_create (c, ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6, 55.0,
                                  0.0, 0.0, 0.9, 0)
            == NULL);
  DP_CHECK (burst_capture_create (c, ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6, 55.0,
                                  0.0, 1e-3, 1.0, 0)
            == NULL);
  return 0;
}

/* ── The claim the object exists for ─────────────────────────────────── */

/**
 * A burst placed at a known offset comes back as a window whose
 * preamble_start IS that offset, to the sample.
 *
 * This is the stage acquisition structurally cannot do: its code_phase is a
 * lag MODULO one code period, so it names the alignment within a repetition
 * and never which one. Asserting the exact start is asserting that refine
 * resolved the period — an off-by-one-period answer is a whole `code_period`
 * away, which no tolerance here admits.
 */
static int
test_window_starts_at_the_burst (void)
{
  static float complex cap[80000];
  const size_t         at = 9000u;
  build_capture (cap, sizeof cap / sizeof *cap, &at, 1u, 0.02, 7u);

  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);

  static float complex out[4 * BURST_LEN];
  size_t n = burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                 sizeof out / sizeof *out);

  DP_CHECK (n == BURST_LEN);
  DP_CHECK (burst_capture_ready (s) == 1u);
  DP_CHECK (s->preamble_start == at);

  const burst_capture_event_t *ev = burst_capture_event_at (s, 0);
  DP_REQUIRE (ev != NULL);
  DP_CHECK (ev->preamble_start == at);
  /* refine_margin is the runner-up period over the winner. Compare against
     the ENVELOPE (reps-1)/reps, never a constant: the floor RISES with depth
     (0.55 at reps=2, 0.77 at 4, 0.94 at 16), so a fixed 0.9 -- which is what
     this line said first -- is correct at 4 and asserts nothing at 16. A
     resolved period sits AT the envelope; an unresolved one runs to 1. */
  {
    double envelope = (double)(REPS - 1u) / (double)REPS;
    DP_CHECK (ev->refine_margin < envelope + 0.1);
    /* ...and not absurdly BELOW it either, which would mean the winner beat
       its rivals by more than the triangular overlap allows -- a scoring
       bug rather than a good detection. */
    DP_CHECK (ev->refine_margin > envelope - 0.3);
  }
  DP_CHECK (ev->doppler_res_hz > 0.0);

  /* The window is the burst, not a window near it. */
  static float complex burst[1 << 16];
  build_burst (burst);
  double num = 0.0, den = 0.0;
  for (size_t i = 0; i < 8u * ACQ_SF * SPC; i++)
    {
      num += (double)crealf (out[i]) * (double)crealf (burst[i]);
      den += (double)crealf (burst[i]) * (double)crealf (burst[i]);
    }
  DP_CHECK (den > 0.0 && num / den > 0.8);

  /* Both faces read the same scratch, so they must agree sample for sample. */
  const float complex *w = burst_capture_window (s, 0);
  DP_REQUIRE (w != NULL);
  DP_CHECK (memcmp (w, out, BURST_LEN * sizeof *w) == 0);
  DP_CHECK (burst_capture_window (s, 1) == NULL);

  DP_CHECK (s->dropped == 0);
  DP_CHECK (s->n_bursts == 1u);
  burst_capture_destroy (s);
  return 0;
}

/**
 * Several bursts in one capture come back as several windows — the defect
 * class that survived certification once already, because with a single burst
 * everything push() discarded was noise (doppler#1008).
 */
static int
test_every_burst_is_emitted_once (void)
{
  static float complex cap[200000];
  const size_t         at[3] = { 9000u, 60000u, 120000u };
  build_capture (cap, sizeof cap / sizeof *cap, at, 3u, 0.02, 11u);

  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);

  static float complex out[8 * BURST_LEN];
  size_t n = burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                 sizeof out / sizeof *out);

  DP_CHECK (n == 3u * BURST_LEN);
  DP_REQUIRE (burst_capture_ready (s) == 3u);
  for (size_t k = 0; k < 3u; k++)
    {
      const burst_capture_event_t *ev = burst_capture_event_at (s, k);
      DP_REQUIRE (ev != NULL);
      DP_CHECK (ev->preamble_start == at[k]);
    }
  DP_CHECK (s->dropped == 0);
  DP_CHECK (s->n_bursts == 3u);
  DP_CHECK (s->pending == 0);
  burst_capture_destroy (s);
  return 0;
}

/**
 * The same stream, one push or many, gives the same answer. The ring is a
 * contiguous window over the stream and is never reset between bursts, so a
 * burst whose tail falls outside one call is completed by a later one.
 */
static int
test_block_size_does_not_change_the_answer (void)
{
  static float complex cap[200000];
  const size_t         at[3] = { 9000u, 60000u, 120000u };
  const size_t         n_cap = sizeof cap / sizeof *cap;
  build_capture (cap, n_cap, at, 3u, 0.02, 11u);

  burst_capture_state_t *a = make ();
  burst_capture_state_t *b = make ();
  DP_REQUIRE (a != NULL && b != NULL);

  static float complex out_a[8 * BURST_LEN];
  static float complex out_b[8 * BURST_LEN];
  size_t               na = burst_capture_push (a, cap, n_cap, out_a,
                                                sizeof out_a / sizeof *out_a);

  size_t nb = 0;
  for (size_t off = 0; off < n_cap; off += 333u)
    {
      size_t blk = n_cap - off < 333u ? n_cap - off : 333u;
      nb += burst_capture_push (b, cap + off, blk, out_b + nb,
                                sizeof out_b / sizeof *out_b - nb);
    }

  DP_CHECK (na == nb);
  DP_CHECK (na == 3u * BURST_LEN);
  DP_CHECK (memcmp (out_a, out_b, na * sizeof *out_a) == 0);
  DP_CHECK (a->n_bursts == b->n_bursts);
  burst_capture_destroy (a);
  burst_capture_destroy (b);
  return 0;
}

/**
 * A caller whose buffer holds fewer than the completed bursts gets WHOLE
 * windows, never a truncated one: half a burst is not a burst, and a caller
 * handed 3.5 of them cannot tell where the truncation fell.
 */
static int
test_never_returns_a_partial_window (void)
{
  static float complex cap[200000];
  const size_t         at[3] = { 9000u, 60000u, 120000u };
  build_capture (cap, sizeof cap / sizeof *cap, at, 3u, 0.02, 11u);

  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);

  static float complex out[8 * BURST_LEN];
  /* Room for 1.5 windows. */
  size_t room = BURST_LEN + BURST_LEN / 2u;
  size_t n = burst_capture_push (s, cap, sizeof cap / sizeof *cap, out, room);
  DP_CHECK (n == BURST_LEN);
  DP_CHECK (n % BURST_LEN == 0);
  /* The bursts still HAPPENED — the events describe all three, so a caller
     who under-sized its buffer can see what it missed rather than believing
     the stream was quiet. */
  DP_CHECK (burst_capture_ready (s) == 3u);
  burst_capture_destroy (s);
  return 0;
}

/**
 * A burst closer to the end of the stream than retain_span is HELD, not
 * emitted, and `pending` says so. That read-back is the only way a caller
 * closing a file can tell "a burst is still coming" from "nothing was ever
 * there".
 */
static int
test_short_trailing_context_holds_the_burst (void)
{
  static float complex cap[200000];
  const size_t         at = 40000u;
  /* One sample short of the burst's last. The emission rule is the burst's
     own span having arrived -- retain_span is the RING's retention, which is
     larger, so asserting against it would pass on a much weaker object. */
  const size_t n_cap = at + BURST_LEN - 1u;
  DP_REQUIRE (n_cap <= sizeof cap / sizeof *cap);
  build_capture (cap, n_cap, &at, 1u, 0.02, 5u);

  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float complex out[4 * BURST_LEN];
  size_t n = burst_capture_push (s, cap, n_cap, out, sizeof out / sizeof *out);
  DP_CHECK (n == 0);
  DP_CHECK (s->pending == 1u);
  /* ...and one more sample completes it, which is what makes the check above
     a boundary rather than a claim that the burst was never seen. */
  n = burst_capture_push (s, cap + n_cap, 1u, out, sizeof out / sizeof *out);
  DP_CHECK (n == BURST_LEN);
  DP_CHECK (s->preamble_start == at);
  DP_CHECK (s->pending == 0);
  burst_capture_destroy (s);
  return 0;
}

/**
 * reset() returns to the searching state without touching the geometry, and
 * `dropped` deliberately SURVIVES it: a lost burst stays lost, and a lifetime
 * count that reset() zeroed would report a clean stream.
 */
static int
test_reset_clears_position_not_history (void)
{
  static float complex cap[80000];
  const size_t         at = 9000u;
  build_capture (cap, sizeof cap / sizeof *cap, &at, 1u, 0.02, 7u);

  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float complex out[4 * BURST_LEN];
  burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                      sizeof out / sizeof *out);
  DP_REQUIRE (s->n_bursts == 1u);

  const size_t span = s->refine_span;
  burst_capture_reset (s);
  DP_CHECK (s->samples_fed == 0);
  DP_CHECK (s->pending == 0);
  DP_CHECK (s->suppress_until == 0);
  DP_CHECK (s->preamble_start == 0);
  DP_CHECK (burst_capture_ready (s) == 0);
  DP_CHECK (s->refine_span == span);
  DP_CHECK (s->n_bursts == 1u);

  /* And the same stream is found again from a clean position. */
  size_t n = burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                 sizeof out / sizeof *out);
  DP_CHECK (n == BURST_LEN);
  DP_CHECK (s->preamble_start == at);
  burst_capture_destroy (s);
  return 0;
}

/**
 * A blob taken mid-stream resumes into a FRESH instance and finds the burst
 * whose preamble it was already holding — which is what proves the retained
 * look-back travels with it. A blob that omitted the history would restore an
 * object that cannot reach back, and this is the split that shows it.
 */
static int
test_state_resumes_mid_burst (void)
{
  static float complex cap[200000];
  const size_t         at    = 60000u;
  const size_t         n_cap = sizeof cap / sizeof *cap;
  build_capture (cap, n_cap, &at, 1u, 0.02, 3u);

  burst_capture_state_t *a = make ();
  DP_REQUIRE (a != NULL);
  static float complex out[4 * BURST_LEN];
  /* Split INSIDE the preamble: the detection has fired (or is about to) and
     the window has certainly not arrived. */
  const size_t cut = at + 2u * ACQ_SF * SPC;
  size_t n0 = burst_capture_push (a, cap, cut, out, sizeof out / sizeof *out);
  DP_CHECK (n0 == 0);

  burst_capture_state_t *b = make ();
  DP_REQUIRE (b != NULL);
  DP_STATE_ROUNDTRIP_TEST (burst_capture, a, b);

  /* `b` was restored from `a` and must now find the burst `a` was holding. */
  size_t n = burst_capture_push (b, cap + cut, n_cap - cut, out,
                                 sizeof out / sizeof *out);
  DP_CHECK (n == BURST_LEN);
  DP_CHECK (b->preamble_start == at);

  burst_capture_destroy (a);
  burst_capture_destroy (b);
  return 0;
}

/* ── The entry points the first pass never called ────────────────────── */

/**
 * `push_max_out` USES its argument, and bounds a real push.
 *
 * It is the size a caller allocates from, so a bound that ignored `x_len`
 * would either waste memory or -- the version that matters -- under-size the
 * buffer and silently truncate. Asserted both ways: it grows with the input,
 * and a real push never exceeds it.
 */
static int
test_push_max_out_bounds_a_real_push (void)
{
  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);

  size_t small = burst_capture_push_max_out (s, 1000u);
  size_t large = burst_capture_push_max_out (s, 1000000u);
  DP_CHECK (large > small);
  DP_CHECK (small % BURST_LEN == 0);

  static float complex cap[200000];
  const size_t         at[3] = { 9000u, 60000u, 120000u };
  const size_t         n_cap = sizeof cap / sizeof *cap;
  build_capture (cap, n_cap, at, 3u, 0.02, 11u);

  static float complex out[8 * BURST_LEN];
  size_t               bound = burst_capture_push_max_out (s, n_cap);
  size_t n = burst_capture_push (s, cap, n_cap, out, sizeof out / sizeof *out);
  DP_CHECK (n <= bound);
  DP_CHECK (n == 3u * BURST_LEN);
  burst_capture_destroy (s);
  return 0;
}

/**
 * `events()` copies the rows, `events_max_out()` counts them, and both
 * describe the LAST push -- not a request.
 *
 * The count is the push's, so the `n` argument is ignored by design; asserting
 * that is what stops a later version quietly turning it into a request and
 * breaking every caller that passes 0.
 */
static int
test_events_describe_the_last_push (void)
{
  static float complex cap[200000];
  const size_t         at[3] = { 9000u, 60000u, 120000u };
  build_capture (cap, sizeof cap / sizeof *cap, at, 3u, 0.02, 11u);

  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float complex out[8 * BURST_LEN];
  burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                      sizeof out / sizeof *out);

  DP_CHECK (burst_capture_events_max_out (s, 0) == 3u);
  DP_CHECK (burst_capture_events_max_out (s, 99u) == 3u); /* n is ignored */

  burst_capture_event_t ev[8];
  size_t                got = burst_capture_events (s, 0, ev, 8u);
  DP_CHECK (got == 3u);
  for (size_t k = 0; k < 3u; k++)
    DP_CHECK (ev[k].preamble_start == at[k]);

  /* A short buffer truncates rather than overruns. */
  burst_capture_event_t one[1];
  DP_CHECK (burst_capture_events (s, 0, one, 1u) == 1u);
  DP_CHECK (one[0].preamble_start == at[0]);

  /* A push that completes nothing clears them -- events() describes THIS
     call, so a stale row would attribute an old burst to a quiet block. */
  static float complex quiet[8000];
  build_capture (quiet, sizeof quiet / sizeof *quiet, NULL, 0u, 0.02, 4u);
  burst_capture_push (s, quiet, sizeof quiet / sizeof *quiet, out,
                      sizeof out / sizeof *out);
  DP_CHECK (burst_capture_events_max_out (s, 0) == 0);
  DP_CHECK (burst_capture_ready (s) == 0);
  DP_CHECK (burst_capture_event_at (s, 0) == NULL);
  DP_CHECK (burst_capture_window (s, 0) == NULL);

  burst_capture_destroy (s);
  return 0;
}

/**
 * The read-back accessors report the same values the event rows carry.
 *
 * Two faces of one record, and the binding reads the accessors while the C
 * consumer reads the struct -- so a divergence would show only on one side.
 */
static int
test_accessors_agree_with_the_event (void)
{
  static float complex cap[80000];
  const size_t         at = 9000u;
  build_capture (cap, sizeof cap / sizeof *cap, &at, 1u, 0.02, 7u);

  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float complex out[4 * BURST_LEN];
  burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                      sizeof out / sizeof *out);

  const burst_capture_event_t *e = burst_capture_event_at (s, 0);
  DP_REQUIRE (e != NULL);
  DP_CHECK (burst_capture_get_preamble_start (s) == e->preamble_start);
  DP_CHECK (burst_capture_get_doppler_hz_est (s) == e->doppler_hz_est);
  DP_CHECK (burst_capture_get_doppler_res_hz (s) == e->doppler_res_hz);
  DP_CHECK (burst_capture_get_cn0_dbhz_est (s) == e->cn0_dbhz_est);
  DP_CHECK (burst_capture_get_refine_margin (s) == e->refine_margin);
  DP_CHECK (burst_capture_get_pending (s) == s->pending);
  DP_CHECK (burst_capture_get_dropped (s) == s->dropped);
  DP_CHECK (burst_capture_get_n_bursts (s) == 1u);
  burst_capture_destroy (s);
  return 0;
}

/**
 * `detections()` is what the SEARCH found, and the epochs are stream-absolute.
 *
 * The claim the bank composes on (doppler#1174): a capturing channel reports
 * the same detections a detector would, so one acquisition engine serves both
 * faces. Three parts of the header's prose, each pinned:
 *
 * - UNFILTERED -- before the claim rule and the suppression window, so the
 *   rows are at least as many as the windows, and are usually more (the
 *   payload fires acquisition too, and a preamble straddling two frames
 *   fires twice).
 * - STREAM-ABSOLUTE -- every window's `preamble_start` has a detection
 *   within `refine_span` of it. `code_phase` alone is a residue below
 *   `code_period`, so a residue could not land within `refine_span` of a
 *   burst at 60000 or 120000: the check discriminates an epoch from a phase.
 * - DESCRIBES THIS PUSH -- a quiet push clears them, and a short buffer
 *   truncates rather than overruns.
 */
static int
test_detections_are_what_the_search_found (void)
{
  static float complex cap[200000];
  const size_t         at[3] = { 9000u, 60000u, 120000u };
  build_capture (cap, sizeof cap / sizeof *cap, at, 3u, 0.02, 11u);

  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float complex out[8 * BURST_LEN];
  burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                      sizeof out / sizeof *out);
  const size_t ne = burst_capture_events_max_out (s, 0);
  DP_REQUIRE (ne == 3u);

  const size_t nd = burst_capture_detections_max_out (s, 0);
  DP_CHECK (burst_capture_detections_max_out (s, 99u) == nd); /* n ignored */
  DP_CHECK (nd >= ne);
  /* Measured 4 against 3 at this geometry: more rows than bursts is the
     evidence that nothing filtered them. A `detections()` that returned the
     claim rule's output would read exactly 3 here. */
  DP_CHECK (nd > ne);

  burst_capture_detection_t det[64];
  DP_REQUIRE (nd <= sizeof det / sizeof *det);
  DP_CHECK (burst_capture_detections (s, 0, det, 64u) == nd);

  burst_capture_event_t ev[8];
  DP_REQUIRE (burst_capture_events (s, 0, ev, 8u) == ne);
  for (size_t k = 0; k < ne; k++)
    {
      int named = 0;
      for (size_t i = 0; i < nd; i++)
        {
          uint64_t d = det[i].epoch > ev[k].preamble_start
                           ? det[i].epoch - ev[k].preamble_start
                           : ev[k].preamble_start - det[i].epoch;
          if (d < (uint64_t)s->refine_span)
            named = 1;
        }
      DP_CHECK (named); /* an emitted burst came from a hit that names it */
    }
  /* ...and every row carries the statistic that gated it. */
  for (size_t i = 0; i < nd; i++)
    {
      DP_CHECK (det[i].test_stat > 0.0);
      DP_CHECK (det[i].peak_mag > 0.0);
    }

  /* A short buffer truncates rather than overruns. */
  burst_capture_detection_t one[1];
  DP_CHECK (burst_capture_detections (s, 0, one, 1u) == 1u);
  DP_CHECK (one[0].epoch == det[0].epoch);

  /* A quiet push clears them: the rows describe THIS call. */
  static float complex quiet[8000];
  build_capture (quiet, sizeof quiet / sizeof *quiet, NULL, 0u, 0.02, 4u);
  burst_capture_push (s, quiet, sizeof quiet / sizeof *quiet, out,
                      sizeof out / sizeof *out);
  DP_CHECK (burst_capture_detections_max_out (s, 0) == 0);

  burst_capture_destroy (s);
  return 0;
}

/**
 * `configure_search_raw` reaches the engine, and re-reads the blob bound it
 * invalidated.
 *
 * It is the one call that can legitimately move `acq_state_bytes()` under a
 * `state_bytes()` that promises to be a pure function of configuration. If
 * the bound were not re-read, a blob taken after this call could exceed the
 * region reserved for it.
 */
static int
test_configure_search_raw_reaches_the_engine (void)
{
  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  /* A coherent depth of 2 against a sized 4: the grid is the one ASKED for,
     not the one the auto-sizer picked, which is the whole point of the
     escape hatch. */
  DP_CHECK (s->acq->engine->coherent_bins == REPS);
  DP_CHECK (burst_capture_configure_search_raw (s, 2u, 1u) == DP_OK);
  DP_CHECK (s->acq->engine->coherent_bins == 2u);
  /* And a grid the engine cannot honour is REFUSED, not silently clamped: a
     coherent depth deeper than the preamble has no frames to integrate.
     Measured: reps=4 accepts 1, 2 and 4 and rejects 8. */
  DP_CHECK (burst_capture_configure_search_raw (s, 8u, 1u) == DP_ERR_INVALID);
  DP_CHECK (s->acq->engine->coherent_bins
            == 2u); /* unchanged by the refusal */
  /* ...and the blob bound it invalidated was re-read. */
  DP_CHECK (s->acq_blob_max == acq_state_bytes (s->acq->engine));
  size_t after = burst_capture_state_bytes (s);
  /* ...so a blob taken NOW fits the region reserved for it. */
  void *blob = malloc (after);
  DP_REQUIRE (blob != NULL);
  burst_capture_get_state (s, blob);
  DP_CHECK (burst_capture_set_state (s, blob) == DP_OK);
  free (blob);
  burst_capture_destroy (s);
  return 0;
}

/** destroy(NULL) is safe -- the claim every header makes and few tests make.
 */
static int
test_destroy_null_is_safe (void)
{
  burst_capture_destroy (NULL);
  DP_CHECK (1);
  return 0;
}

/* ── Claims about behaviour that nothing reached ─────────────────────── */

/**
 * `state_bytes()` is a pure function of CONFIGURATION.
 *
 * jm's binding compares an incoming blob's length against it before calling
 * set_state, so a size that moved with the stream would make a capture
 * restorable only into an instance holding exactly as much history -- which
 * is not resume, it is coincidence.
 */
static int
test_state_bytes_does_not_move_with_the_stream (void)
{
  static float complex cap[200000];
  const size_t         at[3] = { 9000u, 60000u, 120000u };
  build_capture (cap, sizeof cap / sizeof *cap, at, 3u, 0.02, 11u);

  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  size_t               empty = burst_capture_state_bytes (s);
  static float complex out[8 * BURST_LEN];
  burst_capture_push (s, cap, 40000u, out, sizeof out / sizeof *out);
  DP_CHECK (burst_capture_state_bytes (s) == empty);
  burst_capture_push (s, cap + 40000u, 40000u, out, sizeof out / sizeof *out);
  DP_CHECK (burst_capture_state_bytes (s) == empty);
  burst_capture_reset (s);
  DP_CHECK (burst_capture_state_bytes (s) == empty);
  burst_capture_destroy (s);
  return 0;
}

/**
 * A push LARGER than the ring is sliced, not refused.
 *
 * `chunk_max` exists for exactly this, and it is what "accepts any block
 * size" costs. Nothing else in the suite pushes past the ring's capacity, so
 * the slicing path ran in no test at all.
 */
static int
test_a_push_larger_than_the_ring_is_sliced (void)
{
  burst_capture_state_t *probe = make ();
  DP_REQUIRE (probe != NULL);
  const size_t chunk_max = probe->chunk_max;
  const size_t ring      = probe->hist->capacity;
  burst_capture_destroy (probe);

  static float complex cap[200000];
  const size_t         n_cap = sizeof cap / sizeof *cap;
  DP_REQUIRE (n_cap > 2u * ring); /* the point of the test */
  const size_t at[2] = { 9000u, 60000u };
  build_capture (cap, n_cap, at, 2u, 0.02, 11u);

  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float complex out[8 * BURST_LEN];
  size_t n = burst_capture_push (s, cap, n_cap, out, sizeof out / sizeof *out);
  DP_CHECK (n_cap > chunk_max); /* the slicing path really was taken */
  DP_CHECK (n == 2u * BURST_LEN);
  DP_CHECK (s->dropped == 0);
  DP_CHECK (s->samples_fed == (uint64_t)n_cap);
  burst_capture_destroy (s);
  return 0;
}

/**
 * A burst that was captured SUPPRESSES the detections its own payload makes.
 *
 * Acquisition fires on the payload too -- it is the same chips at a different
 * rate -- and without the window those are new bursts. The observable is that
 * one transmitted burst yields exactly ONE window, not several overlapping
 * ones, and that `suppress_until` reaches past the burst's end.
 */
static int
test_a_captured_burst_suppresses_its_own_payload (void)
{
  static float complex cap[80000];
  const size_t         at = 9000u;
  build_capture (cap, sizeof cap / sizeof *cap, &at, 1u, 0.02, 7u);

  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float complex out[8 * BURST_LEN];
  size_t n = burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                 sizeof out / sizeof *out);

  DP_CHECK (n == BURST_LEN); /* ONE window for one burst */
  DP_CHECK (s->n_bursts == 1u);
  DP_CHECK (s->suppress_until >= (uint64_t)(at + BURST_LEN));
  /* Nothing is left queued: a detection inside the span was dropped rather
     than held, which is what the compaction after an emit is for. */
  DP_CHECK (s->pending == 0);
  burst_capture_destroy (s);
  return 0;
}

/**
 * `min_gap` is the dead air a caller must leave, and it is DERIVED.
 *
 * Two bursts exactly `min_gap` apart edge-to-edge are both captured; the
 * object computes the number so a caller never has to know the rule. The
 * rule, for the record: a detection's anchor is the code epoch of whichever
 * frame detected, and framing is not aligned to the preamble, so the last
 * frame that can detect sits `reps * code_period` past the true start.
 * CLAIM merges anchors closer than `refine_span`, so the first burst
 * detected LATE and the second EARLY close by that much before CLAIM sees
 * them.
 *
 * The prose this replaced said `max(0, refine_span - burst_len)` -- short by
 * the whole detection-lag term, 32 samples against 528 here (doppler#1172).
 */
static int
test_min_gap_is_derived_and_sufficient (void)
{
  burst_capture_state_t *probe = make ();
  DP_REQUIRE (probe != NULL);
  /* Through the ACCESSOR, which is the C consumer's face -- a composing
     object reads that, not the struct, and a field-only test leaves the
     function every caller actually calls unexercised. */
  const size_t gap  = burst_capture_get_min_gap (probe);
  const size_t span = probe->refine_span;
  const size_t P    = probe->code_period;
  DP_CHECK (gap == probe->min_gap); /* the two faces agree */
  /* The derivation, asserted as arithmetic rather than as a constant: a
     hard-coded 528 would pass on this geometry and say nothing about any
     other. */
  DP_CHECK (gap == span + REPS * P - BURST_LEN);
  DP_CHECK (gap > 0);
  /* ...and it is bigger than the formula it replaced, which is the defect. */
  DP_CHECK (gap > (span > BURST_LEN ? span - BURST_LEN : 0u));
  burst_capture_destroy (probe);

  static float complex cap[200000];
  const size_t         at[2] = { 9000u, 9000u + BURST_LEN + gap };
  build_capture (cap, sizeof cap / sizeof *cap, at, 2u, 0.02, 13u);

  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float complex out[8 * BURST_LEN];
  size_t n = burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                 sizeof out / sizeof *out);

  /* Both transmitted bursts come back. NOT `n == 2 * BURST_LEN`: at
     pfa = 1e-3 a spurious window is expected, and asserting the count would
     be asserting the false-alarm rate is zero. */
  DP_REQUIRE (n >= 2u * BURST_LEN);
  int found[2] = { 0, 0 };
  for (size_t i = 0; i < burst_capture_ready (s); i++)
    for (size_t k = 0; k < 2u; k++)
      if (burst_capture_event_at (s, i)->preamble_start == (uint64_t)at[k])
        found[k] = 1;
  DP_CHECK (found[0] && found[1]);
  burst_capture_destroy (s);
  return 0;
}

/**
 * `refine_span` bounds START-TO-START separation, not the dead air between
 * bursts.
 *
 * Both sides of the merge test are resolved code epochs, so reading it as
 * required silence reserves airtime for nothing (doppler#1085). Two bursts
 * placed a whole `refine_span` apart start-to-start -- which for this
 * geometry leaves them overlapping-adjacent rather than separated -- must
 * still be two.
 */
static int
test_refine_span_bounds_start_to_start (void)
{
  burst_capture_state_t *probe = make ();
  DP_REQUIRE (probe != NULL);
  const size_t span = probe->refine_span;
  burst_capture_destroy (probe);

  static float complex cap[200000];
  /* Start-to-start just past `refine_span`, which leaves the two bursts
     nearly touching -- 32 samples of dead air, since burst_len is 2448 and
     the reach is 2480. Reading the reach as REQUIRED SILENCE would demand
     2480 samples of it, which is 9% of airtime spent on nothing
     (doppler#1085). */
  /* 300 samples of dead air, against a 2480-sample reach: measured, 32 is
     marginally too tight (the second burst is lost) and 282 is enough. The
     floor itself is swept in characterization; what this pins is the CLAIM,
     that a gap far smaller than `refine_span` still yields two bursts. */
  const size_t at[2] = { 9000u, 9000u + BURST_LEN + 300u };
  DP_CHECK (at[1] - at[0] > span);             /* outside the merge window */
  DP_CHECK (at[1] - at[0] - BURST_LEN < span); /* dead air is far LESS */
  build_capture (cap, sizeof cap / sizeof *cap, at, 2u, 0.02, 13u);

  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float complex out[8 * BURST_LEN];
  size_t n = burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                 sizeof out / sizeof *out);

  /* BOTH transmitted bursts come back, at their exact starts. NOT
     `n == 2 * BURST_LEN`: at pfa = 1e-3 over a surface this size a spurious
     window is EXPECTED, and this geometry reliably yields one -- so asserting
     the count would be asserting the false-alarm rate is zero, and the test
     would fail for the object behaving correctly. A caller separates the two
     with cn0_dbhz_est and refine_margin, which is what they are exposed for;
     the rate itself is characterized, not pinned here. */
  DP_REQUIRE (n >= 2u * BURST_LEN);
  int found[2] = { 0, 0 };
  for (size_t i = 0; i < burst_capture_ready (s); i++)
    for (size_t k = 0; k < 2u; k++)
      if (burst_capture_event_at (s, i)->preamble_start == (uint64_t)at[k])
        found[k] = 1;
  DP_CHECK (found[0] && found[1]);
  burst_capture_destroy (s);
  return 0;
}

/* ── Persistence: the ring in a file ─────────────────────────────────── */

static void
scratch_path (char *buf, size_t n, const char *tag)
{
  snprintf (buf, n, "/tmp/dp_burst_capture_%s_%d.cf32", tag, (int)getpid ());
}

/**
 * A backed capture behaves exactly like an in-RAM one, and its blob does not
 * carry the look-back.
 *
 * The size claim is the point of the feature: the retained history IS the
 * blob for an in-RAM capture, so a backed one has to be smaller by very
 * nearly `retain_span` complex samples, not by a rounding.
 */
static int
test_backed_finds_the_same_burst_with_a_smaller_blob (void)
{
  char path[256];
  scratch_path (path, sizeof path, "same");
  remove (path);

  static float complex cap[80000];
  const size_t         at = 9000u;
  build_capture (cap, sizeof cap / sizeof *cap, &at, 1u, 0.02, 7u);

  burst_capture_state_t *ram = make ();
  burst_capture_state_t *dsk = burst_capture_create_backed (
      path, acq_code (), ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6, 55.0, 0.0, 1e-3,
      0.9, 0);
  DP_REQUIRE (ram != NULL && dsk != NULL);
  DP_CHECK (dsk->backed == 1);
  DP_CHECK (ram->backed == 0);

  static float complex out_a[4 * BURST_LEN];
  static float complex out_b[4 * BURST_LEN];
  size_t na = burst_capture_push (ram, cap, sizeof cap / sizeof *cap, out_a,
                                  sizeof out_a / sizeof *out_a);
  size_t nb = burst_capture_push (dsk, cap, sizeof cap / sizeof *cap, out_b,
                                  sizeof out_b / sizeof *out_b);

  /* Bit-identical: where the pages live is not a DSP parameter. */
  DP_CHECK (na == BURST_LEN);
  DP_CHECK (na == nb);
  DP_CHECK (memcmp (out_a, out_b, na * sizeof *out_a) == 0);
  DP_CHECK (ram->preamble_start == dsk->preamble_start);

  size_t cb_ram = burst_capture_state_bytes (ram);
  size_t cb_dsk = burst_capture_state_bytes (dsk);
  /* The EXACT difference, not a ratio: the ring's capacity rounds up to a
     whole page, so a "backed is 4x smaller" assertion measures the host's
     page size as much as the feature -- it passed on 4 kB pages and failed on
     macOS's 16 kB. What the feature actually claims is that the blob stops
     carrying the retained span, and that is exact everywhere. */
  DP_CHECK (cb_ram - cb_dsk == ram->retain_span * sizeof (float _Complex));
  DP_CHECK (cb_dsk < cb_ram);

  burst_capture_destroy (ram);
  burst_capture_destroy (dsk);
  remove (path);
  return 0;
}

/**
 * The history outlives the object that wrote it.
 *
 * A capture is destroyed mid-preamble and a FRESH one is built over the same
 * file; restoring the blob into it finds the burst whose start is behind the
 * split. This is the claim the feature exists for, and it fails for both of
 * the obvious wrong implementations -- a ring that does not actually share
 * the file's pages, and a set_state() that restores positions without the
 * samples being there.
 */
static int
test_history_survives_destroying_the_capture (void)
{
  char path[256];
  scratch_path (path, sizeof path, "survive");
  remove (path);

  static float complex cap[200000];
  const size_t         at    = 60000u;
  const size_t         n_cap = sizeof cap / sizeof *cap;
  build_capture (cap, n_cap, &at, 1u, 0.02, 3u);
  const size_t cut = at + 2u * ACQ_SF * SPC; /* inside the preamble */

  static float complex out[4 * BURST_LEN];
  void                *blob = NULL;
  size_t               cb   = 0;
  {
    burst_capture_state_t *a = burst_capture_create_backed (
        path, acq_code (), ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6, 55.0, 0.0,
        1e-3, 0.9, 0);
    DP_REQUIRE (a != NULL);
    DP_CHECK (a->recovered == 0); /* the file did not exist yet */
    DP_CHECK (burst_capture_push (a, cap, cut, out, sizeof out / sizeof *out)
              == 0);
    cb   = burst_capture_state_bytes (a);
    blob = malloc (cb);
    DP_REQUIRE (blob != NULL);
    burst_capture_get_state (a, blob);
    burst_capture_destroy (a); /* the ring's memory is gone with it */
  }

  burst_capture_state_t *b = burst_capture_create_backed (
      path, acq_code (), ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6, 55.0, 0.0, 1e-3,
      0.9, 0);
  DP_REQUIRE (b != NULL);
  /* The file was adopted rather than re-made, which is what carries the
     samples across. */
  DP_CHECK (b->recovered == 1);
  DP_CHECK (burst_capture_set_state (b, blob) == DP_OK);

  size_t n = burst_capture_push (b, cap + cut, n_cap - cut, out,
                                 sizeof out / sizeof *out);
  DP_CHECK (n == BURST_LEN);
  DP_CHECK (b->preamble_start == at);

  free (blob);
  burst_capture_destroy (b);
  remove (path);
  return 0;
}

/**
 * A blob claiming retained history, restored against a file that has none, is
 * REFUSED rather than resumed into silence.
 *
 * The positions would be perfectly valid and the samples would be zeros, so
 * the capture would simply never find another burst -- indistinguishable from
 * a quiet stream, which is the failure mode this object exists to prevent.
 */
static int
test_a_blob_without_its_file_is_refused (void)
{
  char src[256], dst[256];
  scratch_path (src, sizeof src, "have");
  scratch_path (dst, sizeof dst, "empty");
  remove (src);
  remove (dst);

  static float complex cap[200000];
  const size_t         at = 60000u;
  build_capture (cap, sizeof cap / sizeof *cap, &at, 1u, 0.02, 3u);

  burst_capture_state_t *a
      = burst_capture_create_backed (src, acq_code (), ACQ_SF, BURST_LEN, REPS,
                                     SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0);
  DP_REQUIRE (a != NULL);
  static float complex out[4 * BURST_LEN];
  burst_capture_push (a, cap, at + 2u * ACQ_SF * SPC, out,
                      sizeof out / sizeof *out);
  /* What makes the blob refusable is that it CLAIMS retained history, which
     any push leaves behind -- not that a detection happened to fire yet. */
  DP_REQUIRE (a->samples_fed > 0);

  size_t cb   = burst_capture_state_bytes (a);
  void  *blob = malloc (cb);
  DP_REQUIRE (blob != NULL);
  burst_capture_get_state (a, blob);

  burst_capture_state_t *b
      = burst_capture_create_backed (dst, acq_code (), ACQ_SF, BURST_LEN, REPS,
                                     SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0);
  DP_REQUIRE (b != NULL);
  DP_CHECK (b->recovered == 0);
  DP_CHECK (burst_capture_set_state (b, blob) == DP_ERR_INVALID);

  free (blob);
  burst_capture_destroy (a);
  burst_capture_destroy (b);
  remove (src);
  remove (dst);
  return 0;
}

/** A backed constructor with no usable path fails as an argument error. */
static int
test_backed_rejects_a_bad_path (void)
{
  DP_CHECK (burst_capture_create_backed (NULL, acq_code (), ACQ_SF, BURST_LEN,
                                         REPS, SPC, 1.0e6, 55.0, 0.0, 1e-3,
                                         0.9, 0)
            == NULL);
  DP_CHECK (burst_capture_create_backed ("", acq_code (), ACQ_SF, BURST_LEN,
                                         REPS, SPC, 1.0e6, 55.0, 0.0, 1e-3,
                                         0.9, 0)
            == NULL);
  DP_CHECK (burst_capture_create_backed ("/nonexistent-dir-dp/ring.cf32",
                                         acq_code (), ACQ_SF, BURST_LEN, REPS,
                                         SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0)
            == NULL);
  return 0;
}

/** A blob whose envelope is wrong is REJECTED, never reinterpreted. */
static int
test_state_rejects_a_foreign_blob (void)
{
  burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  size_t cb   = burst_capture_state_bytes (s);
  void  *blob = malloc (cb);
  DP_REQUIRE (blob != NULL);
  burst_capture_get_state (s, blob);
  ((unsigned char *)blob)[0] ^= 0xFFu;
  DP_CHECK (burst_capture_set_state (s, blob) == DP_ERR_INVALID);
  free (blob);
  burst_capture_destroy (s);
  return 0;
}

int
main (void)
{
  /* DP_TEST_END, not JM_TEST_EPILOGUE: the two headers keep SEPARATE
     counters, so a file asserting with DP_CHECK and ending with jm's
     epilogue reports PASSED while printing its own failures. This file did
     exactly that until it was run by hand (doppler#1169). */
  if (test_create_copies_and_derives ())
    return 1;
  if (test_create_rejects_bad_parameters ())
    return 1;
  if (test_window_starts_at_the_burst ())
    return 1;
  if (test_every_burst_is_emitted_once ())
    return 1;
  if (test_block_size_does_not_change_the_answer ())
    return 1;
  if (test_never_returns_a_partial_window ())
    return 1;
  if (test_short_trailing_context_holds_the_burst ())
    return 1;
  if (test_reset_clears_position_not_history ())
    return 1;
  if (test_state_resumes_mid_burst ())
    return 1;
  if (test_state_rejects_a_foreign_blob ())
    return 1;
  if (test_push_max_out_bounds_a_real_push ())
    return 1;
  if (test_events_describe_the_last_push ())
    return 1;
  if (test_detections_are_what_the_search_found ())
    return 1;
  if (test_accessors_agree_with_the_event ())
    return 1;
  if (test_configure_search_raw_reaches_the_engine ())
    return 1;
  if (test_destroy_null_is_safe ())
    return 1;
  if (test_state_bytes_does_not_move_with_the_stream ())
    return 1;
  if (test_a_push_larger_than_the_ring_is_sliced ())
    return 1;
  if (test_a_captured_burst_suppresses_its_own_payload ())
    return 1;
  if (test_min_gap_is_derived_and_sufficient ())
    return 1;
  if (test_refine_span_bounds_start_to_start ())
    return 1;
  if (test_backed_finds_the_same_burst_with_a_smaller_blob ())
    return 1;
  if (test_history_survives_destroying_the_capture ())
    return 1;
  if (test_a_blob_without_its_file_is_refused ())
    return 1;
  if (test_backed_rejects_a_bad_path ())
    return 1;
  DP_TEST_END ("test_burst_capture_core");
}
