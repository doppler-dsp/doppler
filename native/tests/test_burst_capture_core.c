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
#include "doppler/burst_capture/burst_capture_core.h"
#include "doppler/pn/pn_core.h"

#include "dp_chunk_inv.h"
#include "dp_preamble_test.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h> /* getpid, under its POSIX name; the UCRT has no <unistd.h> */
#else
#include <unistd.h>
#endif

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
      dp_pn_state_t *pn = dp_pn_create (pn_mls_poly (5), 1u, 5u, 0);
      if (pn)
        {
          for (size_t i = 0; i < ACQ_SF; i++)
            c[i] = pn_step (pn);
          dp_pn_destroy (pn);
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

/* A capture from a PN code: the preamble is the code's samples, mapped by
 * dp_bin_to_nrz() and held `spc` a chip (dp_preamble_test.h), at
 * fs = chip_rate * spc -- how a caller builds one. A missing or empty code
 * still reaches the constructor, so its refusal is what is tested. */
static dp_burst_capture_state_t *
capture_from_code_impl (int backed, const char *path, const uint8_t *code,
                        size_t code_len, size_t burst_len, size_t reps,
                        size_t spc, double chip_rate, double cn0_dbhz,
                        double doppler_uncertainty, double pfa, double pd,
                        int noise_mode, double doppler_rate)
{
  const double    fs  = chip_rate * (double)spc;
  const size_t    n   = (code && code_len && spc) ? code_len * spc : 0;
  float _Complex *pre = n ? dp_code_preamble (code, code_len, spc) : NULL;
  dp_burst_capture_state_t *s
      = backed ? dp_burst_capture_create_backed (
                     path, pre, n, burst_len, reps, fs, cn0_dbhz,
                     doppler_uncertainty, pfa, pd, noise_mode, doppler_rate)
               : dp_burst_capture_create (pre, n, burst_len, reps, fs,
                                          cn0_dbhz, doppler_uncertainty, pfa,
                                          pd, noise_mode, doppler_rate);
  free (pre);
  return s;
}

static dp_burst_capture_state_t *
capture_from_code (const uint8_t *code, size_t code_len, size_t burst_len,
                   size_t reps, size_t spc, double chip_rate, double cn0_dbhz,
                   double doppler_uncertainty, double pfa, double pd,
                   int noise_mode, double doppler_rate)
{
  return capture_from_code_impl (0, NULL, code, code_len, burst_len, reps, spc,
                                 chip_rate, cn0_dbhz, doppler_uncertainty, pfa,
                                 pd, noise_mode, doppler_rate);
}

static dp_burst_capture_state_t *
capture_from_code_backed (const char *path, const uint8_t *code,
                          size_t code_len, size_t burst_len, size_t reps,
                          size_t spc, double chip_rate, double cn0_dbhz,
                          double doppler_uncertainty, double pfa, double pd,
                          int noise_mode, double doppler_rate)
{
  return capture_from_code_impl (1, path, code, code_len, burst_len, reps, spc,
                                 chip_rate, cn0_dbhz, doppler_uncertainty, pfa,
                                 pd, noise_mode, doppler_rate);
}

/** @brief One burst: REPS preamble repetitions then a spread payload. */
static size_t
build_burst (float _Complex *y)
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
build_capture (float _Complex *cap, size_t n_cap, const size_t *at,
               size_t n_at, double sigma, uint32_t seed)
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
  static float _Complex burst[1 << 16];
  size_t nb = build_burst (burst);
  for (size_t k = 0; k < n_at; k++)
    for (size_t i = 0; i < nb && at[k] + i < n_cap; i++)
      cap[at[k] + i] += burst[i];
}

/* No design C/N0: the suite's scenes are strong (sigma 0.02, ~67 dB-Hz
 * measured), so the search integrates the whole preamble in one look and
 * there is no target to be under. 55 dB-Hz used to sit here and was "met"
 * only through a second non-coherent look the burst could not fill
 * (doppler#1181); under the honest model it is underpowered at this depth. */
static dp_burst_capture_state_t *
make (void)
{
  return capture_from_code (acq_code (), ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6,
                            ACQ_CN0_NONE, 0.0, 1e-3, 0.9, 0, 0.0);
}

/**
 * @brief 1 if every transmitted burst in @p at came back EXACTLY once.
 *
 * The seed-11 scene most tests here share carries one false alarm under the
 * one-look grid (at 21298, ~50 dB-Hz against ~67 for the real ones) -- the
 * design pfa at work, and the object's certification (F2) says a caller
 * filters those on `cn0_dbhz_est`. So "the bursts came back" is asked by
 * POSITION, and a count is compared against `dp_burst_capture_ready()` rather
 * than against the number transmitted.
 */
static int
real_windows_once (const dp_burst_capture_state_t *s, const size_t *at,
                   size_t n_at)
{
  for (size_t k = 0; k < n_at; k++)
    {
      size_t seen = 0;
      for (size_t i = 0; i < dp_burst_capture_ready (s); i++)
        seen += dp_burst_capture_event_at (s, i)->preamble_start == at[k];
      if (seen != 1u)
        return 0;
    }
  return 1;
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
  dp_burst_capture_state_t *s = capture_from_code (
      code, ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0, 0.0);
  DP_REQUIRE (s != NULL);

  /* The preamble lives on as the engine's reference row, the one replica
     refine reads. capture_from_code() freed its buffer before this reads it,
     so a borrowing constructor would show up here; +-1 chips are already
     unit RMS, so the row IS the preamble. */
  float _Complex *pre = dp_code_preamble (code, ACQ_SF, SPC);
  DP_CHECK (memcmp (s->acq->engine->ref, pre, ACQ_SF * SPC * sizeof *pre)
            == 0);
  free (pre);
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
  DP_CHECK (dp_burst_capture_get_doppler_res_hz (s) > 0.0);
  DP_CHECK (dp_burst_capture_get_doppler_res_hz (s)
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
    dp_burst_capture_state_t *big
        = capture_from_code (code, ACQ_SF, 20u * BURST_LEN, REPS, SPC, 1.0e6,
                             55.0, 0.0, 1e-3, 0.9, 0, 0.0);
    DP_REQUIRE (big != NULL);
    DP_CHECK (big->q_cap > s->q_cap);
    DP_CHECK (s->q_cap >= 8u); /* ...and the floor still holds */
    dp_burst_capture_destroy (big);
  }

  dp_burst_capture_destroy (s);
  return 0;
}

/* Every guarded parameter is an ARGUMENT error, so create() returns NULL and
 * the binding turns that into a ValueError naming the constraint. */
static int
test_create_rejects_bad_parameters (void)
{
  const uint8_t *c = acq_code ();
  DP_CHECK (capture_from_code (NULL, ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6, 55.0,
                               0.0, 1e-3, 0.9, 0, 0.0)
            == NULL);
  DP_CHECK (capture_from_code (c, 0, BURST_LEN, REPS, SPC, 1.0e6, 55.0, 0.0,
                               1e-3, 0.9, 0, 0.0)
            == NULL);
  /* burst_len is the parameter this object exists to take; zero of it is not
     a capture. */
  DP_CHECK (capture_from_code (c, ACQ_SF, 0, REPS, SPC, 1.0e6, 55.0, 0.0, 1e-3,
                               0.9, 0, 0.0)
            == NULL);
  DP_CHECK (capture_from_code (c, ACQ_SF, BURST_LEN, 0, SPC, 1.0e6, 55.0, 0.0,
                               1e-3, 0.9, 0, 0.0)
            == NULL);
  DP_CHECK (capture_from_code (c, ACQ_SF, BURST_LEN, REPS, 0, 1.0e6, 55.0, 0.0,
                               1e-3, 0.9, 0, 0.0)
            == NULL);
  DP_CHECK (capture_from_code (c, ACQ_SF, BURST_LEN, REPS, SPC, 0.0, 55.0, 0.0,
                               1e-3, 0.9, 0, 0.0)
            == NULL);
  DP_CHECK (capture_from_code (c, ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6,
                               INFINITY, 0.0, 1e-3, 0.9, 0, 0.0)
            == NULL); /* no design point is infinite; NaN means "none" */
  DP_CHECK (capture_from_code (c, ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6, 55.0,
                               0.0, 0.0, 0.9, 0, 0.0)
            == NULL);
  DP_CHECK (capture_from_code (c, ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6, 55.0,
                               0.0, 1e-3, 1.0, 0, 0.0)
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
  static float _Complex cap[80000];
  const size_t at = 9000u;
  build_capture (cap, sizeof cap / sizeof *cap, &at, 1u, 0.02, 7u);

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);

  static float _Complex out[4 * BURST_LEN];
  size_t n = dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                    sizeof out / sizeof *out);

  DP_CHECK (n == BURST_LEN);
  DP_CHECK (dp_burst_capture_ready (s) == 1u);
  DP_CHECK (s->preamble_start == at);

  const burst_capture_event_t *ev = dp_burst_capture_event_at (s, 0);
  DP_REQUIRE (ev != NULL);
  DP_CHECK (ev->preamble_start == at);
  DP_CHECK (ev->doppler_res_hz > 0.0);

  /* The window is the burst, not a window near it. */
  static float _Complex burst[1 << 16];
  build_burst (burst);
  double num = 0.0, den = 0.0;
  for (size_t i = 0; i < 8u * ACQ_SF * SPC; i++)
    {
      num += (double)crealf (out[i]) * (double)crealf (burst[i]);
      den += (double)crealf (burst[i]) * (double)crealf (burst[i]);
    }
  DP_CHECK (den > 0.0 && num / den > 0.8);

  /* Both faces read the same scratch, so they must agree sample for sample. */
  const float _Complex *w = dp_burst_capture_window (s, 0);
  DP_REQUIRE (w != NULL);
  DP_CHECK (memcmp (w, out, BURST_LEN * sizeof *w) == 0);
  DP_CHECK (dp_burst_capture_window (s, 1) == NULL);

  DP_CHECK (s->dropped == 0);
  DP_CHECK (s->n_bursts == 1u);
  dp_burst_capture_destroy (s);
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
  static float _Complex cap[200000];
  const size_t at[3] = { 9000u, 60000u, 120000u };
  build_capture (cap, sizeof cap / sizeof *cap, at, 3u, 0.02, 11u);

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);

  static float _Complex out[8 * BURST_LEN];
  size_t n = dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                    sizeof out / sizeof *out);

  /* Every transmitted burst comes back EXACTLY once. Windows beyond those
     are the design pfa doing what it says -- this scene carries one, at
     21298 (cn0 50.5 against ~67 for the real ones) -- and the object's own
     certification (F2) says a caller tells them apart by `cn0_dbhz_est`,
     which is pinned here rather than assumed away: an extra window may not
     out-score a real one. The old assertion was "exactly three", and it
     held only because a second non-coherent look the burst could not fill
     happened to gate that false alarm (doppler#1181). */
  const size_t ready = dp_burst_capture_ready (s);
  DP_CHECK (n == ready * BURST_LEN);
  DP_REQUIRE (ready >= 3u);
  double weakest_real = 1e9;
  for (size_t k = 0; k < 3u; k++)
    {
      size_t seen = 0;
      for (size_t i = 0; i < ready; i++)
        if (dp_burst_capture_event_at (s, i)->preamble_start == at[k])
          {
            seen++;
            if (dp_burst_capture_event_at (s, i)->cn0_dbhz_est < weakest_real)
              weakest_real = dp_burst_capture_event_at (s, i)->cn0_dbhz_est;
          }
      DP_CHECK (seen == 1u);
    }
  for (size_t i = 0; i < ready; i++)
    {
      const burst_capture_event_t *ev   = dp_burst_capture_event_at (s, i);
      int                          real = 0;
      for (size_t k = 0; k < 3u; k++)
        real |= ev->preamble_start == at[k];
      if (!real)
        DP_CHECK (ev->cn0_dbhz_est < weakest_real);
    }
  DP_CHECK (s->dropped == 0);
  DP_CHECK (s->n_bursts == ready);
  DP_CHECK (s->pending == 0);
  dp_burst_capture_destroy (s);
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
  static float _Complex cap[200000];
  const size_t at[3] = { 9000u, 60000u, 120000u };
  const size_t n_cap = sizeof cap / sizeof *cap;
  build_capture (cap, n_cap, at, 3u, 0.02, 11u);

  dp_burst_capture_state_t *a = make ();
  dp_burst_capture_state_t *b = make ();
  DP_REQUIRE (a != NULL && b != NULL);

  static float _Complex out_a[8 * BURST_LEN];
  static float _Complex out_b[8 * BURST_LEN];
  size_t na = dp_burst_capture_push (a, cap, n_cap, out_a,
                                     sizeof out_a / sizeof *out_a);

  size_t nb = 0;
  for (size_t off = 0; off < n_cap; off += 333u)
    {
      size_t blk = n_cap - off < 333u ? n_cap - off : 333u;
      nb += dp_burst_capture_push (b, cap + off, blk, out_b + nb,
                                   sizeof out_b / sizeof *out_b - nb);
    }

  DP_CHECK (na == nb);
  DP_CHECK (na == dp_burst_capture_ready (a) * BURST_LEN);
  DP_CHECK (
      real_windows_once (a, at, 3u)); /* b's events are its LAST push's */
  DP_CHECK (memcmp (out_a, out_b, na * sizeof *out_a) == 0);
  DP_CHECK (a->n_bursts == b->n_bursts);
  dp_burst_capture_destroy (a);
  dp_burst_capture_destroy (b);
  return 0;
}

/**
 * Block size does not change the answer BELOW `min_gap` either, where bursts
 * sit closer than `refine_span` start to start (doppler#1527). The claim loop
 * used to claim a whole chunk's detections before draining once, so a burst
 * a small-block caller had already been handed was still pending when the
 * next burst's detections arrived, and they merged: four bursts at a quarter
 * of min_gap came back as ONE wrong window pushed whole and as four exact
 * ones in small blocks. Starts are compared per block size, every event of
 * every push collected, and each true start must come back exactly once.
 */
static int
test_block_size_below_min_gap (void)
{
  static float _Complex cap[120000];
  const size_t              n_cap = sizeof cap / sizeof *cap;
  dp_burst_capture_state_t *probe = make ();
  DP_REQUIRE (probe != NULL);
  const size_t gap = dp_burst_capture_get_min_gap (probe) / 4u;
  DP_REQUIRE (gap > 0u && BURST_LEN + gap < probe->refine_span);
  dp_burst_capture_destroy (probe);

  size_t at[4];
  for (size_t k = 0; k < 4u; k++)
    at[k] = 3000u + k * (BURST_LEN + gap);
  DP_REQUIRE (at[3] + BURST_LEN + 2u * BURST_LEN < n_cap);
  build_capture (cap, n_cap, at, 4u, 0.02, 11u);

  const size_t blocks[3] = { n_cap, 8192u, 333u };
  uint64_t     got[3][16];
  size_t       n_got[3] = { 0 };
  for (size_t b = 0; b < 3u; b++)
    {
      dp_burst_capture_state_t *s = make ();
      DP_REQUIRE (s != NULL);
      for (size_t off = 0; off < n_cap; off += blocks[b])
        {
          size_t blk = n_cap - off < blocks[b] ? n_cap - off : blocks[b];
          (void)dp_burst_capture_push (s, cap + off, blk, NULL, 0);
          for (size_t i = 0; i < dp_burst_capture_ready (s) && n_got[b] < 16u;
               i++)
            got[b][n_got[b]++]
                = dp_burst_capture_event_at (s, i)->preamble_start;
        }
      dp_burst_capture_destroy (s);
    }

  for (size_t b = 1; b < 3u; b++)
    {
      DP_CHECK (n_got[b] == n_got[0]);
      for (size_t i = 0; i < n_got[0] && i < n_got[b]; i++)
        DP_CHECK (got[b][i] == got[0][i]);
    }
  for (size_t k = 0; k < 4u; k++)
    {
      size_t seen = 0;
      for (size_t i = 0; i < n_got[0]; i++)
        seen += got[0][i] == at[k];
      DP_CHECK (seen == 1u);
    }
  return 0;
}

