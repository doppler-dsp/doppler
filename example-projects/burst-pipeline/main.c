/*
 * main.c — a burst waveform, end to end: describe, generate, write, consume.
 *
 * Copy this directory as the starting point for a downstream that generates
 * realistic burst traffic and then does something with it. It walks the four
 * decisions that actually cost time or correctness:
 *
 *   1. THE FRAME. A `wfm_frame_desc_t` the caller builds — fields in wire
 *      order, a stage covering a span it names — put on a source. No wfmgen
 *      flag spells the layout used here.
 *
 *   2. THE BURST TRAIN, naive then declared. Sixty bursts as sixty listed
 *      segments, against ONE segment with `repeats = 60` — and they are NOT
 *      the same waveform. The listed form restarts the source from its seed
 *      each time, so all sixty bursts carry identical noise; `repeats` draws
 *      fresh noise per instance. Measured here, both ways, because the
 *      difference is invisible in a plot and fatal to a Monte-Carlo run.
 *
 *   3. SNR ACROSS THE SCENE, measured rather than asserted. SNR is a property
 *      of a SOURCE, and the noise floor it implies runs through the whole
 *      segment — including the gaps, where the signal is off. So the gap is
 *      the noise floor, the burst is signal + that same floor, and the two
 *      together recover the SNR that was asked for.
 *
 *   4. PREPARE ONCE, SWEEP MANY. A sweep over SNR re-renders the same bursts
 *      at every point. `wfm_plan_prepare` caches the clean signal and
 *      `wfm_plan_at` re-weights it, which is the difference between doing the
 *      DSP once and doing it per point. Both times are printed, and NEITHER
 *      is asserted: a Plan caches the signal but redraws the noise, so what
 *      it saves is the signal's share of the work — measure it for your own
 *      scene rather than taking a ratio on trust. What IS asserted is that
 *      the cached render is byte-identical to composing the scene. A cache
 *      that is fast because it quietly does less is exactly the failure this
 *      example found (doppler#1158), and no stopwatch can see it.
 *
 * Then it writes the result to a BLUE file and reads it back the way a
 * consumer would — in blocks, as fast as the reader will produce them —
 * timing both halves and checking the bytes survived.
 *
 * Every timing here is measured on the machine that runs it and printed with
 * its units; nothing is hard-coded, and the ratio is the point rather than
 * any absolute number. Every check exits non-zero on failure, because an
 * example that validates nothing still exits 0.
 *
 * Builds against either link mode; see CMakeLists.txt (find_package) and the
 * pkg-config commands in docs/install/c.md.
 */
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "wfm/wfm_compose.h"
#include "wfm/wfm_frame.h"
#include "wfm/wfm_plan.h"
#include "wfm_reader/wfm_reader_core.h"
#include "wfm_synth/wfm_synth_core.h"
#include "wfm_writer/wfm_writer_core.h"

#define FS 1.0e6      /* sample rate, Hz                                  */
#define SPS 4         /* samples per symbol; rectangular                  */
#define HDR_BITS 16u  /* the caller's own header                          */
#define PAY_BITS 240u /* payload                                          */
#define FRAME_BITS (HDR_BITS + PAY_BITS + WFM_FRAME_CRC_BITS)

#define BURST_ON (FRAME_BITS * (unsigned)SPS) /* one frame, exactly       */
#define BURST_OFF 4096u                       /* gap between bursts       */
#define N_BURSTS 60u
#define TOTAL ((size_t)(BURST_ON + BURST_OFF) * N_BURSTS)

#define SNR_DB 12.0 /* declared per-source SNR, in `fs` mode           */
#define N_SWEEP 24u /* operating points in the sweep                   */
#define READ_BLOCK 4096u

static int failures = 0;

/** @brief Report one named check; the first failure sets the exit status. */
static void
check (int ok, const char *what)
{
  printf ("  [%s] %s\n", ok ? "ok" : "FAIL", what);
  if (!ok)
    failures++;
}

