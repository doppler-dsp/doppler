/**
 * @file async_dsss_receiver_pullin.c
 * @brief From the hand-over to a link that holds: the AsyncDsssReceiver at
 *        the 415.1-B-1 point, the use case's whole grid.
 *
 * The use case (docs/design/async-dsss-receiver.md §1): 3,000 symbols a
 * second at 4 dB Es/N0 and up, a 1023-chip Gold code at the carrier times
 * 31/21216 chips a second, received at 11 MSa/s -- deliberately not a
 * multiple of the chip rate (3.42 samples a chip at 2.2 GHz, 2.69 at
 * 2.8) -- with a 275-symbol code-only window opening every 1,000-symbol
 * frame, a carrier anywhere in +/-1,500 Hz and a Doppler rate up to 400
 * Hz/s. A link works when, once its lock flags are sustained, they never
 * change again over the data: a user of the flag acts on every change.
 *
 * Stimulus: the use-case scene, native/tests/dp_air_test.h -- the shipped
 * synth sending this harness's payload with the window, the front end's
 * fixed-ratio converter to 11 MSa/s, a DopplerChannel at the carrier, the
 * shipped awgn. The C/N0 is at the antenna: the front end keeps 0.946 of a
 * rectangular chip's power at 2.2 GHz and 0.917 at 2.8 (0.24 and 0.38 dB),
 * and every loss below includes it. Each draw draws its own frame start
 * (0 to 999 symbols), carrier (+/-1,500 Hz at time 0), data and noise; a
 * ramp row runs at +400 Hz/s on odd seeds and -400 on even ones.
 *
 * Receiver: one standalone AsyncDsssReceiver at the specification's
 * configuration (pfa 1e-3, the search +/-1,500 Hz at 400 Hz/s through the
 * window, the refine on, its design C/N0 the true one), fed 4,096-sample
 * blocks. Per block it records its three flags: tracking, code lock
 * (get_code_locked()) and symbol lock (get_locked()), the flag its users
 * act on.
 *
 * Per draw, against the scene's truth (dp_air_truth()):
 *   - the hand-over: the Doppler the live chain was built from minus the
 *     channel's own offset at the hand-over sample, and which estimate the
 *     refine took (get_handover_source());
 *   - sustained lock, per flag: dp_ber_lock_symbol() over the blocks (200
 *     in a row up, 90% of the rest), in seconds from the start;
 *   - transitions after it, per flag, over DATA blocks only
 *     (dp_ber_lock_transitions(), looking where the truth puts the block's
 *     middle in a data symbol), and the first one's time;
 *   - a false lock: tracking at the end with the carrier a quarter of the
 *     symbol rate or more from the truth (the M-PSK ambiguity sits at half);
 *   - the error rate through dp_ber_test.h: aligned on a marker of the
 *     payload placed by dp_ber_marker_place(), checked for a slip at the
 *     end, scored from the settled point; EVM and the M2M4 SNR from
 *     dp_sym_test.h over the same window.
 * A draw WORKS when both flags are sustained, neither changes after, and
 * it is not a false lock.
 *
 * Per cell (carrier x Es/N0 x dynamics x carrier_window): works, acquired
 * and false locks with Wilson 95% intervals; time to sustained symbol lock
 * as median, 90th percentile and worst; the pooled error rate of the
 * aligned, unslipped draws with its interval and the loss against the BPSK
 * bound it implies. The full grid is 2 x 4 x 2 x 2 = 32 cells, DRAWS draws
 * of DRAW_S each (`--draws`, `--secs`, `--shard i/n` split it across
 * processes for the record's run; `make validate-c` runs it whole).
 *
 * What is asserted, the same in the sweep and in --check: where the
 * contract claims a working link -- 10 dB, static, the receiver as built
 * (the partials carrier window) -- every draw works,
 * aligns and does not slip, and its error rate's 95% upper limit is under
 * the bound's 3 dB lower (a margin, not a derived limit); and the live
 * carrier holds a 400 Hz/s ramp at 10 dB, its end within the false-lock
 * limit (a quarter of the symbol rate) of the truth -- a 3 s draw in
 * --check, long enough that the hand-over's frozen Doppler is past that
 * limit, so it tells the live carrier (status().doppler_hz) from it. The 4 and
 * 5 dB cells, the use case's own floor, are printed with their intervals and
 * not claimed: they do not meet the zero-transition criterion today; nor
 * is the code-period window at 10 dB, whose code lock changes over the data
 * on some draws (2.8 GHz, seed 1: twice, from 12.7 s). The
 * payload outlasts every run. --check: 2.8 GHz (2.69 samples a chip), 10
 * dB, static, both carrier windows, one 20 s draw each, and the 3 s ramp
 * draw.
 *
 * Telemetry: `--telemetry DIR --draw FC_GHZ,ESN0,RATE,CW,SEED [--from S
 * --to S]` runs that one draw (its row as the sweep prints it) and captures
 * the receiver's own probes over the window, with the scene's truth beside
 * them as capture probes once a block, on the block's stamp: truth.car
 * (cycles), truth.code (chips), truth.hz, truth.data. The five lock rules
 * the flags were decided by go to <stem>.rules.csv beside it. Two conventions
 * a reader of the capture needs: the carrier truth is the channel's own phase;
 * and rx.phase.code is the code NCO's word, the start of a sample's
 * dwell, where the Dll takes its prompt at the dwell's centre half an
 * increment later (dll_tap_base(), nco_dwell_center()): a prompt on the
 * code truth reads half a receive sample before it, with no correlation
 * lost -- measured at 30 dB, -0.186 chip at 2.8 GHz (2.69 samples a chip,
 * 0.500 sample) and -0.148 at 2.2 (3.42, 0.505). The code error is
 * therefore rx.phase.code - truth.code + 0.5 chip_rate / fs.
 *
 * `--spc N` renders the scene at N samples a chip (2: the front end
 * interpolates, and the held chips' images pass its transition band) for
 * the comparison with the default 4.
 *
 * Usage:  validate_async_dsss_receiver_pullin [--check] [--emit]
 *             [--draws N] [--secs S] [--shard i/n] [--spc N]
 *             [--fc GHZ] [--esn0 DB] [--rate HZ_S]   (a slice of the grid)
 *             [--telemetry DIR --draw FC,ESN0,RATE,CW,SEED [--from S]
 *              [--to S]]
 */