/**
 * A held head does not stall the bursts behind it (doppler#1534).
 *
 * Bursts LONGER than `refine_span`, whose payload keeps firing against the
 * acquisition code after the window is emitted: those late detections are
 * held (shadowed) for a release() verdict. emit() used to stop at a held
 * head, so a complete window behind it waited, kept the history tail
 * pinned, and inside one long push the ring refused a chunk -- a whole
 * push of four such bursts lost one and dropped tens of thousands of
 * samples, where 333-sample blocks lost nothing. Asserted: nothing dropped,
 * and every burst at its exact start, whole and in blocks.
 *
 * Since push() stopped refusing input (doppler#2015) this scene cannot lose
 * a burst that way: a stall behind a held head now costs latency -- trim
 * sweeps the held entry once the tail passes it -- not samples, so putting
 * the stall back leaves this test green. The stall is caught where it
 * still loses one, test_release_on_every_window_never_wedges.
 */
static int
test_a_held_head_does_not_stall_long_bursts (void)
{
  const size_t LONG_LEN = 4u * BURST_LEN, GAP = 500u, N = 4u;
  const size_t n_syms = (LONG_LEN / SPC - REPS * ACQ_SF) / DATA_SF;
  static float _Complex burst[4u * BURST_LEN];
  {
    const uint8_t *acode = acq_code (), *dcode = data_code ();
    size_t         k = 0;
    for (size_t r = 0; r < REPS; r++)
      for (size_t c = 0; c < ACQ_SF; c++)
        for (size_t m = 0; m < SPC; m++)
          burst[k++] = csign (acode[c]);
    uint32_t st = 1534u;
    for (size_t j = 0; j < n_syms; j++)
      {
        float a = csign ((uint8_t)(dp_xs32 (&st) >> 31));
        for (size_t c = 0; c < DATA_SF; c++)
          for (size_t m = 0; m < SPC; m++)
            burst[k++] = a * csign (dcode[c]);
      }
    DP_REQUIRE (k <= LONG_LEN);
  }
  static float _Complex cap[6u * 4u * BURST_LEN];
  const size_t n_cap = sizeof cap / sizeof *cap;
  size_t       at[4];
  for (size_t b = 0; b < N; b++)
    at[b] = 1000u + b * (LONG_LEN + GAP);
  DP_REQUIRE (at[N - 1] + 2u * LONG_LEN <= n_cap);
  build_capture (cap, n_cap, NULL, 0u, 0.02, 1534u);
  for (size_t b = 0; b < N; b++)
    for (size_t i = 0; i < LONG_LEN; i++)
      cap[at[b] + i] += burst[i];

  const size_t blocks[2] = { n_cap, 333u };
  for (size_t v = 0; v < 2u; v++)
    {
      dp_burst_capture_state_t *s
          = capture_from_code (acq_code (), ACQ_SF, LONG_LEN, REPS, SPC, 1.0e6,
                               ACQ_CN0_NONE, 0.0, 1e-3, 0.9, 0, 0.0);
      DP_REQUIRE (s != NULL);
      DP_REQUIRE (LONG_LEN > s->refine_span); /* the premise */
      size_t seen[4] = { 0 };
      for (size_t off = 0; off < n_cap; off += blocks[v])
        {
          size_t blk = n_cap - off < blocks[v] ? n_cap - off : blocks[v];
          (void)dp_burst_capture_push (s, cap + off, blk, NULL, 0);
          for (size_t i = 0; i < dp_burst_capture_ready (s); i++)
            for (size_t b = 0; b < N; b++)
              seen[b]
                  += dp_burst_capture_event_at (s, i)->preamble_start == at[b];
        }
      DP_CHECK (s->dropped == 0);
      for (size_t b = 0; b < N; b++)
        DP_CHECK (seen[b] == 1u);
      dp_burst_capture_destroy (s);
    }
  return 0;
}

/**
 * release() of every window never wedges the history ring (doppler#2028),
 * and every epoch is the burst's stream position (doppler#2015).
 *
 * A dense train -- dead air 0.08 to 0.18 of `min_gap` -- pushed in constant
 * 8192-sample blocks, the consumer releasing every window it is handed. Here
 * a burst's early anchor lands inside the previous window, is shadowed, and
 * comes back on release. emit() used to SWAP the next entry to the queue
 * head, so the released burst sat behind a newer one whose window had not
 * arrived; drain stopped there, the complete burst pinned the ring, and at
 * this block size the ring refused every later push -- and every epoch after
 * the first refusal was early by what it had refused. Asserted at each gap:
 * every burst exactly once at its true start, nothing dropped, and the
 * stream position equal to what was pushed.
 */
static int
test_release_on_every_window_never_wedges (void)
{
  static float _Complex cap[5000u + 12u * (BURST_LEN + 200u) + 3u * BURST_LEN];
  const size_t              n_cap = sizeof cap / sizeof *cap;
  dp_burst_capture_state_t *probe = make ();
  DP_REQUIRE (probe != NULL);
  const size_t min_gap = dp_burst_capture_get_min_gap (probe);
  dp_burst_capture_destroy (probe);

  const double frac[3] = { 0.08, 0.13, 0.18 };
  for (size_t g = 0; g < 3u; g++)
    {
      const size_t gap = (size_t)(frac[g] * (double)min_gap);
      DP_REQUIRE (gap > 0u && gap <= 200u);
      size_t at[12];
      for (size_t k = 0; k < 12u; k++)
        at[k] = 5000u + k * (BURST_LEN + gap);
      build_capture (cap, n_cap, at, 12u, 0.02, 2028u + (uint32_t)g);

      dp_burst_capture_state_t *s = make ();
      DP_REQUIRE (s != NULL);
      size_t seen[12] = { 0 };
      for (size_t off = 0; off < n_cap; off += 8192u)
        {
          size_t blk = n_cap - off < 8192u ? n_cap - off : 8192u;
          (void)dp_burst_capture_push (s, cap + off, blk, NULL, 0);
          for (size_t i = 0; i < dp_burst_capture_ready (s); i++)
            {
              uint64_t ps = dp_burst_capture_event_at (s, i)->preamble_start;
              for (size_t k = 0; k < 12u; k++)
                seen[k] += ps == at[k];
              DP_CHECK (dp_burst_capture_release (s, i) == DP_OK);
            }
        }
      DP_CHECK (s->dropped == 0);
      DP_CHECK (s->samples_fed == n_cap);
      for (size_t k = 0; k < 12u; k++)
        DP_CHECK (seen[k] == 1u);
      dp_burst_capture_destroy (s);
    }
  return 0;
}

/**
 * A detection whose history is gone is swept, counted, and never wedges the
 * ring (doppler#2015).
 *
 * The fault is INJECTED: an entry refined to a start the ring has already
 * released. Its window's first sample is behind the tail, no push can bring
 * it back, and left in place it would block drain and pin trim until the
 * ring filled. The sweep at the next trim takes it, and `dropped` counts
 * exactly the part of its window that is gone -- 300 samples here.
 *
 * Asserted: the bursts on either side of where it would have stalled the
 * stream -- one arriving while it is queued, one well after -- come out at
 * their exact starts as the input's own samples, nothing spliced across a
 * gap, and the stream position equals what was pushed. With the sweep
 * removed, push() stops (aborts) rather than spinning.
 */
static int
test_a_dead_entry_is_swept (void)
{
  static float _Complex cap[40000];
  const size_t n_cap = sizeof cap / sizeof *cap, LEAD = 12000u;
  const size_t at[2] = { 14000u, 26000u };
  build_capture (cap, n_cap, at, 2u, 0.02, 2015u);

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  (void)dp_burst_capture_push (s, cap, LEAD, NULL, 0);
  DP_REQUIRE (dp_burst_capture_ready (s) == 0u && s->pending == 0u);
  DP_REQUIRE (s->hist->tail > 300u);

  /* Refined, unshadowed, its window starting 300 samples behind the tail. */
  burst_capture_pending_t *dead = &s->q[s->q_head];
  memset (dead, 0, sizeof *dead);
  dead->start   = (uint64_t)s->hist->tail - 300u;
  dead->anchor  = dead->start;
  dead->refined = 1;
  s->pending    = 1u;

  size_t found[2] = { 0 };
  for (size_t off = LEAD; off < n_cap; off += 8192u)
    {
      size_t blk = n_cap - off < 8192u ? n_cap - off : 8192u;
      (void)dp_burst_capture_push (s, cap + off, blk, NULL, 0);
      for (size_t i = 0; i < dp_burst_capture_ready (s); i++)
        for (size_t k = 0; k < 2u; k++)
          if (dp_burst_capture_event_at (s, i)->preamble_start == at[k])
            {
              found[k]++;
              DP_CHECK (memcmp (dp_burst_capture_window (s, i), cap + at[k],
                                BURST_LEN * sizeof *cap)
                        == 0);
            }
    }
  DP_CHECK (found[0] == 1u && found[1] == 1u);
  DP_CHECK (s->dropped == 300u);
  DP_CHECK (s->samples_fed == n_cap);
  dp_burst_capture_destroy (s);
  return 0;
}

/**
 * A resume is EXACT: a capture restored from its blob emits what the live one
 * does (doppler#2015 review).
 *
 * A live queue can hold history past `retain_span`: a burst released after
 * its push keeps its look-back until the next push emits it. A blob that
 * carried only `retain_span` dropped that burst -- a checkpoint that resumed
 * as something else, which is a reset. The blob now carries everything the
 * ring holds. The scene is gate (a)'s; a live capture and one restored from
 * its blob after every push and release() run in lockstep, and every window
 * the restored one emits is compared with the live one's. The ring is
 * required to have held more than `retain_span` somewhere -- the case the old
 * blob lost -- so the test cannot pass on a scene that never reaches it.
 */
static int
test_a_resume_is_exact (void)
{
  static float _Complex cap[5000u + 12u * (BURST_LEN + 200u) + 3u * BURST_LEN];
  const size_t              n_cap = sizeof cap / sizeof *cap;
  dp_burst_capture_state_t *a     = make ();
  dp_burst_capture_state_t *b     = make ();
  DP_REQUIRE (a != NULL && b != NULL);
  const size_t gap = (size_t)(0.13 * (double)dp_burst_capture_get_min_gap (a));
  size_t       at[12];
  for (size_t k = 0; k < 12u; k++)
    at[k] = 5000u + k * (BURST_LEN + gap);
  build_capture (cap, n_cap, at, 12u, 0.02, 2029u);

  const size_t   cb   = dp_burst_capture_state_bytes (a);
  unsigned char *blob = malloc (cb);
  DP_REQUIRE (blob != NULL);
  size_t over = 0, seen = 0;
  for (size_t off = 0; off < n_cap; off += 8192u)
    {
      size_t blk = n_cap - off < 8192u ? n_cap - off : 8192u;
      (void)dp_burst_capture_push (a, cap + off, blk, NULL, 0);
      (void)dp_burst_capture_push (b, cap + off, blk, NULL, 0);
      DP_CHECK (dp_burst_capture_ready (b) == dp_burst_capture_ready (a));
      for (size_t i = 0;
           i < dp_burst_capture_ready (a) && i < dp_burst_capture_ready (b);
           i++)
        {
          DP_CHECK (dp_burst_capture_event_at (b, i)->preamble_start
                    == dp_burst_capture_event_at (a, i)->preamble_start);
          DP_CHECK (memcmp (dp_burst_capture_window (b, i),
                            dp_burst_capture_window (a, i),
                            BURST_LEN * sizeof (float _Complex))
                    == 0);
          seen++;
        }
      for (size_t i = 0; i < dp_burst_capture_ready (a); i++)
        DP_CHECK (dp_burst_capture_release (a, i) == DP_OK);
      over += dp_f32_available (a->hist) > a->retain_span;
      dp_burst_capture_get_state (a, blob);
      DP_REQUIRE (dp_burst_capture_set_state (b, blob) == DP_OK);
      DP_CHECK (b->pending == a->pending);
      DP_CHECK (b->dropped == a->dropped);
    }
  DP_CHECK (over > 0u); /* the premise: past retain_span, where it shed */
  DP_CHECK (seen >= 12u);
  DP_CHECK (a->dropped == 0u);
  free (blob);
  dp_burst_capture_destroy (a);
  dp_burst_capture_destroy (b);
  return 0;
}

/**
 * A blob's queue is checked, not trusted (doppler#2015 review).
 *
 * set_state() takes a blob's queue as bytes, so a forged one can name
 * anything, and the ring bound push() relies on rests on that queue. An
 * entry whose span the restored ring cannot reach -- here refined 100
 * samples behind the tail, which left in place would pin the ring for
 * ever -- is swept and counted, and the stream goes on: the later burst is
 * exact. What the bound rests on is refused: phases past their array,
 * anchors out of order, a refined start outside the range refine chooses
 * from.
 * Refine's own first and last candidates are accepted, so the check is not
 * too tight.
 */
static int
test_a_forged_blob_is_checked (void)
{
  static float _Complex cap[40000];
  const size_t n_cap = sizeof cap / sizeof *cap, LEAD = 12000u;
  const size_t AT = 26000u;
  build_capture (cap, n_cap, &AT, 1u, 0.02, 2030u);

  dp_burst_capture_state_t *a = make ();
  dp_burst_capture_state_t *b = make ();
  DP_REQUIRE (a != NULL && b != NULL);
  (void)dp_burst_capture_push (a, cap, LEAD, NULL, 0);
  DP_REQUIRE (a->pending == 0u && a->hist->tail > 100u);
  const size_t   cb   = dp_burst_capture_state_bytes (a);
  unsigned char *blob = malloc (cb);
  DP_REQUIRE (blob != NULL);
  const uint64_t P    = a->code_period;
  const uint64_t head = a->hist->head;

  /* A live-looking entry: unarrived, its phase its anchor's own. */
  burst_capture_pending_t live;
  memset (&live, 0, sizeof live);
  live.anchor   = head - 100u;
  live.n_phase  = 1u;
  live.phase[0] = (uint32_t)(live.anchor % P);

  /* Dead: swept and counted, never left to pin the ring. */
  {
    burst_capture_pending_t *e = &a->q[a->q_head];
    memset (e, 0, sizeof *e);
    e->start   = (uint64_t)a->hist->tail - 100u;
    e->anchor  = e->start;
    e->refined = 1;
    a->pending = 1u;
    dp_burst_capture_get_state (a, blob);
    DP_CHECK (dp_burst_capture_set_state (b, blob) == DP_OK);
    DP_CHECK (b->pending == 0u);
    DP_CHECK (b->dropped == a->dropped + 100u);
    size_t found = 0;
    for (size_t off = LEAD; off < n_cap; off += 8192u)
      {
        size_t blk = n_cap - off < 8192u ? n_cap - off : 8192u;
        (void)dp_burst_capture_push (b, cap + off, blk, NULL, 0);
        for (size_t i = 0; i < dp_burst_capture_ready (b); i++)
          found += dp_burst_capture_event_at (b, i)->preamble_start == AT;
      }
    DP_CHECK (found == 1u);
    DP_CHECK (b->samples_fed == n_cap);
  }

  /* What the bound rests on. Each case restores into a fresh instance, so
     one refusal cannot leave the next one's target dirty. */
  static const char *const what[6]
      = { "phases past their array",
          "anchors out of order",
          "a start past refine's last candidate",
          "a start before refine's first candidate",
          "refine's last candidate",
          "refine's first candidate" };
  static const int ok[6] = { 0, 0, 0, 0, 1, 1 };
  for (size_t c = 0; c < 6u; c++)
    {
      burst_capture_pending_t *e0 = &a->q[a->q_head];
      burst_capture_pending_t *e1 = &a->q[(a->q_head + 1u) % a->q_cap];
      *e0                         = live;
      a->pending                  = 1u;
      switch (c)
        {
        case 0:
          e0->n_phase = BURST_CAPTURE_MAX_PHASES + 1u;
          break;
        case 1:
          *e1 = live;
          e1->anchor -= 1u;
          e1->phase[0] = (uint32_t)(e1->anchor % P);
          a->pending   = 2u;
          break;
        case 2:
          e0->refined = 1;
          e0->start   = live.anchor + (a->k_hi + 1u) * P;
          break;
        case 3:
          e0->refined = 1;
          e0->start   = live.anchor - (a->k_lo + 1u) * P;
          break;
        case 4:
          e0->refined = 1;
          e0->start   = live.anchor + a->k_hi * P;
          break;
        default:
          e0->refined = 1;
          e0->start   = live.anchor - a->k_lo * P;
          break;
        }
      dp_burst_capture_get_state (a, blob);
      dp_burst_capture_state_t *t = make ();
      DP_REQUIRE (t != NULL);
      const int rc = dp_burst_capture_set_state (t, blob);
      if ((rc == DP_OK) != ok[c])
        printf ("  forged blob, %s: set_state returned %d\n", what[c], rc);
      DP_CHECK ((rc == DP_OK) == ok[c]);
      dp_burst_capture_destroy (t);
    }
  free (blob);
  dp_burst_capture_destroy (a);
  dp_burst_capture_destroy (b);
  return 0;
}

