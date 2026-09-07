/**
 * async_dsss_pool_demo.c — the multi-emitter pool as ONE object, in C.
 *
 * `AsyncDsssPool` holds the population of the continuous async-DSSS use
 * case (docs/design/async-dsss-receiver.md §8.2): one searcher, `n_slots`
 * hand-off receivers created idle, the assigned table and the event log,
 * behind a single `push()`. The Python twin
 * (`src/doppler/examples/async_dsss_pool_demo.py`) shows the same
 * lifecycle with a figure; this one shows what the binding does FOR you
 * and therefore hides: the lifecycle you manage yourself, who owns the
 * symbol buffer and how big it has to be, and the log you attach and
 * close.
 *
 * **The waveform is not built here.** Each emitter is the shipped
 * `wfm_synth` continuous DSSS -- the code from wfm's own Gold generator,
 * asynchronous BPSK from its own PRBS -- at its own carrier offset, and
 * the noise is the shipped awgn sized from the C/N0. Nothing here spreads
 * a chip, draws a bit or a sigma: a second implementation of any of those
 * would drift from the one that ships.
 *
 * Four sections, each printing a number a reader can check:
 *
 *   §1  Two emitters on one code, two rows apart: each takes exactly one
 *       slot, its seed within the searcher's row of its own Doppler; both
 *       track with code lock; their symbols come out by slot.
 *   §2  One leaves the air. Its receiver reports lost once both lock
 *       flags have been down for the release interval, and the pool
 *       releases the slot -- the other emitter is untouched.
 *   §3  It returns: a new detection into whichever slot is free -- the
 *       one re-assignment the lifecycle permits.
 *   §4  The event log, attached by borrowing: every transition the pool
 *       counted is one appended line, in order.
 *
 * Build:
 *   cmake --build build
 *   ./build/native/examples/async_dsss_pool_demo
 */
#include <async_dsss_pool/async_dsss_pool_core.h>
#include <awgn/awgn_core.h>
#include <complex.h>
#include <dp_event_log/dp_event_log_core.h>
#include <gold/gold_core.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wfm_synth/wfm_synth_core.h>

/* ── geometry: the operating point of design §6.1, at D = 1 ─────────────
 * 1023 chips at 5 Mcps, two samples per chip, 2700 sym/s asynchronous BPSK.
 * The searcher runs epoch by epoch (no code-only window in this stream, so
 * D = 1) over ±6 kHz: a Doppler row is one epoch rate, 4.89 kHz, and two
 * emitters two rows apart are two peaks. The release interval is short
 * so a departure is released inside a second of stream. */
#define SF 1023u
#define SPC 2u
#define TE (SF * SPC)
#define CHIP_RATE 5.0e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define CN0_DBHZ 47.0
#define DU 6000.0
#define LOST_S 0.3
#define N_SLOTS 4u
#define DOPPLER_A 1500.0
#define DOPPLER_B (-3500.0)
#define CHIP0_B 900.0 /* B's code phase at sample 0: its burn-in, chips */

#define CHECK(cond, ...)                                                      \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL: " __VA_ARGS__);                             \
          fputc ('\n', stderr);                                               \
          return 1;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/** @brief One emitter: the shipped continuous-DSSS synth at a carrier
 *  offset, its own PRBS data, clean -- the noise is added once at the sum
 *  so two emitters share one channel's noise. */
static wfm_synth_state_t *
emitter (const uint8_t *code, double doppler_hz, uint32_t seed)
{
  wfm_synth_state_t *syn
      = wfm_synth_create (WFM_SYNTH_DSSS, FS, doppler_hz, WFM_SYNTH_SNR_CLEAN,
                          1, seed, (int)SPC, 15, 0, 0, 0.0);
  if (syn
      && wfm_synth_set_dsss_cont (syn, code, SF, CHIP_RATE / SYM_RATE,
                                  WFM_DSSS_DATA_PRBS, NULL, 0)
             != 0)
    {
      wfm_synth_destroy (syn);
      syn = NULL;
    }
  return syn;
}

/** @brief The slot whose SEED is the emitter at @p doppler_hz -- within
 *  the searcher's row AND within a chip of the emitter's own code phase at
 *  the seed's sample (the synth's chip clock, `chip0` chips in at sample
 *  0) -- or N_SLOTS; and how many slots hold it. The searcher's false
 *  alarms are part of the lifecycle: a noise peak in the same row seeds a
 *  free slot for one release interval, at another phase, so a slot is the
 *  emitter's by both coordinates, never by a count. */
static size_t
slot_of (async_dsss_pool_state_t *p, double doppler_hz, double chip0,
         size_t *count)
{
  size_t found = N_SLOTS, n = 0;
  for (size_t i = 0; i < N_SLOTS; i++)
    {
      async_dsss_pool_slot_t r = async_dsss_pool_status (p, i);
      if (!r.assigned
          || fabs (r.seed_doppler_hz - doppler_hz) > p->acq->doppler_res_hz)
        continue;
      double truth = dp_fmod_pos ((double)r.seed_sample / SPC + chip0, SF);
      double dc    = fabs (r.seed_chip_phase - truth);
      if (dc > SF / 2.0)
        dc = SF - dc;
      if (dc <= 1.0)
        {
          found = i;
          n++;
        }
    }
  *count = n;
  return found;
}