/** @brief Monotonic seconds. CLOCK_MONOTONIC, so it is not NTP's to move. */
static double
now_s (void)
{
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

/** @brief Mean power of a span, linear. */
static double
power (const float complex *x, size_t n)
{
  double acc = 0.0;
  for (size_t i = 0; i < n; i++)
    acc += (double)crealf (x[i]) * crealf (x[i])
           + (double)cimagf (x[i]) * cimagf (x[i]);
  return n ? acc / (double)n : 0.0;
}

/** @brief A `wfm_seq_t` over bits the caller owns and keeps. */
static wfm_seq_t
literal (const uint8_t *bits, size_t len)
{
  wfm_seq_t s = { 0 };
  s.kind      = WFM_SEQ_LITERAL;
  s.bits      = bits;
  s.len       = len;
  return s;
}

/** @brief The burst: one framed BPSK source at the declared SNR. */
static wfm_source_t
burst_source (const uint8_t *payload, const wfm_frame_desc_t *d)
{
  /* `= { 0 }` then named fields, never a positional initialiser list:
     wfm_source_t carries 30-odd members and a positional list silently
     shifts the moment one is inserted. */
  wfm_source_t src = { 0 };
  src.type         = WFM_SYNTH_BITS;
  src.payload      = literal (payload, PAY_BITS);
  src.modulation   = 1; /* bpsk */
  src.sps          = SPS;
  src.snr          = SNR_DB;
  src.snr_mode     = 1; /* fs: SNR against the noise in the whole band */
  src.seed         = 7u;
  src.frame        = d;
  return src;
}

/** @brief Drain a composer into `out`, returning what it produced. */
static size_t
drain (wfm_compose_state_t *c, float complex *out, size_t cap)
{
  size_t n = 0, got;
  while (n < cap && (got = wfm_compose_execute (c, out + n, cap - n)) > 0)
    n += got;
  return n;
}

int
main (void)
{
  printf ("=== a burst waveform, end to end ===\n\n");

  /* Borrowed by the description and by every source below, so they outlive
     every compose call. */
  static uint8_t hdr[HDR_BITS], pay[PAY_BITS];
  for (unsigned i = 0; i < HDR_BITS; i++)
    hdr[i] = (uint8_t)((0x5C5Cu >> (HDR_BITS - 1u - i)) & 1u);
  for (unsigned i = 0; i < PAY_BITS; i++)
    pay[i] = (uint8_t)((i * 7u + 1u) & 1u);

  /* ── 1. the frame ───────────────────────────────────────────────────── */
  printf ("--- 1. The frame this waveform carries ---\n");

  wfm_seq_t        h = literal (hdr, HDR_BITS), p = literal (pay, PAY_BITS);
  wfm_frame_desc_t d;
  memset (&d, 0, sizeof d);
  /* The cover names its ends and REACHES the derived field, which is what
     wires that field's producer — so the CRC's position and the fact that a
     CRC fills it are one declaration rather than two that can disagree. The
     header sits outside the cover: a receiver finds it before it can check
     anything. */
  int built
      = wfm_frame_add_field (&d, "hdr", &h, 0u) == 0
        && wfm_frame_add_field (&d, "payload", &p, 0u) == 1
        && wfm_frame_add_derived (&d, "crc", WFM_FRAME_CRC_BITS) == 2
        && wfm_frame_add_stage (&d, WFM_STAGE_CRC16, "payload", "crc") == 0;
  check (built, "a header, a payload and a derived CRC-16 over a named span");
  if (!built)
    return 1;

  wfm_frame_desc_layout_t lay;
  check (wfm_frame_desc_layout (&d, &lay) == 0 && lay.frame_bits == FRAME_BITS,
         "the description lays out at the length the burst is sized from");
  printf ("  %zu frame bits x %d sps = %u samples of burst, "
          "%u samples of gap\n\n",
          lay.frame_bits, SPS, BURST_ON, BURST_OFF);

  float complex *listed   = calloc (TOTAL, sizeof *listed);
  float complex *declared = calloc (TOTAL, sizeof *declared);
  float complex *swept    = calloc (TOTAL, sizeof *swept);
  float complex *readback = calloc (TOTAL, sizeof *readback);
  if (!listed || !declared || !swept || !readback)
    {
      fprintf (stderr, "out of memory\n");
      return 1;
    }

  /* ── 2. the burst train, listed then declared ───────────────────────── */
  printf ("--- 2. Sixty bursts: listed, then declared ---\n");

  /* The naive shape: one segment per burst. It works, and it is sixty copies
     of one fact — change the gap and you change it sixty times. */
  wfm_source_t   src  = burst_source (pay, &d);
  wfm_segment_t *many = calloc (N_BURSTS, sizeof *many);
  if (!many)
    {
      fprintf (stderr, "out of memory\n");
      return 1;
    }
  for (unsigned i = 0; i < N_BURSTS; i++)
    {
      many[i].sources     = &src;
      many[i].n_sources   = 1u;
      many[i].fs          = FS;
      many[i].num_samples = BURST_ON;
      many[i].off_samples = BURST_OFF;
    }

  double               t0       = now_s ();
  wfm_compose_state_t *c        = wfm_compose_create (many, N_BURSTS, 0, 0);
  size_t               n_listed = c ? drain (c, listed, TOTAL) : 0;
  wfm_compose_destroy (c);
  const double t_listed = now_s () - t0;

  /* The declared shape: ONE segment that plays `repeats` times. Each
     instance is delay + on-time + gap, the signal is fixed across them and
     the AWGN is fresh per instance — a burst train from one declaration. */
  wfm_segment_t one = { 0 };
  one.sources       = &src;
  one.n_sources     = 1u;
  one.fs            = FS;
  one.num_samples   = BURST_ON;
  one.off_samples   = BURST_OFF;
  one.repeats       = N_BURSTS;

  t0                = now_s ();
  c                 = wfm_compose_create (&one, 1u, 0, 0);
  size_t n_declared = c ? drain (c, declared, TOTAL) : 0;
  wfm_compose_destroy (c);
  const double t_declared = now_s () - t0;

  check (n_listed == TOTAL && n_declared == TOTAL,
         "both shapes produce the whole train");
  printf ("  %u segments: %.1f ms   |   1 segment x %u repeats: %.1f ms\n",
          N_BURSTS, t_listed * 1e3, N_BURSTS, t_declared * 1e3);

  /* AND THEY ARE NOT THE SAME WAVEFORM. This is the reason to reach for
     `repeats`, and it is not brevity.

     A listed segment is a fresh segment: it starts this source from its
     declared `seed`, so all sixty draw the SAME noise. Sixty bursts that
     look independent and are not — which is precisely the mistake a
     Monte-Carlo run cannot afford, and it is invisible in a plot.

     `repeats` says "the same burst, again": the signal is fixed across
     instances and the AWGN is FRESH per instance. */
  const size_t period = BURST_ON + BURST_OFF;
  check (memcmp (listed, listed + period, BURST_ON * sizeof *listed) == 0,
         "listed: every burst carries IDENTICAL noise — one seed, sixty "
         "fresh segments");
  check (memcmp (declared, declared + period, BURST_ON * sizeof *declared)
             != 0,
         "repeats: each instance draws its own noise, as a burst train "
         "should");
  printf ("  so `repeats` is not shorthand for the listed form — it is the "
          "one\n  that gives sixty INDEPENDENT bursts.\n\n");

  /* ── 3. SNR across the scene ────────────────────────────────────────── */
  printf ("--- 3. Where the declared %.0f dB SNR actually shows up ---\n",
          SNR_DB);

  /* The gap carries the noise floor: the source's AWGN keeps running while
     the signal stops, so an inter-burst region is the CHANNEL rather than
     digital silence. That is what makes the floor measurable here at all. */
  const size_t burst0_off = 0;
  const size_t gap0_off   = BURST_ON;
  const double p_burst    = power (declared + burst0_off, BURST_ON);
  const double p_gap      = power (declared + gap0_off, BURST_OFF);
  const double snr_meas   = 10.0 * log10 ((p_burst - p_gap) / p_gap);

  check (p_gap > 0.0,
         "the GAP is not silent — it carries the segment's noise floor");
  check (fabs (snr_meas - SNR_DB) < 1.0,
         "burst power over gap power recovers the declared SNR");
  printf ("  burst %.4f   gap %.4f   ->  %.2f dB measured "
          "against %.0f dB declared\n",
          p_burst, p_gap, snr_meas, SNR_DB);
  printf ("  snr_mode=fs, so the floor is the noise in the WHOLE band;\n"
          "  every instance of the burst sees the same declaration.\n\n");

  /* ── 4. prepare once, sweep many ────────────────────────────────────── */
  printf ("--- 4. A %u-point SNR sweep, re-composed then cached ---\n",
          N_SWEEP);

  char *spec = wfm_spec_to_json (&one, 1u, 0, 0, WFM_SEED_ADVANCE_NONE, 0.0);
  if (!spec)
    {
      fprintf (stderr, "wfm_spec_to_json failed\n");
      return 1;
    }

  /* Naive: every operating point re-runs the whole scene — the framing, the
     bit mapping, the pulse, for bursts that did not change. */
  t0 = now_s ();
  for (unsigned k = 0; k < N_SWEEP; k++)
    {
      wfm_source_t s2         = src;
      s2.snr                  = 4.0 + (double)k;
      wfm_segment_t g         = one;
      g.sources               = &s2;
      wfm_compose_state_t *cc = wfm_compose_create (&g, 1u, 0, 0);
      if (cc)
        (void)drain (cc, swept, TOTAL);
      wfm_compose_destroy (cc);
    }
  const double t_recompose = now_s () - t0;

  /* Cached: prepare renders and keeps each source's clean ON-time once; a
     point is then a re-weighted sum plus fresh noise. */
  t0                     = now_s ();
  wfm_plan_t  *plan      = wfm_plan_prepare (spec);
  const double t_prepare = now_s () - t0;
  check (plan != NULL, "the scene is in scope for a Plan");

  double t_sweep  = 0.0;
  int    all_full = 1;
  if (plan)
    {
      t0 = now_s ();
      for (unsigned k = 0; k < N_SWEEP; k++)
        /* The return is the length of THIS draw. Discarding it is how a Plan
           that renders nothing still looks like a fast sweep. */
        if (wfm_plan_at (plan, 4.0 + (double)k, 1234u + k, swept) != TOTAL)
          all_full = 0;
      t_sweep = now_s () - t0;
    }
  check (plan != NULL && all_full,
         "every point of the sweep rendered the whole train");

  /* THE CHECK THAT MATTERS, and it is not the clock. A cache is worth having
     only if it renders what compose would have: at the scene's own SNR and
     anchor seed, `wfm_plan_at` is documented to reproduce `wfm_compose` to
     the bit (wfm_plan.h). Pinning that here rather than a speed ratio is
     deliberate — the ratio is a property of the machine, and asserting one
     made this example fail on macOS while passing on Linux. Worse, the
     speedup it was celebrating was partly a Plan that dropped the noise
     floor entirely (doppler#1158); a timing gate cannot tell "faster" from
     "did less". Byte-identity can. */
  int exact = 0;
  if (plan)
    {
      float complex *ref = calloc (TOTAL, sizeof *ref);
      if (!ref)
        {
          fprintf (stderr, "out of memory\n");
          return 1;
        }
      wfm_compose_state_t *ref_c = wfm_compose_create (&one, 1u, 0, 0);
      const size_t         n_ref = ref_c ? drain (ref_c, ref, TOTAL) : 0;
      wfm_compose_destroy (ref_c);
      const size_t n_plan
          = wfm_plan_at (plan, SNR_DB, wfm_plan_anchor_seed (plan), swept);
      exact = n_ref == TOTAL && n_plan == TOTAL
              && memcmp (ref, swept, TOTAL * sizeof *swept) == 0;
      free (ref);
    }
  check (exact, "a cached render is byte-identical to composing the scene");

  free (spec);

  printf ("  re-composing each point : %7.1f ms\n", t_recompose * 1e3);
  printf ("  prepare once            : %7.1f ms\n", t_prepare * 1e3);
  printf ("  then %2u cached renders  : %7.1f ms\n", N_SWEEP, t_sweep * 1e3);
  if (t_sweep > 0.0)
    printf ("  -> %.1fx on the sweep itself, on this machine\n",
            t_recompose / t_sweep);
  printf ("  a Plan caches the SIGNAL, not the noise: the AWGN is redrawn\n"
          "  every point either way. So the saving is the signal's share of\n"
          "  the work — modest for a low-duty burst train like this one\n"
          "  (%u on, %u off), and larger as the on-time fraction or the\n"
          "  per-sample signal cost (RRC, spreading, long codes) grows.\n"
          "  Measure it for YOUR scene; that is why both numbers are here.\n",
          BURST_ON, BURST_OFF);
  printf ("\n");

  /* ── 5. write it to a BLUE file ─────────────────────────────────────── */
  printf ("--- 5. Out to a BLUE type-1000 file ---\n");

  const char *path = "burst_train.blue";
  t0               = now_s ();
  /* sample_type 0 is cf32 — the composer's own type, so the file is a
     lossless copy and the readback below can be compared exactly. `total`
     lets the 512-byte header carry the length up front. */
  wfm_writer_state_t *w = wfm_writer_create (path, FS, WFM_FT_BLUE, 0, 0, 0.0,
                                             TOTAL, 0.0, 0.0, false);
  size_t              wrote = w ? wfm_writer_write (w, declared, TOTAL) : 0;
  /* close() FINALISES AND FREES, and `wfm_writer_destroy` is the same
     function under a second name -- "C callers may use either". Calling both,
     as a create/destroy pair invites, closes the FILE twice; it segfaults in
     ferror() on the second pass. One call, and check its status: BLUE patches
     its length and flushes here, so a failure at close is a failed capture. */
  int          wrc     = w ? wfm_writer_close (w) : -1;
  const double t_write = now_s () - t0;

  check (wrote == TOTAL && wrc == 0, "the whole train reached the file");
  printf ("  %zu samples in %.1f ms  (%.1f Msample/s)\n\n", wrote,
          t_write * 1e3, (double)wrote / t_write / 1e6);

  /* ── 6. a consumer, reading as fast as it is given samples ──────────── */
  printf ("--- 6. A consumer reading it back, block by block ---\n");

  t0                      = now_s ();
  wfm_reader_state_t *r   = wfm_reader_create (path, 0, 0);
  size_t              got = 0, bursts = 0;
  double              acc = 0.0;
  if (r)
    {
      /* Read in blocks and do the work IN the loop, so the timing below
         covers reading AND processing rather than reading alone. The work
         here is running total energy — trivial, but real: drop it and the
         number becomes the reader's throughput rather than a consumer's. */
      size_t n;
      while ((n = wfm_reader_read (r, READ_BLOCK, readback + got, TOTAL - got))
             > 0)
        {
          for (size_t i = 0; i < n; i++)
            {
              const float complex v = readback[got + i];
              acc += (double)crealf (v) * crealf (v)
                     + (double)cimagf (v) * cimagf (v);
            }
          got += n;
          if (got >= TOTAL)
            break;
        }
      wfm_reader_destroy (r);
    }
  const double t_read = now_s () - t0;

  /* Count the bursts in what was READ, on the burst period, so the count is
     a property of the recovered data rather than of the loop above. The
     threshold sits between the floor and the burst, both measured in
     section 3 — nothing here is a tuned constant. */
  for (unsigned i = 0; i < N_BURSTS; i++)
    if (power (readback + (size_t)i * period, BURST_ON)
        > 0.5 * (p_burst + p_gap))
      bursts++;

  check (got == TOTAL, "the consumer read every sample that was written");
  check (memcmp (readback, declared, TOTAL * sizeof *declared) == 0,
         "and cf32 through BLUE is byte-exact — what went in came back");
  check (bursts == N_BURSTS, "it finds all sixty bursts in what it read");
  printf ("  %zu samples in %.1f ms  (%.1f Msample/s), mean power %.4f\n", got,
          t_read * 1e3, (double)got / t_read / 1e6,
          got ? acc / (double)got : 0.0);
  printf ("  %zu bursts recovered\n\n", bursts);

  (void)remove (path);
  free (many);
  free (listed);
  free (declared);
  free (swept);
  free (readback);
  wfm_plan_destroy (plan);

  if (failures)
    {
      printf ("=== %d check(s) FAILED ===\n", failures);
      return 1;
    }
  printf ("=== all checks passed ===\n");
  return 0;
}