/**
 * A refused blob changes nothing a checkpoint holds (doppler#2033 review).
 *
 * set_state() checks a blob whole before it writes anything, so a refusal
 * leaves the SAME instance as it was: its next get_state() is
 * byte-identical, and push() goes on from where it stood -- no abort, the
 * later burst exact, nothing dropped. Each forgery edits a copy of the
 * capture's own blob, so all but the forged field is a blob it accepts:
 *   (A) a queue the trim bound cannot rest on: phases past their array, and
 *       anchors out of order;
 *   (B) a look-back longer than the stream, which would wrap the tail;
 *   (C) an anchor past the stream position;
 *   (D) an acquisition child taken at another stream position, which acq
 *       itself accepts -- refused after it is restored, and put back;
 *   (1) a stream position at 2^63 or past it, with an acquisition child
 *       that stands there too, so nothing else refuses it;
 *   (2) a suppression span that ends past the stream position;
 *   (3) a queued entry with a non-finite Doppler, C/N0 or peak;
 *   (4) an engine whose state no longer fits the blob's child region (a C
 *       caller raised max_peaks after create) -- on its own instance, since
 *       raising max_peaks clears the engine's peaks.
 * Every case restores into the instance it compares; none starts afresh.
 */
static int
test_a_refused_blob_changes_nothing (void)
{
  static float _Complex cap[40000];
  const size_t n_cap = sizeof cap / sizeof *cap, LEAD = 6000u;
  const size_t AT = 26000u;
  build_capture (cap, n_cap, &AT, 1u, 0.02, 2033u);

  dp_burst_capture_state_t *a = make ();
  dp_burst_capture_state_t *b = make ();
  DP_REQUIRE (a != NULL && b != NULL);
  (void)dp_burst_capture_push (a, cap, LEAD, NULL, 0);
  (void)dp_burst_capture_push (b, cap, LEAD + 1000u, NULL, 0);
  DP_REQUIRE (!a->backed && a->samples_fed == LEAD);

  const size_t   cb    = dp_burst_capture_state_bytes (a);
  unsigned char *blob  = malloc (cb);
  unsigned char *other = malloc (cb);
  unsigned char *bad   = malloc (cb);
  unsigned char *now   = malloc (cb);
  unsigned char *huge  = malloc (cb);
  DP_REQUIRE (blob && other && bad && now && huge);
  dp_burst_capture_get_state (a, blob);
  dp_burst_capture_get_state (b, other);

  /* An acquisition child past 2^63 that acq itself accepts: b's engine moved
     on by a whole number of dwells -- its position, its framer's frame count
     and written count together -- so the carry, the block epoch and the
     look count all still agree with the position. */
  uint64_t huge_fed;
  {
    dp_acq_state_t *e   = b->acq->engine;
    const uint64_t  per = (uint64_t)e->coherent_bins * e->n_noncoh;
    const uint64_t  k
        = ((UINT64_C (1) << 63) / (per * e->frame_n) + 1u) * per * e->frame_n;
    e->samples_consumed += k;
    e->framer.frames += k / e->frame_n;
    e->framer.written += k;
    huge_fed = dp_acq_position (e);
    DP_REQUIRE (huge_fed >= (UINT64_C (1) << 63));
    dp_burst_capture_get_state (b, huge);
  }

  /* The blob's layout, as get_state writes it; the last line proves it. */
  const size_t E      = sizeof (burst_capture_pending_t);
  const size_t o_fed  = sizeof (dp_state_hdr_t);
  const size_t o_sup  = o_fed + 7u * 8u;
  const size_t o_pend = sizeof (dp_state_hdr_t) + 8u * 8u;
  const size_t o_q    = o_pend + 2u * sizeof (uint32_t);
  const size_t o_n    = o_q + a->q_cap * E;
  const size_t o_an
      = o_n + sizeof (uint32_t) + a->hist->capacity * sizeof (float _Complex);
  DP_REQUIRE (o_an + sizeof (uint32_t) + a->acq_blob_max == cb);
  uint32_t q_head;
  memcpy (&q_head, blob + o_pend + sizeof (uint32_t), sizeof q_head);
  DP_REQUIRE (q_head < a->q_cap);
  DP_REQUIRE ((uint64_t)LEAD + 1u <= a->hist->capacity);

  const uint64_t          P = a->code_period;
  burst_capture_pending_t live;
  memset (&live, 0, sizeof live);
  live.anchor   = LEAD - 100u;
  live.n_phase  = 1u;
  live.phase[0] = (uint32_t)(live.anchor % P);

  static const char *const what[10]
      = { "(A) phases past their array",
          "(A) anchors out of order",
          "(B) look-back longer than the stream",
          "(C) anchor past the stream position",
          "(D) acquisition child at another position",
          "(1) stream position past 2^63, child there too",
          "(2) suppression past the stream position",
          "(3) NaN Doppler",
          "(3) infinite C/N0",
          "(3) NaN peak" };
  for (size_t c = 0; c < 10u; c++)
    {
      memcpy (bad, blob, cb);
      burst_capture_pending_t e0 = live, e1 = live;
      uint32_t                pending = 1u;
      switch (c)
        {
        case 0:
          e0.n_phase = BURST_CAPTURE_MAX_PHASES + 1u;
          break;
        case 1:
          e1.anchor -= 1u;
          e1.phase[0] = (uint32_t)(e1.anchor % P);
          pending     = 2u;
          break;
        case 2:
          {
            const uint32_t n = (uint32_t)LEAD + 1u;
            memcpy (bad + o_n, &n, sizeof n);
            pending = 0u;
          }
          break;
        case 3:
          e0.anchor   = LEAD + 1u;
          e0.phase[0] = (uint32_t)(e0.anchor % P);
          break;
        case 4:
          memcpy (bad + o_an, other + o_an,
                  sizeof (uint32_t) + a->acq_blob_max);
          pending = 0u;
          break;
        case 5:
          memcpy (bad + o_an, huge + o_an,
                  sizeof (uint32_t) + a->acq_blob_max);
          memcpy (bad + o_fed, &huge_fed, sizeof huge_fed);
          pending = 0u;
          break;
        case 6:
          {
            const uint64_t sup = LEAD + 1u;
            memcpy (bad + o_sup, &sup, sizeof sup);
            pending = 0u;
          }
          break;
        case 7:
          e0.doppler_hz = NAN;
          break;
        case 8:
          e0.cn0_dbhz = INFINITY;
          break;
        default:
          e0.peak_mag = NAN;
          break;
        }
      if (pending)
        {
          memcpy (bad + o_pend, &pending, sizeof pending);
          memcpy (bad + o_q + q_head * E, &e0, E);
          if (pending > 1u)
            memcpy (bad + o_q + ((q_head + 1u) % a->q_cap) * E, &e1, E);
        }
      const int rc = dp_burst_capture_set_state (a, bad);
      dp_burst_capture_get_state (a, now);
      if (rc != DP_ERR_INVALID || memcmp (now, blob, cb) != 0)
        fprintf (stderr, "  refused blob: %s\n", what[c]);
      DP_CHECK (rc == DP_ERR_INVALID);
      DP_CHECK (memcmp (now, blob, cb) == 0);
    }

  /* And the stream goes on from where it stood. */
  size_t found = 0;
  for (size_t off = LEAD; off < n_cap; off += 8192u)
    {
      size_t blk = n_cap - off < 8192u ? n_cap - off : 8192u;
      (void)dp_burst_capture_push (a, cap + off, blk, NULL, 0);
      for (size_t i = 0; i < dp_burst_capture_ready (a); i++)
        found += dp_burst_capture_event_at (a, i)->preamble_start == AT;
    }
  DP_CHECK (found == 1u);
  DP_CHECK (a->samples_fed == n_cap);
  DP_CHECK (a->dropped == 0u);

  /* (4) The engine outgrew the child region: refused before the undo
     snapshot or acq's restore could run past it. get_state() leaves a child
     it cannot fit unwritten, so both reads start from zeroed buffers. */
  {
    dp_burst_capture_state_t *d = make ();
    DP_REQUIRE (d != NULL);
    (void)dp_burst_capture_push (d, cap, LEAD, NULL, 0);
    DP_REQUIRE (dp_burst_acq_set_max_peaks (d->acq, ACQ_MAX_PEAKS) == 0);
    DP_REQUIRE (dp_acq_state_bytes (d->acq->engine) > d->acq_blob_max);
    memset (now, 0, cb);
    memset (bad, 0, cb);
    dp_burst_capture_get_state (d, now);
    const int rc = dp_burst_capture_set_state (d, blob);
    dp_burst_capture_get_state (d, bad);
    if (rc != DP_ERR_INVALID || memcmp (now, bad, cb) != 0)
      fprintf (stderr, "  refused blob: (4) engine outgrew its region\n");
    DP_CHECK (rc == DP_ERR_INVALID);
    DP_CHECK (memcmp (now, bad, cb) == 0);
    for (size_t off = LEAD; off < n_cap; off += 8192u)
      (void)dp_burst_capture_push (
          d, cap + off, n_cap - off < 8192u ? n_cap - off : 8192u, NULL, 0);
    DP_CHECK (d->samples_fed == n_cap);
    dp_burst_capture_destroy (d);
  }

  free (blob);
  free (other);
  free (bad);
  free (now);
  free (huge);
  dp_burst_capture_destroy (a);
  dp_burst_capture_destroy (b);
  return 0;
}

/**
 * The history ring always has room after trim, so push() never stops on a
 * full one -- at the worst case the geometry allows.
 *
 * burst_capture_trim states the bound: after sweep, drain and trim,
 * head - tail < max(burst_len, reps*P) + (2*k_lo + k_hi + 2)*P, below the
 * ring's capacity. The worst case is planted directly: two entries with the
 * same anchor, X refined at its LATEST candidate on a phase half a period
 * late, Y at its EARLIEST on a phase half a period early. X is first in
 * anchor order and arrives last, so Y's window completes behind it and Y
 * pins the tail as far back as anything can. The stream is silence, so no
 * detection disturbs the pair. One sample per push, so trim runs at every
 * position: the occupancy it leaves must stay under the bound, must REACH it
 * (it is tight, so this is the worst case), and does exceed
 * retain_span + P -- which is why the bound is not that.
 */
static int
test_the_ring_always_has_room (void)
{
  static float _Complex z[20000];
  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  (void)dp_burst_capture_push (s, z, sizeof z / sizeof *z, NULL, 0);
  DP_REQUIRE (s->pending == 0u);

  const uint64_t          P = s->code_period, head0 = s->hist->head;
  const uint64_t          A = head0 + (s->k_lo + 1u) * P;
  burst_capture_pending_t x, y;
  memset (&x, 0, sizeof x);
  memset (&y, 0, sizeof y);
  x.anchor = y.anchor = A;
  x.n_phase = y.n_phase = 1u;
  x.peak_mag = y.peak_mag = 1e30;
  /* The fold is [-P/2, P/2): P/2 - 1 is the latest phase, -P/2 the
     earliest. */
  x.phase[0] = (uint32_t)((A + P / 2u - 1u) % P);
  y.phase[0] = (uint32_t)((A + P - P / 2u) % P);
  x.refined = y.refined             = 1;
  x.start                           = A + P / 2u - 1u + s->k_hi * P;
  y.start                           = A - P / 2u - s->k_lo * P;
  s->q[s->q_head]                   = x;
  s->q[(s->q_head + 1u) % s->q_cap] = y;
  s->pending                        = 2u;

  const uint64_t big   = BURST_LEN > REPS * P ? BURST_LEN : REPS * P;
  const uint64_t bound = big + (2u * s->k_lo + s->k_hi + 2u) * P;
  DP_REQUIRE (bound < s->hist->capacity); /* the claim, at this geometry */

  /* Up to one sample short of X's window: every trim on the way is checked,
     and Y pins the tail throughout. */
  static const float _Complex one[1] = { 0 };
  uint64_t worst                     = 0;
  for (uint64_t t = head0; t + 1u < x.start + BURST_LEN; t++)
    {
      (void)dp_burst_capture_push (s, one, 1u, NULL, 0);
      /* What trim left, before this push's one sample was written. */
      const uint64_t occ = (uint64_t)(s->hist->head - s->hist->tail) - 1u;
      if (occ > worst)
        worst = occ;
    }
  DP_CHECK (worst < bound);
  DP_CHECK (dp_burst_capture_ready (s) == 0u);
  /* The next trim cannot release anything -- Y still pins and X has not
     arrived -- so this is what it leaves: the peak, one short of the bound
     less the sample about to be written. Tight: this IS the worst case. And
     past retain_span + P, which is why the bound is not that. */
  const uint64_t peak = (uint64_t)(s->hist->head - s->hist->tail);
  DP_CHECK (peak < bound);
  DP_CHECK (peak + 2u == bound);
  DP_CHECK (peak > s->retain_span + P);

  /* At that peak a whole slice does not fit -- the room is less than
     chunk_max -- and it is taken anyway: written as it fits, X emitted on
     arrival, trim releasing behind it. An all-or-nothing write refused it. */
  DP_REQUIRE (dp_f32_space (s->hist) < s->chunk_max);
  const uint64_t fed   = s->samples_fed;
  size_t         x_out = 0;
  (void)dp_burst_capture_push (s, z, s->chunk_max, NULL, 0);
  for (size_t i = 0; i < dp_burst_capture_ready (s); i++)
    x_out += dp_burst_capture_event_at (s, i)->preamble_start == x.start;
  DP_CHECK (s->samples_fed == fed + s->chunk_max);
  DP_CHECK (x_out == 1u);
  DP_CHECK (s->dropped == 0u);
  dp_burst_capture_destroy (s);
  return 0;
}

/**
 * A resume into a FULL ring makes room on the first push (doppler#2015
 * review).
 *
 * A live push can end with the history ring exactly full -- its last write
 * took all the room there was -- and the blob then carries every sample.
 * Restored behind a dead head, a complete burst Y holds trim below the tail:
 * its window has arrived, but its need sits a period past the restored tail.
 * What makes room is the ORDER push() keeps: the sweep takes the dead entry,
 * the drain emits Y, and only then does trim release. With the drain moved
 * after trim, trim releases nothing, the ring has no room, and push()
 * aborts. Asserted: Y comes out at its start, the dead entry is counted, and
 * the one sample is taken.
 */