int
main (void)
{
  uint8_t       code[SF];
  gold_state_t *gd = gold_create (934, 350, 567, 73, 10);
  gold_generate (gd, SF, code, SF);
  gold_destroy (gd);

  wfm_synth_state_t *a = emitter (code, DOPPLER_A, 1u);
  wfm_synth_state_t *b = emitter (code, DOPPLER_B, 2u);
  awgn_state_t      *g = awgn_create (
      7u,
      awgn_amplitude_for_snr ((float)(CN0_DBHZ - 10.0 * log10 (FS)), 1.0f));
  CHECK (a && b && g, "the emitters and the noise open");
  /* B starts 900 chips into its code: two emitters at one code phase are
     one peak to the searcher and one row to the pool's zone (§7.1 -- a
     pair the surface cannot tell apart), so they must differ in phase as
     well as in Doppler, as real emitters do. The burn-in is discarded. */
  {
    float complex *burn = malloc ((size_t)CHIP0_B * SPC * sizeof *burn);
    CHECK (burn != NULL, "the burn-in allocates");
    wfm_synth_steps (b, burn, (size_t)CHIP0_B * SPC);
    free (burn);
  }

  /* The pool: the searcher's and the receivers' parameters pass through
     create() untouched; everything is sized here, once. */
  async_dsss_pool_state_t *pool = async_dsss_pool_create (
      code, SF, CHIP_RATE, SYM_RATE, SPC, 2, CN0_DBHZ, 1e-3, 0.9, DU, 1, 0.0,
      4, N_SLOTS, 1, 0.0, LOST_S, 0.0, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false,
      100000);
  CHECK (pool != NULL, "the pool opens");

  /* §4's log, attached before the first push so nothing is missed. It is
     borrowed: the pool never closes it. */
  char path[64];
  (void)snprintf (path, sizeof path, "/tmp/async_dsss_pool_demo_%d.events",
                  (int)getpid ());
  dp_event_log_t *log = dp_event_log_open (path, 0.0);
  CHECK (log != NULL && async_dsss_pool_set_event_log (pool, log) == DP_OK,
         "the event log opens and attaches");

  float complex *sa  = malloc ((size_t)TE * sizeof *sa);
  float complex *sb  = malloc ((size_t)TE * sizeof *sb);
  float complex *x   = malloc ((size_t)TE * sizeof *x);
  size_t         cap = 0; /* the symbol buffer's capacity: known only after
                             the first push, since it follows the block */
  float complex *syms            = NULL;
  size_t         n_syms[N_SLOTS] = { 0 };
  CHECK (sa && sb && x, "the blocks allocate");

  /* One block: both emitters (b's gain is its visibility), plus noise. */
#define BLOCK(gain_b)                                                         \
  do                                                                          \
    {                                                                         \
      wfm_synth_steps (a, sa, TE);                                            \
      wfm_synth_steps (b, sb, TE);                                            \
      awgn_generate (g, TE, x, TE);                                           \
      for (size_t i = 0; i < TE; i++)                                         \
        x[i] += sa[i] + (float)(gain_b) * sb[i];                              \
    }                                                                         \
  while (0)

  /* ── §1 ───────────────────────────────────────────────────────────── */
  printf ("§1  two emitters, one code, %.0f and %.0f Hz\n", DOPPLER_A,
          DOPPLER_B);
  const size_t on_blocks = (size_t)(1.0 * FS / TE); /* one second */
  for (size_t k = 0; k < on_blocks; k++)
    {
      BLOCK (1.0);
      (void)async_dsss_pool_push (pool, x, TE);
      if (!syms)
        {
          /* Sized by the pool from the largest block it has seen; a
             caller reads it after the first push and keeps it. */
          cap  = async_dsss_pool_symbols_max_out (pool);
          syms = malloc (cap * sizeof *syms);
          CHECK (syms != NULL, "the symbol buffer allocates");
        }
      for (size_t i = 0; i < N_SLOTS; i++)
        n_syms[i] += async_dsss_pool_symbols (pool, i, syms, cap);
    }
  size_t na, nb;
  size_t slot_a = slot_of (pool, DOPPLER_A, 0.0, &na);
  size_t slot_b = slot_of (pool, DOPPLER_B, CHIP0_B, &nb);
  CHECK (na == 1 && nb == 1 && slot_a != slot_b,
         "each emitter holds exactly one slot (A %zu, B %zu)", na, nb);
  async_dsss_pool_slot_t ra = async_dsss_pool_status (pool, slot_a);
  async_dsss_pool_slot_t rb = async_dsss_pool_status (pool, slot_b);
  printf ("    A: slot %zu, seeded at %.3f s at %+.0f Hz, now %+.1f Hz, "
          "state %d, code lock %d, symbol lock %d, %zu symbols so far\n",
          slot_a, (double)ra.seed_sample / FS, ra.seed_doppler_hz,
          ra.doppler_hz, ra.state, ra.code_locked, ra.locked, n_syms[slot_a]);
  printf ("    B: slot %zu, seeded at %.3f s at %+.0f Hz, now %+.1f Hz, "
          "state %d, code lock %d, symbol lock %d, %zu symbols so far\n",
          slot_b, (double)rb.seed_sample / FS, rb.seed_doppler_hz,
          rb.doppler_hz, rb.state, rb.code_locked, rb.locked, n_syms[slot_b]);
  CHECK (ra.state == ASYNC_DSSS_RX_TRACKING && ra.code_locked
             && rb.state == ASYNC_DSSS_RX_TRACKING && rb.code_locked,
         "both track their emitter with code lock");
  CHECK (fabs (ra.doppler_hz - DOPPLER_A) < 100.0
             && fabs (rb.doppler_hz - DOPPLER_B) < 100.0,
         "the live Doppler is each emitter's own");
  CHECK (n_syms[slot_a] > 1000 && n_syms[slot_b] > 1000,
         "symbols come out by slot");
  CHECK (pool->dropped == 0, "nothing dropped with slots to spare");

  /* ── §2 ───────────────────────────────────────────────────────────── */
  printf ("§2  B leaves the air\n");
  const size_t off_blocks  = (size_t)(3.0 * LOST_S * FS / TE);
  size_t       released_at = 0;
  for (size_t k = 0; k < off_blocks; k++)
    {
      BLOCK (0.0);
      (void)async_dsss_pool_push (pool, x, TE);
      if (!released_at && !async_dsss_pool_status (pool, slot_b).assigned)
        released_at = k + 1;
    }
  CHECK (released_at > 0, "B's slot is released once it is gone");
  printf ("    released %.0f ms after the departure (the interval is %.0f "
          "ms); A still tracking: %d\n",
          (double)released_at * TE / FS * 1e3, LOST_S * 1e3,
          async_dsss_pool_status (pool, slot_a).state
              == ASYNC_DSSS_RX_TRACKING);
  CHECK ((double)released_at * TE / FS >= LOST_S,
         "not before the release interval has run");
  CHECK (async_dsss_pool_status (pool, slot_a).state == ASYNC_DSSS_RX_TRACKING,
         "A is untouched by B's release");

  /* ── §3 ───────────────────────────────────────────────────────────── */
  printf ("§3  B returns\n");
  for (size_t k = 0; k < on_blocks; k++)
    {
      BLOCK (1.0);
      (void)async_dsss_pool_push (pool, x, TE);
    }
  size_t slot_b2 = slot_of (pool, DOPPLER_B, CHIP0_B, &nb);
  CHECK (nb == 1 && slot_b2 < N_SLOTS,
         "B is a new detection into a free slot");
  printf ("    B: slot %zu (was %zu), state %d; A: still slot %zu, %d\n",
          slot_b2, slot_b, async_dsss_pool_status (pool, slot_b2).state,
          slot_a, async_dsss_pool_status (pool, slot_a).state);
  CHECK (async_dsss_pool_status (pool, slot_b2).state
             == ASYNC_DSSS_RX_TRACKING,
         "and tracks again");

  /* ── §4 ───────────────────────────────────────────────────────────── */
  printf ("§4  the event log\n");
  CHECK (dp_event_log_close (log) == DP_OK, "the log closes");
  CHECK (dp_event_log_count (log) == pool->events,
         "every transition the pool counted reached the log (%zu of %llu)",
         dp_event_log_count (log), (unsigned long long)pool->events);
  static const char *const want[] = { "\"seeded\"", "\"tracking\"", "\"lost\"",
                                      "\"released\"", "\"seeded\"" };
  FILE                    *f      = fopen (path, "r");
  size_t                   found  = 0;
  char                     line[DP_EVENT_LOG_LINE_MAX];
  CHECK (f != NULL, "the flat file reads back");
  while (found < 5 && fgets (line, sizeof line, f))
    if (strstr (line, want[found]))
      found++;
  fclose (f);
  printf ("    %zu lines; seeded -> tracking -> lost -> released -> seeded, "
          "in order: %s\n",
          dp_event_log_count (log), found == 5 ? "yes" : "NO");
  CHECK (found == 5, "the labels are the design's, in order");
  (void)async_dsss_pool_set_event_log (pool, NULL);
  dp_event_log_destroy (log);
  remove (path);

  free (syms);
  free (x);
  free (sb);
  free (sa);
  async_dsss_pool_destroy (pool);
  awgn_destroy (g);
  wfm_synth_destroy (b);
  wfm_synth_destroy (a);
  printf ("OK\n");
  return 0;
}