#include "async_dsss_receiver_common.h"
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/ber/ber_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include "doppler/dp_tlm_capture/dp_tlm_capture_core.h"
#include "doppler/timing/timing_core.h"
#include "dp_air_test.h"
#include "dp_ber_test.h"
#include "dp_dsss_test.h"
#include "dp_stats_test.h"
#include "dp_sym_test.h"
#include "dp_test.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI_FS 11e6
#define PI_SYM 3000.0
#define PI_PUSH 4096u
#define PI_SUSTAIN 200u /* blocks: 74 ms */
#define PI_DRAWS 30u
#define PI_DRAW_S 60.0
#define PI_CHECK_S 20.0
#define PI_SPAN_HZ 1500.0
#define PI_RATE_HZ_S 400.0
#define PI_MARGIN_DB 3.0
#define PI_MAX_CELL 64u
/* The --check ramp draw: long enough that the hand-over's Doppler, frozen,
   is past the false-lock limit (a quarter of the symbol rate, 750 Hz) at
   400 Hz/s with margin, so the test can tell the live carrier from it. */
#define PI_RAMP_CHECK_S 3.0

typedef struct
{
  double fc, esn0, rate;
  int    cw;
  double f0;
  size_t frame_off;
  double ho_err_hz;
  int    ho_src;
  double t_track_s, t_sym_s, t_code_s; /* NAN: never */
  size_t sym_tog, code_tog;
  double sym_tog1_s, code_tog1_s; /* NAN: none */
  double end_resid_hz;
  double end_stale_hz; /* the hand-over's Doppler against the end's
                          truth: what end_resid_hz would read if it
                          took get_doppler_hz() (the --check margin) */
  int           false_lock, aligned, slipped, works, data_ended;
  unsigned long errors, scored;
  double        evm_db, m2m4_db;
} draw_t;

/* A draw's telemetry: the receiver's own probes and the scene's truth,
   captured from `from_s` to `to_s` of the draw into `path`, with the
   receiver's five lock rules beside it in `rules`. */
typedef struct
{
  const char *path;  /* NULL: no capture */
  const char *rules; /* where the lock rules go */
  double      from_s, to_s;
} capture_t;

static double
uni (uint32_t *st)
{
  return dp_uni (st);
}