static int
test_a_full_ring_restored_makes_room (void)
{
  static float _Complex z[20000];
  dp_burst_capture_state_t *a = make ();
  dp_burst_capture_state_t *b = make ();
  DP_REQUIRE (a != NULL && b != NULL);
  (void)dp_burst_capture_push (a, z, sizeof z / sizeof *z, NULL, 0);
  DP_REQUIRE (a->pending == 0u);

  /* Full: the rest of the ring's room, as the stream's next samples. */
  const size_t room = dp_f32_space (a->hist);
  DP_REQUIRE (room > 0u && room <= sizeof z / sizeof *z);
  DP_REQUIRE (dp_f32_write (a->hist, (const float *)z, room));
  a->samples_fed += room;
  DP_REQUIRE (dp_f32_space (a->hist) == 0u);
  /* acq takes the same samples, as push() would have it: set_state refuses
     a child that stands anywhere but the capture's stream position. */
  for (size_t took = 0; took < room;)
    {
      acq_result_t hits[BURST_CAPTURE_HITS];
      (void)dp_burst_acq_push (a->acq, z + took, room - took, hits,
                               BURST_CAPTURE_HITS);
      DP_REQUIRE (dp_burst_acq_consumed (a->acq) > 0u);
      took += dp_burst_acq_consumed (a->acq);
    }
  DP_REQUIRE (dp_acq_position (a->acq->engine) == a->samples_fed);

  const uint64_t           P    = a->code_period;
  const uint64_t           tail = a->hist->tail;
  burst_capture_pending_t *d    = &a->q[a->q_head];
  burst_capture_pending_t *y    = &a->q[(a->q_head + 1u) % a->q_cap];
  memset (d, 0, sizeof *d);
  memset (y, 0, sizeof *y);
  /* Dead: refined 50 samples behind the tail. */
  d->start   = tail - 50u;
  d->anchor  = d->start;
  d->refined = 1;
  /* Complete, behind it: refined at the first candidate of its anchor, 10
     samples past the tail, so its window is in the ring and its need is a
     period behind the tail. */
  y->start    = tail + 10u;
  y->anchor   = y->start + a->k_lo * P;
  y->n_phase  = 1u;
  y->phase[0] = (uint32_t)(y->anchor % P);
  y->refined  = 1;
  a->pending  = 2u;
  DP_REQUIRE (y->start + BURST_LEN <= (uint64_t)a->hist->head);

  const size_t   cb   = dp_burst_capture_state_bytes (a);
  unsigned char *blob = malloc (cb);
  DP_REQUIRE (blob != NULL);
  dp_burst_capture_get_state (a, blob);
  DP_REQUIRE (dp_burst_capture_set_state (b, blob) == DP_OK);
  DP_CHECK (dp_f32_space (b->hist) == 0u);
  DP_CHECK (b->dropped == a->dropped + 50u);

  /* set_state swept that one, so push()'s own opening drain would emit Y
     unaided. A second dead head, dying after the restore, blocks that
     drain: the loop's sweep takes it, and only the drain after the sweep
     can emit Y before trim -- which is the order under test. */
  DP_REQUIRE (b->pending == 1u);
  {
    const burst_capture_pending_t yb = b->q[b->q_head];
    burst_capture_pending_t      *d2 = &b->q[b->q_head];
    memset (d2, 0, sizeof *d2);
    d2->start                         = tail - 30u;
    d2->anchor                        = d2->start;
    d2->refined                       = 1;
    b->q[(b->q_head + 1u) % b->q_cap] = yb;
    b->pending                        = 2u;
  }

  static const float _Complex one[1] = { 0 };
  (void)dp_burst_capture_push (b, one, 1u, NULL, 0);
  DP_CHECK (dp_burst_capture_ready (b) == 1u
            && dp_burst_capture_event_at (b, 0)->preamble_start == y->start);
  DP_CHECK (b->dropped == a->dropped + 50u + 30u);
  DP_CHECK (b->samples_fed == a->samples_fed + 1u);
  free (blob);
  dp_burst_capture_destroy (a);
  dp_burst_capture_destroy (b);
  return 0;
}

/** @brief 1 if the queue is in anchor order, which the ring bound rests on. */
static int
q_in_anchor_order (const dp_burst_capture_state_t *s)
{
  for (size_t j = 1; j < s->pending; j++)
    if (s->q[(s->q_head + j) % s->q_cap].anchor
        < s->q[(s->q_head + j - 1u) % s->q_cap].anchor)
      return 0;
  return 1;
}

/**
 * A merge that moves an anchor forward keeps the queue in anchor order
 * (doppler#2015 review).
 *
 * The claim rule merges a hit into the FIRST queued entry within
 * refine_span, not the nearest, and a stronger hit moves that entry's
 * anchor to its own epoch -- which can pass the entries queued after it.
 * The ring bound rests on anchor order (burst_capture_trim), so the moved
 * entry is put back in place. Planted ahead of a burst whose first hit lands
 * at epoch e (found by a probe over the same stream in the same blocks): D
 * at e - 10 with the weakest peak there is, then L at e - 5. Both are
 * unarrived, so neither is emitted first. The hit merges into D, moves it to
 * e -- past L -- and the queue must come out sorted.
 */
static int
test_a_moved_anchor_keeps_anchor_order (void)
{
  static float _Complex cap[40000];
  const size_t n_cap = sizeof cap / sizeof *cap, LEAD = 12000u;
  const size_t AT = 20000u, BLK = 1000u;
  build_capture (cap, n_cap, &AT, 1u, 0.02, 2031u);

  uint64_t e       = 0;
  size_t   hit_off = 0;
  {
    dp_burst_capture_state_t *p = make ();
    DP_REQUIRE (p != NULL);
    (void)dp_burst_capture_push (p, cap, LEAD, NULL, 0);
    DP_REQUIRE (p->det_len == 0u);
    for (size_t off = LEAD; off + BLK <= n_cap && !e; off += BLK)
      {
        (void)dp_burst_capture_push (p, cap + off, BLK, NULL, 0);
        if (p->det_len)
          {
            e       = p->det[0].epoch;
            hit_off = off;
          }
      }
    dp_burst_capture_destroy (p);
  }
  DP_REQUIRE (e > LEAD + 100u);

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  (void)dp_burst_capture_push (s, cap, LEAD, NULL, 0);
  DP_REQUIRE (s->pending == 0u);
  const uint64_t           P = s->code_period;
  burst_capture_pending_t *d = &s->q[s->q_head];
  burst_capture_pending_t *l = &s->q[(s->q_head + 1u) % s->q_cap];
  memset (d, 0, sizeof *d);
  memset (l, 0, sizeof *l);
  d->anchor   = e - 10u;
  d->n_phase  = 1u;
  d->phase[0] = (uint32_t)(d->anchor % P);
  d->peak_mag = 0.0;
  l->anchor   = e - 5u;
  l->n_phase  = 1u;
  l->phase[0] = (uint32_t)(l->anchor % P);
  l->peak_mag = 1e30;
  s->pending  = 2u;

  for (size_t off = LEAD; off <= hit_off; off += BLK)
    {
      (void)dp_burst_capture_push (s, cap + off, BLK, NULL, 0);
      DP_CHECK (q_in_anchor_order (s));
    }
  /* The premise: the hit did move D to e, past L. */
  int moved = 0;
  for (size_t j = 0; j < s->pending; j++)
    moved |= s->q[(s->q_head + j) % s->q_cap].anchor == e;
  DP_REQUIRE (moved);
  DP_CHECK (q_in_anchor_order (s));
  dp_burst_capture_destroy (s);
  return 0;
}

/* ── Chunk invariance (native/tests/dp_chunk_inv.h) ───────────────────── */

/** One emitted window and its epoch: what the harness compares, so a
 *  partition that moves an epoch fails as surely as one that moves a
 *  sample. */
typedef struct
{
  uint64_t start;
  float _Complex w[BURST_LEN];
} bc_ci_rec_t;

static void *
bc_ci_create (void *arg)
{
  (void)arg;
  return make ();
}

static void
bc_ci_destroy (void *obj)
{
  dp_burst_capture_destroy (obj);
}

static size_t
bc_ci_process (void *obj, const void *in, size_t n, void *out, size_t out_cap)
{
  dp_burst_capture_state_t *s = obj;
  bc_ci_rec_t              *r = out;
  (void)dp_burst_capture_push (s, in, n, NULL, 0);
  size_t k = 0;
  for (; k < dp_burst_capture_ready (s) && k < out_cap; k++)
    {
      r[k].start = dp_burst_capture_event_at (s, k)->preamble_start;
      memcpy (r[k].w, dp_burst_capture_window (s, k), sizeof r[k].w);
    }
  return k;
}

/**
 * The windows and their epochs are a function of the STREAM, not of how it
 * was split into calls (doppler#1896): single samples, a prime, the burst
 * length and its neighbours, sizes straddling the internal slice
 * (`chunk_max`), and seeded random splits all reproduce the one-shot run.
 * The scene is test_block_size_below_min_gap's -- four bursts a quarter of
 * `min_gap` apart -- because there the one-shot answer depends on the claim
 * loop draining before it claims, and that is the chunk dependence this
 * object has actually had (doppler#1527).
 */
static int
test_chunk_invariance (void)
{
  static float _Complex cap[24000];
  const size_t              n_cap = sizeof cap / sizeof *cap;
  dp_burst_capture_state_t *probe = make ();
  DP_REQUIRE (probe != NULL);
  const size_t gap       = dp_burst_capture_get_min_gap (probe) / 4u;
  const size_t chunk_max = probe->chunk_max;
  dp_burst_capture_destroy (probe);

  size_t at[4];
  for (size_t k = 0; k < 4u; k++)
    at[k] = 3000u + k * (BURST_LEN + gap);
  DP_REQUIRE (at[3] + 3u * BURST_LEN < n_cap);
  build_capture (cap, n_cap, at, 4u, 0.02, 11u);

  const size_t extra[4]
      = { chunk_max, chunk_max + 1u, 2u * chunk_max + 3u, 0 };
  dp_ci_spec_t spec = { .name        = "burst_capture",
                        .create      = bc_ci_create,
                        .destroy     = bc_ci_destroy,
                        .process     = bc_ci_process,
                        .in_size     = sizeof (float _Complex),
                        .out_size    = sizeof (bc_ci_rec_t),
                        .out_cap     = 16u,
                        .frame_n     = BURST_LEN,
                        .extra_sizes = extra };
  DP_CHECK (dp_chunk_invariance (&spec, cap, n_cap) == 0);

  /* ...and the one-shot it compares against found every burst: invariance
     over a run that lost one would certify the loss. */
  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  (void)dp_burst_capture_push (s, cap, n_cap, NULL, 0);
  DP_CHECK (real_windows_once (s, at, 4u));
  dp_burst_capture_destroy (s);
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
  static float _Complex cap[200000];
  const size_t at[3] = { 9000u, 60000u, 120000u };
  build_capture (cap, sizeof cap / sizeof *cap, at, 3u, 0.02, 11u);

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);

  static float _Complex out[8 * BURST_LEN];
  /* Room for 1.5 windows. */
  size_t room = BURST_LEN + BURST_LEN / 2u;
  size_t n
      = dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out, room);
  DP_CHECK (n == BURST_LEN);
  DP_CHECK (n % BURST_LEN == 0);
  /* The bursts still HAPPENED — the events describe all three, so a caller
     who under-sized its buffer can see what it missed rather than believing
     the stream was quiet. */
  DP_CHECK (dp_burst_capture_ready (s) >= 3u);
  DP_CHECK (real_windows_once (s, at, 3u));
  dp_burst_capture_destroy (s);
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
  static float _Complex cap[200000];
  const size_t at = 40000u;
  /* One sample short of the burst's last. The emission rule is the burst's
     own span having arrived -- retain_span is the RING's retention, which is
     larger, so asserting against it would pass on a much weaker object. */
  const size_t n_cap = at + BURST_LEN - 1u;
  DP_REQUIRE (n_cap <= sizeof cap / sizeof *cap);
  build_capture (cap, n_cap, &at, 1u, 0.02, 5u);

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float _Complex out[4 * BURST_LEN];
  size_t n
      = dp_burst_capture_push (s, cap, n_cap, out, sizeof out / sizeof *out);
  DP_CHECK (n == 0);
  DP_CHECK (s->pending == 1u);
  /* ...and one more sample completes it, which is what makes the check above
     a boundary rather than a claim that the burst was never seen. */
  n = dp_burst_capture_push (s, cap + n_cap, 1u, out,
                             sizeof out / sizeof *out);
  DP_CHECK (n == BURST_LEN);
  DP_CHECK (s->preamble_start == at);
  DP_CHECK (s->pending == 0);
  dp_burst_capture_destroy (s);
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
  static float _Complex cap[80000];
  const size_t at = 9000u;
  build_capture (cap, sizeof cap / sizeof *cap, &at, 1u, 0.02, 7u);

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float _Complex out[4 * BURST_LEN];
  dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                         sizeof out / sizeof *out);
  DP_REQUIRE (s->n_bursts == 1u);

  const size_t span = s->refine_span;
  dp_burst_capture_reset (s);
  DP_CHECK (s->samples_fed == 0);
  DP_CHECK (s->pending == 0);
  DP_CHECK (s->suppress_until == 0);
  DP_CHECK (s->preamble_start == 0);
  DP_CHECK (dp_burst_capture_ready (s) == 0);
  DP_CHECK (s->refine_span == span);
  DP_CHECK (s->n_bursts == 1u);

  /* And the same stream is found again from a clean position. */
  size_t n = dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                    sizeof out / sizeof *out);
  DP_CHECK (n == BURST_LEN);
  DP_CHECK (s->preamble_start == at);
  dp_burst_capture_destroy (s);
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
  static float _Complex cap[200000];
  const size_t at    = 60000u;
  const size_t n_cap = sizeof cap / sizeof *cap;
  build_capture (cap, n_cap, &at, 1u, 0.02, 3u);

  dp_burst_capture_state_t *a = make ();
  DP_REQUIRE (a != NULL);
  static float _Complex out[4 * BURST_LEN];
  /* Split INSIDE the preamble: the detection has fired (or is about to) and
     the window has certainly not arrived. */
  const size_t cut = at + 2u * ACQ_SF * SPC;
  size_t       n0
      = dp_burst_capture_push (a, cap, cut, out, sizeof out / sizeof *out);
  DP_CHECK (n0 == 0);

  dp_burst_capture_state_t *b = make ();
  DP_REQUIRE (b != NULL);
  DP_STATE_ROUNDTRIP_TEST (dp_burst_capture, a, b);

  /* `b` was restored from `a` and must now find the burst `a` was holding. */
  size_t n = dp_burst_capture_push (b, cap + cut, n_cap - cut, out,
                                    sizeof out / sizeof *out);
  DP_CHECK (n == BURST_LEN);
  DP_CHECK (b->preamble_start == at);

  dp_burst_capture_destroy (a);
  dp_burst_capture_destroy (b);
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
  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);

  size_t small = dp_burst_capture_push_max_out (s, 1000u);
  size_t large = dp_burst_capture_push_max_out (s, 1000000u);
  DP_CHECK (large > small);
  DP_CHECK (small % BURST_LEN == 0);

  static float _Complex cap[200000];
  const size_t at[3] = { 9000u, 60000u, 120000u };
  const size_t n_cap = sizeof cap / sizeof *cap;
  build_capture (cap, n_cap, at, 3u, 0.02, 11u);

  static float _Complex out[8 * BURST_LEN];
  size_t bound = dp_burst_capture_push_max_out (s, n_cap);
  size_t n
      = dp_burst_capture_push (s, cap, n_cap, out, sizeof out / sizeof *out);
  DP_CHECK (n <= bound);
  DP_CHECK (n == dp_burst_capture_ready (s) * BURST_LEN);
  DP_CHECK (real_windows_once (s, at, 3u));
  dp_burst_capture_destroy (s);
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
  static float _Complex cap[200000];
  const size_t at[3] = { 9000u, 60000u, 120000u };
  build_capture (cap, sizeof cap / sizeof *cap, at, 3u, 0.02, 11u);

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float _Complex out[8 * BURST_LEN];
  dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                         sizeof out / sizeof *out);

  /* Four rows, not three: this scene carries one false alarm (at 21298,
     the design pfa at work -- see test_every_burst_is_emitted_once), and
     it has a row like any other window. */
  const size_t ready = dp_burst_capture_events_max_out (s, 0);
  DP_CHECK (ready >= 3u);
  DP_CHECK (dp_burst_capture_events_max_out (s, 99u) == ready); /* n ignored */

  burst_capture_event_t ev[8];
  size_t                got = dp_burst_capture_events (s, 0, ev, 8u);
  DP_CHECK (got == ready);
  for (size_t k = 0; k < 3u; k++)
    {
      size_t seen = 0;
      for (size_t i = 0; i < got; i++)
        seen += ev[i].preamble_start == at[k];
      DP_CHECK (seen == 1u);
    }

  /* A short buffer truncates rather than overruns. */
  burst_capture_event_t one[1];
  DP_CHECK (dp_burst_capture_events (s, 0, one, 1u) == 1u);
  DP_CHECK (one[0].preamble_start == ev[0].preamble_start);

  /* A push that completes nothing clears them -- events() describes THIS
     call, so a stale row would attribute an old burst to a quiet block. */
  static float _Complex quiet[8000];
  build_capture (quiet, sizeof quiet / sizeof *quiet, NULL, 0u, 0.02, 4u);
  dp_burst_capture_push (s, quiet, sizeof quiet / sizeof *quiet, out,
                         sizeof out / sizeof *out);
  DP_CHECK (dp_burst_capture_events_max_out (s, 0) == 0);
  DP_CHECK (dp_burst_capture_ready (s) == 0);
  DP_CHECK (dp_burst_capture_event_at (s, 0) == NULL);
  DP_CHECK (dp_burst_capture_window (s, 0) == NULL);

  dp_burst_capture_destroy (s);
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
  static float _Complex cap[80000];
  const size_t at = 9000u;
  build_capture (cap, sizeof cap / sizeof *cap, &at, 1u, 0.02, 7u);

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float _Complex out[4 * BURST_LEN];
  dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                         sizeof out / sizeof *out);

  const burst_capture_event_t *e = dp_burst_capture_event_at (s, 0);
  DP_REQUIRE (e != NULL);
  DP_CHECK (dp_burst_capture_get_preamble_start (s) == e->preamble_start);
  DP_CHECK (dp_burst_capture_get_doppler_hz_est (s) == e->doppler_hz_est);
  DP_CHECK (dp_burst_capture_get_doppler_res_hz (s) == e->doppler_res_hz);
  DP_CHECK (dp_burst_capture_get_cn0_dbhz_est (s) == e->cn0_dbhz_est);
  DP_CHECK (dp_burst_capture_get_pending (s) == s->pending);
  DP_CHECK (dp_burst_capture_get_dropped (s) == s->dropped);
  DP_CHECK (dp_burst_capture_get_n_bursts (s) == 1u);
  dp_burst_capture_destroy (s);
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
  static float _Complex cap[200000];
  const size_t at[3] = { 9000u, 60000u, 120000u };
  build_capture (cap, sizeof cap / sizeof *cap, at, 3u, 0.02, 11u);

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float _Complex out[8 * BURST_LEN];
  dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                         sizeof out / sizeof *out);
  const size_t ne = dp_burst_capture_events_max_out (s, 0);
  DP_REQUIRE (ne >= 3u); /* the three bursts, plus the scene's false alarm */

  const size_t nd = dp_burst_capture_detections_max_out (s, 0);
  DP_CHECK (dp_burst_capture_detections_max_out (s, 99u)
            == nd); /* n ignored */
  DP_CHECK (nd >= ne);
  /* Measured 5 against 4 at this geometry: more rows than windows is the
     evidence that nothing filtered them. A `detections()` that returned the
     claim rule's output would read exactly `ne` here. */
  DP_CHECK (nd > ne);

  burst_capture_detection_t det[64];
  DP_REQUIRE (nd <= sizeof det / sizeof *det);
  DP_CHECK (dp_burst_capture_detections (s, 0, det, 64u) == nd);

  burst_capture_event_t ev[8];
  DP_REQUIRE (dp_burst_capture_events (s, 0, ev, 8u) == ne);
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
  DP_CHECK (dp_burst_capture_detections (s, 0, one, 1u) == 1u);
  DP_CHECK (one[0].epoch == det[0].epoch);

  /* A quiet push clears them: the rows describe THIS call. */
  static float _Complex quiet[8000];
  build_capture (quiet, sizeof quiet / sizeof *quiet, NULL, 0u, 0.02, 4u);
  dp_burst_capture_push (s, quiet, sizeof quiet / sizeof *quiet, out,
                         sizeof out / sizeof *out);
  DP_CHECK (dp_burst_capture_detections_max_out (s, 0) == 0);

  dp_burst_capture_destroy (s);
  return 0;
}

