/**
 * @file async_dsss_pool_soak.c
 * @brief The pool's lifecycle soak: emitters arriving and leaving at their
 *        own Dopplers, each acquired once, assigned once, tracked until it
 *        leaves, released by the rule, re-acquired on return -- and what
 *        each of those took.
 *
 * The continuous async-DSSS design (docs/design/async-dsss-receiver.md
 * §6.1, §12 step 7) has one searcher and a pool of hand-off receivers
 * holding a population that changes about once a minute: an emitter comes
 * into view at whatever point of its frame it has reached, is detected in
 * its next code-only window, seeded into a free receiver, tracked by that
 * receiver alone until its own loss decision, released, and -- back on the
 * air -- is a new detection into whichever slot is free. `AsyncDsssPool`
 * (§8.2, §12.13) is the object that holds that lifecycle; this harness is
 * the measurement that certifies it, on the shipped stimulus at the
 * operating point, with nothing built by hand: no chip, no bit, no sigma,
 * no dilation, no seed, no score of its own beyond reading the pool's
 * status against the stimulus's truth.
 *
 * Stimulus. Per emitter, one `wfm_synth` continuous DSSS -- Gold-1023 at
 * 5 Mcps, two samples per chip, asynchronous BPSK PRBS data at 2700 sym/s,
 * the frame's window of 450 code-only symbols in every 4950
 * (`wfm_synth_set_dsss_window`) -- through the shipped `doppler_channel`
 * at its own Doppler, drawn uniformly within ±20 ppm of a 2.5 GHz carrier
 * with no rate (offset and rate are two conditions, never together; the
 * rate is §12.9's). Every synth runs for the whole soak from a random
 * burn-in of up to one frame, so an emitter appears at a random frame
 * phase and nothing in the source restarts (§6.1): visibility is a gain of
 * 1 or 0 at the sum. Noise from the shipped awgn at the receiver, sized by
 * awgn_amplitude_for_snr() from the C/N0, the same for every emitter.
 * Emitter 0 is always on; the others alternate on-times drawn uniformly in
 * [on_min, on_max] and off-times in [off_min, off_max], the first arrival
 * within [0, off_min] -- the design's 5 to 15 minutes scaled to what a
 * harness can run, and `on_max` is also the pool's maximum on-air time, so
 * the always-on emitter exercises the on-time release and the
 * re-acquisition it leads to. The off-times exceed the release interval,
 * so a departed emitter is released before it returns.
 *
 * The pool: the operating point of §6.1 -- `code_only_epochs` 813 so the
 * coherent depth is D = 154 against a 500 Hz/s rate bound, ±50 kHz, a peak
 * list of 16, twelve slots, the carrier told, a 2 s release interval, the
 * threads the machine has -- fed one epoch (2046 samples) at a time, with
 * an event log attached.
 *
 * Score. An emitter's slot is the one whose SEED is the emitter's -- its
 * chip phase within a chip of the stimulus's truth at the seed's sample
 * and its Doppler within one native tile (1/T_epoch), the way
 * test_async_dsss_pool_core's slot_of() keys it: a tile rather than the
 * block searcher's row because the emitter's own data blocks seed it too,
 * hundreds of Hz off (§12.7, §12.14), and the receiver pulls in from
 * there. Never a count of slots: the searcher's false alarms are part of
 * the lifecycle (§12.13) and each costs a free slot for one release
 * interval. The truth is the synth's own clock through the channel's
 * documented mapping: output sample k carries the input at
 * `k (1 + d) - delay`, plus the burn-in, over the samples per chip,
 * folded on the code. Per on-air stint: the time from arrival to a slot
 * holding it (a seed, or -- back inside its own receiver's release
 * interval -- that receiver re-locking, the recovered assignment of
 * §12.3) and to tracking, the seed's error, the fraction of blocks tracked
 * with code lock while held, releases while on the air (a `lost` one is a
 * FALSE release; an `on_time` one is the rule), blocks on which two slots
 * held it and on how many both tracked with code lock (a double
 * assignment; one receiver that has lost code lock while a second seeds
 * is the recovery), and -- after departure -- the release latency. The
 * run's counts: stints missed, false releases, releases later than two
 * intervals plus half a second (one false re-lock on noise restarts the
 * clock once, §12.15; the count past one interval is reported), double
 * assignments (#1264's and #1265's until the Dll's looks and the
 * refine's dwell were fixed, §12.15, §12.16),
 * seeds matching no emitter, stints that waited for a slot with the pool
 * full of emitters, the most slots ever assigned, and the log's events by
 * label, which must number what the pool counted.
 *
 * Usage:
 *   validate_async_dsss_pool_soak            full soak: ten emitters,
 *                                            SWEEP_S seconds, at 45 and
 *                                            40 dB-Hz; the lifecycle's
 *                                            expectations asserted
 *   validate_async_dsss_pool_soak --check    two emitters, CHECK_S seconds
 *                                            at 45 dB-Hz: the always-on
 *                                            one released for its on-time
 *                                            and re-acquired, the other
 *                                            leaving, released, returning
 *                                            and re-acquired
 *   ... --emit                               CSV blocks on stdout for the
 *                                            report's validator, beside
 *                                            the tables: `# stints` and
 *                                            `# totals` per run (the
 *                                            validator runs `--check
 *                                            --emit`: the regression
 *                                            subset is its run, on every
 *                                            push)
 *   ... --budget                             the whole population as one
 *                                            run (§12 step 8): the shipped
 *                                            DDC from 13 MSa/s in front of
 *                                            the pool, the time inside
 *                                            both as a fraction of real
 *                                            time at 13 and 30 MSa/s,
 *                                            beside the count tracked
 *   ... --duration S                         the sweep's length per C/N0
 *                                            (SWEEP_S unless given): the
 *                                            duration requirement of §5.1
 *                                            run as long as the machine
 *                                            allows -- the heap after the
 *                                            warm-up is asserted flat
 *                                            (under glibc the process
 *                                            re-execs itself once with
 *                                            GLIBC_TUNABLES turning the
 *                                            tcache off, so the count is
 *                                            the pool's, not the cache's)
 *   ... --refine-margin DB                   the receivers' refine design
 *                                            margin (the pool's 14 dB
 *                                            unless given): the dwell it
 *                                            sizes is #1265's axis
 *   ... --events DIR                         keep each run's event log as
 *                                            DIR/pool_soak_<cn0>.events
 *                                            (otherwise a temporary file,
 *                                            removed after it is read)
 */
#include "async_dsss_pool/async_dsss_pool_core.h"
#include "awgn/awgn_core.h"
#include "clib_common.h"
#include "ddc/ddc_core.h"
#include "doppler_channel/doppler_channel_core.h"
#include "dp_event_log/dp_event_log_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include "gold/gold_core.h"
#include "wfm_synth/wfm_synth_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#ifdef __GLIBC__
#include <malloc.h>
#endif

#define SF 1023u
#define SPC 2u
#define TE (SF * SPC) /* one epoch, the feed block: 2046 samples */
#define CHIP_RATE 5.0e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define CPS (CHIP_RATE / SYM_RATE) /* chips per symbol, 1851.85       */
#define W_SYM 450u                 /* code-only symbols per frame     */
#define F_SYM 4950u                /* frame, symbols (§5.4)           */
#define FRAME_S ((double)F_SYM / SYM_RATE)
#define CARRIER_HZ 2.5e9 /* the channel's carrier: ppm -> Hz, and the aid */
#define MAX_PPM 20.0     /* SPEC's Doppler: ±50 kHz at the carrier        */
#define DU (MAX_PPM * 1e-6 * CARRIER_HZ) /* the searcher's span, ±50 kHz */