/* One draw: the receiver on `secs` of the scene. */
static int
run_draw (const uint8_t *code, double fc, double esn0, double rate_mag, int cw,
          uint32_t seed, double secs, unsigned spc, const capture_t *cp,
          draw_t *d)
{
  const double chip = fc * 31.0 / 21216.0;
  const double cn0  = esn0 + 10.0 * log10 (PI_SYM);
  uint32_t     st   = trial_rng (seed * 7919u + (uint32_t)(fc / 1e8)
                                 + (uint32_t)(esn0 * 10.0));
  memset (d, 0, sizeof *d);
  d->fc        = fc;
  d->esn0      = esn0;
  d->cw        = cw;
  d->rate      = rate_mag == 0.0 ? 0.0 : (seed & 1u ? 1.0 : -1.0) * rate_mag;
  d->f0        = -PI_SPAN_HZ + 2.0 * PI_SPAN_HZ * uni (&st);
  d->frame_off = (size_t)(1000.0 * uni (&st));
  if (d->frame_off > 999)
    d->frame_off = 999;

  /* The payload: one bit a data symbol, more than the run can send. */
  const size_t n_pay = (size_t)(secs * PI_SYM) + 2000;
  uint8_t     *pay   = dp_xmalloc (n_pay);
  for (size_t i = 0; i < n_pay; i++)
    pay[i] = (uint8_t)(uni (&st) < 0.5);
  dp_air_emitter_t e = { 0 };
  e.code             = code;
  e.code_len         = SF;
  e.chip_rate        = chip;
  e.sym_rate         = PI_SYM;
  e.carrier_hz       = fc;
  e.f0_hz            = d->f0;
  e.rate_hz_s        = d->rate;
  e.cn0_dbhz         = cn0;
  e.code_only_sym    = 275;
  e.frame_sym        = 1000;
  e.frame_off        = d->frame_off;
  e.seed             = 100u + seed;
  e.bits             = pay;
  e.n_bits           = n_pay;
  e.spc              = spc;
  dp_air_t air;
  DP_REQUIRE (dp_air_init (&air, &e, 1, PI_FS, cn0, 8807u + seed, PI_PUSH));

  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      code, SF, chip, PI_SYM, PI_FS, 2, cn0, 1e-3, 0.9, PI_SPAN_HZ, 4, 8, 0,
      0.5, 4, 14.0, 64, 8, false, 100000, fc, 0.0, false, 275.0 / PI_SYM,
      PI_RATE_HZ_S, true, cw);
  DP_REQUIRE (rx != NULL);

  const size_t   n_blk = (size_t)(secs * PI_FS) / PI_PUSH;
  uint8_t       *f_sym = dp_xmalloc (n_blk);
  uint8_t       *f_cod = dp_xmalloc (n_blk);
  uint8_t       *data  = dp_xmalloc (n_blk);
  float complex *x     = dp_xmalloc (PI_PUSH * sizeof *x);
  const size_t   ocap  = (size_t)(secs * PI_SYM) + 4096;
  float complex *syms  = dp_xmalloc (ocap * sizeof *syms);
  float complex *tmp   = dp_xmalloc (ocap * sizeof *tmp);
  size_t         ns = 0, first_in = SIZE_MAX;
  d->ho_err_hz = NAN;
  d->t_track_s = NAN;
  /* The capture: probes registered first (the capture sizes its ring
     from the table), the scene's truth beside the receiver's own, once a
     block on the block's stamp -- the stamp the receiver's phase probes
     are at. Attached for the window only. */
  dp_tlm_t         *tlm    = NULL;
  dp_tlm_capture_t *cap    = NULL;
  int               id_car = -1, id_code = -1, id_hz = -1, id_data = -1;
  dp_sample_clock_t clk;
  const size_t      b_from
      = cp && cp->path ? (size_t)(cp->from_s * PI_FS) / PI_PUSH : SIZE_MAX;
  const size_t b_to
      = cp && cp->path ? (size_t)(cp->to_s * PI_FS) / PI_PUSH : 0;
  for (size_t b = 0; b < n_blk; b++)
    {
      if (b == b_from)
        {
          tlm = dp_tlm_create (1u << 18);
          DP_REQUIRE (tlm != NULL);
          DP_REQUIRE (dp_async_dsss_receiver_set_telemetry (rx, tlm, "rx", 1)
                      == DP_OK);
          id_car  = dp_tlm_probe (tlm, "truth.car", 1);
          id_code = dp_tlm_probe (tlm, "truth.code", 1);
          id_hz   = dp_tlm_probe (tlm, "truth.hz", 1);
          id_data = dp_tlm_probe (tlm, "truth.data", 1);
          DP_REQUIRE (id_car >= 0 && id_code >= 0 && id_hz >= 0
                      && id_data >= 0);
          dp_sample_clock_init (&clk, PI_FS, 0);
          cap = dp_tlm_capture_open (tlm, PI_PUSH, cp->path, &clk);
          DP_REQUIRE_MSG (cap != NULL, "the draw's capture opens");
        }
      if (tlm)
        {
          const uint64_t       at = (uint64_t)b * PI_PUSH;
          const dp_air_truth_t t  = dp_air_truth (&air, 0, at);
          dp_tlm_set_now (tlm, at);
          dp_tlm_emit (tlm, id_car, t.car_cycles - floor (t.car_cycles));
          dp_tlm_emit (tlm, id_code, t.code_chips);
          dp_tlm_emit (tlm, id_hz, t.car_hz);
          dp_tlm_emit (tlm, id_data,
                       (double)(t.symbol >= 0 && t.symbol % 1000 >= 275));
        }
      DP_REQUIRE (dp_air_next (&air, x, PI_PUSH));
      const size_t got
          = dp_async_dsss_receiver_steps (rx, x, PI_PUSH, tmp, ocap);
      const int tracking = rx->state == ASYNC_DSSS_RX_TRACKING;
      if (tracking && isnan (d->ho_err_hz))
        {
          const uint64_t at = rx->samples_in - rx->state_samples;
          d->ho_err_hz      = dp_async_dsss_receiver_get_doppler_hz (rx)
                              - dp_air_truth (&air, 0, at).car_hz;
          d->ho_src         = dp_async_dsss_receiver_get_handover_source (rx);
          d->t_track_s      = (double)at / PI_FS;
        }
      if (!tracking)
        { /* only the current track's symbols are scored */
          ns       = 0;
          first_in = SIZE_MAX;
        }
      else
        {
          if (got && first_in == SIZE_MAX)
            first_in = (b + 1) * PI_PUSH;
          if (ns + got <= ocap)
            {
              memcpy (syms + ns, tmp, got * sizeof *tmp);
              ns += got;
            }
        }
      f_sym[b] = (uint8_t)(tracking && dp_async_dsss_receiver_get_locked (rx));
      f_cod[b]
          = (uint8_t)(tracking && dp_async_dsss_receiver_get_code_locked (rx));
      /* A block is DATA when the truth puts its middle in a data symbol. */
      const long sym
          = dp_air_truth (&air, 0, b * PI_PUSH + PI_PUSH / 2).symbol;
      data[b] = (uint8_t)(sym >= 0 && sym % 1000 >= 275);
      if (tlm && (b + 1 >= b_to || b + 1 == n_blk))
        {
          dp_tlm_set_now (tlm, (uint64_t)(b + 1) * PI_PUSH);
          (void)dp_async_dsss_receiver_set_telemetry (rx, NULL, NULL, 1);
          DP_CHECK_MSG (dp_tlm_capture_close (cap) == DP_OK,
                        "the draw's capture lost nothing");
          (void)dp_tlm_capture_destroy (cap);
          dp_tlm_destroy (tlm);
          tlm = NULL;
          /* The lock rules the flags were decided by, from the objects. */
          FILE *fr = fopen (cp->rules, "w");
          DP_REQUIRE (fr != NULL);
          (void)fprintf (fr, "rule,up_thresh,down_thresh,n_up,n_down\n");
          const struct
          {
            const char       *name;
            dp_lockdet_rule_t r;
          } rules[] = {
            { "sym", dp_async_dsss_receiver_get_sym_lock_rule (rx) },
            { "car", dp_async_dsss_receiver_get_car_lock_rule (rx) },
            { "code", dp_async_dsss_receiver_get_code_lock_rule (rx) },
            { "demod", dp_async_dsss_receiver_get_demod_lock_rule (rx) },
            { "demod_sync",
              dp_async_dsss_receiver_get_demod_sync_lock_rule (rx) },
          };
          for (size_t k = 0; k < sizeof rules / sizeof *rules; k++)
            (void)fprintf (fr, "%s,%.6g,%.6g,%u,%u\n", rules[k].name,
                           rules[k].r.up_thresh, rules[k].r.down_thresh,
                           rules[k].r.n_up, rules[k].r.n_down);
          DP_REQUIRE (fclose (fr) == 0);
        }
    }

  /* Lock and its transitions, over the data. */
  const int64_t s_sym = dp_ber_lock_symbol (f_sym, n_blk, PI_SUSTAIN, 0.9);
  const int64_t s_cod = dp_ber_lock_symbol (f_cod, n_blk, PI_SUSTAIN, 0.9);
  const double  blk_s = (double)PI_PUSH / PI_FS;
  d->t_sym_s          = s_sym >= 0 ? (double)s_sym * blk_s : NAN;
  d->t_code_s         = s_cod >= 0 ? (double)s_cod * blk_s : NAN;
  int64_t first       = -1;
  d->sym_tog1_s = d->code_tog1_s = NAN;
  if (s_sym >= 0)
    {
      d->sym_tog = dp_ber_lock_transitions (f_sym, data, n_blk, (size_t)s_sym,
                                            &first);
      if (first >= 0)
        d->sym_tog1_s = (double)first * blk_s;
    }
  if (s_cod >= 0)
    {
      d->code_tog = dp_ber_lock_transitions (f_cod, data, n_blk, (size_t)s_cod,
                                             &first);
      if (first >= 0)
        d->code_tog1_s = (double)first * blk_s;
    }
  const uint64_t n_in = (uint64_t)n_blk * PI_PUSH;
  /* The live carrier (status().doppler_hz, where the emitter is now), not
     get_doppler_hz(), the hand-over's: under a ramp that one is the
     Doppler of tens of seconds ago. */
  d->end_resid_hz = dp_async_dsss_receiver_status (rx).doppler_hz
                    - dp_air_truth (&air, 0, n_in).car_hz;
  d->end_stale_hz = dp_async_dsss_receiver_get_doppler_hz (rx)
                    - dp_air_truth (&air, 0, n_in).car_hz;
  d->false_lock   = rx->state == ASYNC_DSSS_RX_TRACKING
                    && fabs (d->end_resid_hz) >= 0.25 * PI_SYM;

  /* The error rate of the current track, against the payload as each
     symbol carried it. */
  d->evm_db = d->m2m4_db = NAN;
  if (rx->state == ASYNC_DSSS_RX_TRACKING && first_in != SIZE_MAX
      && ns > 2 * ((size_t)DP_BER_LAG_SPAN + DP_BER_SYNC_SYMS) + 500)
    {
      int          ok     = 0;
      const size_t settle = dp_ber_settle (
          rx->rx->l.timing.bn, rx->rx->l.bn_carrier, NULL, NULL, ns, &ok);
      long base = dp_air_truth (&air, 0, first_in).symbol - 60;
      if (base < 0)
        base = 0;
      const size_t n_truth = (size_t)(secs * PI_SYM) + 1000;
      uint8_t     *truth   = dp_xmalloc (n_truth);
      for (size_t i = 0; i < n_truth; i++)
        {
          const int bit = dp_air_symbol_bit (&air, 0, base + (long)i);
          truth[i]      = (uint8_t)(bit > 0);
        }
      const size_t guard = (size_t)DP_BER_LAG_SPAN + DP_BER_SYNC_SYMS;
      if (ok && settle + 2 * guard + 500 < ns)
        {
          dp_ber_marker_t mk     = { NULL, DP_BER_SYNC_SYMS, 0, 0, 0 };
          dp_ber_marker_t mk_end = mk;
          size_t          at     = dp_ber_marker_place (
              truth, n_truth, 2, settle + DP_BER_LAG_SPAN, 0, DP_BER_LAG_SPAN,
              DP_BER_SYNC_PFA, 1);
          mk.t0            = at == SIZE_MAX ? n_truth : at;
          dp_ber_sync_t sy = dp_ber_sync (syms, ns, truth, n_truth, &mk, 2,
                                          DP_BER_LAG_SPAN, DP_BER_SYNC_PFA);
          if (sy.ok)
            {
              size_t at_end
                  = dp_ber_marker_place (truth, n_truth, 2, ns - guard, 0,
                                         DP_BER_LAG_SPAN, DP_BER_SYNC_PFA, -1);
              mk_end.t0 = at_end == SIZE_MAX ? n_truth : at_end;
              dp_ber_sync_t end
                  = dp_ber_sync (syms, ns, truth, n_truth, &mk_end, 2,
                                 DP_BER_LAG_SPAN, DP_BER_SYNC_PFA);
              d->aligned        = 1;
              d->slipped        = !end.ok || end.lag != sy.lag
                                  || cos (end.phase - sy.phase) < 0.0;
              const long   lo_e = (long)(mk.t0 + mk.n) - sy.lag;
              const size_t lo   = (lo_e > 0 && (size_t)lo_e > settle)
                                      ? (size_t)lo_e
                                      : settle;
              dp_ber_t     acc;
              dp_ber_init (&acc, 2, ULONG_MAX);
              dp_ber_score (&acc, syms, lo, ns, truth, n_truth, &mk, &sy);
              d->errors  = acc.errors;
              d->scored  = acc.symbols;
              d->evm_db  = dp_test_evm_db_hard (syms + lo, ns - lo);
              d->m2m4_db = dp_test_m2m4_snr_db (syms + lo, ns - lo);
              dp_ber_free (&acc);
            }
        }
      free (truth);
    }
  d->works = s_sym >= 0 && s_cod >= 0 && d->sym_tog == 0 && d->code_tog == 0
             && !d->false_lock;
  d->data_ended = dp_air_data_ended (&air);

  free (f_sym);
  free (f_cod);
  free (data);
  free (x);
  free (syms);
  free (tmp);
  free (pay);
  dp_air_free (&air);
  dp_async_dsss_receiver_destroy (rx);
  return 0;
}