/* ── The sizing contract: one look, and a design point that is optional ── */

/**
 * With NO design C/N0 the capture integrates the whole preamble in ONE look
 * and refines EXACTLY -- and with a design point the coherent ceiling cannot
 * meet, it is `underpowered` rather than escalated. Under the old sizer the
 * DEFAULT cn0_dbhz produced n_noncoh=6 at this geometry and starts 9 and 3
 * periods late with a plausible margin, because acquisition stamps a hit at
 * the end of the LAST accumulated look and refine reaches k_lo periods back
 * and no further (doppler#1181).
 */
static int
test_one_look_and_the_design_point_is_optional (void)
{
  static float _Complex cap[200000];
  const size_t at[3] = { 9000u, 60000u, 120000u };
  build_capture (cap, sizeof cap / sizeof *cap, at, 3u, 0.02, 11u);
  static float _Complex out[8 * BURST_LEN];

  /* No design point: the whole preamble, one look, nothing to be under. */
  dp_burst_capture_state_t *s
      = capture_from_code (acq_code (), ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6,
                           ACQ_CN0_NONE, 0.0, 1e-3, 0.9, 0, 0.0);
  DP_REQUIRE (s != NULL);
  DP_CHECK (s->acq->engine->n_noncoh == 1u);
  DP_CHECK (s->acq->engine->coherent_bins == REPS);
  DP_CHECK (!s->underpowered);
  DP_CHECK (isnan (s->acq->engine->pd_predicted));
  dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                         sizeof out / sizeof *out);
  /* Each transmitted burst exactly once, at its exact sample; the scene's
     false alarm (21298, the design pfa) is a fourth row and not the point. */
  burst_capture_event_t ev[8];
  size_t                got = dp_burst_capture_events (s, 0, ev, 8u);
  DP_CHECK (got >= 3u);
  for (size_t k = 0; k < 3u; k++)
    {
      size_t seen = 0;
      for (size_t i = 0; i < got; i++)
        seen += ev[i].preamble_start == at[k];
      DP_CHECK (seen == 1u);
    }
  dp_burst_capture_destroy (s);

  /* A design point the ceiling cannot meet: still one look, and it says so.
     The scene is strong, so the bursts are still found -- and found where
     they are, which the escalated grid could not do. */
  dp_burst_capture_state_t *low
      = capture_from_code (acq_code (), ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6,
                           40.0, 0.0, 1e-3, 0.9, 0, 0.0);
  DP_REQUIRE (low != NULL);
  DP_CHECK (low->acq->engine->n_noncoh == 1u);
  DP_CHECK (low->acq->engine->coherent_bins == REPS);
  DP_CHECK (low->underpowered);
  DP_CHECK (!isnan (low->acq->engine->pd_predicted));
  dp_burst_capture_push (low, cap, sizeof cap / sizeof *cap, out,
                         sizeof out / sizeof *out);
  got = dp_burst_capture_events (low, 0, ev, 8u);
  DP_CHECK (got >= 3u);
  for (size_t k = 0; k < 3u; k++)
    {
      size_t seen = 0;
      for (size_t i = 0; i < got; i++)
        seen += ev[i].preamble_start == at[k];
      DP_CHECK (seen == 1u);
    }
  dp_burst_capture_destroy (low);

  /* An infinite C/N0 is an argument error -- refused by the ENGINE, whose
     rule this composer no longer copies (doppler#1484) -- and a negative one
     is a design point, however hopeless. */
  DP_CHECK (capture_from_code (acq_code (), ACQ_SF, BURST_LEN, REPS, SPC,
                               1.0e6, -INFINITY, 0.0, 1e-3, 0.9, 0, 0.0)
            == NULL);
  {
    dp_burst_capture_state_t *neg
        = capture_from_code (acq_code (), ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6,
                             -1.0, 0.0, 1e-3, 0.9, 0, 0.0);
    DP_CHECK (neg != NULL && neg->underpowered);
    dp_burst_capture_destroy (neg);
  }
  return 0;
}

/**
 * A pinned grid whose anchor can lag the preamble past refine's reach is
 * REFUSED, and refusing changes nothing. `n_noncoh * doppler_bins` code
 * periods against `k_lo`: at this geometry k_lo = 14, so (4, 4) = 16 is
 * refused and (2, 7) = 14 is the last grid accepted.
 */
static int
test_configure_search_raw_refuses_a_grid_beyond_reach (void)
{
  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  const size_t k_lo = s->k_lo;
  const size_t db0  = s->acq->engine->coherent_bins;
  const size_t nc0  = s->acq->engine->n_noncoh;
  DP_CHECK (dp_burst_capture_configure_search_raw (s, REPS, k_lo / REPS + 1u)
            == DP_ERR_INVALID);
  DP_CHECK (s->acq->engine->coherent_bins == db0);
  DP_CHECK (s->acq->engine->n_noncoh == nc0);
  DP_CHECK (dp_burst_capture_configure_search_raw (s, 2u, k_lo / 2u) == DP_OK);
  DP_CHECK (s->acq->engine->n_noncoh == k_lo / 2u);
  dp_burst_capture_destroy (s);
  return 0;
}

/* ── Hold and release: whose verdict a span is (doppler#1181) ────────── */

/**
 * A window that was not a burst can be given back, and the burst it hid
 * comes out.
 *
 * The shape that lost a burst: a decoy (a bare preamble at 0.35 amplitude,
 * no payload) 2100 samples ahead of a real burst. Its window completes at
 * `decoy + burst_len` = AT + 348; the real preamble's first detecting frame
 * ends at 9424. A push boundary between the two -- here at 9400 -- means the
 * decoy is EMITTED before the real burst's hit arrives, and the hit lands
 * inside a span that is already owned. One push over the whole scene never
 * shows this: the two hits meet in the claim rule and the stronger wins.
 *
 * The capture cannot know the decoy was not a burst -- that is a consumer's
 * error detection. So the hit is HELD: released, it is searched again and
 * the burst comes out at its exact sample; not released, it is dropped at
 * the next push, which is what a consumer with no verdict always got. Both
 * halves are pinned, because both are the contract.
 */
static int
test_release_gives_back_a_shadowed_burst (void)
{
  const size_t AT = 9000u, LEAD = 2100u, CUT = 9400u;
  static float _Complex cap[80000];
  build_capture (cap, sizeof cap / sizeof *cap, &AT, 1u, 0.02, 7u);
  {
    static float _Complex burst[1 << 16];
    build_burst (burst);
    for (size_t i = 0; i < REPS * ACQ_SF * SPC; i++)
      cap[AT - LEAD + i] += 0.35f * burst[i];
  }
  static float _Complex out[4 * BURST_LEN];

  /* Released: the burst comes out. */
  {
    dp_burst_capture_state_t *s = make ();
    DP_REQUIRE (s != NULL);
    dp_burst_capture_push (s, cap, CUT, out, sizeof out / sizeof *out);
    DP_REQUIRE (dp_burst_capture_ready (s) == 1u); /* the decoy's window */
    DP_CHECK (dp_burst_capture_event_at (s, 0)->preamble_start == AT - LEAD);
    DP_CHECK (dp_burst_capture_release (s, 1u)
              == DP_ERR_INVALID); /* no such */
    DP_CHECK (dp_burst_capture_release (s, 0u) == DP_OK);
    size_t n
        = dp_burst_capture_push (s, cap + CUT, sizeof cap / sizeof *cap - CUT,
                                 out, sizeof out / sizeof *out);
    DP_CHECK (n == BURST_LEN);
    DP_REQUIRE (dp_burst_capture_ready (s) == 1u);
    DP_CHECK (dp_burst_capture_event_at (s, 0)->preamble_start == AT);
    DP_CHECK (dp_burst_capture_get_pending (s) == 0);
    dp_burst_capture_destroy (s);
  }

  /* Not released: the hit is never emitted, and `pending` never counted it,
     because it is not a burst a caller would lose by stopping. Held only
     while its history lasts: this push is long enough to trim past it, and
     the decoy that shadowed it belongs to the PREVIOUS push, which no
     release can reach any more -- so the hold bought nothing and pinned the
     history ring until a chunk was refused (doppler#1527). The held-then-
     released path is the long-burst block below, where the shadowing
     window is emitted by the same push. */
  {
    dp_burst_capture_state_t *s = make ();
    DP_REQUIRE (s != NULL);
    dp_burst_capture_push (s, cap, CUT, out, sizeof out / sizeof *out);
    DP_REQUIRE (dp_burst_capture_ready (s) == 1u);
    dp_burst_capture_push (s, cap + CUT, sizeof cap / sizeof *cap - CUT, out,
                           sizeof out / sizeof *out);
    DP_CHECK (dp_burst_capture_ready (s) == 0);
    DP_CHECK (dp_burst_capture_get_pending (s) == 0); /* not counted */
    static float _Complex quiet[8000];
    build_capture (quiet, sizeof quiet / sizeof *quiet, NULL, 0u, 0.02, 4u);
    dp_burst_capture_push (s, quiet, sizeof quiet / sizeof *quiet, out,
                           sizeof out / sizeof *out);
    DP_CHECK (dp_burst_capture_ready (s) == 0);
    DP_CHECK (s->pending == 0); /* dropped when the next push began */
    /* A late release names a window of a push that is gone. */
    DP_CHECK (dp_burst_capture_release (s, 0u) == DP_ERR_INVALID);
    dp_burst_capture_destroy (s);
  }

  /* A LONG burst -- the link geometry, where `burst_len` is several times
     `refine_span` -- takes the other path: the decoy and the real hit are
     too far apart to merge, both are queued, and the real one is shadowed
     when the decoy's window is EMITTED, not when it arrived. Released, it
     comes out at its exact sample all the same. */
  {
    /* Not LONG and FAR: <windows.h> typedefs LONG and #defines FAR to
       nothing, so `AT - FAR` became `AT - ` on every Windows build. */
    const size_t              LONG_LEN = 4u * BURST_LEN, DECOY_LEAD = 3000u;
    dp_burst_capture_state_t *s
        = capture_from_code (acq_code (), ACQ_SF, LONG_LEN, REPS, SPC, 1.0e6,
                             ACQ_CN0_NONE, 0.0, 1e-3, 0.9, 0, 0.0);
    DP_REQUIRE (s != NULL);
    DP_REQUIRE (DECOY_LEAD >= s->refine_span
                && DECOY_LEAD < LONG_LEN); /* the premise */
    static float _Complex scene[80000];
    build_capture (scene, sizeof scene / sizeof *scene, &AT, 1u, 0.02, 7u);
    {
      static float _Complex burst[1 << 16];
      build_burst (burst);
      for (size_t i = 0; i < REPS * ACQ_SF * SPC; i++)
        scene[AT - DECOY_LEAD + i] += 0.35f * burst[i];
    }
    static float _Complex big[8 * 4 * BURST_LEN];
    /* One push past the decoy's window end (AT - DECOY_LEAD + LONG_LEN) and
       past the real preamble's first frame, so both hits are queued before the
       drain emits the decoy over the real one. */
    const size_t first = AT - DECOY_LEAD + LONG_LEN + 500u;
    dp_burst_capture_push (s, scene, first, big, sizeof big / sizeof *big);
    DP_REQUIRE (dp_burst_capture_ready (s) == 1u);
    DP_CHECK (dp_burst_capture_event_at (s, 0)->preamble_start
              == AT - DECOY_LEAD);
    DP_CHECK (dp_burst_capture_get_pending (s) == 0); /* shadowed, uncounted */
    DP_CHECK (s->pending >= 1u);
    DP_CHECK (dp_burst_capture_release (s, 0u) == DP_OK);
    DP_CHECK (dp_burst_capture_get_pending (s) >= 1u); /* given back */
    size_t n = dp_burst_capture_push (s, scene + first,
                                      sizeof scene / sizeof *scene - first,
                                      big, sizeof big / sizeof *big);
    DP_CHECK (n == LONG_LEN);
    DP_REQUIRE (dp_burst_capture_ready (s) == 1u);
    DP_CHECK (dp_burst_capture_event_at (s, 0)->preamble_start == AT);
    dp_burst_capture_destroy (s);
  }
  return 0;
}

/**
 * `configure_search_raw` reaches the engine, and re-reads the blob bound it
 * invalidated.
 *
 * It is the one call that can legitimately move `dp_acq_state_bytes()` under a
 * `state_bytes()` that promises to be a pure function of configuration. If
 * the bound were not re-read, a blob taken after this call could exceed the
 * region reserved for it.
 */
static int
test_configure_search_raw_reaches_the_engine (void)
{
  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  /* A coherent depth of 2 against a sized 4: the grid is the one ASKED for,
     not the one the auto-sizer picked, which is the whole point of the
     escape hatch. */
  DP_CHECK (s->acq->engine->coherent_bins == REPS);
  DP_CHECK (dp_burst_capture_configure_search_raw (s, 2u, 1u) == DP_OK);
  DP_CHECK (s->acq->engine->coherent_bins == 2u);
  /* And a grid the engine cannot honour is REFUSED, not silently clamped: a
     coherent depth deeper than the preamble has no frames to integrate.
     Measured: reps=4 accepts 1, 2 and 4 and rejects 8. */
  DP_CHECK (dp_burst_capture_configure_search_raw (s, 8u, 1u)
            == DP_ERR_INVALID);
  DP_CHECK (s->acq->engine->coherent_bins
            == 2u); /* unchanged by the refusal */
  /* ...and the blob bound it invalidated was re-read. */
  DP_CHECK (s->acq_blob_max == dp_acq_state_bytes (s->acq->engine));
  size_t after = dp_burst_capture_state_bytes (s);
  /* ...so a blob taken NOW fits the region reserved for it. */
  void *blob = malloc (after);
  DP_REQUIRE (blob != NULL);
  dp_burst_capture_get_state (s, blob);
  DP_CHECK (dp_burst_capture_set_state (s, blob) == DP_OK);
  free (blob);
  dp_burst_capture_destroy (s);
  return 0;
}