/* The pool at the operating point (§6.1, §8.2). */
#define CODE_ONLY_EPOCHS 813u /* the window in whole epochs: D = 154   */
#define DOPPLER_RATE 500.0    /* Hz/s, the depth's bound               */
#define MAX_PEAKS 16u
#define N_SLOTS 12u
#define LOST_CONFIRM_S 2.0
#define THREADS 0 /* the machine's cores */

/* The two soaks. */
#define SWEEP_S 120.0
#define SWEEP_EMIT 10u
#define CHECK_S 16.0
#define CHECK_EMIT 2u

/* --budget: the front end's group delay, output samples at FS, measured
   once with an impulse (measure_chain_delay); 0 without the front end. */
static double g_chain_delay = 0.0;

typedef struct
{
  double   cn0_dbhz;
  size_t   n_emit;
  double   duration_s;
  double   on_min_s, on_max_s;
  double   off_min_s, off_max_s;
  double   max_on_s; /* the pool's maximum on-air time, >= on_max_s */
  uint32_t seed;
} cfg_t;

/* One on-air stint of one emitter, and what the pool did with it. */
typedef struct
{
  uint64_t t_on, t_off;   /* samples; t_off = 0 while still on         */
  int      truncated;     /* cut by the run's end: no release to score  */
  int      n_assign;      /* seeds during the stint                     */
  uint64_t t_assign;      /* the first seed's sample                    */
  uint64_t t_held;        /* first block a slot held it: a seed, or the
                             receiver it had before re-locking on it     */
  double   seed_err_hz;   /* the first seed against the truth           */
  double   seed_err_chip; /*                                            */
  uint64_t t_track;       /* first block reported tracking              */
  size_t   held_blocks;   /* blocks a slot held it                      */
  size_t   trk_blocks;    /* of them, tracking with code lock           */
  size_t   sym_blocks;    /* of them, tracking with symbol lock: the
                             carrier pulled in (#1265)                    */
  size_t on_blocks;       /* blocks from the first tracking to t_off    */
  int    false_rel;       /* `lost` releases while on the air           */
  int    on_time_rel;     /* `on_time` releases while on the air        */
  size_t dbl;             /* blocks two slots held it                   */
  size_t dbl_locked;      /* of them, both tracking with code lock: a
                             double assignment, not a recovery          */
  int      waited;        /* hits dropped while it had no slot          */
  uint64_t t_rel;         /* the release after t_off, or 0              */
  int      rel_on_time;   /* that release was the on-time rule's        */
} stint_t;

typedef struct
{
  double                   ppm, doppler_hz, delay;
  uint64_t                 burn;
  wfm_synth_state_t       *syn;
  doppler_channel_state_t *ch;
  float complex           *sig;  /* TE, the synth's block                 */
  float complex           *fifo; /* the channel's output, carried         */
  size_t                   pend;
  float complex           *blk; /* TE: this block's received samples     */
  int                      on;
  uint64_t                 toggle; /* the sample `on` flips at next       */
  uint32_t                 rng;    /* the schedule's own draws            */
  stint_t                 *st;
  size_t                   n_st, cap_st;
  size_t                   slot;     /* the slot holding it now, or N_SLOTS  */
  uint64_t                 slot_age; /* that slot's assigned_samples      */
} emitter_t;

typedef struct
{
  const cfg_t *cfg;
  emitter_t   *e;
} produce_ctx_t;

static void
gold_1023 (uint8_t *code)
{
  gold_state_t *gd = gold_create (934, 350, 567, 73, 10);
  gold_generate (gd, SF, code, SF);
  gold_destroy (gd);
}

static double
draw (uint32_t *st, double lo, double hi)
{
  return lo + (hi - lo) * dp_uni (st);
}

static uint64_t
samples (double s)
{
  return (uint64_t)llround (s * FS);
}

/* The emitter's code phase at stream sample `k`, by the synth's own clock
   through the channel's documented mapping (doppler_channel_core.h): output
   k carries the input at `k (1 + d) - delay`; the burn-in came first. */
static double
truth_chip (const emitter_t *e, uint64_t k)
{
  const double n_in = ((double)k - g_chain_delay) * (1.0 + e->ppm * 1e-6)
                      - e->delay + (double)e->burn;
  return dp_fmod_pos (n_in / (double)SPC, (double)SF);
}

static double
chip_err (double a, double b)
{
  double d = dp_fmod_pos (a - b, (double)SF);
  return d > SF / 2.0 ? d - (double)SF : d;
}

static stint_t *
open_stint (emitter_t *e, uint64_t t_on)
{
  if (e->n_st == e->cap_st)
    {
      e->cap_st = e->cap_st ? 2 * e->cap_st : 8;
      e->st     = dp_xrealloc (e->st, e->cap_st * sizeof *e->st);
    }
  stint_t *s = &e->st[e->n_st++];
  memset (s, 0, sizeof *s);
  s->t_on = t_on;
  return s;
}

static int
make_emitter (emitter_t *e, size_t k, const cfg_t *cfg, const uint8_t *code)
{
  memset (e, 0, sizeof *e);
  /* Adjacent xorshift seeds share their first draws; the golden-ratio
     mix spreads the emitters' streams apart (dp_rng_test.h's note). */
  e->rng = (cfg->seed + (uint32_t)k + 1u) * 0x9e3779b9u;
  for (int w = 0; w < 4; w++)
    (void)dp_xs32 (&e->rng);
  e->ppm        = draw (&e->rng, -MAX_PPM, MAX_PPM);
  e->doppler_hz = e->ppm * 1e-6 * CARRIER_HZ;
  e->burn       = (uint64_t)(draw (&e->rng, 0.0, 1.0) * FRAME_S * FS);
  e->syn = wfm_synth_create (WFM_SYNTH_DSSS, FS, 0.0, WFM_SYNTH_SNR_CLEAN, 1,
                             e->rng, (int)SPC, 15, 0, 0, 0.0);
  DP_REQUIRE_MSG (e->syn != NULL, "the emitter's synth opens");
  (void)wfm_synth_set_dsss_cont (e->syn, code, SF, CPS, WFM_DSSS_DATA_PRBS,
                                 NULL, 0);
  (void)wfm_synth_set_dsss_window (e->syn, W_SYM, F_SYM);
  e->ch = doppler_channel_create (FS, CARRIER_HZ, e->ppm, 0.0);
  DP_REQUIRE_MSG (e->ch != NULL, "the emitter's channel opens");
  e->delay = doppler_channel_get_delay_samples (e->ch);
  e->sig   = dp_xmalloc (TE * sizeof *e->sig);
  e->fifo  = dp_xmalloc ((size_t)4 * TE * sizeof *e->fifo);
  e->blk   = dp_xmalloc (TE * sizeof *e->blk);
  /* The burn-in: the synth runs on from a random point of its frame; the
     channel starts there too (the received clock has no history). */
  for (uint64_t left = e->burn; left;)
    {
      size_t n = left < TE ? (size_t)left : TE;
      wfm_synth_steps (e->syn, e->sig, n);
      left -= n;
    }
  e->slot = N_SLOTS;
  if (k == 0)
    {
      e->on     = 1;
      e->toggle = UINT64_MAX;
      (void)open_stint (e, 0);
    }
  else
    {
      e->on     = 0;
      e->toggle = samples (draw (&e->rng, 0.0, cfg->off_min_s));
    }
  return 0;
}

static void
destroy_emitter (emitter_t *e)
{
  free (e->st);
  free (e->blk);
  free (e->fifo);
  free (e->sig);
  doppler_channel_destroy (e->ch);
  wfm_synth_destroy (e->syn);
}

/* One received block per emitter, on or off: the synth through the
   channel until a whole epoch is pending, then that epoch. Independent per
   emitter, so it fans. */