static void
emit_row (const draw_t *d, uint32_t seed)
{
  printf ("%.1f,%.1f,%.0f,%s,%u,%.1f,%zu,%.2f,%d,%.4f,%.4f,%.4f,%zu,%.4f,"
          "%zu,%.4f,%.1f,%d,%d,%d,%lu,%lu,%.3f,%.3f,%d\n",
          d->fc / 1e9, d->esn0, d->rate, d->cw ? "code_period" : "partials",
          seed, d->f0, d->frame_off, d->ho_err_hz, d->ho_src, d->t_track_s,
          d->t_sym_s, d->t_code_s, d->sym_tog, d->sym_tog1_s, d->code_tog,
          d->code_tog1_s, d->end_resid_hz, d->false_lock, d->aligned,
          d->slipped, d->errors, d->scored, d->evm_db, d->m2m4_db, d->works);
}

static int
cmp_dbl (const void *a, const void *b)
{
  const double x = *(const double *)a, y = *(const double *)b;
  return (x > y) - (x < y);
}

static void
print_cell (const draw_t *ds, size_t n)
{
  size_t        works = 0, acq = 0, fl = 0, al = 0, slip = 0;
  unsigned long err = 0, sc = 0;
  double        t[PI_DRAWS * 4];
  size_t        nt = 0;
  for (size_t i = 0; i < n; i++)
    {
      works += (size_t)ds[i].works;
      acq += !isnan (ds[i].t_sym_s) && !isnan (ds[i].t_code_s);
      fl += (size_t)ds[i].false_lock;
      al += (size_t)ds[i].aligned;
      slip += (size_t)(ds[i].aligned && ds[i].slipped);
      if (ds[i].aligned && !ds[i].slipped)
        {
          err += ds[i].errors;
          sc += ds[i].scored;
        }
      if (!isnan (ds[i].t_sym_s) && nt < sizeof t / sizeof *t)
        t[nt++] = ds[i].t_sym_s;
    }
  qsort (t, nt, sizeof *t, cmp_dbl);
  double wl, wh;
  dp_wilson ((double)works, (double)n, &wl, &wh);
  const dp_ber_ci_t ci = dp_ber_ci (err, sc, 0.95);
  const double      loss
      = sc && err
            ? ds[0].esn0 - dp_ber_esn0_db_for_ser (2, (double)err / (double)sc)
            : NAN;
  printf ("  %.1f GHz %4.1f dB %+4.0f Hz/s %-11s works %2zu/%zu [%.2f, "
          "%.2f], acquired %zu, false %zu; lock %.2f/%.2f/%.2f s; aligned "
          "%zu, slipped %zu; SER %.2e [%.2e, %.2e] (%lu/%lu), loss %.2f dB\n",
          ds[0].fc / 1e9, ds[0].esn0, ds[0].rate == 0.0 ? 0.0 : PI_RATE_HZ_S,
          ds[0].cw ? "code_period" : "partials", works, n, wl, wh, acq, fl,
          nt ? t[nt / 2] : NAN, nt ? t[(size_t)(0.9 * (double)nt)] : NAN,
          nt ? t[nt - 1] : NAN, al, slip, ci.p_hat, ci.lo, ci.hi, err, sc,
          loss);
}

