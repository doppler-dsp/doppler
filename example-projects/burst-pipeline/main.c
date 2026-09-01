/*
 * main.c — a burst waveform, end to end: describe, generate, write, receive.
 *
 * Copy this directory as the starting point for a downstream that generates
 * realistic burst traffic and then does something with it. Five steps, each
 * checked, each exiting non-zero if the check fails:
 *
 *   1. THE FRAME. A `wfm_frame_desc_t` the caller builds — fields in wire
 *      order, a stage covering a span it names — put on a source. No wfmgen
 *      flag spells this layout.
 *
 *   2. THE BURST TRAIN, listed then declared. Sixty bursts as sixty listed
 *      segments are NOT the same waveform as one segment with `repeats = 60`:
 *      a listed segment restarts the source from its seed, so all sixty carry
 *      identical noise, while `repeats` draws fresh noise per instance. The
 *      difference is invisible in a plot and fatal to a Monte-Carlo run.
 *
 *   3. SNR ACROSS THE SCENE. SNR is a property of a SOURCE, and the noise
 *      floor it implies runs through the whole segment — including the gaps,
 *      where the signal is off. So a gap is the channel, not digital silence.
 *
 *   4. PREPARE ONCE, SWEEP MANY. `wfm_plan_prepare` caches the clean signal
 *      and `wfm_plan_at` re-weights it. Both timings are printed and NEITHER
 *      is asserted: a Plan caches the signal but redraws the noise, so what
 *      it saves is the signal's share of the work, and that depends on the
 *      scene and the machine. What IS asserted is that a cached render is
 *      byte-identical to composing the scene — a cache that is fast because
 *      it quietly does less is not something a stopwatch can catch.
 *
 *   5. WRITE IT, THEN RECEIVE IT. Out to a BLUE file, back in through the
 *      reader, and then found the way a receiver would have to: demodulate
 *      the whole record, gaps included, and search for the frame's sync
 *      marker. The marker is rebuilt from the frame's own declaration rather
 *      than held as a second copy, and the search tolerance is derived by
 *      `syncword_max_errors_for` rather than guessed. Each frame found is
 *      checked with `wfm_frame_desc_crc_ok`, which needs no payload truth —
 *      a frame error rate a receiver could compute on someone else's capture.
 *
 * Measurement comes from the library where the library has it:
 * `snr_data_aided_db` / `snr_m2m4_db` for SNR, `syncword_*` for the search
 * and its threshold arithmetic. Every number printed is measured on the
 * machine that runs it and carries its units.
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

#include "mpsk/mpsk_core.h"
#include "snr/snr_core.h"
#include "syncword/syncword_core.h"
#include "wfm/wfm_compose.h"
#include "wfm/wfm_frame.h"
#include "wfm/wfm_plan.h"
#include "wfm_reader/wfm_reader_core.h"
#include "wfm_synth/wfm_synth_core.h"
#include "wfm_writer/wfm_writer_core.h"

#define FS 1.0e6 /* sample rate, Hz                                  */
/* ONE SAMPLE PER SYMBOL. The frame, the sync search and the CRC are all
   questions about SYMBOLS, so this example stays in that domain: a sample is
   a symbol, and the receive half is a demapper.

   Oversampling would add a pulse shape to match and a sample phase to
   recover. Both are a timing loop's job (`symsync`, `ratesync`) and neither
   changes any answer here, so carrying them would only put a second subject
   in front of the first. */
#define SPS 1
#define SEED 7u
#define HDR_BITS 16u  /* the caller's own header                          */
#define PAY_BITS 240u /* payload                                          */
/* A GENERATED sync field rather than a literal: declaring a `kind` means the
   description PRODUCES the marker, so the receiver rebuilds the same bits
   from the same declaration (`wfm_seq_bits`) instead of holding a second copy
   that can drift. A literal field works too, for a marker you must match.

   LENGTH IS THE PROPERTY THAT MATTERS, because the search is a Hamming
   distance over bits: what makes a marker safe is being unlikely to appear by
   chance anywhere in the window searched, which is 2^-63 here and 2^-31 at
   half the length. (An m-sequence's two-valued autocorrelation is why it
   suits a sample-domain correlator; that is not what this is.)
   `syncword_max_errors_for` puts a number on it — see section 7. */