static void
produce_one (size_t i, void *ctx)
{
  emitter_t *e = ((produce_ctx_t *)ctx)->e + i;
  while (e->pend < TE)
    {
      wfm_synth_steps (e->syn, e->sig, TE);
      e->pend += doppler_channel_execute (e->ch, e->sig, TE, e->fifo + e->pend,
                                          2 * TE);
    }
  memcpy (e->blk, e->fifo, TE * sizeof *e->blk);
  e->pend -= TE;
  memmove (e->fifo, e->fifo + TE, e->pend * sizeof *e->fifo);
}

/* The schedule at block start `now`: flip whoever is due, open or close
   the stint. */
static void
schedule (emitter_t *e, const cfg_t *cfg, uint64_t now)
{
  if (now < e->toggle)
    return;
  e->on = !e->on;
  if (e->on)
    {
      (void)open_stint (e, now);
      e->toggle = now + samples (draw (&e->rng, cfg->on_min_s, cfg->on_max_s));
    }
  else
    {
      e->st[e->n_st - 1].t_off = now;
      e->toggle
          = now + samples (draw (&e->rng, cfg->off_min_s, cfg->off_max_s));
    }
}

typedef struct
{
  size_t n_stints, scored, missed, false_rel, late_rel, on_time_rel;
  size_t over_rel; /* releases past the interval + 0.5 s but within the
                      bound: one false re-lock on noise restarted the
                      clock (0.004 per s, §12.15)                     */
  size_t   dbl, dbl_locked, reassign, off_air_assign, max_assigned;
  size_t   waited, false_alarms, relocked;
  size_t   clock_restarts; /* release clocks that restarted */
  double   wait_max;
  uint64_t dropped, events;
  size_t   held_blocks, trk_blocks, sym_blocks, on_blocks;
  double   assign_min, assign_sum, assign_max; /* arrival -> seed, s   */
  double   track_min, track_sum, track_max;    /* arrival -> tracking  */
  double   rel_min, rel_sum, rel_max;          /* departure -> release */
  size_t   n_assign, n_track, n_rel;
  double   seed_hz_max, seed_chip_max;
  size_t   log_seeded, log_tracking, log_degrade, log_lost, log_rel_lost,
      log_rel_on_time, log_dropped, log_lines;
  double heap_base, heap_max, heap_end; /* bytes: the baseline is taken
                                           after the warm-up and again at
                                           every slot's first tracking --
                                           a receiver builds its chains
                                           on its first seed and hand-over
                                           -- so a step there is a first
                                           use, and what remains is growth
                                           with time                     */
  double heap_step_max;                 /* the largest such first-use step */
  double rss_base_kib, rss_end_kib;     /* resident high-water mark      */
  double warm_s;
  size_t slots_used;              /* slots that have tracked once   */
  double ddc_s, push_s, signal_s; /* --budget: seconds inside each  */
} totals_t;

static void
acc (double x, double *mn, double *sum, double *mx, size_t *n)
{
  if (*n == 0 || x < *mn)
    *mn = x;
  if (*n == 0 || x > *mx)
    *mx = x;
  *sum += x;
  (*n)++;
}

/* The log's flat file, read back: one JSON object per line, the label as
   `"core:label":"<label>"` and the release's reason as
   `"doppler:reason":"<reason>"`. */
static int
count_log (const char *path, totals_t *t)
{
  FILE *f = fopen (path, "r");
  DP_REQUIRE_MSG (f != NULL, "the event log's flat file reads back");
  char line[DP_EVENT_LOG_LINE_MAX];
  while (fgets (line, sizeof line, f))
    {
      t->log_lines++;
      if (strstr (line, "\"core:label\":\"seeded\""))
        t->log_seeded++;
      else if (strstr (line, "\"core:label\":\"tracking\""))
        t->log_tracking++;
      else if (strstr (line, "\"core:label\":\"degrade\""))
        t->log_degrade++;
      else if (strstr (line, "\"core:label\":\"lost\""))
        t->log_lost++;
      else if (strstr (line, "\"core:label\":\"dropped\""))
        t->log_dropped++;
      else if (strstr (line, "\"core:label\":\"released\""))
        {
          if (strstr (line, "\"doppler:reason\":\"on_time\""))
            t->log_rel_on_time++;
          else
            t->log_rel_lost++;
        }
    }
  fclose (f);
  return 0;
}

static const char *g_events_dir = NULL;
/* --duration S: the sweep's length per C/N0 -- the duration requirement
   of section 5.1 run as long as the machine allows; nothing in the pool
   may grow with time. */
static double g_duration_s = 0.0;
/* --budget: the whole population as one run (section 12 step 8, section
   6.4): the summed stimulus is carried up to the front end's 13 MSa/s
   (untimed -- it is the stimulus), then the shipped DDC brings it back
   to two samples per chip on the polyphase arbitrary path, the one the
   ratio forces, and the pool takes the block; the time inside the DDC
   and inside push() is the population's cost, reported as a fraction of
   real time at 13 MSa/s and, the same chain fed 2.3 times faster, at the
   30 MSa/s floor. */
static int g_budget = 0;
/* --emit: CSV blocks on stdout for the validator that renders the report
   (src/doppler/dsss/tests/validation/async_dsss_pool/validate.py):
   `# stints` one row per stint, `# totals` one row per run. The tables
   above are for a reader; these are for the gate. */
static int g_emit = 0;
#define DDC_IN_RATE 13.0e6
/* The front end's group delay, output samples at FS, measured once with
   an impulse through a throwaway copy of the chain: the score's truth is
   the synth's clock, and in budget mode the pool's stream is that clock
   delayed by the two resamplers. */

static double
measure_chain_delay (void)
{
  RateConverter_state_t *up  = RateConverter_create (DDC_IN_RATE / FS, 0);
  ddc_state_t           *ddc = ddc_create (0.0, FS / DDC_IN_RATE);
  DP_REQUIRE_MSG (up && ddc, "the front end opens for its delay");
  const size_t   n = 8 * TE, k0 = 4 * TE;
  float complex *x    = dp_xcalloc (n, sizeof *x);
  size_t         ucap = RateConverter_execute_max_out (up);
  float complex *u    = dp_xmalloc (ucap * sizeof *u);
  size_t         dcap = ddc_execute_max_out (ddc, ucap);
  float complex *y    = dp_xmalloc (dcap * sizeof *y);
  x[k0]               = 1.0f;
  size_t total = 0, best = 0;
  float  bmag = 0.0f;
  for (size_t pos = 0; pos < n; pos += TE)
    {
      size_t nu = RateConverter_execute (up, x + pos, TE, u, ucap);
      size_t nd = ddc_execute (ddc, u, nu, y, dcap);
      for (size_t i = 0; i < nd; i++)
        if (cabsf (y[i]) > bmag)
          {
            bmag = cabsf (y[i]);
            best = total + i;
          }
      total += nd;
    }
  free (y);
  free (u);
  free (x);
  ddc_destroy (ddc);
  RateConverter_destroy (up);
  return (double)best - (double)k0;
}