/** destroy(NULL) is safe -- the claim every header makes and few tests make.
 */
static int
test_destroy_null_is_safe (void)
{
  dp_burst_capture_destroy (NULL);
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
  static float _Complex cap[200000];
  const size_t at[3] = { 9000u, 60000u, 120000u };
  build_capture (cap, sizeof cap / sizeof *cap, at, 3u, 0.02, 11u);

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  size_t empty = dp_burst_capture_state_bytes (s);
  static float _Complex out[8 * BURST_LEN];
  dp_burst_capture_push (s, cap, 40000u, out, sizeof out / sizeof *out);
  DP_CHECK (dp_burst_capture_state_bytes (s) == empty);
  dp_burst_capture_push (s, cap + 40000u, 40000u, out,
                         sizeof out / sizeof *out);
  DP_CHECK (dp_burst_capture_state_bytes (s) == empty);
  dp_burst_capture_reset (s);
  DP_CHECK (dp_burst_capture_state_bytes (s) == empty);
  dp_burst_capture_destroy (s);
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
  dp_burst_capture_state_t *probe = make ();
  DP_REQUIRE (probe != NULL);
  const size_t chunk_max = probe->chunk_max;
  const size_t ring      = probe->hist->capacity;
  dp_burst_capture_destroy (probe);

  static float _Complex cap[200000];
  const size_t n_cap = sizeof cap / sizeof *cap;
  DP_REQUIRE (n_cap > 2u * ring); /* the point of the test */
  const size_t at[2] = { 9000u, 60000u };
  build_capture (cap, n_cap, at, 2u, 0.02, 11u);

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float _Complex out[8 * BURST_LEN];
  size_t n
      = dp_burst_capture_push (s, cap, n_cap, out, sizeof out / sizeof *out);
  DP_CHECK (n_cap > chunk_max); /* the slicing path really was taken */
  DP_CHECK (n == dp_burst_capture_ready (s) * BURST_LEN);
  DP_CHECK (real_windows_once (s, at, 2u));
  DP_CHECK (s->dropped == 0);
  DP_CHECK (s->samples_fed == (uint64_t)n_cap);
  dp_burst_capture_destroy (s);
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
  static float _Complex cap[80000];
  const size_t at = 9000u;
  build_capture (cap, sizeof cap / sizeof *cap, &at, 1u, 0.02, 7u);

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float _Complex out[8 * BURST_LEN];
  size_t n = dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                    sizeof out / sizeof *out);

  DP_CHECK (n == BURST_LEN); /* ONE window for one burst */
  DP_CHECK (s->n_bursts == 1u);
  DP_CHECK (s->suppress_until >= (uint64_t)(at + BURST_LEN));
  /* Nothing is left queued: a detection inside the span was dropped rather
     than held, which is what the compaction after an emit is for. */
  DP_CHECK (s->pending == 0);
  dp_burst_capture_destroy (s);
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
  dp_burst_capture_state_t *probe = make ();
  DP_REQUIRE (probe != NULL);
  /* Through the ACCESSOR, which is the C consumer's face -- a composing
     object reads that, not the struct, and a field-only test leaves the
     function every caller actually calls unexercised. */
  const size_t gap  = dp_burst_capture_get_min_gap (probe);
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
  dp_burst_capture_destroy (probe);

  static float _Complex cap[200000];
  const size_t at[2] = { 9000u, 9000u + BURST_LEN + gap };
  build_capture (cap, sizeof cap / sizeof *cap, at, 2u, 0.02, 13u);

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float _Complex out[8 * BURST_LEN];
  size_t n = dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                    sizeof out / sizeof *out);

  /* Both transmitted bursts come back. NOT `n == 2 * BURST_LEN`: at
     pfa = 1e-3 a spurious window is expected, and asserting the count would
     be asserting the false-alarm rate is zero. */
  DP_REQUIRE (n >= 2u * BURST_LEN);
  int found[2] = { 0, 0 };
  for (size_t i = 0; i < dp_burst_capture_ready (s); i++)
    for (size_t k = 0; k < 2u; k++)
      if (dp_burst_capture_event_at (s, i)->preamble_start == (uint64_t)at[k])
        found[k] = 1;
  DP_CHECK (found[0] && found[1]);
  dp_burst_capture_destroy (s);
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
  dp_burst_capture_state_t *probe = make ();
  DP_REQUIRE (probe != NULL);
  const size_t span = probe->refine_span;
  dp_burst_capture_destroy (probe);

  static float _Complex cap[200000];
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

  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  static float _Complex out[8 * BURST_LEN];
  size_t n = dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                    sizeof out / sizeof *out);

  /* BOTH transmitted bursts come back, at their exact starts. NOT
     `n == 2 * BURST_LEN`: at pfa = 1e-3 over a surface this size a spurious
     window is EXPECTED, and this geometry reliably yields one -- so asserting
     the count would be asserting the false-alarm rate is zero, and the test
     would fail for the object behaving correctly. A caller separates the two
     with cn0_dbhz_est, which is what it is exposed for;
     the rate itself is characterized, not pinned here. */
  DP_REQUIRE (n >= 2u * BURST_LEN);
  int found[2] = { 0, 0 };
  for (size_t i = 0; i < dp_burst_capture_ready (s); i++)
    for (size_t k = 0; k < 2u; k++)
      if (dp_burst_capture_event_at (s, i)->preamble_start == (uint64_t)at[k])
        found[k] = 1;
  DP_CHECK (found[0] && found[1]);
  dp_burst_capture_destroy (s);
  return 0;
}

/* ── Persistence: the ring in a file ─────────────────────────────────── */

static void
scratch_path (char *buf, size_t n, const char *tag)
{
  snprintf (buf, n, "%s/dp_burst_capture_%s_%d.cf32", dp_test_tmpdir (), tag,
            (int)getpid ());
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

  static float _Complex cap[80000];
  const size_t at = 9000u;
  build_capture (cap, sizeof cap / sizeof *cap, &at, 1u, 0.02, 7u);

  /* The same configuration on both sides, so the backing is the ONE
     difference: make()'s cn0 design differs, which picks a different
     acquisition grid and so a different acq blob. That used to hide under
     the ring's power-of-two capacity; the framer's snapshot is sized by the
     frame itself, so it no longer does. */
  dp_burst_capture_state_t *ram
      = capture_from_code (acq_code (), ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6,
                           55.0, 0.0, 1e-3, 0.9, 0, 0.0);
  dp_burst_capture_state_t *dsk
      = capture_from_code_backed (path, acq_code (), ACQ_SF, BURST_LEN, REPS,
                                  SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0, 0.0);
  DP_REQUIRE (ram != NULL && dsk != NULL);
  DP_CHECK (dsk->backed == 1);
  DP_CHECK (ram->backed == 0);

  static float _Complex out_a[4 * BURST_LEN];
  static float _Complex out_b[4 * BURST_LEN];
  size_t na = dp_burst_capture_push (ram, cap, sizeof cap / sizeof *cap, out_a,
                                     sizeof out_a / sizeof *out_a);
  size_t nb = dp_burst_capture_push (dsk, cap, sizeof cap / sizeof *cap, out_b,
                                     sizeof out_b / sizeof *out_b);

  /* Bit-identical: where the pages live is not a DSP parameter. */
  DP_CHECK (na == BURST_LEN);
  DP_CHECK (na == nb);
  DP_CHECK (memcmp (out_a, out_b, na * sizeof *out_a) == 0);
  DP_CHECK (ram->preamble_start == dsk->preamble_start);

  size_t cb_ram = dp_burst_capture_state_bytes (ram);
  size_t cb_dsk = dp_burst_capture_state_bytes (dsk);
  /* The EXACT difference, not a ratio: the ring's capacity rounds up to a
     whole page, so a "backed is 4x smaller" assertion measures the host's
     page size as much as the feature -- it passed on 4 kB pages and failed on
     macOS's 16 kB. What the feature actually claims is that the blob stops
     carrying the retained span, and that is exact everywhere. */
  DP_CHECK (cb_ram - cb_dsk == ram->hist->capacity * sizeof (float _Complex));
  DP_CHECK (cb_dsk < cb_ram);

  dp_burst_capture_destroy (ram);
  dp_burst_capture_destroy (dsk);
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

  static float _Complex cap[200000];
  const size_t at    = 60000u;
  const size_t n_cap = sizeof cap / sizeof *cap;
  build_capture (cap, n_cap, &at, 1u, 0.02, 3u);
  const size_t cut = at + 2u * ACQ_SF * SPC; /* inside the preamble */

  static float _Complex out[4 * BURST_LEN];
  void  *blob = NULL;
  size_t cb   = 0;
  {
    dp_burst_capture_state_t *a
        = capture_from_code_backed (path, acq_code (), ACQ_SF, BURST_LEN, REPS,
                                    SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0, 0.0);
    DP_REQUIRE (a != NULL);
    DP_CHECK (a->recovered == 0); /* the file did not exist yet */
    DP_CHECK (
        dp_burst_capture_push (a, cap, cut, out, sizeof out / sizeof *out)
        == 0);
    cb   = dp_burst_capture_state_bytes (a);
    blob = malloc (cb);
    DP_REQUIRE (blob != NULL);
    dp_burst_capture_get_state (a, blob);
    dp_burst_capture_destroy (a); /* the ring's memory is gone with it */
  }

  dp_burst_capture_state_t *b
      = capture_from_code_backed (path, acq_code (), ACQ_SF, BURST_LEN, REPS,
                                  SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0, 0.0);
  DP_REQUIRE (b != NULL);
  /* The file was adopted rather than re-made, which is what carries the
     samples across. */
  DP_CHECK (b->recovered == 1);
  DP_CHECK (dp_burst_capture_set_state (b, blob) == DP_OK);

  size_t n = dp_burst_capture_push (b, cap + cut, n_cap - cut, out,
                                    sizeof out / sizeof *out);
  DP_CHECK (n == BURST_LEN);
  DP_CHECK (b->preamble_start == at);

  free (blob);
  dp_burst_capture_destroy (b);
  remove (path);
  return 0;
}

/**
 * The live capture restores its own checkpoint (doppler#1190).
 *
 * The object that CREATED the file is the one whose `recovered` is zero, and
 * it used to refuse every blob it took after a push -- the samples the blob
 * named were in the file, in the bytes it had written itself. A fresh object
 * over the same file accepted the same blob. `set_state -> push -> get_state`
 * per call is the service shape a downstream demo runs, so: a checkpoint
 * taken mid-preamble is restored at once by the SAME object, which then
 * finds the burst at the right start; a checkpoint taken after the capture
 * restores after a further push that stays inside the ring. One taken
 * before any push is accepted too, as it always was.
 */
static int
test_the_live_capture_restores_its_own_checkpoint (void)
{
  char path[256];
  scratch_path (path, sizeof path, "self");
  remove (path);

  static float _Complex cap[200000];
  const size_t at    = 60000u;
  const size_t n_cap = sizeof cap / sizeof *cap;
  build_capture (cap, n_cap, &at, 1u, 0.02, 3u);
  const size_t cut = at + 2u * ACQ_SF * SPC; /* inside the preamble */

  dp_burst_capture_state_t *a
      = capture_from_code_backed (path, acq_code (), ACQ_SF, BURST_LEN, REPS,
                                  SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0, 0.0);
  DP_REQUIRE (a != NULL);
  DP_CHECK (a->recovered == 0); /* the file did not exist yet */

  size_t cb    = dp_burst_capture_state_bytes (a);
  void  *empty = malloc (cb);
  void  *blob  = malloc (cb);
  DP_REQUIRE (empty != NULL && blob != NULL);
  dp_burst_capture_get_state (a, empty); /* before any push */

  static float _Complex out[4 * BURST_LEN];
  DP_CHECK (dp_burst_capture_push (a, cap, cut, out, sizeof out / sizeof *out)
            == 0);
  dp_burst_capture_get_state (a, blob); /* mid-preamble, history in the file */
  /* The issue's case: the same object, its own post-push blob, at once --
     then the burst is found at the right start, from the bytes this object
     wrote. */
  DP_CHECK (dp_burst_capture_set_state (a, blob) == DP_OK);
  DP_CHECK (a->samples_fed == cut);
  DP_CHECK (dp_burst_capture_push (a, cap + cut, n_cap - cut, out,
                                   sizeof out / sizeof *out)
            == BURST_LEN);
  DP_CHECK (a->preamble_start == at);
  /* A checkpoint after the capture, restored after a further push that
     stays inside the ring: the same object, later in its life. */
  dp_burst_capture_get_state (a, blob);
  DP_CHECK (
      dp_burst_capture_push (a, cap, 1000u, out, sizeof out / sizeof *out)
      == 0);
  DP_CHECK (dp_burst_capture_set_state (a, blob) == DP_OK);
  DP_CHECK (a->samples_fed == n_cap);
  /* And the pre-push checkpoint, which names no history, still restores. */
  DP_CHECK (dp_burst_capture_set_state (a, empty) == DP_OK);
  DP_CHECK (a->samples_fed == 0);

  free (empty);
  free (blob);
  dp_burst_capture_destroy (a);
  remove (path);
  return 0;
}

/**
 * A span the ring has wrapped past is refused: the file no longer holds it.
 *
 * The same object, having pushed more than the ring's capacity beyond a
 * checkpoint, cannot restore it -- the bytes the blob names have been
 * overwritten, and a resume there would read the wrong stream's samples as
 * history. Sibling of the fresh-file refusal below; both are the one rule,
 * "the file must hold the span the blob names".
 */
static int
test_a_span_the_ring_wrapped_past_is_refused (void)
{
  char path[256];
  scratch_path (path, sizeof path, "wrap");
  remove (path);

  static float _Complex cap[200000];
  const size_t at    = 60000u;
  const size_t n_cap = sizeof cap / sizeof *cap;
  build_capture (cap, n_cap, &at, 1u, 0.02, 3u);
  const size_t cut = at + 2u * ACQ_SF * SPC;

  dp_burst_capture_state_t *a
      = capture_from_code_backed (path, acq_code (), ACQ_SF, BURST_LEN, REPS,
                                  SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0, 0.0);
  DP_REQUIRE (a != NULL);
  static float _Complex out[4 * BURST_LEN];
  dp_burst_capture_push (a, cap, cut, out, sizeof out / sizeof *out);
  size_t cb   = dp_burst_capture_state_bytes (a);
  void  *blob = malloc (cb);
  DP_REQUIRE (blob != NULL);
  dp_burst_capture_get_state (a, blob);

  /* Push on past the ring's capacity from the checkpoint's span. */
  const size_t capacity = a->hist->capacity;
  DP_REQUIRE (n_cap - cut > capacity);
  dp_burst_capture_push (a, cap + cut, n_cap - cut, out,
                         sizeof out / sizeof *out);
  DP_CHECK (dp_burst_capture_set_state (a, blob) == DP_ERR_INVALID);

  free (blob);
  dp_burst_capture_destroy (a);
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

  static float _Complex cap[200000];
  const size_t at = 60000u;
  build_capture (cap, sizeof cap / sizeof *cap, &at, 1u, 0.02, 3u);

  dp_burst_capture_state_t *a
      = capture_from_code_backed (src, acq_code (), ACQ_SF, BURST_LEN, REPS,
                                  SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0, 0.0);
  DP_REQUIRE (a != NULL);
  static float _Complex out[4 * BURST_LEN];
  dp_burst_capture_push (a, cap, at + 2u * ACQ_SF * SPC, out,
                         sizeof out / sizeof *out);
  /* What makes the blob refusable is that it CLAIMS retained history, which
     any push leaves behind -- not that a detection happened to fire yet. */
  DP_REQUIRE (a->samples_fed > 0);

  size_t cb   = dp_burst_capture_state_bytes (a);
  void  *blob = malloc (cb);
  DP_REQUIRE (blob != NULL);
  dp_burst_capture_get_state (a, blob);

  dp_burst_capture_state_t *b
      = capture_from_code_backed (dst, acq_code (), ACQ_SF, BURST_LEN, REPS,
                                  SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0, 0.0);
  DP_REQUIRE (b != NULL);
  DP_CHECK (b->recovered == 0);
  DP_CHECK (dp_burst_capture_set_state (b, blob) == DP_ERR_INVALID);

  free (blob);
  dp_burst_capture_destroy (a);
  dp_burst_capture_destroy (b);
  remove (src);
  remove (dst);
  return 0;
}