#define SYNC_BITS 63u
#define SYNC_REG_BITS 6u
#define FRAME_BITS (SYNC_BITS + HDR_BITS + PAY_BITS + WFM_FRAME_CRC_BITS)
#define N_SYMS FRAME_BITS /* one BPSK symbol per bit */
#define TOTAL_BITS (TOTAL / (size_t)SPS)

#define BURST_ON (FRAME_BITS * (unsigned)SPS) /* one frame, exactly       */
#define BURST_OFF 4096u                       /* gap between bursts       */
/* A lead-in, so the first burst does not start at sample 0 and the search has
   somewhere to look before it. */
#define DELAY_SAMPLES 2u
#define N_BURSTS 60u
#define PERIOD ((size_t)(DELAY_SAMPLES + BURST_ON + BURST_OFF))
#define TOTAL (PERIOD * N_BURSTS)

#define SNR_DB 12.0 /* declared per-source SNR, in Es/N0 mode          */
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

  /* Borrowed below, so they outlive every compose call. */
  static uint8_t hdr[HDR_BITS], pay[PAY_BITS];
  for (unsigned i = 0; i < HDR_BITS; i++)
    hdr[i] = (uint8_t)((0x5C5Cu >> (HDR_BITS - 1u - i)) & 1u);
  for (unsigned i = 0; i < PAY_BITS; i++)
    pay[i] = (uint8_t)((i * 7u + 1u) & 1u);

  /* ── 1. the frame ───────────────────────────────────────────────────── */
  printf ("--- 1. The frame this waveform carries ---\n");

  wfm_seq_t sy = { 0 };
  sy.kind      = WFM_SEQ_PN;
  sy.len       = SYNC_BITS;
  sy.reg_bits  = SYNC_REG_BITS;
  sy.seed      = 1u; /* 0 would select 1 anyway; said out loud */
  sy.poly      = 0u; /* 0 selects the maximal-length polynomial for reg_bits */

  wfm_frame_desc_t d;
  wfm_seq_t h = { .kind = WFM_SEQ_LITERAL, .bits = hdr, .len = HDR_BITS };
  wfm_seq_t p = { .kind = WFM_SEQ_LITERAL, .bits = pay, .len = PAY_BITS };
  memset (&d, 0, sizeof d);
  /* The cover names its ends and REACHES the derived field, which is what
     wires that field's producer — so the CRC's position and the fact that a
     CRC fills it are one declaration rather than two that can disagree.
     Naming the ends is also why inserting the sync field AHEAD of them moved
     nothing here: `first_field`/`n_fields` would both have had to shift.
     The sync marker and the header sit outside the cover — a receiver finds
     them before it can check anything. */
  int built
      = wfm_frame_add_field (&d, "sync", &sy, 0u) == 0
        && wfm_frame_add_field (&d, "hdr", &h, 0u) == 1
        && wfm_frame_add_field (&d, "payload", &p, 0u) == 2
        && wfm_frame_add_derived (&d, "crc", WFM_FRAME_CRC_BITS) == 3
        && wfm_frame_add_stage (&d, WFM_STAGE_CRC16, "payload", "crc") == 0;
  check (built, "a generated sync marker, a header, a payload and a derived "
                "CRC-16 over a named span");
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

  /* THE SOURCE, spelled out: these are the fields a framed BPSK burst needs
     and there is no shorter honest way to say it. `= { 0 }` then named
     members, never a positional initialiser list — wfm_source_t carries
     30-odd members and a positional list shifts silently the moment one is
     inserted. */
  wfm_source_t src = { 0 };
  src.type         = WFM_SYNTH_BITS;
  src.payload
      = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .bits = pay, .len = PAY_BITS };
  src.modulation = WFM_BITMOD_BPSK;
  src.sps        = SPS;
  src.snr        = SNR_DB;
  src.snr_mode   = WFM_SNR_ESNO;
  src.seed       = SEED;
  src.frame      = &d; /* borrowed; the description outlives compose */

  /* The naive shape: one segment per burst. It works, and it is sixty copies
     of one fact — change the gap and you change it sixty times. */
  wfm_segment_t *many = calloc (N_BURSTS, sizeof *many);
  if (!many)
    {
      fprintf (stderr, "out of memory\n");
      return 1;
    }
  for (unsigned i = 0; i < N_BURSTS; i++)
    {
      many[i].sources       = &src;
      many[i].n_sources     = 1u;
      many[i].fs            = FS;
      many[i].num_samples   = BURST_ON;
      many[i].off_samples   = BURST_OFF;
      many[i].delay_samples = DELAY_SAMPLES;
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
  one.delay_samples = DELAY_SAMPLES;
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

  /* AND THEY ARE NOT THE SAME WAVEFORM — the reason to reach for `repeats`
     is not brevity. A listed segment is a fresh segment: it restarts the
     source from its declared `seed`, so all sixty draw the SAME noise, which
     is sixty bursts that look independent and are not. `repeats` says "the
     same burst, again": signal fixed across instances, AWGN fresh per
     instance. */
  const size_t period = PERIOD;
  /* Instance k is [delay | on | off], so a burst starts DELAY_SAMPLES in. */
  const size_t burst0 = DELAY_SAMPLES;
  check (memcmp (listed + burst0, listed + burst0 + period,
                 BURST_ON * sizeof *listed)
             == 0,
         "listed: every burst carries IDENTICAL noise — one seed, sixty "
         "fresh segments");
  check (memcmp (declared + burst0, declared + burst0 + period,
                 BURST_ON * sizeof *declared)
             != 0,
         "repeats: each instance draws its own noise, as a burst train "
         "should");
  printf ("  so `repeats` is not shorthand for the listed form — it is the "
          "one\n  that gives sixty INDEPENDENT bursts.\n\n");

  /* ── 3. SNR across the scene ────────────────────────────────────────── */
  printf ("--- 3. Where the declared %.0f dB SNR actually shows up ---\n",
          SNR_DB);

  /* Two estimators, for the two situations a caller is in: `snr_m2m4_db` is
     blind (moments only, nothing known about what was sent), while
     `snr_data_aided_db` strips the known transmitted sign. Both read symbols,
     which at one sample per symbol is the burst as composed. */
  static uint8_t       tx_bits[FRAME_BITS];
  const float complex *tx_sym = declared + burst0; /* a sample IS a symbol */

  const size_t n_tx = wfm_frame_assemble (&d, NULL, tx_bits, FRAME_BITS);
  check (n_tx == FRAME_BITS,
         "the description assembles the bits the burst carries");

  const double snr_blind = snr_m2m4_db (tx_sym, N_SYMS);
  const double snr_da    = snr_data_aided_db (tx_sym, N_SYMS, tx_bits, n_tx);

  /* THE GAP IS NOT SILENCE, and the estimator is what says so: it returns
     NaN for a block with zero power, so a digitally silent gap would fail
     this check rather than quietly reading as a very good SNR. What the gap
     actually holds is the segment's noise floor — the source's AWGN keeps
     running while the signal stops. */
  const double snr_gap = snr_m2m4_db (declared + burst0 + BURST_ON, BURST_OFF);
  check (!isnan (snr_gap),
         "the GAP is not silent — it carries the segment's noise floor");
  check (snr_gap < snr_blind - 6.0,
         "and it is NOISE, not signal — the blind estimator separates them");

  check (fabs (snr_da - SNR_DB) < 1.5,
         "data-aided Es/N0 recovers the SNR the source declared");
  printf ("  declared %.0f dB   data-aided %.2f dB   blind (M2M4) %.2f dB   "
          "gap %.2f dB\n",
          SNR_DB, snr_da, snr_blind, snr_gap);
  printf ("  snr_mode is Es/N0, so the declaration is per transmitted "
          "symbol;\n"
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
     anchor seed, `wfm_plan_at` reproduces `wfm_compose` to the bit
     (wfm_plan.h). A speed ratio asserts nothing useful in its place — it is
     a property of the machine, and it cannot tell "faster" from "did less".
     Byte-identity can. */
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

  check (got == TOTAL, "the consumer read every sample that was written");
  check (memcmp (readback, declared, TOTAL * sizeof *declared) == 0,
         "and cf32 through BLUE is byte-exact — what went in came back");
  printf ("  %zu samples in %.1f ms  (%.1f Msample/s)\n\n", got, t_read * 1e3,
          (double)got / t_read / 1e6);

  /* ── 7. finding the bursts, by their sync marker ────────────────────── */
  printf ("--- 7. Finding each burst by its sync marker ---\n");

  /* Demap the WHOLE record, gaps included: the gaps demap to noise bits,
     which is the point — on a real capture nothing announces a burst, and the
     searcher has to survive the space between them. `mpsk_demap` at m=2 is
     the element-wise inverse of the BPSK mapping the source used. */
  static uint8_t rx_bits[TOTAL_BITS];
  mpsk_demap (readback, TOTAL_BITS, rx_bits, 2);

  /* THE MARKER COMES FROM THE DESCRIPTION, not from a constant here. The
     field declared a generated sequence, so the receiver materialises the
     same bits from the same declaration — one statement of what the marker
     is, and the two ends cannot drift apart. */
  static uint8_t marker[SYNC_BITS];
  check (wfm_seq_bits (&sy, marker, SYNC_BITS) == SYNC_BITS,
         "the receiver rebuilds the marker from the frame's own declaration");

  syncword_state_t *sw = syncword_create (marker, SYNC_BITS);
  if (!sw)
    {
      fprintf (stderr, "syncword_create failed\n");
      return 1;
    }

  /* THE TOLERANCE IS DERIVED, not guessed. `max_errors` is a function of how
     much stream is searched rather than of how long the marker is: the
     search reports the FIRST acceptable offset, so every offset ahead of the
     true one is an independent chance to false-hit first. Ask for a false
     frame probability and let the library answer. */
  const size_t window_bits = PERIOD / (size_t)SPS;
  const double pfa_target  = 1e-6;
  const int    max_err = syncword_max_errors_for (sw, window_bits, pfa_target);
  /* -1 means no tolerance clears the target — the marker is too short for
     the window. It is a REFUSAL, and casting it to the unsigned tolerance
     the search takes would turn "impossible" into "accept anything", which
     is how a receiver locks onto noise and reports sixty happy frames. */
  check (max_err >= 0,
         "a tolerance exists for this marker over this search window");
  if (max_err < 0)
    {
      printf ("  %u-bit marker is too short to search %zu bits at Pfa "
              "%.0e\n\n",
              SYNC_BITS, window_bits, pfa_target);
      syncword_destroy (sw);
      goto done;
    }
  printf ("  %u-bit marker, %zu-bit window, Pfa <= %.0e  ->  tolerate %d "
          "bit error(s)\n",
          SYNC_BITS, window_bits, pfa_target, max_err);
  printf ("  (at that tolerance the actual Pfa is %.2e)\n",
          syncword_pfa (sw, (uint32_t)max_err));

  /* Walk the record one burst at a time, and CHECK each frame the search
     lands on. wfm_frame_desc_crc_ok needs the description and the received
     bits and no payload truth at all, so this is the frame error rate a
     receiver can compute on a capture it did not generate. */
  size_t         pos = 0, at_expected = 0;
  unsigned       crc_pass = 0, inverted = 0;
  static uint8_t frame[FRAME_BITS];

  for (unsigned k = 0; k < N_BURSTS && pos < TOTAL_BITS; k++)
    {
      const syncword_hit_t hit = syncword_find (
          sw, rx_bits + pos, TOTAL_BITS - pos, (uint32_t)max_err);
      if (!hit.found)
        break;
      const size_t at = pos + hit.offset;

      /* The sync field is field 0, so where the marker starts IS where the
         frame starts — in bits, which is the unit the search deals in. */
      if (at == (size_t)k * window_bits + DELAY_SAMPLES / (size_t)SPS)
        at_expected++;
      inverted += (unsigned)(hit.inverted != 0);

      if (at + FRAME_BITS <= TOTAL_BITS)
        {
          /* A hit carries its POLARITY, and a receiver that took the offset
             without it would hand the frame decoder inverted bits that fail
             the CRC for the wrong reason. */
          for (size_t j = 0; j < FRAME_BITS; j++)
            frame[j]
                = hit.inverted ? (uint8_t)!rx_bits[at + j] : rx_bits[at + j];
          if (wfm_frame_desc_crc_ok (&d, frame) == 1)
            crc_pass++;
        }
      bursts++;
      pos = at + FRAME_BITS;
    }
  syncword_destroy (sw);

  check (bursts == N_BURSTS, "every burst is found by its marker");
  check (at_expected == N_BURSTS,
         "and each lands on the exact BIT the scene's geometry predicts");
  check (crc_pass == N_BURSTS,
         "every recovered frame passes its CRC — a frame error rate that "
         "needs no payload truth");
  printf ("  %zu burst(s) found, %zu at the exact predicted bit, "
          "%u inverted, %u/%u CRC pass\n",
          bursts, at_expected, inverted, crc_pass, N_BURSTS);
  printf ("  mean power %.4f over the record\n\n",
          got ? acc / (double)got : 0.0);

done:
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