static double
now_s (void)
{
  struct timespec ts;
  clock_gettime (CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* The heap the process holds, bytes: glibc's own count of in-use arena
   and mmap bytes where it exists, else the resident high-water mark
   (which can only rise, so growth reads the same way). */
static double
heap_bytes (void)
{
#ifdef __GLIBC__
  struct mallinfo2 mi = mallinfo2 ();
  return (double)mi.uordblks + (double)mi.hblkhd;
#else
  struct rusage ru;
  getrusage (RUSAGE_SELF, &ru);
  return (double)ru.ru_maxrss * 1024.0;
#endif
}

static double
rss_kib (void)
{
  struct rusage ru;
  getrusage (RUSAGE_SELF, &ru);
  return (double)ru.ru_maxrss;
}
/* --refine-margin: the receivers' refine_design_margin_db, the pool's
   default of 14 dB unless given -- the dwell it sizes is #1265's axis. */
static double g_refine_margin_db = 14.0;

static int
run_soak (const cfg_t *cfg, const uint8_t *code, int trace, double late_s,
          totals_t *t)
{
  memset (t, 0, sizeof *t);
  t->warm_s    = cfg->duration_s * 0.2 < 6.0 ? 6.0 : cfg->duration_s * 0.2;
  emitter_t *e = dp_xcalloc (cfg->n_emit, sizeof *e);
  for (size_t k = 0; k < cfg->n_emit; k++)
    DP_REQUIRE (make_emitter (&e[k], k, cfg, code) == 0);
  awgn_state_t *g
      = awgn_create (cfg->seed * 7919u + 1u,
                     awgn_amplitude_for_snr (
                         (float)(cfg->cn0_dbhz - 10.0 * log10 (FS)), 1.0f));
  async_dsss_pool_state_t *p = async_dsss_pool_create (
      code, SF, CHIP_RATE, SYM_RATE, SPC, 2, cfg->cn0_dbhz, 1e-3, 0.9, DU,
      CODE_ONLY_EPOCHS, DOPPLER_RATE, MAX_PEAKS, N_SLOTS, THREADS, CARRIER_HZ,
      LOST_CONFIRM_S, cfg->max_on_s, 4, 8, 0, 0.5, 4, g_refine_margin_db, 64,
      8, false, 100000);
  DP_REQUIRE_MSG (g && p, "the noise and the pool open");
  char path[256];
  if (g_events_dir)
    (void)snprintf (path, sizeof path, "%s/pool_soak_%.0f.events",
                    g_events_dir, cfg->cn0_dbhz);
  else
    (void)snprintf (path, sizeof path, "/tmp/dp_pool_soak_%d_%.0f.events",
                    (int)getpid (), cfg->cn0_dbhz);
  dp_event_log_t *log = dp_event_log_open (path, 0.0);
  DP_REQUIRE_MSG (log != NULL, "the event log opens");
  DP_REQUIRE (async_dsss_pool_set_event_log (p, log) == DP_OK);
  dp_pool_t    *fan = dp_pool_create (THREADS);
  produce_ctx_t ctx = { cfg, e };

  printf ("  %zu emitters, %.0f s at %.0f dB-Hz; D = %zu (%.1f Hz rows), "
          "%d threads; on [%.1f, %.1f] s, off [%.1f, %.1f] s, the pool's "
          "maximum on-air time %.0f s; refine margin %.0f dB\n",
          cfg->n_emit, cfg->duration_s, cfg->cn0_dbhz, p->acq->coherent_bins,
          p->acq->doppler_res_hz, dp_pool_threads (fan), cfg->on_min_s,
          cfg->on_max_s, cfg->off_min_s, cfg->off_max_s, cfg->max_on_s,
          g_refine_margin_db);
  for (size_t k = 0; k < cfg->n_emit; k++)
    printf ("    emitter %zu: %+.2f ppm (%+.0f Hz), burn-in %.3f s%s\n", k,
            e[k].ppm, e[k].doppler_hz, (double)e[k].burn / FS,
            k == 0 ? ", always on" : "");

  float complex         *x      = dp_xmalloc (TE * sizeof *x);
  float complex         *nz     = dp_xmalloc (TE * sizeof *nz);
  RateConverter_state_t *up     = NULL;
  ddc_state_t           *ddc    = NULL;
  float complex         *up_buf = NULL, *ddc_buf = NULL;
  size_t                 up_cap = 0, ddc_cap = 0;
  if (g_budget)
    {
      up  = RateConverter_create (DDC_IN_RATE / FS, 0);
      ddc = ddc_create (0.0, FS / DDC_IN_RATE);
      DP_REQUIRE_MSG (up && ddc, "the front end opens");
      up_cap  = RateConverter_execute_max_out (up);
      up_buf  = dp_xmalloc (up_cap * sizeof *up_buf);
      ddc_cap = ddc_execute_max_out (ddc, up_cap);
      ddc_buf = dp_xmalloc (ddc_cap * sizeof *ddc_buf);
    }
  const uint64_t n_blocks    = samples (cfg->duration_s) / TE;
  const uint64_t trace_every = trace ? samples (1.0) / TE : 0;
  /* A seed is an emitter's at its code phase within a chip and its
     Doppler within one native tile (1/T_epoch): the searcher's aligned
     block reports the emitter within a row, but its data blocks report it
     hundreds of Hz off (section 12.7's smeared copy, section 12.14), and
     both seed a receiver that pulls in. Within a tile and a chip two
     emitters are one peak on this surface (section 7.1). */
  const double tile_hz
      = p->acq->doppler_res_hz * (double)p->acq->coherent_bins;
  size_t   owner[N_SLOTS]; /* the emitter each slot's seed is, or n_emit */
  uint64_t owner_seed[N_SLOTS]; /* the seed the owner was decided for      */
  for (size_t i = 0; i < N_SLOTS; i++)
    {
      owner[i]      = cfg->n_emit;
      owner_seed[i] = UINT64_MAX;
    }
  size_t held[64]; /* slots holding each emitter this block (n_emit <= 64) */
  /* The release clock's restarts (trace): a slot whose both-down clock
     ran and then read lower had a flag come back for a block. */
  uint64_t prev_down[N_SLOTS];
  memset (prev_down, 0, sizeof prev_down);
  int slot_tracked[N_SLOTS]; /* has tracked at least once (chains built) */
  memset (slot_tracked, 0, sizeof slot_tracked);
  /* Code-phase crossings (trace): two on-air emitters within two chips of
     each other put one's full peak through the other's prompt correlator
     for as long as the crossing lasts -- seconds when their Dopplers are
     close, since the relative chip rate is the Doppler difference over
     the carrier times the chip rate. Logged on entry and exit with the
     Doppler difference, to be read against the receivers' losses
     (#1265). */
  int near[64][64];
  memset (near, 0, sizeof near);
  DP_REQUIRE (cfg->n_emit <= 64);

  for (uint64_t b = 0; b < n_blocks; b++)
    {
      const uint64_t now = b * TE;
      for (size_t k = 0; k < cfg->n_emit; k++)
        schedule (&e[k], cfg, now);
      dp_pool_run (fan, cfg->n_emit, produce_one, &ctx);
      awgn_generate (g, TE, nz, TE);
      memcpy (x, nz, TE * sizeof *x);
      for (size_t k = 0; k < cfg->n_emit; k++)
        if (e[k].on)
          for (size_t i = 0; i < TE; i++)
            x[i] += e[k].blk[i];
      const uint64_t       drops_before = p->dropped;
      const float complex *blk_in       = x;
      size_t               blk_n        = TE;
      if (g_budget)
        {
          /* Up to the front end's rate (the stimulus's cost, untimed),
             then the DDC back down (timed): the block the pool takes is
             the DDC's, a sample or so either side of an epoch. */
          size_t n_up     = RateConverter_execute (up, x, TE, up_buf, up_cap);
          const double t0 = now_s ();
          blk_n           = ddc_execute (ddc, up_buf, n_up, ddc_buf, ddc_cap);
          t->ddc_s += now_s () - t0;
          blk_in = ddc_buf;
        }
      const double t_push   = now_s ();
      size_t       assigned = async_dsss_pool_push (p, blk_in, blk_n);
      t->push_s += now_s () - t_push;
      t->signal_s += (double)TE / FS;
      const int new_drops = p->dropped > drops_before;
      /* The duration requirement (section 5.1): after a warm-up long
         enough for every buffer to reach its high-water mark -- the
         symbol buffer at the first push, the log's first event of every
         label, the searcher's and receivers' scratch -- the heap must not
         grow. Sampled once a second. */
      if (b % (samples (1.0) / TE) == 0)
        {
          const double t_now = (double)now / FS;
          const double h     = heap_bytes ();
          if (t_now >= t->warm_s)
            {
              size_t used = 0;
              for (size_t i = 0; i < N_SLOTS; i++)
                used += slot_tracked[i];
              if (t->heap_base == 0.0 || used > t->slots_used)
                {
                  /* A new baseline: the warm-up's end, or a slot that has
                     just tracked for the first time (its chains are built
                     now). The step from the last high-water mark is the
                     first use's, kept apart from growth. */
                  if (t->heap_base > 0.0 && h - t->heap_max > t->heap_step_max)
                    t->heap_step_max = h - t->heap_max;
                  t->heap_base    = h;
                  t->heap_max     = h;
                  t->rss_base_kib = rss_kib ();
                  t->slots_used   = used;
                }
              if (h > t->heap_max)
                t->heap_max = h;
              t->heap_end = h;
            }
        }
      if (assigned > t->max_assigned)
        t->max_assigned = assigned;
      const uint64_t end = p->samples_consumed;

      /* Whose is each slot: decided once per seed. */
      memset (held, 0, cfg->n_emit * sizeof *held);
      async_dsss_pool_slot_t r[N_SLOTS];
      for (size_t i = 0; i < N_SLOTS; i++)
        {
          r[i] = async_dsss_pool_status (p, i);
          if (r[i].assigned && r[i].both_down_samples < prev_down[i]
              && prev_down[i] >= samples (0.2)
              && r[i].state == ASYNC_DSSS_RX_TRACKING)
            t->clock_restarts++;
          if (trace_every && r[i].assigned
              && r[i].both_down_samples < prev_down[i]
              && prev_down[i] >= samples (0.2)
              && r[i].state == ASYNC_DSSS_RX_TRACKING)
            printf ("    release clock restarted at %.3f s on slot %zu after "
                    "%.2f s: code %d sym %d metric %.2f%s\n",
                    (double)end / FS, i, (double)prev_down[i] / FS,
                    r[i].code_locked, r[i].locked, r[i].lock_metric,
                    owner[i] < cfg->n_emit && e[owner[i]].on
                        ? ""
                        : " (its emitter is off the air)");
          prev_down[i] = r[i].assigned ? r[i].both_down_samples : 0;
          slot_tracked[i] |= r[i].state == ASYNC_DSSS_RX_TRACKING;
          if (!r[i].assigned)
            {
              owner[i]      = cfg->n_emit;
              owner_seed[i] = UINT64_MAX;
              continue;
            }
          if (owner_seed[i] != r[i].seed_sample)
            {
              owner_seed[i] = r[i].seed_sample;
              owner[i]      = cfg->n_emit;

              for (size_t k = 0; k < cfg->n_emit; k++)
                {
                  double dh = r[i].seed_doppler_hz - e[k].doppler_hz;
                  double dc = chip_err (r[i].seed_chip_phase,
                                        truth_chip (&e[k], r[i].seed_sample));
                  if (fabs (dh) <= tile_hz && fabs (dc) <= 1.0)
                    {
                      owner[i] = k;
                      /* The seed's error, for the record: the first seed
                         of the stint it lands in. */
                      emitter_t *ek = &e[k];
                      if (ek->n_st && ek->on
                          && ek->st[ek->n_st - 1].n_assign == 0)
                        {
                          ek->st[ek->n_st - 1].seed_err_hz   = dh;
                          ek->st[ek->n_st - 1].seed_err_chip = dc;
                        }
                      break;
                    }
                }
              if (owner[i] == cfg->n_emit)
                t->false_alarms++;
              if (trace_every)
                {
                  printf ("    seed at %.3f s into slot %zu: %+.1f Hz, chip "
                          "%.2f, C/N0 %.1f -> ",
                          (double)r[i].seed_sample / FS, i,
                          r[i].seed_doppler_hz, r[i].seed_chip_phase,
                          r[i].seed_cn0_dbhz);
                  if (owner[i] < cfg->n_emit)
                    {
                      const emitter_t *ek = &e[owner[i]];
                      printf ("emitter %zu%s (%+.1f Hz, %+.2f chip)\n",
                              owner[i], ek->on ? "" : ", off the air",
                              r[i].seed_doppler_hz - ek->doppler_hz,
                              chip_err (r[i].seed_chip_phase,
                                        truth_chip (ek, r[i].seed_sample)));
                    }
                  else
                    printf ("no emitter\n");
                }
            }
          if (owner[i] < cfg->n_emit)
            held[owner[i]]++;
        }

      for (size_t k = 0; k < cfg->n_emit; k++)
        {
          emitter_t *ek   = &e[k];
          stint_t   *s    = ek->n_st ? &ek->st[ek->n_st - 1] : NULL;
          size_t     slot = N_SLOTS;
          for (size_t i = 0; i < N_SLOTS && slot == N_SLOTS; i++)
            if (owner[i] == k)
              slot = i;
          if (held[k] >= 2 && s && ek->on)
            {
              s->dbl++;
              size_t locked = 0;
              for (size_t i = 0; i < N_SLOTS; i++)
                locked += owner[i] == k && r[i].state == ASYNC_DSSS_RX_TRACKING
                          && r[i].code_locked;
              s->dbl_locked += locked >= 2;
            }
          /* Hits dropped this push while this emitter was on the air with
             no slot: it is the one waiting (the pool is full of emitters
             and of departed ones inside their release interval). */
          if (new_drops && s && ek->on && slot == N_SLOTS)
            s->waited = 1;
          if (slot < N_SLOTS && ek->slot == N_SLOTS)
            {
              /* An assignment. */
              if (ek->on && s)
                {
                  if (s->n_assign == 0)
                    s->t_assign = r[slot].seed_sample;
                  else
                    t->reassign++;
                  s->n_assign++;
                }
              else
                t->off_air_assign++;
            }
          else if (slot == N_SLOTS && ek->slot < N_SLOTS)
            {
              /* A release: the rule after departure, or on the air. */
              if (ek->on && s)
                {
                  if (ek->slot_age + TE > p->max_on_samples)
                    s->on_time_rel++;
                  else
                    s->false_rel++;
                }
              else if (s && s->t_off && !s->t_rel)
                {
                  s->t_rel       = end;
                  s->rel_on_time = ek->slot_age + TE > p->max_on_samples;
                }
            }
          ek->slot = slot;
          if (slot < N_SLOTS)
            ek->slot_age = r[slot].assigned_samples;
          if (ek->on && s)
            {
              const int trk
                  = slot < N_SLOTS && r[slot].state == ASYNC_DSSS_RX_TRACKING;
              if (slot < N_SLOTS && !s->t_held)
                s->t_held = end;
              if (trk && !s->t_track)
                s->t_track = end;
              if (s->t_track)
                s->on_blocks++;
              if (slot < N_SLOTS)
                {
                  s->held_blocks++;
                  s->trk_blocks += trk && r[slot].code_locked;
                  s->sym_blocks += trk && r[slot].locked;
                }
            }
        }

      if (trace_every)
        for (size_t a = 0; a < cfg->n_emit; a++)
          for (size_t c = a + 1; c < cfg->n_emit; c++)
            {
              const int    both     = e[a].on && e[c].on;
              const double dc       = both ? chip_err (truth_chip (&e[a], end),
                                                       truth_chip (&e[c], end))
                                           : 1e9;
              const int    now_near = both && fabs (dc) <= 2.0;
              if (now_near != near[a][c])
                printf ("    crossing %s at %.3f s: emitters %zu and %zu, "
                        "%+.1f Hz apart (%.2f chip/s), %+.2f chip\n",
                        now_near ? "begins" : "ends", (double)end / FS, a, c,
                        e[c].doppler_hz - e[a].doppler_hz,
                        (e[c].doppler_hz - e[a].doppler_hz) / CARRIER_HZ
                            * CHIP_RATE,
                        now_near ? dc : 0.0);
              near[a][c] = now_near;
            }
      if (trace_every && b % trace_every == 0)
        {
          size_t on = 0;
          for (size_t k = 0; k < cfg->n_emit; k++)
            on += e[k].on;
          printf ("    t=%6.1f s  on-air %2zu  assigned %2zu  dropped %llu  "
                  "heap %+.1f KiB  rss %+.0f KiB |",
                  (double)now / FS, on, assigned,
                  (unsigned long long)p->dropped,
                  t->heap_base > 0.0 ? (heap_bytes () - t->heap_base) / 1024.0
                                     : 0.0,
                  t->heap_base > 0.0 ? rss_kib () - t->rss_base_kib : 0.0);
          for (size_t k = 0; k < cfg->n_emit; k++)
            {
              if (!e[k].on)
                printf ("  %zu:off", k);
              else if (e[k].slot == N_SLOTS)
                printf ("  %zu:--", k);
              else
                printf ("  %zu:s%zu%c", k, e[k].slot,
                        r[e[k].slot].state == ASYNC_DSSS_RX_TRACKING
                            ? (r[e[k].slot].code_locked ? 'T' : 't')
                            : 'r');
            }
          printf ("\n");
          /* Every held slot's own flags, whoever it holds: the release
             rule is decided on these. */
          for (size_t i = 0; i < N_SLOTS; i++)
            if (r[i].assigned)
              printf ("        slot %2zu: %s code %d sym %d  %+9.1f Hz  chip "
                      "%7.2f  C/N0 %4.1f  metric %.2f  both-down %.2f s%s\n",
                      i,
                      r[i].state == ASYNC_DSSS_RX_TRACKING   ? "tracking"
                      : r[i].state == ASYNC_DSSS_RX_REFINING ? "refining"
                      : r[i].state == ASYNC_DSSS_RX_LOST     ? "lost    "
                                                             : "idle    ",
                      r[i].code_locked, r[i].locked, r[i].doppler_hz,
                      r[i].chip_phase, r[i].cn0_dbhz_est, r[i].lock_metric,
                      (double)r[i].both_down_samples / FS,
                      owner[i] < cfg->n_emit ? "" : "  (no emitter)");
        }
    }
  const uint64_t end = n_blocks * TE;
  for (size_t k = 0; k < cfg->n_emit; k++)
    if (e[k].n_st && !e[k].st[e[k].n_st - 1].t_off)
      {
        e[k].st[e[k].n_st - 1].t_off     = end;
        e[k].st[e[k].n_st - 1].truncated = 1;
      }

  /* The report: one line per stint, then the run. */
  printf ("    emitter  stint   on at    off at   assigned  seed err      "
          "tracking   held   tracked  symbol  false  on-time  dbl  "
          "released\n");
  printf ("                       s         s      +s      Hz   chip       "
          "+s                     locked    rel    rel    (lkd)    +s\n");
  const uint64_t min_on = samples (cfg->on_min_s);
  for (size_t k = 0; k < cfg->n_emit; k++)
    for (size_t j = 0; j < e[k].n_st; j++)
      {
        const stint_t *s = &e[k].st[j];
        /* A stint the run cut short before the design's own bound on the
           acquisition (a frame, plus the refine) is not scored for its
           assignment; one cut before an off-time has run is not scored for
           its release. */
        const int scoreable = !s->truncated || s->t_off - s->t_on >= min_on;
        printf ("    %7zu  %5zu  %7.2f  %7.2f  ", k, j + 1,
                (double)s->t_on / FS, (double)s->t_off / FS);
        if (s->n_assign)
          printf ("%6.2f  %+6.1f  %+5.2f  ",
                  (double)(s->t_assign - s->t_on) / FS, s->seed_err_hz,
                  s->seed_err_chip);
        else if (s->t_held)
          printf ("%6.2f  %6s  %5s  ", (double)(s->t_held - s->t_on) / FS,
                  "same", "rx");
        else
          printf ("%6s  %6s  %5s  ", "--", "", "");
        if (s->t_track)
          printf ("%7.2f  ", (double)(s->t_track - s->t_on) / FS);
        else
          printf ("%7s  ", "--");
        printf ("%5.3f  %7.3f  %6.3f  %5d  %7d  %3zu (%zu)  ",
                s->on_blocks ? (double)s->held_blocks / (double)s->on_blocks
                             : 0.0,
                s->held_blocks ? (double)s->trk_blocks / (double)s->held_blocks
                               : 0.0,
                s->held_blocks ? (double)s->sym_blocks / (double)s->held_blocks
                               : 0.0,
                s->false_rel, s->on_time_rel, s->dbl, s->dbl_locked);
        if (s->truncated)
          printf ("%8s\n", "(end)");
        else if (s->t_rel)
          printf ("%8.2f%s\n", (double)(s->t_rel - s->t_off) / FS,
                  s->rel_on_time ? " (on-time)" : "");
        else
          printf ("%8s\n", "NEVER");

        t->n_stints++;
        if (!scoreable)
          continue;
        t->scored++;
        if (!s->t_held)
          t->missed++;
        else if (s->waited)
          {
            /* Its arrival -> held is the wait for a slot, not the
               searcher's: kept apart from the assignment latency. */
            const double w = (double)(s->t_held - s->t_on) / FS;
            t->waited++;
            if (w > t->wait_max)
              t->wait_max = w;
          }
        else
          {
            /* Arrival -> held: a new seed, or -- back inside the release
               interval of its own receiver -- that receiver re-locking,
               the design's recovered assignment (section 12.3). */
            if (s->n_assign == 0)
              t->relocked++;
            acc ((double)(s->t_held - s->t_on) / FS, &t->assign_min,
                 &t->assign_sum, &t->assign_max, &t->n_assign);
            if (fabs (s->seed_err_hz) > t->seed_hz_max)
              t->seed_hz_max = fabs (s->seed_err_hz);
            if (fabs (s->seed_err_chip) > t->seed_chip_max)
              t->seed_chip_max = fabs (s->seed_err_chip);
          }
        if (s->t_track)
          acc ((double)(s->t_track - s->t_on) / FS, &t->track_min,
               &t->track_sum, &t->track_max, &t->n_track);
        t->false_rel += (size_t)s->false_rel;
        t->on_time_rel += (size_t)s->on_time_rel;
        t->dbl += s->dbl;
        t->dbl_locked += s->dbl_locked;
        t->held_blocks += s->held_blocks;
        t->trk_blocks += s->trk_blocks;
        t->sym_blocks += s->sym_blocks;
        t->on_blocks += s->on_blocks;
        if (!s->truncated && s->t_held)
          {
            /* Absent: the run gave the release its latency and none came.
               A departure closer than that to the run's end is not
               scored either way. */
            if (!s->t_rel)
              t->late_rel += (double)(end - s->t_off) / FS >= late_s;
            else if (s->rel_on_time)
              t->on_time_rel++;
            else
              {
                const double lat = (double)(s->t_rel - s->t_off) / FS;
                acc (lat, &t->rel_min, &t->rel_sum, &t->rel_max, &t->n_rel);
                if (lat > late_s)
                  t->late_rel++;
                else if (lat > LOST_CONFIRM_S + 0.5)
                  t->over_rel++;
              }
          }
      }
  t->dropped = p->dropped;
  t->events  = p->events;
  DP_CHECK (dp_event_log_close (log) == DP_OK);
  DP_CHECK_MSG (dp_event_log_count (log) == p->events,
                "every transition the pool counted reached the log");
  DP_REQUIRE (count_log (path, t) == 0);
  DP_CHECK (async_dsss_pool_set_event_log (p, NULL) == DP_OK);
  dp_event_log_destroy (log);
  if (!g_events_dir)
    remove (path);

  printf ("  stints %zu (%zu scored): missed %zu, false releases %zu, late "
          "or absent releases %zu, on-time releases %zu, re-assignments "
          "%zu, double-held blocks %zu (both code-locked: %zu), assigned "
          "off the air %zu, seeds matching no emitter %zu, stints re-locked "
          "by their own receiver %zu; dropped %llu (stints that waited for "
          "a slot %zu, longest wait %.2f s), most assigned %zu of %u\n",
          t->n_stints, t->scored, t->missed, t->false_rel, t->late_rel,
          t->on_time_rel, t->reassign, t->dbl, t->dbl_locked,
          t->off_air_assign, t->false_alarms, t->relocked,
          (unsigned long long)t->dropped, t->waited, t->wait_max,
          t->max_assigned, N_SLOTS);
  if (t->n_assign)
    printf ("  arrival -> held %.2f / %.2f / %.2f s (min / mean / max, %zu); "
            "seed error at most %.1f Hz, %.2f chip\n",
            t->assign_min, t->assign_sum / (double)t->n_assign, t->assign_max,
            t->n_assign, t->seed_hz_max, t->seed_chip_max);
  if (t->n_track)
    printf ("  arrival -> tracking %.2f / %.2f / %.2f s (%zu)\n", t->track_min,
            t->track_sum / (double)t->n_track, t->track_max, t->n_track);
  if (t->n_rel)
    printf ("  departure -> release %.2f / %.2f / %.2f s (%zu; %zu past the "
            "interval + 0.5 s, a false re-lock on noise having restarted "
            "the clock once)\n",
            t->rel_min, t->rel_sum / (double)t->n_rel, t->rel_max, t->n_rel,
            t->over_rel);
  printf (
      "  on the air after first tracking: held %.4f of %zu blocks; held "
      "and tracking with code lock %.4f of %zu, with symbol lock "
      "%.4f\n",
      t->on_blocks ? (double)t->held_blocks / (double)t->on_blocks : 0.0,
      t->on_blocks,
      t->held_blocks ? (double)t->trk_blocks / (double)t->held_blocks : 0.0,
      t->held_blocks,
      t->held_blocks ? (double)t->sym_blocks / (double)t->held_blocks : 0.0);
  t->rss_end_kib = rss_kib ();
  if (g_emit)
    {
      printf ("# stints cn0=%.0f\n", cfg->cn0_dbhz);
      printf ("emitter,stint,t_on_s,t_off_s,truncated,seeds,t_held_s,"
              "t_track_s,seed_err_hz,seed_err_chip,held_of_on,trk_of_held,"
              "sym_of_held,false_rel,on_time_rel,dbl_locked,t_rel_s,"
              "rel_on_time\n");
      for (size_t k = 0; k < cfg->n_emit; k++)
        for (size_t j = 0; j < e[k].n_st; j++)
          {
            const stint_t *s = &e[k].st[j];
            printf (
                "%zu,%zu,%.4f,%.4f,%d,%d,%.4f,%.4f,%.2f,%.3f,%.5f,%.5f,"
                "%.5f,%d,%d,%zu,%.4f,%d\n",
                k, j + 1, (double)s->t_on / FS, (double)s->t_off / FS,
                s->truncated, s->n_assign,
                s->t_held ? (double)(s->t_held - s->t_on) / FS : -1.0,
                s->t_track ? (double)(s->t_track - s->t_on) / FS : -1.0,
                s->seed_err_hz, s->seed_err_chip,
                s->on_blocks ? (double)s->held_blocks / (double)s->on_blocks
                             : 0.0,
                s->held_blocks ? (double)s->trk_blocks / (double)s->held_blocks
                               : 0.0,
                s->held_blocks ? (double)s->sym_blocks / (double)s->held_blocks
                               : 0.0,
                s->false_rel, s->on_time_rel, s->dbl_locked,
                s->t_rel ? (double)(s->t_rel - s->t_off) / FS : -1.0,
                s->rel_on_time);
          }
      printf ("\n# totals cn0=%.0f\n", cfg->cn0_dbhz);
      printf ("cn0_dbhz,n_emit,duration_s,threads,stints,scored,missed,"
              "false_rel,late_rel,over_rel,on_time_rel,reassign,dbl_locked,"
              "false_alarms,relocked,dropped,waited,max_assigned,n_slots,"
              "held_of_on,trk_of_held,sym_of_held,events,log_lines,"
              "clock_restarts,heap_base_mib,heap_step_max_kib,"
              "heap_growth_kib,rss_base_mib,rss_end_mib,ddc_s,push_s,"
              "signal_s\n");
      printf (
          "%.0f,%zu,%.0f,%d,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,"
          "%llu,%zu,%zu,%u,%.5f,%.5f,%.5f,%llu,%zu,%zu,%.2f,%.2f,%.2f,"
          "%.2f,%.2f,%.3f,%.3f,%.3f\n\n",
          cfg->cn0_dbhz, cfg->n_emit, cfg->duration_s, dp_pool_threads (fan),
          t->n_stints, t->scored, t->missed, t->false_rel, t->late_rel,
          t->over_rel, t->on_time_rel, t->reassign, t->dbl_locked,
          t->false_alarms, t->relocked, (unsigned long long)t->dropped,
          t->waited, t->max_assigned, N_SLOTS,
          t->on_blocks ? (double)t->held_blocks / (double)t->on_blocks : 0.0,
          t->held_blocks ? (double)t->trk_blocks / (double)t->held_blocks
                         : 0.0,
          t->held_blocks ? (double)t->sym_blocks / (double)t->held_blocks
                         : 0.0,
          (unsigned long long)t->events, t->log_lines, t->clock_restarts,
          t->heap_base / 1048576.0, t->heap_step_max / 1024.0,
          (t->heap_max - t->heap_base) / 1024.0, t->rss_base_kib / 1024.0,
          t->rss_end_kib / 1024.0, t->ddc_s, t->push_s, t->signal_s);
    }
  if (g_budget)
    printf ("  budget: %.1f s of signal; inside the DDC %.1f s, inside "
            "push() %.1f s on %d threads -- %.3f of real time at 13 MSa/s "
            "(DDC %.3f + pool %.3f), %.3f at the 30 MSa/s floor; the "
            "target is under 0.5 at both (section 6.4)\n",
            t->signal_s, t->ddc_s, t->push_s, dp_pool_threads (fan),
            (t->ddc_s + t->push_s) / t->signal_s, t->ddc_s / t->signal_s,
            t->push_s / t->signal_s,
            (t->ddc_s + t->push_s) / t->signal_s * 30.0e6 / DDC_IN_RATE);
  printf ("  heap %.1f MiB after the %.0f s warm-up; %zu slots tracked at "
          "least once, the largest first-use step %.1f KiB; since the last "
          "first use: at most %+.1f KiB, at the end %+.1f KiB; resident "
          "high-water mark %.1f MiB -> %.1f MiB\n",
          t->heap_base / 1048576.0, t->warm_s, t->slots_used,
          t->heap_step_max / 1024.0, (t->heap_max - t->heap_base) / 1024.0,
          (t->heap_end - t->heap_base) / 1024.0, t->rss_base_kib / 1024.0,
          t->rss_end_kib / 1024.0);
  printf ("  event log: %zu lines for %llu transitions -- seeded %zu, "
          "tracking %zu, degrade %zu, lost %zu, released %zu (lost) + %zu "
          "(on_time), dropped %zu\n\n",
          t->log_lines, (unsigned long long)t->events, t->log_seeded,
          t->log_tracking, t->log_degrade, t->log_lost, t->log_rel_lost,
          t->log_rel_on_time, t->log_dropped);

  /* The lifecycle's expectations (§12 step 7), asserted on every run. */
  DP_CHECK_MSG (t->scored > 0 && t->n_assign > 0, "stints were scored");
  /* Nothing grows with time (section 5.1): the heap after the warm-up is
     the heap for the rest of the run, to within a page of allocator
     slack. The heap is glibc's in-use count over every arena with the
     tcache off (main), so what it reads is the pool's. The resident
     high-water mark is reported beside it and NOT gated: it is the
     process's, and with twenty workers it moves with the schedule --
     measured, the same stimulus twice at 45 dB-Hz for 20 s read +1.7 MiB
     on one run and +0.0 on the next, and the 30 s and 600 s runs settled
     at +2.5 to +5.9 MiB, no larger for twenty times the length. Whether
     that is the workers' stacks or their arenas' first-touched pages is
     not pinned; either way it is not a function of time. */
  if (!g_budget) /* the front end's own grow-on-demand buffers are not
                    the pool's; the heap is the duration mode's gate */
    DP_CHECK_MSG (t->heap_base > 0.0 && t->heap_max - t->heap_base <= 4096.0,
                  "the heap does not grow once every slot in use has "
                  "built its chains");
  DP_CHECK_MSG (t->missed == 0, "no emitter above the floor is missed");
  DP_CHECK_MSG (t->false_rel == 0,
                "no emitter is released while on the air by the rule");
  /* The bound is TWO intervals plus half a second: the code flag still
     returns on noise at 0.004 per second (§12.15), and one return inside
     the interval restarts the clock once -- the rule's own worst case at
     that rate (two returns inside one interval is a 1e-4 event per
     departure). The count past one interval is reported beside it. */
  DP_CHECK_MSG (t->late_rel == 0, "every departed emitter is released "
                                  "within two intervals plus half a second");
  DP_CHECK_MSG (t->dbl_locked == 0, "no emitter is tracked by two "
                                    "receivers at once");
  /* A hit is dropped only while the pool is full of emitters -- on the
     air, or departed and inside their release interval: at this soak's
     churn (a departure every few seconds against the design's one a
     minute, section 6.1) departures clump inside one interval, and the
     emitter waits for a slot. That is the design's degradation; a drop
     with a slot held by anything else is not. */
  DP_CHECK_MSG (t->dropped == 0 || cfg->n_emit + 2 >= N_SLOTS,
                "nothing is dropped while the population fits the pool");
  DP_CHECK_MSG (t->max_assigned <= N_SLOTS, "the pool never exceeds its "
                                            "slots");
  DP_CHECK_MSG (t->assign_max <= FRAME_S + 0.25,
                "every arrival with a slot free is seeded within a frame "
                "and the refine");
  DP_CHECK_MSG (t->wait_max <= FRAME_S + LOST_CONFIRM_S + 0.25,
                "an arrival that waited for a slot is seeded within a "
                "release interval more");
  DP_CHECK_MSG (t->log_lines == t->events
                    && t->log_seeded + t->log_tracking + t->log_degrade
                               + t->log_lost + t->log_rel_lost
                               + t->log_rel_on_time + t->log_dropped
                           == t->log_lines,
                "the log's labels are the design's and number the count");

  free (ddc_buf);
  free (up_buf);
  ddc_destroy (ddc);
  RateConverter_destroy (up);
  free (nz);
  free (x);
  dp_pool_destroy (fan);
  async_dsss_pool_destroy (p);
  awgn_destroy (g);
  for (size_t k = 0; k < cfg->n_emit; k++)
    destroy_emitter (&e[k]);
  free (e);
  return 0;
}

int
main (int argc, char **argv)
{
#ifdef __GLIBC__
  /* glibc's tcache keeps freed chunks per thread and mallinfo2 counts
     them as in use, so a heap that is flat by every other measure climbs
     by the cache's fill for as long as the run meets new sizes -- the
     event log and the receivers' seed/track/reset cycle each measured
     0 B in isolation; the growth was the accounting. The tunable turns
     the cache off, but only before malloc initialises, which only an
     exec can do: the process re-runs itself once with it set. */
  {
    static const char tun[] = "glibc.malloc.tcache_count=0";
    const char       *have  = getenv ("GLIBC_TUNABLES");
    if (!have || !strstr (have, tun))
      {
        char buf[512];
        snprintf (buf, sizeof buf, "%s%s%s", have ? have : "", have ? ":" : "",
                  tun);
        setenv ("GLIBC_TUNABLES", buf, 1);
        execv ("/proc/self/exe", argv);
        /* exec refused: run as is; the heap gate reports what it sees */
      }
  }
#endif
  int check = 0;
  for (int a = 1; a < argc; a++)
    {
      if (strcmp (argv[a], "--check") == 0)
        check = 1;
      else if (strcmp (argv[a], "--events") == 0 && a + 1 < argc)
        g_events_dir = argv[++a];
      else if (strcmp (argv[a], "--refine-margin") == 0 && a + 1 < argc)
        g_refine_margin_db = atof (argv[++a]);
      else if (strcmp (argv[a], "--duration") == 0 && a + 1 < argc)
        g_duration_s = atof (argv[++a]);
      else if (strcmp (argv[a], "--budget") == 0)
        g_budget = 1;
      else if (strcmp (argv[a], "--emit") == 0)
        g_emit = 1;
    }
  if (g_budget)
    {
      g_chain_delay = measure_chain_delay ();
      printf ("budget: the front end's delay is %.1f samples at %.0f MSa/s\n",
              g_chain_delay, FS / 1e6);
    }
  uint8_t code[SF];
  gold_1023 (code);

  printf ("the pool's lifecycle soak: Gold-1023 at 5 Mcps, spc 2, 2700 "
          "sym/s async BPSK, %u code-only symbols of every %u (%.2f s "
          "frame), each emitter through the channel at its own Doppler "
          "within ±%.0f ppm of %.1f GHz; the pool at D from %u epochs, "
          "±%.0f kHz, %u peaks, %u slots, release at %.0f s; block = one "
          "epoch (%.3f ms)\n\n",
          W_SYM, F_SYM, FRAME_S, MAX_PPM, CARRIER_HZ / 1e9, CODE_ONLY_EPOCHS,
          DU / 1e3, MAX_PEAKS, N_SLOTS, LOST_CONFIRM_S, (double)TE / FS * 1e3);

  if (check)
    {
      /* Two emitters for CHECK_S at 45 dB-Hz: the always-on one is
         released for its on-time and re-acquired; the other arrives,
         leaves, is released, returns and is re-acquired. A trace line per
         second keeps the diagnostics in the record. */
      const cfg_t cfg
          = { 45.0, CHECK_EMIT, CHECK_S, 3.0, 3.5, 6.0, 6.5, 12.0, 1u };
      totals_t t;
      printf ("=== check: C/N0 %.0f dB-Hz ===\n", cfg.cn0_dbhz);
      DP_REQUIRE (run_soak (&cfg, code, 1, 2.0 * LOST_CONFIRM_S + 0.5, &t)
                  == 0);
      DP_TEST_END ("validate_async_dsss_pool_soak");
    }

  const double cn0s[] = { 45.0, 40.0 };
  for (size_t ci = 0; ci < 2; ci++)
    {
      const cfg_t cfg = {
        cn0s[ci], SWEEP_EMIT, g_duration_s > 0.0 ? g_duration_s : SWEEP_S,
        15.0,     30.0,       4.0,
        8.0,      35.0,       1u
      };
      totals_t t;
      printf ("=== C/N0 %.0f dB-Hz (Es/N0 %.1f dB) ===\n", cfg.cn0_dbhz,
              cfg.cn0_dbhz - 10.0 * log10 (SYM_RATE));
      DP_REQUIRE (run_soak (&cfg, code, 1, 2.0 * LOST_CONFIRM_S + 0.5, &t)
                  == 0);
    }
  DP_TEST_END ("validate_async_dsss_pool_soak");
}