/** A backed constructor with no usable path fails as an argument error. */
static int
test_backed_rejects_a_bad_path (void)
{
  DP_CHECK (capture_from_code_backed (NULL, acq_code (), ACQ_SF, BURST_LEN,
                                      REPS, SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9,
                                      0, 0.0)
            == NULL);
  DP_CHECK (capture_from_code_backed ("", acq_code (), ACQ_SF, BURST_LEN, REPS,
                                      SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0, 0.0)
            == NULL);
  DP_CHECK (capture_from_code_backed ("/nonexistent-dir-dp/ring.cf32",
                                      acq_code (), ACQ_SF, BURST_LEN, REPS,
                                      SPC, 1.0e6, 55.0, 0.0, 1e-3, 0.9, 0, 0.0)
            == NULL);
  return 0;
}

/** A blob whose envelope is wrong is REJECTED, never reinterpreted. */
static int
test_state_rejects_a_foreign_blob (void)
{
  dp_burst_capture_state_t *s = make ();
  DP_REQUIRE (s != NULL);
  size_t cb   = dp_burst_capture_state_bytes (s);
  void  *blob = malloc (cb);
  DP_REQUIRE (blob != NULL);
  dp_burst_capture_get_state (s, blob);
  ((unsigned char *)blob)[0] ^= 0xFFu;
  DP_CHECK (dp_burst_capture_set_state (s, blob) == DP_ERR_INVALID);
  free (blob);
  dp_burst_capture_destroy (s);
  return 0;
}

/** The Doppler rate reaches the embedded engine and caps its coherent
 *  depth at floor(f_epoch / sqrt(2 * rate)) (doppler#1490), on both
 *  constructors; a negative one is refused. */
static int
test_doppler_rate_caps_the_depth (void)
{
  const double f_epoch = 1.0e6 / (double)ACQ_SF;
  const double rate    = f_epoch * f_epoch / (2.0 * 2.5 * 2.5); /* cap: 2 */
  dp_burst_capture_state_t *free_
      = capture_from_code (acq_code (), ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6,
                           ACQ_CN0_NONE, 0.0, 1e-3, 0.9, 0, 0.0);
  dp_burst_capture_state_t *held
      = capture_from_code (acq_code (), ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6,
                           ACQ_CN0_NONE, 0.0, 1e-3, 0.9, 0, rate);
  DP_REQUIRE (free_ != NULL && held != NULL);
  DP_CHECK (dp_burst_capture_get_doppler_bins (free_) == REPS);
  DP_CHECK (dp_burst_capture_get_doppler_bins (held) == 2);
  DP_CHECK (dp_burst_capture_get_doppler_rate (held) == rate);
  dp_burst_capture_destroy (free_);
  dp_burst_capture_destroy (held);

  char path[256];
  scratch_path (path, sizeof path, "rate");
  remove (path);
  dp_burst_capture_state_t *dsk = capture_from_code_backed (
      path, acq_code (), ACQ_SF, BURST_LEN, REPS, SPC, 1.0e6, ACQ_CN0_NONE,
      0.0, 1e-3, 0.9, 0, rate);
  DP_REQUIRE (dsk != NULL);
  DP_CHECK (dp_burst_capture_get_doppler_bins (dsk) == 2);
  dp_burst_capture_destroy (dsk);
  remove (path);

  DP_CHECK (capture_from_code (acq_code (), ACQ_SF, BURST_LEN, REPS, SPC,
                               1.0e6, ACQ_CN0_NONE, 0.0, 1e-3, 0.9, 0, -1.0)
            == NULL);
  return 0;
}

/* ── Any repeated preamble (doppler#1470) ────────────────────────────── */

#define ZC_N 127u
#define ZC_REPS 8u
#define ZC_PAYLOAD 2000u
#define ZC_BURST (ZC_REPS * ZC_N + ZC_PAYLOAD)

static void
zadoff_chu (float _Complex *zc)
{
  for (size_t k = 0; k < ZC_N; k++)
    zc[k] = (float _Complex)cexp (-I * M_PI * 5.0 * (double)k * (double)(k + 1)
                                  / (double)ZC_N);
}

/** Noise, then one ZC burst at @p at: ZC_REPS periods and a QPSK payload,
 *  rotated by @p f cycles/sample. */
static void
build_zc_capture (float _Complex *cap, size_t n_cap, size_t at, double f,
                  uint32_t seed)
{
  float _Complex zc[ZC_N];
  zadoff_chu (zc);
  uint32_t st = seed;
  for (size_t i = 0; i < n_cap; i++)
    {
      float re = (float)(0.02 * dp_gauss (&st));
      float im = (float)(0.02 * dp_gauss (&st));
      cap[i]   = re + im * I;
    }
  for (size_t i = 0; i < ZC_BURST && at + i < n_cap; i++)
    {
      float _Complex v
          = i < ZC_REPS * ZC_N
                ? zc[i % ZC_N]
                : (float _Complex)cexp (I * M_PI / 2.0
                                        * (double)(dp_xs32 (&st) >> 30));
      cap[at + i] += v * (float _Complex)cexp (I * 2.0 * M_PI * f * (double)i);
    }
}

static dp_burst_capture_state_t *
make_zc (double doppler_rate)
{
  float _Complex zc[ZC_N];
  zadoff_chu (zc);
  /* pfa 1e-6: at 1e-3 the engine's known ~1.5x over-delivery (acq F7)
     puts a false capture in ~6% of these 39-frame streams, measured over
     200 seeds -- a property of the configured rate, not of the preamble. */
  return dp_burst_capture_create (zc, ZC_N, ZC_BURST, ZC_REPS, 1.0,
                                  ACQ_CN0_NONE, 0.0, 1e-6, 0.9, 0,
                                  doppler_rate);
}

/** A Zadoff-Chu burst is captured, and its window starts at the preamble:
 *  refine resolves the repetition against the engine's own reference row. */
static int
test_captures_a_zadoff_chu_burst (void)
{
  static float _Complex cap[40000];
  const size_t at = 9001u;
  build_zc_capture (cap, sizeof cap / sizeof *cap, at, 0.0, 1470u);

  dp_burst_capture_state_t *s = make_zc (0.0);
  DP_REQUIRE (s != NULL);
  DP_CHECK (s->code_period == ZC_N);

  static float _Complex out[4 * ZC_BURST];
  size_t n = dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                    sizeof out / sizeof *out);
  DP_CHECK (n == ZC_BURST);
  const burst_capture_event_t *ev = dp_burst_capture_event_at (s, 0);
  DP_REQUIRE (ev != NULL);
  DP_CHECK (ev->preamble_start == at);
  dp_burst_capture_destroy (s);
  return 0;
}

/** Under Doppler across the native span, the window still starts at the
 *  preamble, and the reported Doppler is the true one to within its
 *  resolution -- MODULO 1/P: a preamble repeated every P samples is sampled
 *  once a period in slow time, so +span and -span are one frequency. */
static int
test_zadoff_chu_capture_under_doppler (void)
{
  static float _Complex cap[40000];
  static float _Complex out[4 * ZC_BURST];
  const size_t at     = 9001u;
  const double period = 1.0 / (double)ZC_N; /* 1/P, cycles/sample */
  const double fr[]   = { -0.9, -0.5, 0.25, 0.5, 0.9 };
  for (size_t j = 0; j < sizeof fr / sizeof *fr; j++)
    {
      const double f = fr[j] * period / 2.0;
      build_zc_capture (cap, sizeof cap / sizeof *cap, at, f,
                        1471u + (uint32_t)j);
      dp_burst_capture_state_t *s = make_zc (0.0);
      DP_REQUIRE (s != NULL);
      size_t n = dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                        sizeof out / sizeof *out);
      const burst_capture_event_t *ev = dp_burst_capture_event_at (s, 0);
      DP_CHECK (n == ZC_BURST);
      DP_REQUIRE (ev != NULL);
      DP_CHECK (ev->preamble_start == at);
      double e = remainder (ev->doppler_hz_est - f, period);
      DP_CHECK (fabs (e) <= ev->doppler_res_hz);
      dp_burst_capture_destroy (s);
    }
  return 0;
}

/** Refused: no preamble, no samples, no energy. */
static int
test_rejects_a_bad_preamble (void)
{
  float _Complex z[4] = { 0 };
  DP_CHECK (dp_burst_capture_create (NULL, 4, 64, 4, 1.0, ACQ_CN0_NONE, 0.0,
                                     1e-3, 0.9, 0, 0.0)
            == NULL);
  DP_CHECK (dp_burst_capture_create (z, 0, 64, 4, 1.0, ACQ_CN0_NONE, 0.0, 1e-3,
                                     0.9, 0, 0.0)
            == NULL);
  DP_CHECK (dp_burst_capture_create (z, 4, 64, 4, 1.0, ACQ_CN0_NONE, 0.0, 1e-3,
                                     0.9, 0, 0.0)
            == NULL);
  return 0;
}

/** The backed constructor captures a Zadoff-Chu burst too. */
static int
test_backed_captures_a_zadoff_chu_burst (void)
{
  char path[256];
  scratch_path (path, sizeof path, "tmpl");
  remove (path);
  float _Complex zc[ZC_N];
  zadoff_chu (zc);
  dp_burst_capture_state_t *s
      = dp_burst_capture_create_backed (path, zc, ZC_N, ZC_BURST, ZC_REPS, 1.0,
                                        ACQ_CN0_NONE, 0.0, 1e-6, 0.9, 0, 0.0);
  DP_REQUIRE (s != NULL);
  DP_CHECK (s->backed == 1);
  static float _Complex cap[40000];
  build_zc_capture (cap, sizeof cap / sizeof *cap, 9001u, 0.0, 1470u);
  static float _Complex out[4 * ZC_BURST];
  size_t n = dp_burst_capture_push (s, cap, sizeof cap / sizeof *cap, out,
                                    sizeof out / sizeof *out);
  DP_CHECK (n == ZC_BURST && s->preamble_start == 9001u);
  DP_CHECK (dp_burst_capture_create_backed (NULL, zc, ZC_N, ZC_BURST, ZC_REPS,
                                            1.0, ACQ_CN0_NONE, 0.0, 1e-3, 0.9,
                                            0, 0.0)
            == NULL);
  dp_burst_capture_destroy (s);
  remove (path);
  return 0;
}

/**
 * Refine names the right repetition for a burst at the band EDGE, at an
 * even depth (doppler#1502).
 *
 * Refine scores each candidate with acquisition's statistic at Doppler
 * cells around the engine's estimate, mixing WITHIN each period as well as
 * across them. The engine's slow-time axis is periodic in the epoch rate,
 * and at an even depth its Nyquist bin reads -fs/(2P) for a carrier at
 * +fs/(2P): one frequency between periods, a whole span apart within one.
 * Unwrapped into the native span, the cells sat on the wrong alias and
 * every even depth lost Pd -- D = 2 a tenth of it. Zadoff-Chu 127 x 8 at a
 * Doppler 0.9 of the half-span, strong, pinned at D = 2: the window must
 * start exactly on the burst.
 */
static int
test_refine_wraps_its_doppler_cells (void)
{
  enum
  {
    NZ = 127,
    RZ = 8
  };
  static float _Complex zc[NZ];
  for (size_t k = 0; k < NZ; k++)
    zc[k] = (float _Complex)cexp (-I * M_PI * 5.0 * (double)k * (double)(k + 1)
                                  / (double)NZ);
  const size_t              burst = RZ * NZ + 400, at = 3 * NZ + 17;
  dp_burst_capture_state_t *s = dp_burst_capture_create (
      zc, NZ, burst, RZ, 1.0, 0.0, 0.0, 1e-3, 0.9, 0, 0.0);
  DP_REQUIRE (s != NULL);
  DP_REQUIRE (dp_burst_capture_configure_search_raw (s, 2, 1) == 0);

  const size_t    len = at + burst + 2 * s->refine_span + 4 * NZ;
  float _Complex *x   = dp_xmalloc (len * sizeof *x);
  /* 0.9 of the half-span: nearer the Nyquist bin than bin 0, so a D = 2
     engine reports it at the OTHER edge. At 0.45 it reports bin 0 and the
     alias never arises -- a first version sat there and passed with the
     wrap removed. */
  const double f  = 0.9 / (2.0 * (double)NZ); /* cycles/sample */
  uint32_t     st = 1502u;
  for (size_t i = 0; i < len; i++)
    {
      const float re = (float)dp_gauss (&st);
      const float im = (float)dp_gauss (&st);
      x[i]           = 0.05f * (re + I * im);
    }
  for (size_t i = 0; i < RZ * NZ; i++)
    x[at + i]
        += zc[i % NZ] * (float _Complex)cexp (I * 2.0 * M_PI * f * (double)i);

  const size_t    cap = dp_burst_capture_push_max_out (s, len);
  float _Complex *out = dp_xmalloc ((cap ? cap : 1) * sizeof *out);
  (void)dp_burst_capture_push (s, x, len, out, cap);
  DP_REQUIRE (dp_burst_capture_ready (s) >= 1);
  DP_CHECK (dp_burst_capture_event_at (s, 0)->preamble_start == at);
  free (out);
  free (x);
  dp_burst_capture_destroy (s);
  return 0;
}

/** At the edge of the native span a Zadoff-Chu preamble's detections carry
 *  TWO code phases, and refine must score both (doppler#1519).
 *
 * A carrier near +-fs/(2P) is halfway between two slow-time aliases, and
 * ZC's single-epoch correlation splits between the true lag and one u^-1
 * samples along its delay-Doppler ridge (u^-1 = 51 for root 5, N = 127).
 * The engine reports hits at both, and the claim keeps the STRONGER as the
 * anchor -- which is the ridge phase often enough to cost the capture 0.026
 * of Pd at the 0.9 design point (capture_dwell_pd). Refine resolved the
 * repetition but never the phase, so it inherited 51 samples of error.
 * Now every pending burst remembers the phases its detections carried and
 * refine keeps the best-scoring start. Pinned at D = 1, carriers at 0.95 to
 * 0.99 of the half-span on both sides, several offsets: the window must
 * start exactly on the burst every time.
 */
static int
test_refine_scores_every_detected_phase (void)
{
  enum
  {
    NZ = 127,
    RZ = 8
  };
  static float _Complex zc[NZ];
  for (size_t k = 0; k < NZ; k++)
    zc[k] = (float _Complex)cexp (-I * M_PI * 5.0 * (double)k * (double)(k + 1)
                                  / (double)NZ);
  const size_t burst = RZ * NZ + 400;
  const double fr[]  = { 0.95, -0.95, 0.97, -0.97, 0.99, -0.99 };
  const size_t ats[] = { 3 * NZ + 17, 4 * NZ + 90, 5 * NZ + 51 };
  int          exact = 0, total = 0;
  for (size_t j = 0; j < sizeof fr / sizeof *fr; j++)
    for (size_t a = 0; a < sizeof ats / sizeof *ats; a++)
      {
        dp_burst_capture_state_t *s = dp_burst_capture_create (
            zc, NZ, burst, RZ, 1.0, 0.0, 0.0, 1e-3, 0.9, 0, 0.0);
        DP_REQUIRE (s != NULL);
        DP_REQUIRE (dp_burst_capture_configure_search_raw (s, 1, 1) == 0);
        const size_t    at  = ats[a];
        const size_t    len = at + burst + 2 * s->refine_span + 4 * NZ;
        float _Complex *x   = dp_xmalloc (len * sizeof *x);
        const double    f   = fr[j] / (2.0 * (double)NZ); /* cycles/sample */
        uint32_t        st  = 1519u + (uint32_t)(10 * j + a);
        for (size_t i = 0; i < len; i++)
          {
            const float re = (float)dp_gauss (&st);
            const float im = (float)dp_gauss (&st);
            x[i]           = 0.05f * (re + I * im);
          }
        for (size_t i = 0; i < RZ * NZ; i++)
          x[at + i] += zc[i % NZ]
                       * (float _Complex)cexp (I * 2.0 * M_PI * f * (double)i);
        const size_t    cap = dp_burst_capture_push_max_out (s, len);
        float _Complex *out = dp_xmalloc ((cap ? cap : 1) * sizeof *out);
        (void)dp_burst_capture_push (s, x, len, out, cap);
        total++;
        if (dp_burst_capture_ready (s) >= 1
            && dp_burst_capture_event_at (s, 0)->preamble_start == at)
          exact++;
        free (out);
        free (x);
        dp_burst_capture_destroy (s);
      }
  DP_CHECK (exact == total);
  if (exact != total)
    fprintf (stderr, "  refine_scores_every_detected_phase: %d/%d exact\n",
             exact, total);
  return 0;
}