/* The contract's cells: 10 dB, static, the receiver as built (the
   partials carrier window). The code-period window's code lock changes
   over the data on some 10 dB draws, so it is printed, not claimed. */
static void
check_cell (const draw_t *ds, size_t n)
{
  if (ds[0].esn0 < 10.0 || ds[0].rate != 0.0
      || ds[0].cw != ASYNC_DSSS_RX_CARRIER_PARTIALS)
    return;
  unsigned long err = 0, sc = 0;
  for (size_t i = 0; i < n; i++)
    {
      DP_CHECK_MSG (ds[i].works, "at 10 dB static every draw's link works");
      DP_CHECK_MSG (ds[i].aligned && !ds[i].slipped,
                    "and aligns without a slip");
      err += ds[i].errors;
      sc += ds[i].scored;
    }
  const dp_ber_ci_t ci = dp_ber_ci (err, sc, 0.95);
  DP_CHECK_MSG (sc > 0
                    && ci.hi < dp_ber_theory_ser (
                           2, pow (10.0, (ds[0].esn0 - PI_MARGIN_DB) / 10.0)),
                "its error rate is under the bound's PI_MARGIN_DB lower");
}

int
main (int argc, char **argv)
{
  int         check = 0, emit = 0;
  unsigned    draws = PI_DRAWS, shard = 0, shards = 1, spc = 0;
  double      secs = PI_DRAW_S, from_s = 0.0, to_s = PI_DRAW_S;
  const char *tlm_dir = NULL, *one = NULL;
  double      only_fc = NAN, only_es = NAN, only_rate = NAN; /* NAN: all */
  for (int i = 1; i < argc; i++)
    {
      check |= strcmp (argv[i], "--check") == 0;
      emit |= strcmp (argv[i], "--emit") == 0;
      if (strcmp (argv[i], "--draws") == 0 && i + 1 < argc)
        draws = (unsigned)strtoul (argv[++i], NULL, 10);
      else if (strcmp (argv[i], "--secs") == 0 && i + 1 < argc)
        secs = strtod (argv[++i], NULL);
      else if (strcmp (argv[i], "--spc") == 0 && i + 1 < argc)
        spc = (unsigned)strtoul (argv[++i], NULL, 10);
      else if (strcmp (argv[i], "--telemetry") == 0 && i + 1 < argc)
        tlm_dir = argv[++i];
      else if (strcmp (argv[i], "--draw") == 0 && i + 1 < argc)
        one = argv[++i];
      else if (strcmp (argv[i], "--fc") == 0 && i + 1 < argc)
        only_fc = strtod (argv[++i], NULL) * 1e9;
      else if (strcmp (argv[i], "--esn0") == 0 && i + 1 < argc)
        only_es = strtod (argv[++i], NULL);
      else if (strcmp (argv[i], "--rate") == 0 && i + 1 < argc)
        only_rate = strtod (argv[++i], NULL);
      else if (strcmp (argv[i], "--from") == 0 && i + 1 < argc)
        from_s = strtod (argv[++i], NULL);
      else if (strcmp (argv[i], "--to") == 0 && i + 1 < argc)
        to_s = strtod (argv[++i], NULL);
      else if (strcmp (argv[i], "--shard") == 0 && i + 1 < argc)
        DP_REQUIRE (sscanf (argv[++i], "%u/%u", &shard, &shards) == 2
                    && shards > 0 && shard < shards);
    }
  if (check && !tlm_dir) /* the sweep's spot check; a capture keeps --secs */
    {
      draws = 1;
      secs  = PI_CHECK_S;
    }
  DP_REQUIRE (draws >= 1 && draws <= PI_DRAWS * 4);
  uint8_t code[SF];
  dp_dsss_gold_1023 (code);

  static const double fcs[2]   = { 2.2e9, 2.8e9 };
  static const double esn0s[4] = { 4.0, 5.0, 7.0, 10.0 };
  static const double rates[2] = { 0.0, PI_RATE_HZ_S };

  /* One named draw with its telemetry: --telemetry DIR --draw
     FC_GHZ,ESN0,RATE,CW,SEED [--from S --to S]. The capture is the
     receiver's probes and the scene's truth over the window, beside it
     the five lock rules; the draw's row is printed as the sweep prints
     it, so the capture is of a row the record has. */
  if (tlm_dir)
    {
      double   g = 0, es = 0, rt = 0;
      int      cw = 0;
      unsigned sd = 0;
      DP_REQUIRE_MSG (
          one
              && sscanf (one, "%lf,%lf,%lf,%d,%u", &g, &es, &rt, &cw, &sd)
                     == 5,
          "--telemetry names its draw with --draw");
      /* One stem names the draw's files: the capture and its rules. */
      char stem[256], path[1100], rules[1100];
      (void)snprintf (stem, sizeof stem,
                      "pullin_%.1fGHz_%.0fdB_%.0fHzs_%s_s%u%s", g, es, rt,
                      cw ? "code_period" : "partials", sd,
                      spc ? (spc == 2 ? "_spc2" : "_spcN") : "");
      (void)snprintf (path, sizeof path, "%s/%s.tlm", tlm_dir, stem);
      (void)snprintf (rules, sizeof rules, "%s/%s.rules.csv", tlm_dir, stem);
      const capture_t cp = { path, rules, from_s, to_s };
      draw_t          d;
      DP_REQUIRE (run_draw (code, g * 1e9, es, rt, cw, sd, secs, spc, &cp, &d)
                  == 0);
      DP_CHECK_MSG (!d.data_ended, "the payload outlasted the run");
      /* --check (the ctest): the capture holds the receiver's probes and
         the truth, then is removed, so the run leaves nothing behind. */
      if (check)
        {
          FILE *fc = fopen (path, "rb");
          DP_CHECK_MSG (fc != NULL, "the capture was written");
          if (fc)
            {
              DP_CHECK (fseek (fc, 0, SEEK_END) == 0 && ftell (fc) > 0);
              (void)fclose (fc);
            }
          char aux[1200];
          (void)snprintf (aux, sizeof aux, "%s-meta", path);
          DP_CHECK (remove (aux) == 0);
          DP_CHECK (remove (rules) == 0);
          DP_CHECK (remove (path) == 0);
        }
      printf ("# pullin\n");
      emit_row (&d, sd);
      DP_TEST_EMIT_END ("validate_async_dsss_receiver_pullin --telemetry");
      DP_TEST_END ("validate_async_dsss_receiver_pullin --telemetry");
    }
  if (emit)
    printf ("# pullin\nfc_ghz,esn0_db,rate_hz_s,carrier_window,seed,f0_hz,"
            "frame_off,ho_err_hz,ho_src,t_track_s,t_sym_s,t_code_s,sym_tog,"
            "sym_tog1_s,code_tog,code_tog1_s,end_resid_hz,false_lock,"
            "aligned,slipped,errors,scored,evm_db,m2m4_db,works\n");
  else
    printf ("pull-in: 415.1-B-1, 11 MSa/s, 3000 sym/s, %u draw(s) of %.0f s "
            "a cell; C/N0 at the antenna; works = sustained, no change over "
            "the data, not false\n",
            draws, secs);
  draw_t *ds = dp_xmalloc (draws * sizeof *ds);
  for (size_t f = check ? 1 : 0; f < 2; f++)
    for (size_t e = check ? 3 : 0; e < 4; e++)
      for (size_t r = 0; r < (check ? 1u : 2u); r++)
        for (int cw = 0; cw < 2; cw++)
          {
            /* --fc / --esn0 / --rate: one slice of the grid. */
            if ((!isnan (only_fc) && fabs (fcs[f] - only_fc) > 1e6)
                || (!isnan (only_es) && fabs (esn0s[e] - only_es) > 1e-9)
                || (!isnan (only_rate) && fabs (rates[r] - only_rate) > 1e-9))
              continue;
            size_t n = 0;
            for (uint32_t s = 1; s <= draws; s++)
              {
                if ((s - 1) % shards != shard)
                  continue;
                DP_REQUIRE (run_draw (code, fcs[f], esn0s[e], rates[r], cw, s,
                                      secs, spc, NULL, &ds[n])
                            == 0);
                DP_CHECK_MSG (!ds[n].data_ended,
                              "the payload outlasted the run");
                if (emit)
                  emit_row (&ds[n], s);
                n++;
              }
            if (!n)
              continue;
            if (!emit)
              print_cell (ds, n);
            check_cell (ds, n);
          }
  free (ds);
  if (check)
    {
      /* The live carrier holds a 400 Hz/s ramp: at 10 dB, the receiver as
         built, the end's carrier is within the false-lock limit of the
         truth. The draw is sized so the hand-over's Doppler is past it. */
      draw_t rd;
      DP_REQUIRE (run_draw (code, 2.8e9, 10.0, PI_RATE_HZ_S,
                            ASYNC_DSSS_RX_CARRIER_PARTIALS, 1, PI_RAMP_CHECK_S,
                            spc, NULL, &rd)
                  == 0);
      const double lim = 0.25 * PI_SYM;
      printf ("  ramp: 2.8 GHz 10 dB +/-400 Hz/s partials, %.0f s: live "
              "carrier %.1f Hz off the truth; the hand-over's Doppler "
              "%.0f Hz off, %.2fx the %.0f Hz limit\n",
              PI_RAMP_CHECK_S, rd.end_resid_hz, rd.end_stale_hz,
              fabs (rd.end_stale_hz) / lim, lim);
      DP_CHECK_MSG (fabs (rd.end_stale_hz) > 1.2 * lim,
                    "the draw is long enough to tell the live carrier from "
                    "the hand-over's");
      DP_CHECK_MSG (!rd.false_lock, "the live carrier holds a 400 Hz/s ramp "
                                    "at 10 dB");
    }
  if (emit)
    DP_TEST_EMIT_END ("validate_async_dsss_receiver_pullin");
  DP_TEST_END ("validate_async_dsss_receiver_pullin");
}