/** A doppler_uncertainty wider than the native span is SEARCHED, and a
 *  burst in any tile comes back at its true start and its true Doppler --
 *  not modulo the span (doppler#1512).
 *
 * The engine tiles the range with windows one span wide. The capture used
 * to read back only the coherent depth (1), convert every hit's bin over
 * that one bin (so each tiled hit read 0 Hz), and fold refine's cells about
 * 0, mixing a burst in any other tile at the wrong frequency. A 127-chip
 * m-sequence, one sample a chip, 8 repetitions, uncertainty two spans each
 * side: carriers inside the native span, mid-tile and near tile edges, both
 * signs.
 *
 * A PN code, deliberately. A periodic Zadoff-Chu preamble cannot pass this
 * and no capture could make it: its delay-Doppler ridge correlates at FULL
 * magnitude at (k tiles, k*u^-1 samples) for every k, so beyond the native
 * span its Doppler tile and its delay are one unknown. That is documented
 * on dp_burst_capture_create(), not a defect to pin.
 */
static int
test_a_wide_doppler_search_is_searched (void)
{
  enum
  {
    NP  = 127,
    RP  = 8,
    PAY = 400
  };
  uint8_t        code[NP];
  dp_pn_state_t *pn = dp_pn_create (pn_mls_poly (7), 1u, 7u, 0);
  DP_REQUIRE (pn != NULL);
  for (size_t i = 0; i < NP; i++)
    code[i] = pn_step (pn);
  dp_pn_destroy (pn);
  float _Complex *pre = dp_code_preamble (code, NP, 1);

  static float _Complex cap[40000];
  static float _Complex out[4 * (RP * NP + PAY)];
  const size_t n_cap = sizeof cap / sizeof *cap;
  const size_t at    = 9001u;
  const double span  = 1.0 / (double)NP; /* one tile, cycles/sample */
  const double fr[]  = { 0.3, 0.9, 1.5, 2.2, 2.45, -1.2, -1.9, -2.45 };
  for (size_t j = 0; j < sizeof fr / sizeof *fr; j++)
    {
      const double f  = fr[j] * span;
      uint32_t     st = 1512u + (uint32_t)j;
      for (size_t i = 0; i < n_cap; i++)
        {
          const float re = (float)(0.02 * dp_gauss (&st));
          const float im = (float)(0.02 * dp_gauss (&st));
          cap[i]         = re + im * I;
        }
      for (size_t i = 0; i < RP * NP; i++)
        cap[at + i] += pre[i % NP]
                       * (float _Complex)cexp (I * 2.0 * M_PI * f * (double)i);

      dp_burst_capture_state_t *s = dp_burst_capture_create (
          pre, NP, RP * NP + PAY, RP, 1.0, ACQ_CN0_NONE, 2.5 * span, 1e-6, 0.9,
          0, 0.0);
      DP_REQUIRE (s != NULL);
      /* The read-back is the grid the engine searches. */
      DP_CHECK (dp_burst_capture_get_doppler_bins (s)
                == acq_grid_bins (s->acq->engine));
      DP_CHECK (dp_burst_capture_get_doppler_bins (s) > 1);
      size_t n = dp_burst_capture_push (s, cap, n_cap, out,
                                        sizeof out / sizeof *out);
      const burst_capture_event_t *ev = dp_burst_capture_event_at (s, 0);
      DP_CHECK (n == RP * NP + PAY);
      DP_REQUIRE (ev != NULL);
      DP_CHECK (ev->preamble_start == at);
      /* Absolute, not modulo 1/P: the tile is part of the answer. */
      DP_CHECK (fabs (ev->doppler_hz_est - f) <= ev->doppler_res_hz);
      dp_burst_capture_destroy (s);
    }
  free (pre);
  return 0;
}

/** A tiled capture anchors each hit at its own dwell (doppler#2090).
 *
 * Past the native span the engine tiles Doppler: its rows are W frequency
 * hypotheses over ONE frame of P samples, so a hit's code phase is measured
 * in a frame of P, while the engine's `n` is W*P. Backing the epoch off `n`
 * anchored every hit (W - 1)*P early. With reps = 1, refine looks only one
 * period ahead and could not reach the true start; a burst in the stream's
 * first (W - 1)*P samples wrapped its anchor to about 2^64 -- a window at a
 * garbage start, and a live checkpoint set_state() refuses because the
 * anchor is past the stream position. Asserted, tiled with W - 1 > reps + 1
 * and reps = 1: a burst mid-stream comes back exactly once at its true start;
 * one at sample 50 does too, pushed in 128-sample blocks, and a checkpoint
 * taken while it is still queued restores into the same capture.
 */
static int
test_a_tiled_capture_anchors_at_its_dwell (void)
{
  enum
  {
    NP  = 127,
    PAY = 400
  };
  uint8_t        code[NP];
  dp_pn_state_t *pn = dp_pn_create (pn_mls_poly (7), 1u, 7u, 0);
  DP_REQUIRE (pn != NULL);
  for (size_t i = 0; i < NP; i++)
    code[i] = pn_step (pn);
  dp_pn_destroy (pn);
  float _Complex *pre = dp_code_preamble (code, NP, 1);

  static float _Complex cap[20000];
  const size_t n_cap  = sizeof cap / sizeof *cap;
  const double span   = 1.0 / (double)NP;
  const size_t ats[2] = { 9001u, 50u };
  for (size_t j = 0; j < 2u; j++)
    {
      const size_t at = ats[j];
      const double f  = 1.3 * span;
      uint32_t     st = 2090u + (uint32_t)j;
      for (size_t i = 0; i < n_cap; i++)
        {
          const float re = (float)(0.02 * dp_gauss (&st));
          const float im = (float)(0.02 * dp_gauss (&st));
          cap[i]         = re + im * I;
        }
      for (size_t i = 0; i < NP; i++)
        cap[at + i]
            += pre[i] * (float _Complex)cexp (I * 2.0 * M_PI * f * (double)i);

      dp_burst_capture_state_t *s
          = dp_burst_capture_create (pre, NP, NP + PAY, 1u, 1.0, ACQ_CN0_NONE,
                                     2.5 * span, 1e-6, 0.9, 0, 0.0);
      DP_REQUIRE (s != NULL);
      const dp_acq_state_t *e = s->acq->engine;
      /* The premise that makes the 9001 case discriminate: the old anchor,
         (W - 1) periods early, is out of refine's reach (k_hi = reps), even
         with the deciding frame a period past the start. */
      DP_REQUIRE (e->window_bins - 1u > 1u + 1u);
      DP_REQUIRE (j == 0u || at < (e->window_bins - 1u) * NP);

      size_t found = 0, other = 0, restored = 0;
      for (size_t off = 0; off < n_cap; off += 128u)
        {
          size_t blk = n_cap - off < 128u ? n_cap - off : 128u;
          (void)dp_burst_capture_push (s, cap + off, blk, NULL, 0);
          for (size_t i = 0; i < dp_burst_capture_ready (s); i++)
            {
              const uint64_t ps
                  = dp_burst_capture_event_at (s, i)->preamble_start;
              found += ps == at;
              other += ps != at;
            }
          /* A checkpoint while the detection is queued, before its window
             has arrived, restores into the capture that took it. */
          if (!restored && s->pending > 0u && found == 0u)
            {
              const size_t   cb   = dp_burst_capture_state_bytes (s);
              unsigned char *blob = malloc (cb);
              DP_REQUIRE (blob != NULL);
              dp_burst_capture_get_state (s, blob);
              DP_CHECK (dp_burst_capture_set_state (s, blob) == DP_OK);
              free (blob);
              restored = 1;
            }
        }
      if (found != 1u || other != 0u)
        fprintf (stderr, "  tiled at %zu: found %zu, other %zu\n", at, found,
                 other);
      DP_CHECK (restored == 1u);
      DP_CHECK (found == 1u);
      DP_CHECK (other == 0u);
      DP_CHECK (s->dropped == 0u);
      dp_burst_capture_destroy (s);
    }
  free (pre);
  return 0;
}

/** A tiled burst that follows a window closely is not shadowed by it
 *  (doppler#2090).
 *
 * The third symptom of an anchor (W - 1) periods early: emitting a window
 * arms `suppress_until` at its end, and a hit anchored before that is
 * shadowed -- taken for the window's own payload -- and dropped at the next
 * push. So a second burst whose start sits fewer than (W - 1)*P samples past
 * the first window was lost, silently. Here reps >= W - 1, so refine reaches
 * the first burst even from the old anchor, and the second alone carries the
 * symptom: the gap clears min_gap (the capture promises both) and is under
 * (W - 1)*P (the old anchor lands inside the first window's span). Both come
 * out exactly once at their true starts.
 */
static int
test_a_tiled_burst_after_a_window_is_not_shadowed (void)
{
  enum
  {
    NP  = 127,
    RP  = 4,
    PAY = 2600
  };
  uint8_t        code[NP];
  dp_pn_state_t *pn = dp_pn_create (pn_mls_poly (7), 1u, 7u, 0);
  DP_REQUIRE (pn != NULL);
  for (size_t i = 0; i < NP; i++)
    code[i] = pn_step (pn);
  dp_pn_destroy (pn);
  float _Complex *pre = dp_code_preamble (code, NP, 1);

  static float _Complex cap[20000];
  const size_t              n_cap = sizeof cap / sizeof *cap;
  const double              span  = 1.0 / (double)NP;
  const size_t              BL    = RP * NP + PAY;
  dp_burst_capture_state_t *s     = dp_burst_capture_create (
      pre, NP, BL, RP, 1.0, ACQ_CN0_NONE, 2.5 * span, 1e-6, 0.9, 0, 0.0);
  DP_REQUIRE (s != NULL);
  const size_t W   = s->acq->engine->window_bins;
  const size_t gap = 300u;
  /* The premises: tiled; the first burst within refine's reach of the old
     anchor; a gap the capture promises to resolve, under (W - 1)*P. */
  DP_REQUIRE (W >= 3u && W - 1u <= RP);
  DP_REQUIRE (gap >= s->min_gap && gap < (W - 1u) * NP);

  const size_t at[2] = { 3000u, 3000u + BL + gap };
  const double f     = 0.7 * span;
  uint32_t     st    = 2090u;
  for (size_t i = 0; i < n_cap; i++)
    {
      const float re = (float)(0.02 * dp_gauss (&st));
      const float im = (float)(0.02 * dp_gauss (&st));
      cap[i]         = re + im * I;
    }
  for (size_t b = 0; b < 2u; b++)
    for (size_t i = 0; i < RP * NP; i++)
      cap[at[b] + i]
          += pre[i % NP]
             * (float _Complex)cexp (I * 2.0 * M_PI * f * (double)i);

  size_t seen[2] = { 0 }, other = 0;
  for (size_t off = 0; off < n_cap; off += 1000u)
    {
      size_t blk = n_cap - off < 1000u ? n_cap - off : 1000u;
      (void)dp_burst_capture_push (s, cap + off, blk, NULL, 0);
      for (size_t i = 0; i < dp_burst_capture_ready (s); i++)
        {
          const uint64_t ps = dp_burst_capture_event_at (s, i)->preamble_start;
          seen[0] += ps == at[0];
          seen[1] += ps == at[1];
          other += ps != at[0] && ps != at[1];
        }
    }
  if (seen[0] != 1u || seen[1] != 1u || other != 0u)
    fprintf (stderr, "  tiled pair: first %zu, second %zu, other %zu\n",
             seen[0], seen[1], other);
  DP_CHECK (seen[0] == 1u);
  DP_CHECK (seen[1] == 1u);
  DP_CHECK (other == 0u);
  dp_burst_capture_destroy (s);
  free (pre);
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
  if (test_refine_wraps_its_doppler_cells ())
    return 1;
  if (test_refine_scores_every_detected_phase ())
    return 1;
  if (test_a_wide_doppler_search_is_searched ())
    return 1;
  if (test_a_tiled_capture_anchors_at_its_dwell ())
    return 1;
  if (test_a_tiled_burst_after_a_window_is_not_shadowed ())
    return 1;
  if (test_every_burst_is_emitted_once ())
    return 1;
  if (test_block_size_below_min_gap ())
    return 1;
  if (test_a_held_head_does_not_stall_long_bursts ())
    return 1;
  if (test_release_on_every_window_never_wedges ())
    return 1;
  if (test_a_dead_entry_is_swept ())
    return 1;
  if (test_a_resume_is_exact ())
    return 1;
  if (test_a_forged_blob_is_checked ())
    return 1;
  if (test_a_refused_blob_changes_nothing ())
    return 1;
  if (test_the_ring_always_has_room ())
    return 1;
  if (test_a_full_ring_restored_makes_room ())
    return 1;
  if (test_a_moved_anchor_keeps_anchor_order ())
    return 1;
  if (test_chunk_invariance ())
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
  if (test_one_look_and_the_design_point_is_optional ())
    return 1;
  if (test_configure_search_raw_refuses_a_grid_beyond_reach ())
    return 1;
  if (test_release_gives_back_a_shadowed_burst ())
    return 1;
  if (test_destroy_null_is_safe ())
    return 1;
  if (test_doppler_rate_caps_the_depth ())
    return 1;
  if (test_captures_a_zadoff_chu_burst ())
    return 1;
  if (test_zadoff_chu_capture_under_doppler ())
    return 1;
  if (test_rejects_a_bad_preamble ())
    return 1;
  if (test_backed_captures_a_zadoff_chu_burst ())
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
  if (test_the_live_capture_restores_its_own_checkpoint ())
    return 1;
  if (test_a_span_the_ring_wrapped_past_is_refused ())
    return 1;
  if (test_backed_rejects_a_bad_path ())
    return 1;

  /* ── a blob from the PREVIOUS state version is refused ────────────────────
   *
   * The version exists to make an incompatible layout a refusal instead of a
   * reinterpretation, and nothing pinned that: the round-trip macro clobbers
   * the MAGIC, which a blob carrying a real magic and a stale version gets
   * past. doppler#1312 removed a field from both this state and its pending
   * queue -- the size check alone would not have caught it if some other
   * field had absorbed the eight bytes, which is exactly the case the version
   * is for. Rewriting only the version word leaves every other byte valid, so
   * this fails if and only if the version is actually consulted. */
  {
    dp_burst_capture_state_t *s = make ();
    DP_REQUIRE (s != NULL);
    size_t cb = dp_burst_capture_state_bytes (s);
    DP_REQUIRE (cb > sizeof (dp_state_hdr_t));
    unsigned char *blob = malloc (cb);
    DP_REQUIRE (blob != NULL);
    dp_burst_capture_get_state (s, blob);
    /* Unmodified, it restores. */
    DP_CHECK (dp_burst_capture_set_state (s, blob) == DP_OK);
    /* One version back -- every other byte still valid. */
    dp_state_hdr_t hdr;
    memcpy (&hdr, blob, sizeof hdr);
    DP_CHECK (hdr.version == BURST_CAPTURE_STATE_VERSION);
    hdr.version = (uint16_t)(BURST_CAPTURE_STATE_VERSION - 1u);
    memcpy (blob, &hdr, sizeof hdr);
    DP_CHECK (dp_burst_capture_set_state (s, blob) == DP_ERR_INVALID);
    /* ...and one version FORWARD is refused too, so the check is not a
       >= comparison that would accept anything newer. */
    hdr.version = (uint16_t)(BURST_CAPTURE_STATE_VERSION + 1u);
    memcpy (blob, &hdr, sizeof hdr);
    DP_CHECK (dp_burst_capture_set_state (s, blob) == DP_ERR_INVALID);
    free (blob);
    dp_burst_capture_destroy (s);
  }

  DP_TEST_END ("test_burst_capture_core");
}
