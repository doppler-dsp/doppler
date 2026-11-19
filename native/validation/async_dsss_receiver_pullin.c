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
 * Per cell (carrier x Es/N0 x dynamics): works, acquired
 * and false locks with Wilson 95% intervals; time to sustained symbol lock
 * as median, 90th percentile and worst; the pooled error rate of the
 * aligned, unslipped draws with its interval and the loss against the BPSK
 * bound it implies. The full grid is 2 x 4 x 2 = 16 cells, DRAWS draws
 * of DRAW_S each (`--draws`, `--secs`, `--shard i/n` split it across
 * processes for the record's run; `make validate-c` runs it whole).
 *
 * What is asserted. In every cell, 415.1-B-1
 * §7.4.1's acquisition requirement: PN lock -- the receiver's code lock --
 * within PI_ACQ_S of arrival at probability PI_ACQ_P. The sweep
 * DEMONSTRATES it: the cell's Wilson 95% lower limit at or above PI_ACQ_P,
 * over PI_DRAWS draws; a cell that cannot is a gap for the record, with
 * its issue. --check, one draw a cell, can only fail to refute it (the
 * upper limit there), and says it is a regression check that cannot
 * demonstrate the claim; so does a sharded run, whose cells are partial.
 * And, the same in the sweep and in --check,
 * where the contract claims a working link -- 10 dB, static, the receiver as
 * built -- every draw works, aligns and does not slip, and its error rate's
 * 95% upper limit is under the bound's 3 dB lower (a margin, not a derived
 * limit). The 4 and 5 dB cells, the use case's own floor, are printed with
 * their intervals and not claimed: they do not meet the zero-transition
 * criterion today. The payload outlasts every run. --check: 2.8 GHz (2.69
 * samples a chip), 10 dB, static, one 20 s draw.
 *
 * Telemetry: `--telemetry DIR --draw FC_GHZ,ESN0,RATE,SEED [--from S
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
 * Four more capture probes make a slip readable, each from what the
 * harness already holds:
 *   - err.car_unwrapped: err.car_deg's carrier error before the half-cycle
 *     fold, unwrapped block to block from the hand-over (degrees), so a pi
 *     slip is a 180-degree step;
 *   - rx.code.hyp: the code loop's chosen look -- the symbol aid's
 *     hypothesis (dp_dll_state_t::aid_best) once a block;
 *   - err.sym_lag: the symbols' lag against the payload, re-detected with
 *     dp_ber_sync() every PI_LV_RESYNC symbols near the newest, less the
 *     lag found at PI_LV_START symbols (0 aligned, +-1 a symbol slip);
 *   - err.count: the running count of symbol errors against the payload
 *     from that first alignment (dp_ber_score() on each block's new
 *     symbols), so a slip shows as the slope changing;
 *   - err.demod_car_deg: the carrier error the demodulator decides on --
 *     the pre-despread loop's error (err.car_deg's, before its fold) less
 *     the demodulator's LO phase (dp_lo_get_phase() of its matched front
 *     end, which multiplies its input by the LO), folded to BPSK's half
 *     cycle, once a block. The LO's phase is read at the block's start, so
 *     it lags the samples it mixes by the cascade's own delay.
 *
 * `--spc N` renders the scene at N samples a chip (2: the front end
 * interpolates, and the held chips' images pass its transition band) for
 * the comparison with the default 4. `--bn-carrier BN` tracks with the
 * pre-despread carrier loop at noise bandwidth BN per code period in
 * place of ASYNC_DSSS_RX_BN_CARRIER: the loop filter is re-initialised
 * through the receiver's state on each entry into tracking (integrator
 * kept), and each row records the bandwidth it ran at (bn_car).
 * `--bn-carrier rule` takes it from the owner's loop rule instead
 * (docs/design/async-dsss-receiver.md §1.3: about 20 dB of loop SNR at the
 * 4 dB floor, `rho - 10 log10(2 bn)`, with rho the per-update SNR there),
 * per carrier: pi_bn_rule(), the one place the rule is written.
 * `--loop-order N` names the carrier loop's order, recorded per row
 * (loop_order); 2 is the built loop. At 3, `--acq a|b|c` is how the loop
 * gets there (docs/design/async-dsss-receiver.md 4.9, "Acquisition"),
 * recorded per row (acq), with BN the tracking bandwidth (--bn-carrier's,
 * or the rule's, or the built one):
 *   a  enter tracking at order 2 at the built bandwidth; at the stint's
 *      first lock (both flags up, the coast's own event) switch to
 *      order 3 at BN (dp_costas_set_order: the frequency carries, the
 *      rate starts at 0);
 *   b  enter at order 3 at the built bandwidth; at the first lock narrow
 *      to BN (dp_costas_configure: frequency and rate kept);
 *   c  order 3 at BN from the entry into tracking (the default; the
 *      control);
 *   d  as a, but the switch waits until the order-2 loop has settled under
 *      the ramp (its error's mean over 5 / BN updates within 3 sigma /
 *      sqrt(n) of the design lag x / ki), then the integrator's slope over
 *      the next 5 / BN updates (slope_settled_hz_s; the seed with
 *      --seed-rate);
 *   e  at the first lock, order 3 at the derived wide bandwidth, held the
 *      derived time, then narrowed to BN in one configure;
 *   f  as e, narrowed in two steps (the geometric mean, held 5 / bn_mid
 *      updates, then BN).
 * The wide bandwidth and the holds are pi_switch_design()'s, printed in the
 * `# switch` section. t_narrow_s is when the narrow order-3 loop is in
 * place; rate_pre/post_narrow_hz_s the rate either side of the last
 * narrowing (configure keeps it).
 * Each row records when design a or b switched (t_switch_s), the slope of
 * the order-2 loop's frequency over the stint up to then (slope_hz_s, a
 * least-squares fit over the blocks, Hz/s) and the rate the third-order
 * filter holds at the end (rate_end_hz_s). `--seed-rate` (design a) seeds
 * the rate at the switch from that slope (dp_costas_set_norm_rate); without
 * it the rate starts at 0. A capture carries car.order and car.rate_hz_s
 * once a block.
 *
 * `--design` prints only the `# design` section, which --emit also prints:
 * per carrier, level and order, the loop the rule and the run's
 * bandwidth ask for, read through the shipped exact functions
 * (dp_state_filter_loop_bandwidth / _loop_stable / _steady_error, with
 * dp_loop_filter_init's and dp_state_filter_init's gains): the detector's
 * slope at that level (a Monte Carlo of the error the receiver forms, on
 * the shipped awgn), the delivered bandwidth at that slope and at the
 * delays the lag instrument measured, the loop SNR in the rule's
 * convention and on the squared phase with the slope referred (reported,
 * not designed to), order 3's lower gain edge, and the ramp's lag.
 *
 * Usage:  validate_async_dsss_receiver_pullin [--check] [--emit]
 *             [--draws N] [--secs S] [--shard i/n] [--spc N]
 *             [--bn-carrier BN|rule] [--loop-order 2|3 [--acq a..f]
 *              [--seed-rate]]
 *             [--design]
 *             [--fc GHZ] [--esn0 DB] [--rate HZ_S]   (a slice of the grid)
 *             [--telemetry DIR --draw FC,ESN0,RATE,SEED [--from S]
 *              [--to S]]
 */
#include "async_dsss_receiver_common.h"
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/ber/ber_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include "doppler/dp_tlm_capture/dp_tlm_capture_core.h"
#include "doppler/loop_filter/loop_filter_core.h"
#include "doppler/state_filter/state_filter_core.h"
#include "doppler/timing/timing_core.h"
#include "dp_air_test.h"
#include "dp_ber_test.h"
#include "dp_dsss_test.h"
#include "dp_s415_test.h"
#include "dp_stats_test.h"
#include "dp_sym_test.h"
#include "dp_test.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI_PUSH 4096u
#define PI_SUSTAIN 200u /* blocks: 74 ms */
/* Draws a cell in the sweep: 60, the count the floor brief's paired-seed
   run makes, and the least at which 415.1-B-1 §7.4.1 can be DEMONSTRATED
   with one miss allowed -- 59 of 60 reads a Wilson 95% lower limit of
   0.911, 58 of 60 reads 0.886 (check_acq()). */
#define PI_DRAWS 60u
#define PI_DRAW_S 60.0
#define PI_CHECK_S 20.0
#define PI_MARGIN_DB 3.0
/* 415.1-B-1 §7.4.1, the acquisition requirement: short-code PN lock (the
   receiver's code lock, get_code_locked()) with probability at least
   PI_ACQ_P within PI_ACQ_S of arrival, the signal within +/-1,500 Hz. */
#define PI_ACQ_S 20.0
#define PI_ACQ_P 0.9
#define PI_MAX_CELL 64u
/* The excursion levels a change's second before is read against, deg of
   carrier phase: short of the fold, where a fast excursion already costs
   the demodulator cos(phi) of the symbol's amplitude. */
static const double PI_EXC_DEG[3] = { 30.0, 45.0, 60.0 };
/* The --check ramp draw: long enough that the hand-over's Doppler, frozen,
   is past the false-lock limit (a quarter of the symbol rate, 750 Hz) at
   400 Hz/s with margin, so the test can tell the live carrier from it. */
#define PI_RAMP_CHECK_S 3.0
/* The #3134 pin (--check): the seed whose code lock changes earliest in
   the 60-draw slice, and a draw that covers that change. */
#define PI_CL_SEED 5u
#define PI_CL_CHECK_S 2.0

typedef struct
{
  double fc, esn0, rate;
  double f0;
  size_t frame_off;
  double ho_err_hz;
  int    ho_src;
  /* How close acquisition brings the receiver before any loop sees it,
     against the channel's truth: the search's own seed at the sample the
     chain is built from (its Doppler, its code phase in chips, the
     dwell-start convention of rx.phase.code), the code loop's starting
     error at the first block after the hand-over (within a block, 0.37
     ms, of it), and the hand-over's age: the refine's span from the seed
     to the hand-over, over which a ramp moves the truth by rate x age. */
  double seed_err_hz, seed_code_chip, seed_t_s;
  double ho_code_chip, ho_age_s;
  /* What they are read against, from the objects: the search's Doppler
     row (dp_correlator_get_doppler_res_hz()) and half a search sample in
     chips; the carrier loop's unambiguous range 1/(4T) (its squared
     detector over one code period) and its pull-in
     (ASYNC_DSSS_RX_CARRIER_PULLIN_HZ); the code discriminator's half
     spacing. */
  double base_row_hz, base_half_sample_chip, base_unamb_hz, base_pullin_hz;
  double base_half_spacing_chip;
  double bn_car;     /* the carrier loop's bandwidth the draw tracked with */
  int    loop_order; /* the carrier loop's order the draw tracked with */
  char   acq;        /* --acq at order 3, '-' at order 2 */
  double t_switch_s; /* design a/b: when the loop went to its tracking
                        form, s into the run (NAN: never) */
  double slope_hz_s; /* the order-2 loop's frequency slope up to then */
  double rate_end_hz_s; /* the order-3 rate at the end, Hz/s */
  double t_narrow_s;    /* the narrow order-3 loop in place, s (NAN: never) */
  double rate_pre_narrow_hz_s, rate_post_narrow_hz_s; /* e/f: across the
                                                         last narrowing */
  double slope_settled_hz_s; /* d: the slope over the settled window */
  int    seed_rate;          /* --seed-rate */
  size_t code_nl;            /* the code lock's look count it ran with */
  size_t win_n, win_natural; /* settled blocks; of them, the look-back
                                kept the natural (whole-epoch) window */
  double t_track_s, t_sym_s, t_code_s; /* NAN: never */
  size_t sym_tog, code_tog;
  /* The other three lock indicators the receiver exposes (owner: no lock
     indicator toggles in the data once acquired; it names no subset):
     the carrier loop's (Costas), the demodulator's carrier lock and its
     timing sync's. Each: changes over the data after its own sustained
     lock, its last change (NAN: none) and whether it is up at the end. */
  size_t car_tog, dem_tog, syn_tog;
  double car_last_s, dem_last_s, syn_last_s;
  int    car_up_end, dem_up_end, syn_up_end;
  double sym_tog1_s, code_tog1_s; /* NAN: none */
  double end_resid_hz;
  double end_stale_hz; /* the hand-over's Doppler against the end's
                          truth: what end_resid_hz would read if it
                          took get_doppler_hz() (the --check margin) */
  int           false_lock, aligned, slipped, end_unsynced, works, data_ended;
  unsigned long errors, scored;
  double        evm_db, m2m4_db;
  /* The carrier loop's stress against the truth (degrees of carrier
     phase; the detector's period is 180, its fold +/-90), and the code
     loop's (chips): steady from 1 s after the hand-over over the blocks on
     the emitter's code (within half a chip), the transient over the first
     0.5 s, with the design's baselines beside them. */
  double car_mean_deg, car_sd_deg, code_mean_chip, code_sd_chip;
  size_t car_slips, ho_slips; /* steps past the fold: steady, transient */
  double cross_win_frac;  /* of the settled blocks on the emitter, the share
                             whose second before holds a fold crossing */
  double exc_win_frac[3]; /* and the share whose second before holds a
                             carrier error past each of PI_EXC_DEG */
  double lock_mem_s;      /* the symbol-lock statistic's memory, 1 /
                             lock_alpha symbols, read from the receiver */
  double exc_mem_frac[3]; /* the share whose lock_mem_s before holds a
                             carrier error past each of PI_EXC_DEG */
  double coast_mem_frac;  /* the share whose lock_mem_s before holds a
                             coasting block: the rate a change's coast_mem
                             is read against */
  double off_code_frac;   /* of the tracking blocks from 1 s after the
                             hand-over, the share more than half a chip off
                             the emitter's code */
  double ho_peak_deg;     /* the largest error over the hand-over's 0.5 s */
  double steady_peak_deg; /* and over 0.5 s from 1 s after: the jitter's
                             own peak, what the hand-over's is read against */
  double base_lag_deg, base_sd_deg, base_ho_deg;
} draw_t;

/* What the report draws, accumulated in C over a cell's draws: each lock
   decision's statistic (tracking, data blocks, from 1 s after the
   hand-over), each search or refine test's statistic over its threshold
   (every test), and the loops' errors (tracking, from 1 s after the
   hand-over; the demod's two are its own detector outputs, no truth). */
enum
{
  PQ_SYM,
  PQ_CAR,
  PQ_CODE,
  PQ_DEMOD,
  PQ_SYNC,
  PQ_ACQ,
  PQ_REF,
  PQ_REFSQ,
  PQ_CAR_ERR,
  PQ_CODE_ERR,
  PQ_DCAR_E,
  PQ_DSYNC_E,
  PQ_LAG,
  PQ_N
};
static const struct
{
  const char *name;  /* the report's name for it */
  const char *probe; /* the receiver probe under "rx." (NULL: computed) */
  const char *thr;   /* the probe it is divided by, or NULL */
  double      lo, hi;
  size_t      nb;
} PQ[PQ_N] = {
  { "sym.lock", "rx.sym.lock", NULL, -1.0, 1.0, 100 },
  { "car.lock", "rx.car.lock", NULL, -1.0, 1.0, 100 },
  { "code.lock", "rx.code.lock", NULL, 0.0, 30.0, 120 },
  { "demod.lock", "rx.demod.lock", NULL, -1.0, 1.0, 100 },
  { "demod.sync.lock", "rx.demod.sync.lock", NULL, -1.0, 1.0, 100 },
  { "acq.stat/gate", "rx.acq.stat", "rx.acq.gate", 0.0, 5.0, 100 },
  { "ref.stat/thr", "rx.ref.stat", "rx.ref.thr", 0.0, 5.0, 100 },
  { "ref_sq.stat/thr", "rx.ref_sq.stat", "rx.ref_sq.thr", 0.0, 5.0, 100 },
  { "car.err_deg", NULL, NULL, -90.0, 90.0, 90 }, /* x sign(ramp) */
  { "code.err_chip", NULL, NULL, -0.5, 0.5, 100 },
  { "demod.car.e", "rx.demod.car.e", NULL, -1.0, 1.0, 100 },
  { "demod.sync.e", "rx.demod.sync.e", NULL, -1.25, 1.25, 100 },
  { "lag.epoch_T", NULL, NULL, 0.0, 1.0, 40 }, /* see the binning */
};

/* One change of a flag users act on, after it was sustained, with what
   the report attributes it by: the flag's statistic at the block it
   changed in, against its rule; the carrier loop's stress over the second
   before (its largest error, its rms, steps past the fold) and the code
   loop's largest error; and the symbols' EVM over +/-0.5 s around it
   (dp_test_evm_db_hard_range(); NAN when the track does not span it). */
typedef struct
{
  double            fc, esn0, rate;
  uint32_t          seed;
  int               flag, up; /* 0 sym, 1 code; the new value */
  double            t_s, stat;
  dp_lockdet_rule_t rule;
  double            car_max_deg, car_rms_deg, code_max_chip;
  double            car_max_mem_deg; /* over the lock statistic's memory */
  int coast_mem, coast_1s; /* the receiver coasted in the change's block or
                              in the lock memory / the second before it */
  size_t car_slips;
  double evm_db, draw_evm_db;
  int    false_lock; /* the draw's verdict */
  int    on_code;    /* the code within half a chip there */
} trans_t;

/* The whole run's histograms and transitions, printed at the end so the
   emitted sections stay whole. */
#define PI_CELLS 32u
/* [cell][population][quantity]. Population 0, ON the emitter: a block
   whose code error is within half a chip, of a draw that is not a false
   lock. Population 1, OFF it: the rest -- a receiver on another code
   phase (a sidelobe, a re-acquisition elsewhere) or on the carrier's
   alias tracks something else, and its statistics are another
   distribution, not the tail of this one. */
static dp_hist_t pi_hist[PI_CELLS][2][PQ_N];
static double    pi_cell_key[PI_CELLS][3];
/* --bn-carrier: the pre-despread carrier loop's noise bandwidth (per code
   period) the draws track with, in place of the built
   ASYNC_DSSS_RX_BN_CARRIER; NAN: as built. */
static double pi_bn_car = NAN;
/* The code lock's look count forced on each entry into tracking, for the
   #3134 control (0: as built, dp_det_n_noncoh()'s sizing). */
static size_t pi_code_nl = 0;
/* --bn-carrier rule: the bandwidth comes from pi_bn_rule() per carrier. */
static int pi_bn_rule_on = 0;
/* --loop-order: the carrier loop's order (2: the built loop). */
static int pi_loop_order = 2;
/* --acq: how an order-3 loop is reached ('a', 'b' or 'c'; see the file
   comment). */
static char pi_acq = 'c';
/* --seed-rate: design a seeds the order-3 rate at the switch from the
   order-2 loop's frequency slope over the acquisition. */
static int pi_seed_rate = 0;

/* The owner's loop rule, docs/design/async-dsss-receiver.md §1.3: a loop
   SNR of PI_LOOP_SNR_DB at the floor, in the rule's convention
   `rho - 10 log10(2 bn)`, rho the per-update SNR (C/N0 at the antenna
   times the update period). The one place the rule is written; every
   bandwidth the rule sets comes from here. */
#define PI_LOOP_SNR_DB 20.0
#define PI_FLOOR_ESN0_DB 4.0
static double
pi_bn_rule (double rho_floor)
{
  return rho_floor / (2.0 * pow (10.0, PI_LOOP_SNR_DB / 10.0));
}

/* The per-update SNR rho = C/N0 T for a carrier fc at Es/N0 esn0. */
static double
pi_rho (double fc, double esn0)
{
  const double chip = dp_s415_chip_rate (fc), T = (double)SF / chip;
  return pow (10.0, (dp_s415_cn0 (esn0)) / 10.0) * T;
}
/* The five lock rules each cell's flags were decided by (the same for
   every draw of a cell: the configuration sets them). */
#define PI_RULES 5
static const char *const pi_rule_name[PI_RULES]
    = { "sym", "car", "code", "demod", "demod_sync" };
static dp_lockdet_rule_t pi_rule[PI_CELLS][PI_RULES];
static size_t            pi_n_cells;
static trans_t          *pi_trans;
static size_t            pi_n_trans, pi_cap_trans;

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

/* What "the second before held X" is read against: of the settled
   blocks on the emitter (tracking from 1 s after the hand-over, within
   half a chip of the emitter's code), the share whose second before --
   the blocks [b - w1, b), the window a change's attribution reads --
   holds a block marked in `ind`. */
static double
win_frac (const uint8_t *ind, const float *e_car, const float *e_cod,
          size_t n_blk, size_t w1, double blk_s, double t_track_s)
{
  size_t in_win = 0, hit = 0, all = 0;
  for (size_t b = 0; b < n_blk; b++)
    {
      if (b >= 1)
        in_win += ind[b - 1];
      if (b > w1)
        in_win -= ind[b - 1 - w1];
      const double t = (double)b * blk_s;
      if (isnan (e_car[b]) || isnan (t_track_s) || t < t_track_s + 1.0
          || fabs (e_cod[b]) > 0.5f)
        continue;
      all++;
      hit += in_win > 0;
    }
  return all ? (double)hit / (double)all : NAN;
}

/* One draw: the receiver on `secs` of the scene. */
/* The detector's mean slope at per-update SNR rho with kw partials: the
   error the receiver forms (adr_track_period: each partial squared, the
   squares summed, half the angle, its sine), on a carrier at +/-PI_SLOPE_D
   rad, the noise the shipped awgn at rho/kw a partial. Common noise for
   both signs, so the difference is the slope and not two noise draws. */
#define PI_SLOPE_N 200000
#define PI_SLOPE_D 0.05
static double
pi_slope (double rho, int kw, uint64_t seed)
{
  const float amp = dp_awgn_amplitude_for_snr (
      (float)(10.0 * log10 (rho / (double)kw)), 1.0f);
  dp_awgn_state_t *g = dp_awgn_create (seed, amp);
  DP_REQUIRE (g != NULL);
  const float _Complex sp = cexpf (I * (float)PI_SLOPE_D), sm = conjf (sp);
  float _Complex n[4];
  double acc = 0.0;
  for (int i = 0; i < PI_SLOPE_N; i++)
    {
      DP_REQUIRE (dp_awgn_generate (g, (size_t)kw, n, 4) == (size_t)kw);
      float _Complex qp = 0.0f, qm = 0.0f;
      for (int k = 0; k < kw; k++)
        {
          const float _Complex a = sp + n[k], b = sm + n[k];
          qp += a * a, qm += b * b;
        }
      acc += sin (0.5 * atan2 (cimagf (qp), crealf (qp)))
             - sin (0.5 * atan2 (cimagf (qm), crealf (qm)));
    }
  dp_awgn_destroy (g);
  return acc / ((double)PI_SLOPE_N * 2.0 * PI_SLOPE_D);
}

/* The order-3 switch, derived (docs/design/async-dsss-receiver.md 4.9),
   once per carrier from what create knows: the update period T, the
   floor's per-update SNR, the detector's slope g there (pi_slope), the
   rule's tracking bandwidth and the specified ramp R (DP_S415_RATE_HZ_S, the
   receiver's doppler_rate). Rates and phases in the loop's own units:
   x = 2 pi R T^2 radians per update^2.
     - lag_e: the order-2 loop's steady error under the ramp at the
       bandwidth it runs at until the switch (the built
       ASYNC_DSSS_RX_BN_CARRIER), x / ki, in the detector's units (what
       design d's settled test holds its mean to), and psi0 the phase it
       means, asin(lag_e / g): where order 3 starts at a switch.
     - The switch transient: the shipped StateFilter step in the canonical
       loop (an accumulating oscillator, a detector e = g sin(psi) one
       update late), started locked in frequency, rate 0, at psi0, the
       input frequency ramping x an update. Its peak |psi|, and the
       updates until the rate is within 10% of x and |psi| under
       PI_SW_SETTLE_DEG for good.
     - bn_wide: the narrowest bandwidth from the tracking one up to
       STATE_FILTER_BNT_MAX whose peak stays under PI_SW_PEAK_FRAC of the
       fold (90 degrees on the carrier phase), and hold the time it needs
       there before narrowing; bn_mid the geometric mean for the two-step
       narrowing, held 5 / bn_mid updates (the house settling figure). */
#define PI_SW_PEAK_FRAC 0.25
#define PI_SW_SETTLE_DEG 11.25
#define PI_SW_DELAY 1
typedef struct
{
  double T, rho, g, x, bn_track, ki2, lag_e, psi0;
  double peak_track_deg, settle_track_s; /* design a's transient */
  double bn_wide, peak_wide_deg, sd_wide_deg, hold_s;
  double bn_mid, hold_mid_s;
  double w_s; /* design d: 5 / bn_track updates, s */
} pi_switch_t;

static double
pi_sw_transient (double bn, const pi_switch_t *sw, double *settle_upd)
{
  dp_state_filter_state_t sf;
  dp_state_filter_init (&sf, bn, 1.0);
  const size_t n_max = (size_t)(200.0 / bn) + 1000;
  double       hist[PI_SW_DELAY + 1], psi = sw->psi0, peak = fabs (psi);
  size_t       last_bad = 0;
  for (int i = 0; i <= PI_SW_DELAY; i++)
    hist[i] = psi;
  for (size_t n = 0; n < n_max; n++)
    {
      const double e = sw->g * sin (hist[PI_SW_DELAY]);
      const double c = dp_state_filter_step (&sf, e);
      psi += sw->x * (double)n - c;
      for (int i = PI_SW_DELAY; i > 0; i--)
        hist[i] = hist[i - 1];
      hist[0] = psi;
      if (fabs (psi) > peak)
        peak = fabs (psi);
      if (fabs (sf.rate - sw->x) > 0.1 * fabs (sw->x)
          || fabs (psi) * 180.0 / M_PI > PI_SW_SETTLE_DEG)
        last_bad = n + 1;
      if (fabs (psi) > M_PI)
        break; /* past the fold: not a transient */
    }
  *settle_upd = (double)last_bad;
  return peak * 180.0 / M_PI;
}

/* Design d's window, 5 / BN updates at the rule's bandwidth, in seconds:
   the house settling figure, without the rest of pi_switch_design(). */
static double
pi_switch_window_s (double fc)
{
  const double T = (double)SF / dp_s415_chip_rate (fc);
  return 5.0 / pi_bn_rule (pi_rho (fc, PI_FLOOR_ESN0_DB)) * T;
}

static void
pi_switch_design (double fc, pi_switch_t *sw)
{
  const double chip = dp_s415_chip_rate (fc);
  sw->T             = (double)SF / chip;
  sw->rho           = pi_rho (fc, PI_FLOOR_ESN0_DB);
  sw->g             = pi_slope (sw->rho, 1, 5501u + (uint64_t)(fc / 1e8));
  sw->x             = 2.0 * M_PI * DP_S415_RATE_HZ_S * sw->T * sw->T;
  sw->bn_track      = pi_bn_rule (sw->rho);
  dp_loop_filter_state_t lf;
  dp_loop_filter_init (&lf, ASYNC_DSSS_RX_BN_CARRIER, 0.707, 1.0);
  sw->ki2   = lf.ki;
  sw->lag_e = sw->x / lf.ki;
  sw->psi0  = asin (fmin (1.0, sw->lag_e / sw->g));
  double s;
  sw->peak_track_deg = pi_sw_transient (sw->bn_track, sw, &s);
  sw->settle_track_s = s * sw->T;
  sw->bn_wide = NAN, sw->peak_wide_deg = NAN, sw->hold_s = NAN;
  for (int i = 0; i <= 400; i++)
    {
      const double bn = sw->bn_track
                        * pow (STATE_FILTER_BNT_MAX / sw->bn_track, i / 400.0);
      const double pk = pi_sw_transient (bn, sw, &s);
      if (pk <= PI_SW_PEAK_FRAC * 90.0)
        {
          sw->bn_wide = bn, sw->peak_wide_deg = pk, sw->hold_s = s * sw->T;
          break;
        }
    }
  /* The jitter there: the squared-phase model of the # design rows
     (noise referred to phase through g; code period, Kw = 1). */
  sw->sd_wide_deg = NAN;
  if (!isnan (sw->bn_wide))
    {
      dp_state_filter_state_t sf;
      dp_state_filter_init (&sf, sw->bn_wide, 1.0);
      const double bd = dp_state_filter_loop_bandwidth (
          3, sf.kp, sf.ki, sf.kr, (double)PI_SW_DELAY, sw->g);
      sw->sd_wide_deg = 180.0 / M_PI
                        * sqrt (bd / (sw->g * sw->g * sw->rho)
                                * (1.0 + 1.0 / (2.0 * sw->rho)));
      sw->bn_mid      = sqrt (sw->bn_wide * sw->bn_track);
      sw->hold_mid_s  = 5.0 / sw->bn_mid * sw->T;
    }
  sw->w_s = 5.0 / sw->bn_track * sw->T;
}

/* The use case's dynamics (docs/design/async-dsss-receiver.md, "The
   dynamics the receiver is specified for"; owner 11-17: LEO, Doppler
   pre-compensated, the residual offset uniform in +/-DP_S415_SPAN_HZ, its rate
   a conservative envelope from an ephemeris error model). A LEO pass is a
   circular orbit over a spherical Earth seen from a fixed point above a
   PI_PASS_MASK_DEG elevation mask, Earth rotation ignored (its worst case
   is the factor pi_pass_rotation()); the passes in scope are those whose
   RAW peak rate is the specification's PI_PASS_RATE_SPEC, one a grid
   altitude with its maximum elevation solved. Over them:
     - timing: the along-track error, residual f(t) - f(t - dt) for every
       dt up to the one whose residual reaches DP_S415_SPAN_HZ, both branches;
     - proportional: the cross-check, residual eps f(t) for every eps up to
       DP_S415_SPAN_HZ over the pass's visible peak offset.
   R_env is pi_r_env(): the timing envelope's maximum (the ephemeris error
   model, owner 11-17), times the rotation factor, rounded up to the whole
   Hz/s. The proportional model is its cross-check, printed with the pass
   that drives it, and does not set it. */
#define PI_PASS_MASK_DEG 10.0
#define PI_PASS_RATE_SPEC 400.0
#define PI_PASS_RE_KM 6371.0
#define PI_PASS_MU 398600.4418
#define PI_PASS_C_KMS 299792.458
#define PI_PASS_OMEGA_E 7.2921e-5
#define PI_PASS_DT 0.02
#define PI_PASS_TRIM 25
#define PI_PASS_BINS                                                          \
  15 /* |offset| bins of DP_S415_SPAN_HZ / PI_PASS_BINS Hz */

/* One pass's residual-free curve: f, f' and f'' (Hz, Hz/s, Hz/s^2) on a
   PI_PASS_DT grid above the mask, the range rate in closed form and every
   end point a numerical derivative touches trimmed. Returns the point
   count (0: the pass never clears the mask); *v the orbital speed, km/s. */
static size_t
pi_pass_curve (double h_km, double el_deg, double fc, double **f, double **f1,
               double **f2, double *v)
{
  const double rs = PI_PASS_RE_KM + h_km,
               w  = sqrt (PI_PASS_MU / (rs * rs * rs));
  const double e = el_deg * M_PI / 180.0, em = PI_PASS_MASK_DEG * M_PI / 180.0;
  const double g0 = acos (PI_PASS_RE_KM * cos (e) / rs) - e;
  const double gm = acos (PI_PASS_RE_KM * cos (em) / rs) - em;
  *v              = rs * w;
  if (g0 >= gm)
    return 0;
  const double tmax = acos (cos (gm) / cos (g0)) / w;
  const size_t n    = (size_t)(2.0 * tmax / PI_PASS_DT);
  if (n < 4 * PI_PASS_TRIM)
    return 0;
  double *a = dp_xmallocn (n, sizeof *a), *b = dp_xmallocn (n, sizeof *b),
         *c = dp_xmallocn (n, sizeof *c);
  for (size_t i = 0; i < n; i++)
    {
      const double t  = -tmax + (double)i * PI_PASS_DT;
      const double cg = cos (g0) * cos (w * t);
      const double d  = sqrt (PI_PASS_RE_KM * PI_PASS_RE_KM + rs * rs
                              - 2.0 * PI_PASS_RE_KM * rs * cg);
      a[i]            = -fc / PI_PASS_C_KMS * PI_PASS_RE_KM * rs * cos (g0) * w
                        * sin (w * t) / d;
    }
  for (size_t i = 1; i + 1 < n; i++)
    b[i] = (a[i + 1] - a[i - 1]) / (2.0 * PI_PASS_DT);
  for (size_t i = 2; i + 2 < n; i++)
    c[i] = (b[i + 1] - b[i - 1]) / (2.0 * PI_PASS_DT);
  const size_t m = n - 2 * PI_PASS_TRIM;
  memmove (a, a + PI_PASS_TRIM, m * sizeof *a);
  memmove (b, b + PI_PASS_TRIM, m * sizeof *b);
  memmove (c, c + PI_PASS_TRIM, m * sizeof *c);
  *f = a, *f1 = b, *f2 = c;
  return m;
}

static double
pi_pass_peak_rate (double h_km, double el_deg, double fc)
{
  double      *f, *f1, *f2, v, pk = 0.0;
  const size_t n = pi_pass_curve (h_km, el_deg, fc, &f, &f1, &f2, &v);
  for (size_t i = 0; i < n; i++)
    pk = fmax (pk, fabs (f1[i]));
  if (n)
    free (f), free (f1), free (f2);
  return pk;
}

/* The pass at altitude h whose raw peak rate is PI_PASS_RATE_SPEC: its
   maximum elevation by bisection; NAN when even overhead stays under. */
static double
pi_pass_el (double h_km, double fc)
{
  /* In scope only where some pass at h has exactly that peak: not when
     even overhead stays under it, nor when even the lowest pass above the
     mask already exceeds it. */
  double lo = PI_PASS_MASK_DEG + 0.5, hi = 89.9;
  if (pi_pass_peak_rate (h_km, hi, fc) < PI_PASS_RATE_SPEC
      || pi_pass_peak_rate (h_km, lo, fc) > PI_PASS_RATE_SPEC)
    return NAN;
  for (int i = 0; i < 30; i++)
    {
      const double m = 0.5 * (lo + hi);
      if (pi_pass_peak_rate (h_km, m, fc) > PI_PASS_RATE_SPEC)
        hi = m;
      else
        lo = m;
    }
  return lo;
}

typedef struct
{
  double step_km;
  int    n_pass;
  double env[PI_PASS_BINS]; /* timing: largest |rate| by |offset| bin */
  double timing_max, prop_max, v_min, f2_max;
  double res_f2_max;     /* timing: the largest |residual rate'|, Hz/s^2 */
  double raw_ppm_max[2]; /* the raw |offset| / carrier, ppm, per carrier */
  double prop_h_km, prop_el_deg, prop_fc, prop_eps; /* its driving pass */
} pi_pass_env_t;

/* The envelope over every in-scope pass on a step_km altitude grid from
   350 km, at both carriers. */
static void
pi_pass_envelope (double step_km, pi_pass_env_t *pe)
{
  memset (pe, 0, sizeof *pe);
  pe->step_km                = step_km;
  pe->v_min                  = INFINITY;
  static const double fcs[2] = { DP_S415_FC_LO, DP_S415_FC_HI };
  for (int k = 0; k < 2; k++)
    for (double h = 350.0; h <= 1200.0 + 1e-9; h += step_km)
      {
        const double el = pi_pass_el (h, fcs[k]);
        if (isnan (el))
          continue;
        double      *f, *f1, *f2, v;
        const size_t n = pi_pass_curve (h, el, fcs[k], &f, &f1, &f2, &v);
        if (!n)
          continue;
        pe->n_pass++;
        pe->v_min  = fmin (pe->v_min, v);
        double fpk = 0.0, f1pk = 0.0;
        for (size_t i = 0; i < n; i++)
          {
            fpk        = fmax (fpk, fabs (f[i]));
            f1pk       = fmax (f1pk, fabs (f1[i]));
            pe->f2_max = fmax (pe->f2_max, fabs (f2[i]));
          }
        /* timing: lag j samples, j stepping 0.05 s, until the residual
           first passes the bound */
        for (size_t j = 1; j < n; j += (size_t)(0.05 / PI_PASS_DT))
          {
            double rmax = 0.0;
            for (size_t i = j; i < n; i++)
              rmax = fmax (rmax, fabs (f[i] - f[i - j]));
            if (rmax > DP_S415_SPAN_HZ)
              break;
            for (size_t i = j; i < n; i++)
              {
                const double o  = fabs (f[i] - f[i - j]);
                int          bi = (int)(o / (DP_S415_SPAN_HZ / PI_PASS_BINS));
                if (bi >= PI_PASS_BINS)
                  bi = PI_PASS_BINS - 1;
                pe->env[bi] = fmax (pe->env[bi], fabs (f1[i] - f1[i - j]));
                pe->res_f2_max
                    = fmax (pe->res_f2_max, fabs (f2[i] - f2[i - j]));
              }
          }
        pe->raw_ppm_max[k] = fmax (pe->raw_ppm_max[k], fpk / fcs[k] * 1e6);
        /* proportional: the largest residual rate is eps_max f'_max */
        if (DP_S415_SPAN_HZ / fpk * f1pk > pe->prop_max)
          {
            pe->prop_max    = DP_S415_SPAN_HZ / fpk * f1pk;
            pe->prop_h_km   = h;
            pe->prop_el_deg = el;
            pe->prop_fc     = fcs[k];
            pe->prop_eps    = DP_S415_SPAN_HZ / fpk;
          }
        free (f), free (f1), free (f2);
      }
  for (int i = 0; i < PI_PASS_BINS; i++)
    pe->timing_max = fmax (pe->timing_max, pe->env[i]);
}

/* Earth rotation, ignored by the pass model: at most the equatorial
   surface speed against the slowest in-scope orbit. */
static double
pi_pass_rotation (double v_min_kms)
{
  return 1.0 + PI_PASS_OMEGA_E * PI_PASS_RE_KM / v_min_kms;
}

/* R_env, the rate a pass draw holds, Hz/s: the timing envelope's maximum
   times the rotation factor, rounded up to the whole Hz/s. */
static double
pi_r_env (double timing_max, double rotation)
{
  return ceil (timing_max * rotation);
}

/* --dynamics pass: R_env for this run (0 until main computes it). */
static double pi_pass_r_env = 0.0;
/* --dynamics: "pass" (the use case: static, and the constant R_env) or
   "ramp" (the constant DP_S415_RATE_HZ_S, outside the use case: owner 11-17,
   "We don't want to test what will never happen"; a diagnostic only). */
static int pi_dyn_pass = 0;

/* The capture's err.sym_lag and err.count, live: once PI_LV_START symbols
   are out, the symbols are aligned against the payload with dp_ber_sync()
   on a marker placed after them (dp_ber_marker_place(), as the scoring
   below does); from then each block's new symbols are scored with
   dp_ber_score() and the running count emitted, and every PI_LV_RESYNC
   symbols the alignment is re-detected near the newest and its lag, less
   the first's, emitted. The truth is built once, from the emitter's own
   bits from about the first symbol out, as the scoring's is. */
#define PI_LV_START 9000u  /* 3 s of symbols: past settling at the floor */
#define PI_LV_RESYNC 1500u /* half a second */
static void
pi_live_align (const dp_air_t *air, size_t first_in, double secs,
               const float complex *syms, size_t ns, uint8_t **truth,
               size_t *n_truth, int *ok, dp_ber_marker_t *mk,
               dp_ber_sync_t *sy, dp_ber_t *acc, size_t *scored, size_t *next,
               dp_tlm_t *tlm, int id_lag, int id_cnt)
{
  const size_t guard = (size_t)DP_BER_LAG_SPAN + DP_BER_SYNC_SYMS;
  if (!*truth)
    {
      long base = dp_air_truth (air, 0, first_in).symbol - 60;
      if (base < 0)
        base = 0;
      *n_truth = (size_t)(secs * DP_S415_SYM) + 1000;
      *truth   = dp_xmalloc (*n_truth);
      for (size_t i = 0; i < *n_truth; i++)
        (*truth)[i]
            = (uint8_t)(dp_air_symbol_bit (air, 0, base + (long)i) > 0);
    }
  if (!*ok)
    {
      if (ns < PI_LV_START + 2 * guard)
        return;
      const size_t at
          = dp_ber_marker_place (*truth, *n_truth, 2, PI_LV_START, 0,
                                 DP_BER_LAG_SPAN, DP_BER_SYNC_PFA, 1);
      mk->t0 = at == SIZE_MAX ? *n_truth : at;
      *sy    = dp_ber_sync (syms, ns, *truth, *n_truth, mk, 2, DP_BER_LAG_SPAN,
                            DP_BER_SYNC_PFA);
      if (!sy->ok)
        return;
      *ok          = 1;
      const long e = (long)(mk->t0 + mk->n) - sy->lag;
      *scored      = e > 0 ? (size_t)e : 0;
      *next        = ns + PI_LV_RESYNC;
      dp_tlm_emit (tlm, id_lag, 0.0);
    }
  if (ns > *scored)
    {
      dp_ber_score (acc, syms, *scored, ns, *truth, *n_truth, mk, sy);
      *scored = ns;
      dp_tlm_emit (tlm, id_cnt, (double)acc->errors);
    }
  if (ns >= *next)
    {
      *next             = ns + PI_LV_RESYNC;
      dp_ber_marker_t m = *mk;
      const size_t    at
          = dp_ber_marker_place (*truth, *n_truth, 2, ns - guard, 0,
                                 DP_BER_LAG_SPAN, DP_BER_SYNC_PFA, -1);
      m.t0                   = at == SIZE_MAX ? *n_truth : at;
      const dp_ber_sync_t s2 = dp_ber_sync (syms, ns, *truth, *n_truth, &m, 2,
                                            DP_BER_LAG_SPAN, DP_BER_SYNC_PFA);
      if (s2.ok)
        dp_tlm_emit (tlm, id_lag, (double)(s2.lag - sy->lag));
    }
}

static int
run_draw (const uint8_t *code, double fc, double esn0, double rate_mag,
          uint32_t seed, double secs, unsigned spc, const capture_t *cp,
          dp_hist_t *cell_hist, draw_t *d)
{
  /* This draw's bins by block population (on / off the emitter's code),
     merged into the cell's once the draw's verdict is known: a false
     lock's go to "off" whole (cell_hist: [2][PQ_N], or NULL). */
  dp_hist_t  draw_hist[2][PQ_N];
  dp_hist_t *hist = NULL;
  if (cell_hist)
    for (int v = 0; v < 2; v++)
      for (int q = 0; q < PQ_N; q++)
        dp_hist_init (&draw_hist[v][q], PQ[q].lo, PQ[q].hi, PQ[q].nb);
  const double chip = dp_s415_chip_rate (fc);
  const double cn0  = dp_s415_cn0 (esn0);
  uint32_t     st   = trial_rng (seed * 7919u + (uint32_t)(fc / 1e8)
                                 + (uint32_t)(esn0 * 10.0));
  memset (d, 0, sizeof *d);
  d->fc   = fc;
  d->esn0 = esn0;
  d->rate = rate_mag == 0.0 ? 0.0 : (seed & 1u ? 1.0 : -1.0) * rate_mag;
  d->f0   = -DP_S415_SPAN_HZ + 2.0 * DP_S415_SPAN_HZ * uni (&st);
  /* A pass draw keeps its offset inside +/-DP_S415_SPAN_HZ for the whole run:
     where a constant rate would carry it out, the sign is toward zero. */
  if (pi_pass_r_env > 0.0 && rate_mag != 0.0
      && fabs (d->f0) > DP_S415_SPAN_HZ - rate_mag * secs)
    d->rate = (d->f0 > 0.0 ? -1.0 : 1.0) * rate_mag;
  d->frame_off = (size_t)(1000.0 * uni (&st));
  if (d->frame_off > 999)
    d->frame_off = 999;

  /* The payload: one bit a data symbol, more than the run can send. */
  const size_t n_pay = (size_t)(secs * DP_S415_SYM) + 2000;
  uint8_t     *pay   = dp_xmalloc (n_pay);
  for (size_t i = 0; i < n_pay; i++)
    pay[i] = (uint8_t)(uni (&st) < 0.5);
  dp_air_emitter_t e
      = dp_s415_emitter (code, fc, d->f0, d->rate, cn0, 0.0, 100u + seed);
  e.frame_off = d->frame_off;
  e.bits      = pay;
  e.n_bits    = n_pay;
  e.spc       = spc;
  dp_air_t air;
  DP_REQUIRE (
      dp_air_init (&air, &e, 1, DP_S415_FS, cn0, 8807u + seed, PI_PUSH));

  dp_async_dsss_receiver_state_t *rx = dp_s415_rx (code, fc, cn0, true);
  DP_REQUIRE (rx != NULL);

  const size_t   n_blk  = (size_t)(secs * DP_S415_FS) / PI_PUSH;
  uint8_t       *f_sym  = dp_xmalloc (n_blk);
  uint8_t       *f_cod  = dp_xmalloc (n_blk);
  uint8_t       *f_x[3] = { dp_xmalloc (n_blk), dp_xmalloc (n_blk),
                            dp_xmalloc (n_blk) }; /* car, demod, sync */
  uint8_t       *data   = dp_xmalloc (n_blk);
  float complex *x      = dp_xmalloc (PI_PUSH * sizeof *x);
  const size_t   ocap   = (size_t)(secs * DP_S415_SYM) + 4096;
  float complex *syms   = dp_xmalloc (ocap * sizeof *syms);
  float complex *tmp    = dp_xmalloc (ocap * sizeof *tmp);
  size_t         ns = 0, first_in = SIZE_MAX;
  d->ho_err_hz            = NAN;
  d->t_switch_s           = NAN;
  d->slope_hz_s           = NAN;
  d->t_narrow_s           = NAN;
  d->rate_pre_narrow_hz_s = d->rate_post_narrow_hz_s = NAN;
  d->slope_settled_hz_s                              = NAN;
  d->t_track_s                                       = NAN;
  d->seed_err_hz = d->seed_code_chip = d->seed_t_s = NAN;
  d->ho_code_chip = d->ho_age_s = NAN;
  /* The instrument: the receiver's own probes on every draw that is not a
     capture, read back each block (dp_tlm_read()), so the stress and the
     statistics come from the probes the capture shows, not a second
     reading of the state. Per block: the loops' true errors and the two
     flags' statistics, for the transitions' attribution. */
  float    *e_car = dp_xmalloc (n_blk * sizeof *e_car);
  float    *e_cod = dp_xmalloc (n_blk * sizeof *e_cod);
  float    *v_sym = dp_xmalloc (n_blk * sizeof *v_sym);
  float    *v_cod = dp_xmalloc (n_blk * sizeof *v_cod);
  size_t   *s_end = dp_xmalloc (n_blk * sizeof *s_end);
  uint32_t *s_gen = dp_xmalloc (n_blk * sizeof *s_gen);
  uint8_t  *cross = dp_xmalloc (n_blk);
  /* Per block: whether the receiver coasted in it (rx.coast, the loop
     control it decides once a code period from the two flags). */
  uint8_t      *coast     = dp_xcalloc (n_blk, 1);
  int           pid_coast = -1;
  uint32_t      gen       = 0;
  dp_tlm_t     *itlm      = NULL;
  dp_tlm_rec_t *recs      = NULL;
  size_t        rcap      = 0;
  int           pid[PQ_N], pthr[PQ_N];
  double        last_v[PQ_N], last_t[PQ_N];
  uint64_t      at_v[PQ_N], at_t[PQ_N];
  int           was_tracking = 0;
  int           narrowed     = 0; /* --acq a/b: this stint's lock seen */
  /* The order-2 loop's frequency over the stint up to the switch, for a
     least-squares slope: n, sums of t, f, t^2, t f. */
  double sl_n = 0, sl_t = 0, sl_f = 0, sl_tt = 0, sl_tf = 0;
  /* Designs d/e/f: the switch's derived parameters, the stint's phase
     (0 before the first lock), when it began, and d's window of errors. */
  pi_switch_t  sw;
  int          sw_phase = 0;
  double       sw_t0    = 0.0;
  double       s2_n = 0, s2_t = 0, s2_f = 0, s2_tt = 0, s2_tf = 0;
  const size_t w_blk = (size_t)ceil (pi_switch_window_s (fc)
                                     / ((double)PI_PUSH / DP_S415_FS));
  double      *w_e   = dp_xmallocn (w_blk + 1, sizeof *w_e);
  size_t       w_n   = 0;
  if (pi_loop_order == 3 && pi_acq >= 'd')
    pi_switch_design (fc, &sw);
  if (!(cp && cp->path))
    {
      itlm = dp_tlm_create (1u << 16);
      DP_REQUIRE (itlm != NULL);
      DP_REQUIRE (dp_async_dsss_receiver_set_telemetry (rx, itlm, "rx", 1)
                  == DP_OK);
      for (int q = 0; q < PQ_N; q++)
        {
          pid[q]  = PQ[q].probe ? dp_tlm_probe_id (itlm, PQ[q].probe) : -1;
          pthr[q] = PQ[q].thr ? dp_tlm_probe_id (itlm, PQ[q].thr) : -1;
          DP_REQUIRE (!PQ[q].probe || pid[q] >= 0);
          DP_REQUIRE (!PQ[q].thr || pthr[q] >= 0);
          last_v[q] = last_t[q] = NAN;
          at_v[q] = at_t[q] = UINT64_MAX;
        }
      rcap      = dp_tlm_capacity (itlm);
      pid_coast = dp_tlm_probe_id (itlm, "rx.coast");
      DP_REQUIRE (pid_coast >= 0);
      recs = dp_xmalloc (rcap * sizeof *recs);
    }
  /* The capture: probes registered first (the capture sizes its ring
     from the table), the scene's truth beside the receiver's own, once a
     block on the block's stamp -- the stamp the receiver's phase probes
     are at. Attached for the window only. */
  dp_tlm_t         *tlm    = NULL;
  dp_tlm_capture_t *cap    = NULL;
  int               id_car = -1, id_code = -1, id_hz = -1, id_data = -1;
  int               id_ecar = -1, id_ecod = -1, id_order = -1, id_rate = -1;
  int               id_ecu = -1, id_hyp = -1, id_lag = -1, id_cnt = -1;
  int               id_edc = -1;
  double            ecu = 0.0, ec_prev = NAN; /* err.car_unwrapped, cycles */
  /* err.sym_lag / err.count: the live alignment against the payload. */
  uint8_t        *lv_truth = NULL;
  size_t          lv_n = 0, lv_scored = 0, lv_next = 0;
  int             lv_ok = 0;
  dp_ber_marker_t lv_mk = { NULL, DP_BER_SYNC_SYMS, 0, 0, 0 };
  dp_ber_sync_t   lv_sy = { 0 };
  dp_ber_t        lv_acc;
  dp_ber_init (&lv_acc, 2, ULONG_MAX);
  dp_sample_clock_t clk;
  const size_t      b_from = cp && cp->path
                                 ? (size_t)(cp->from_s * DP_S415_FS) / PI_PUSH
                                 : SIZE_MAX;
  const size_t      b_to
      = cp && cp->path ? (size_t)(cp->to_s * DP_S415_FS) / PI_PUSH : 0;
  for (size_t b = 0; b < n_blk; b++)
    {
      if (b == b_from)
        {
          tlm = dp_tlm_create (1u << 18);
          DP_REQUIRE (tlm != NULL);
          DP_REQUIRE (dp_async_dsss_receiver_set_telemetry (rx, tlm, "rx", 1)
                      == DP_OK);
          id_car   = dp_tlm_probe (tlm, "truth.car", 1);
          id_code  = dp_tlm_probe (tlm, "truth.code", 1);
          id_hz    = dp_tlm_probe (tlm, "truth.hz", 1);
          id_data  = dp_tlm_probe (tlm, "truth.data", 1);
          id_ecar  = dp_tlm_probe (tlm, "err.car_deg", 1);
          id_ecod  = dp_tlm_probe (tlm, "err.code_chip", 1);
          id_order = dp_tlm_probe (tlm, "car.order", 1);
          id_rate  = dp_tlm_probe (tlm, "car.rate_hz_s", 1);
          id_ecu   = dp_tlm_probe (tlm, "err.car_unwrapped", 1);
          id_hyp   = dp_tlm_probe (tlm, "rx.code.hyp", 1);
          id_lag   = dp_tlm_probe (tlm, "err.sym_lag", 1);
          id_cnt   = dp_tlm_probe (tlm, "err.count", 1);
          id_edc   = dp_tlm_probe (tlm, "err.demod_car_deg", 1);
          DP_REQUIRE (id_car >= 0 && id_code >= 0 && id_hz >= 0 && id_data >= 0
                      && id_ecar >= 0 && id_ecod >= 0 && id_order >= 0
                      && id_rate >= 0 && id_ecu >= 0 && id_hyp >= 0
                      && id_lag >= 0 && id_cnt >= 0 && id_edc >= 0);
          dp_sample_clock_init (&clk, DP_S415_FS, 0);
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
                       (double)(t.symbol >= 0
                                && t.symbol % DP_S415_FRAME_SYM
                                       >= DP_S415_CODE_ONLY_SYM));
        }
      /* The loops' true errors at this call's first sample, read with the
         getters the receiver's rx.phase.car / rx.phase.code probes are
         read with, before the call runs: the carrier against the
         channel's own phase, folded to the detector's period (half a
         cycle); the code against the chip, the half-sample dwell-start
         convention applied. A capture carries them as err.car_deg and
         err.code_chip, so a figure draws them as records. */
      e_car[b] = e_cod[b] = NAN;
      if (was_tracking)
        {
          const dp_air_truth_t t
              = dp_air_truth (&air, 0, (uint64_t)b * PI_PUSH);
          const double ec
              = dp_costas_get_nco_phase_ahead (&rx->car, rx->car_carry_len)
                - (t.car_cycles - floor (t.car_cycles));
          e_car[b] = (float)(ec - 0.5 * floor (ec / 0.5 + 0.5));
          const double ek
              = dp_dll_get_code_phase_ahead (rx->dll, rx->car_carry_len)
                - t.code_chips + 0.5 * chip / DP_S415_FS;
          e_cod[b] = (float)(ek - (double)SF * floor (ek / (double)SF + 0.5));
          if (isnan (d->ho_code_chip))
            d->ho_code_chip = e_cod[b];
          if (tlm)
            {
              /* Unwrapped: each block's step folded to half a cycle the
                 other way, so the sum is continuous and a pi slip stays. */
              if (!isnan (ec_prev))
                {
                  double dd = ec - ec_prev;
                  dd -= floor (dd + 0.5);
                  ecu += dd;
                }
              else
                ecu = ec - floor (ec + 0.5);
              ec_prev = ec;
              dp_tlm_emit (tlm, id_ecar, 360.0 * e_car[b]);
              dp_tlm_emit (tlm, id_ecod, e_cod[b]);
              dp_tlm_emit (tlm, id_ecu, 360.0 * ecu);
              if (rx->dll && rx->dll->aid_nhyp)
                dp_tlm_emit (tlm, id_hyp, (double)rx->dll->aid_best);
              if (rx->rx && !rx->rx->real && rx->rx->fe.c)
                {
                  /* The residual the demodulator sees is truth - NCO =
                     -ec; its LO multiplies it by exp(j 2 pi lo), so what
                     is left at its decisions is lo - ec, cycles. */
                  const double lo = (double)dp_lo_get_phase (rx->rx->fe.c->lo)
                                    / 4294967296.0;
                  double       ed = lo - ec;
                  ed -= 0.5 * floor (ed / 0.5 + 0.5);
                  dp_tlm_emit (tlm, id_edc, 360.0 * ed);
                }
            }
        }
      DP_REQUIRE (dp_air_next (&air, x, PI_PUSH));
      if (itlm)
        dp_tlm_set_now (itlm, (uint64_t)b * PI_PUSH);
      const size_t got
          = dp_async_dsss_receiver_steps (rx, x, PI_PUSH, tmp, ocap);
      const int tracking = rx->state == ASYNC_DSSS_RX_TRACKING;
      /* --bn-carrier / --loop-order: on every entry into tracking (each
         seed rebuilds the chain) the carrier loop is set through Costas's
         own API (configure keeps the integrator, the hand-over's
         frequency; set_order carries it). The block the hand-over fell in
         ran as built: at most one block, 0.37 ms, about 1.5 code periods. */
      const double bn_track = pi_bn_rule_on
                                  ? pi_bn_rule (pi_rho (fc, PI_FLOOR_ESN0_DB))
                              : !isnan (pi_bn_car) ? pi_bn_car
                                                   : ASYNC_DSSS_RX_BN_CARRIER;
      if (tracking && !was_tracking)
        {
          narrowed = 0;
          sw_phase = 0;
          w_n      = 0;
          sl_n = sl_t = sl_f = sl_tt = sl_tf = 0.0;
          if (pi_loop_order == 3)
            {
              /* Through Costas's own switch: the frequency the hand-over
                 seeded carries, the rate starts at 0. Design a enters at
                 order 2 and switches at the lock below. */
              if (pi_acq == 'b')
                DP_REQUIRE (
                    dp_costas_set_order (&rx->car, 3, ASYNC_DSSS_RX_BN_CARRIER)
                    == DP_OK);
              else if (pi_acq == 'c')
                DP_REQUIRE (dp_costas_set_order (&rx->car, 3, bn_track)
                            == DP_OK);
            }
          else if (!isnan (pi_bn_car) || pi_bn_rule_on)
            DP_REQUIRE (dp_costas_configure (&rx->car, bn_track, rx->car.zeta)
                        == DP_OK);
          /* The #3134 control: the code lock at another look count, through
             the Dll's own configure (which re-sizes its threshold for that
             count), keeping the drop count the receiver chose. */
          if (pi_code_nl)
            {
              const uint32_t down = rx->dll->lock.n_down;
              DP_REQUIRE (
                  dp_dll_configure_lock (rx->dll, 1e-3, pi_code_nl, 0.0)
                  == DP_OK);
              (void)dp_dll_set_lock_verify (rx->dll, rx->dll->lock.n_up, down);
            }
          d->code_nl = rx->dll->n_looks;
        }
      /* --acq a/b: at the stint's first lock, the receiver's own event
         (both flags up: had_lock), the loop goes to its tracking form. */
      const double t_blk = (double)(b + 1) * PI_PUSH / DP_S415_FS;
      if (tracking && !narrowed)
        {
          const double f = dp_costas_get_norm_freq (&rx->car) * DP_S415_FS;
          sl_n += 1.0, sl_t += t_blk, sl_f += f, sl_tt += t_blk * t_blk,
              sl_tf += t_blk * f;
        }
      if (tracking && pi_loop_order == 3 && !narrowed && rx->had_lock
          && (pi_acq == 'a' || pi_acq == 'b'))
        {
          narrowed         = 1;
          d->t_switch_s    = t_blk;
          d->t_narrow_s    = t_blk;
          const double den = sl_n * sl_tt - sl_t * sl_t;
          d->slope_hz_s    = sl_n >= 2.0 && den > 0.0
                                 ? (sl_n * sl_tf - sl_t * sl_f) / den
                                 : NAN;
          if (pi_acq == 'a')
            {
              DP_REQUIRE (dp_costas_set_order (&rx->car, 3, bn_track)
                          == DP_OK);
              if (pi_seed_rate && !isnan (d->slope_hz_s))
                DP_REQUIRE (
                    dp_costas_set_norm_rate (
                        &rx->car, d->slope_hz_s / (DP_S415_FS * DP_S415_FS))
                    == DP_OK);
            }
          else
            DP_REQUIRE (dp_costas_configure (&rx->car, bn_track, rx->car.zeta)
                        == DP_OK);
        }
      if (tracking && pi_loop_order == 3 && pi_acq >= 'd' && sw_phase < 3)
        {
          if (sw_phase == 0 && rx->had_lock)
            {
              /* The stint's first lock: the pull-in slope is recorded for
                 every design, as design a records it. */
              const double den = sl_n * sl_tt - sl_t * sl_t;
              d->slope_hz_s    = sl_n >= 2.0 && den > 0.0
                                     ? (sl_n * sl_tf - sl_t * sl_f) / den
                                     : NAN;
              sw_t0            = t_blk;
              sw_phase         = 1;
              if (pi_acq != 'd')
                {
                  DP_REQUIRE (dp_costas_set_order (&rx->car, 3, sw.bn_wide)
                              == DP_OK);
                  d->t_switch_s = t_blk;
                }
            }
          else if (sw_phase == 1 && pi_acq == 'd')
            {
              /* Settled: the order-2 error's mean over the last 5 / BN
                 updates holds at the design lag. */
              if (w_n < w_blk)
                w_e[w_n++] = dp_costas_get_last_error (&rx->car);
              else
                {
                  memmove (w_e, w_e + 1, (w_blk - 1) * sizeof *w_e);
                  w_e[w_blk - 1] = dp_costas_get_last_error (&rx->car);
                }
              if (w_n == w_blk)
                {
                  double m = 0.0, v = 0.0;
                  for (size_t i = 0; i < w_blk; i++)
                    m += w_e[i];
                  m /= (double)w_blk;
                  for (size_t i = 0; i < w_blk; i++)
                    v += (w_e[i] - m) * (w_e[i] - m);
                  const double se
                      = sqrt (v / (double)(w_blk - 1)) / sqrt ((double)w_blk);
                  if (fabs (fabs (m) - sw.lag_e) <= 3.0 * se)
                    {
                      sw_phase = 2;
                      sw_t0    = t_blk;
                      s2_n = s2_t = s2_f = s2_tt = s2_tf = 0.0;
                    }
                }
            }
          else if (sw_phase == 1)
            {
              if (t_blk - sw_t0 >= sw.hold_s)
                {
                  d->rate_pre_narrow_hz_s = dp_costas_get_norm_rate (&rx->car)
                                            * DP_S415_FS * DP_S415_FS;
                  DP_REQUIRE (dp_costas_configure (&rx->car,
                                                   pi_acq == 'e' ? sw.bn_track
                                                                 : sw.bn_mid,
                                                   rx->car.zeta)
                              == DP_OK);
                  d->rate_post_narrow_hz_s = dp_costas_get_norm_rate (&rx->car)
                                             * DP_S415_FS * DP_S415_FS;
                  sw_t0                    = t_blk;
                  sw_phase                 = pi_acq == 'e' ? 3 : 2;
                  if (pi_acq == 'e')
                    d->t_narrow_s = t_blk;
                }
            }
          else if (sw_phase == 2 && pi_acq == 'd')
            {
              const double f = dp_costas_get_norm_freq (&rx->car) * DP_S415_FS;
              s2_n += 1.0, s2_t += t_blk, s2_f += f, s2_tt += t_blk * t_blk,
                  s2_tf += t_blk * f;
              if (t_blk - sw_t0 >= sw.w_s)
                {
                  const double den = s2_n * s2_tt - s2_t * s2_t;
                  d->slope_settled_hz_s
                      = s2_n >= 2.0 && den > 0.0
                            ? (s2_n * s2_tf - s2_t * s2_f) / den
                            : NAN;
                  DP_REQUIRE (dp_costas_set_order (&rx->car, 3, sw.bn_track)
                              == DP_OK);
                  if (pi_seed_rate && !isnan (d->slope_settled_hz_s))
                    DP_REQUIRE (dp_costas_set_norm_rate (
                                    &rx->car, d->slope_settled_hz_s
                                                  / (DP_S415_FS * DP_S415_FS))
                                == DP_OK);
                  d->t_switch_s = d->t_narrow_s = t_blk;
                  sw_phase                      = 3;
                }
            }
          else if (sw_phase == 2)
            {
              if (t_blk - sw_t0 >= sw.hold_mid_s)
                {
                  d->rate_pre_narrow_hz_s = dp_costas_get_norm_rate (&rx->car)
                                            * DP_S415_FS * DP_S415_FS;
                  DP_REQUIRE (
                      dp_costas_configure (&rx->car, sw.bn_track, rx->car.zeta)
                      == DP_OK);
                  d->rate_post_narrow_hz_s = dp_costas_get_norm_rate (&rx->car)
                                             * DP_S415_FS * DP_S415_FS;
                  d->t_narrow_s            = t_blk;
                  sw_phase                 = 3;
                }
            }
        }
      if (tlm)
        {
          dp_tlm_emit (tlm, id_order, (double)dp_costas_get_order (&rx->car));
          dp_tlm_emit (tlm, id_rate,
                       dp_costas_get_norm_rate (&rx->car) * DP_S415_FS
                           * DP_S415_FS);
        }
      if (tracking && isnan (d->ho_err_hz))
        {
          const uint64_t at = rx->samples_in - rx->state_samples;
          d->ho_err_hz      = dp_async_dsss_receiver_get_doppler_hz (rx)
                              - dp_air_truth (&air, 0, at).car_hz;
          d->ho_src         = dp_async_dsss_receiver_get_handover_source (rx);
          d->t_track_s      = (double)at / DP_S415_FS;
          d->ho_age_s       = d->t_track_s - d->seed_t_s;
        }
      /* The first seed: the refine's chain is built at the seed's sample,
         the first it is fed, so that sample is the state's entry. A refine
         spans many blocks, so the seed is seen refining before it hands
         over. */
      if (rx->state == ASYNC_DSSS_RX_REFINING && isnan (d->seed_t_s))
        {
          const uint64_t       i0 = rx->samples_in - rx->state_samples;
          const dp_air_truth_t t  = dp_air_truth (&air, 0, i0);
          const double         ek
              = rx->seed_chip_phase - t.code_chips + 0.5 * chip / DP_S415_FS;
          d->seed_t_s       = (double)i0 / DP_S415_FS;
          d->seed_err_hz    = rx->seed_doppler_hz_est - t.car_hz;
          d->seed_code_chip = ek - (double)SF * floor (ek / (double)SF + 0.5);
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
              if (tlm && first_in != SIZE_MAX)
                pi_live_align (&air, first_in, secs, syms, ns, &lv_truth,
                               &lv_n, &lv_ok, &lv_mk, &lv_sy, &lv_acc,
                               &lv_scored, &lv_next, tlm, id_lag, id_cnt);
            }
        }
      f_sym[b] = (uint8_t)(tracking && dp_async_dsss_receiver_get_locked (rx));
      f_cod[b]
          = (uint8_t)(tracking && dp_async_dsss_receiver_get_code_locked (rx));
      f_x[0][b] = (uint8_t)(tracking && rx->car.lock.locked);
      f_x[1][b] = (uint8_t)(tracking && rx->rx && rx->rx->l.car_lock.locked);
      f_x[2][b]
          = (uint8_t)(tracking && rx->rx && rx->rx->l.timing.lock.locked);
      /* A block is DATA when the truth puts its middle in a data symbol. */
      const long sym
          = dp_air_truth (&air, 0, b * PI_PUSH + PI_PUSH / 2).symbol;
      data[b] = (uint8_t)(sym >= 0
                          && sym % DP_S415_FRAME_SYM >= DP_S415_CODE_ONLY_SYM);
      if (!tracking)
        gen++;
      s_end[b] = tracking ? ns : SIZE_MAX;
      s_gen[b] = gen;
      v_sym[b] = v_cod[b] = NAN;
      if (itlm)
        {
          /* The block's population: on the emitter's code (within half
             a chip) or off it. */
          if (cell_hist)
            hist = draw_hist[isnan (e_cod[b]) || fabs (e_cod[b]) > 0.5f];
          /* Settled: tracking, 1 s after the hand-over. */
          const int settled
              = tracking && !isnan (d->t_track_s)
                && (double)b * PI_PUSH / DP_S415_FS >= d->t_track_s + 1.0;
          const size_t nr = dp_tlm_read (itlm, 0, recs, rcap);
          DP_REQUIRE_MSG (dp_tlm_dropped (itlm) == 0,
                          "the instrument's ring drops nothing");
          for (size_t i = 0; i < nr; i++)
            {
              const int    id = recs[i].probe;
              const double v  = recs[i].value;
              if (id == pid_coast && v != 0.0)
                coast[b] = 1;
              for (int q = 0; q < PQ_N; q++)
                {
                  if (id != pid[q] && id != pthr[q])
                    continue;
                  if (!PQ[q].thr)
                    { /* a lock statistic or a demod error */
                      if (q == PQ_SYM)
                        v_sym[b] = (float)v;
                      if (q == PQ_CODE)
                        v_cod[b] = (float)v;
                      const int on = q == PQ_DCAR_E || q == PQ_DSYNC_E
                                         ? settled
                                         : settled && data[b];
                      if (hist && on)
                        dp_hist_add (&hist[q], v);
                      continue;
                    }
                  /* a test over its threshold: paired on the stamp */
                  if (id == pid[q])
                    {
                      last_v[q] = v;
                      at_v[q]   = recs[i].n;
                    }
                  else
                    {
                      last_t[q] = v;
                      at_t[q]   = recs[i].n;
                    }
                  if (at_v[q] == at_t[q] && at_v[q] != UINT64_MAX)
                    {
                      /* a search or refine test precedes tracking: no code
                         error to place it by, so the draw's own set */
                      if (cell_hist && last_t[q] > 0.0)
                        dp_hist_add (&draw_hist[0][q], last_v[q] / last_t[q]);
                      at_v[q] = at_t[q] = UINT64_MAX;
                    }
                }
            }
          if (hist && settled && !isnan (e_car[b]))
            {
              /* signed with the ramp, so the two directions' lags add
                 rather than cancel in a cell's pool */
              dp_hist_add (&hist[PQ_CAR_ERR],
                           (d->rate < 0.0 ? -360.0 : 360.0) * e_car[b]);
              dp_hist_add (&hist[PQ_CODE_ERR], e_cod[b]);
            }
          /* The carrier error's window, against the update that uses it.
             The receiver updates the carrier at the end of each of its
             periods and has fed the Dll exactly through the last one (the
             carry holds the rest), so the Dll's code phase now, in code
             periods, is how long ago its last epoch ended. The code-period
             window ends there when the look-back kept the natural window
             (its sum IS the epoch's whole sum), and earlier by the chunks
             it borrowed otherwise -- a count the Dll does not keep, so only
             the natural share is counted. Read once a block: the block's
             last update. */
          if (hist && settled && tracking)
            {
              const dp_dll_state_t *dl = rx->dll;
              dp_hist_add (&hist[PQ_LAG], dp_dll_get_code_phase_ahead (dl, 0)
                                              / (double)dl->sf);
              d->win_n++;
              d->win_natural += dl->segments <= 1
                                || dl->win_p == dl->sums[dl->segments - 1];
            }
        }
      was_tracking = tracking;
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
  const double  blk_s = (double)PI_PUSH / DP_S415_FS;
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
  {
    size_t *tog[3]  = { &d->car_tog, &d->dem_tog, &d->syn_tog };
    double *last[3] = { &d->car_last_s, &d->dem_last_s, &d->syn_last_s };
    int    *up[3]   = { &d->car_up_end, &d->dem_up_end, &d->syn_up_end };
    for (int k = 0; k < 3; k++)
      {
        const int64_t s  = dp_ber_lock_symbol (f_x[k], n_blk, PI_SUSTAIN, 0.9);
        int64_t       f1 = -1;
        *tog[k]  = s >= 0 ? dp_ber_lock_transitions (f_x[k], data, n_blk,
                                                     (size_t)s, &f1)
                          : 0;
        *last[k] = NAN;
        for (size_t b = n_blk; b-- > 1;)
          if (f_x[k][b] != f_x[k][b - 1])
            {
              *last[k] = (double)b * blk_s;
              break;
            }
        *up[k] = n_blk ? f_x[k][n_blk - 1] : 0;
      }
  }
  /* The loops' stress, against the design's baselines
     (docs/design/async-dsss-receiver.md §4.8, "The carrier loop's bandwidth":
     its two stresses; the loop as coded: one update a code period T, the loop
     filter's own ki and bn): the ramp's lag 2 pi R T^2 / ki noise-free, the
     jitter sqrt((bn / rho)(1 + K / (2 rho))) with rho the C/N0 at the antenna
     over T and K = 1 (the error is one coherent sum over the code-period
     window), and a hand-over step's peak 0.49 * 2 pi df T / (1.886 bn). */
  {
    const double T  = (double)SF / chip;
    const double bn = rx->car.lf.bn, ki = rx->car.lf.ki;
    const double rho = pow (10.0, cn0 / 10.0) * T;
    const double K   = 1.0;
    const double deg = 180.0 / M_PI;
    d->base_lag_deg  = deg * 2.0 * M_PI * fabs (d->rate) * T * T / ki;
    d->base_sd_deg   = deg * sqrt (bn / rho * (1.0 + K / (2.0 * rho)));
    d->base_ho_deg
        = deg * 0.49 * 2.0 * M_PI * fabs (d->ho_err_hz) * T / (1.886 * bn);
    DP_REQUIRE (dp_correlator_get_doppler_res_hz (rx->acq, &d->base_row_hz)
                == DP_OK);
    d->base_half_sample_chip
        = 0.5 / (rx->spc * (rx->srch_rs ? rx->srch_ratio : 1.0));
    d->base_unamb_hz = 1.0 / (4.0 * T);
    /* The shipped pull-in, scaled to the bandwidth the loop ran at (it is
       proportional to bn: ASYNC_DSSS_RX_CARRIER_PULLIN_HZ's own form). */
    d->base_pullin_hz = ASYNC_DSSS_RX_CARRIER_PULLIN_HZ (chip, SF)
                        * rx->car.lf.bn / ASYNC_DSSS_RX_BN_CARRIER;
    d->bn_car         = rx->car.lf.bn;
    d->loop_order     = pi_loop_order;
    d->acq            = pi_loop_order == 3 ? pi_acq : '-';
    d->rate_end_hz_s
        = dp_costas_get_norm_rate (&rx->car) * DP_S415_FS * DP_S415_FS;
    if (pi_loop_order == 3 && pi_acq == 'c')
      d->t_narrow_s = d->t_track_s; /* narrow from the entry into tracking */
    free (w_e);
    d->seed_rate              = pi_seed_rate;
    d->base_half_spacing_chip = 0.5 * rx->dll->spacing;
    double sc = 0.0, sc2 = 0.0, sk = 0.0, sk2 = 0.0, pk = 0.0, spk = 0.0;
    size_t n_off = 0, n_all = 0;
    size_t n     = 0;
    d->car_slips = d->ho_slips = 0;
    /* One definition of a crossing of the fold, used by the stress, the
       transitions and their baseline: the folded error jumps by more than
       a quarter cycle (90 degrees) between consecutive tracked blocks. A
       crossing that comes back counts twice: these are crossings, not net
       cycle slips. */
    for (size_t b = 0; b < n_blk; b++)
      cross[b] = (uint8_t)(b > 0 && !isnan (e_car[b]) && !isnan (e_car[b - 1])
                           && fabs (e_car[b] - e_car[b - 1]) > 0.25);
    /* The baselines: what "a crossing preceded this change" and "the
       carrier error passed X in the second before it" are read against,
       each in the window a change's attribution reads. */
    {
      const size_t w1 = (size_t)(1.0 / blk_s);
      d->cross_win_frac
          = win_frac (cross, e_car, e_cod, n_blk, w1, blk_s, d->t_track_s);
      /* The lock statistic's own memory, in whole blocks (at least one):
         whether the excursion sits AT the change, not a second before. */
      d->lock_mem_s     = 1.0 / rx->lock_alpha / DP_S415_SYM;
      const size_t wm   = (size_t)fmax (1.0, round (d->lock_mem_s / blk_s));
      uint8_t     *mark = dp_xmalloc (n_blk);
      for (size_t x = 0; x < 3; x++)
        {
          for (size_t b = 0; b < n_blk; b++)
            mark[b] = (uint8_t)(!isnan (e_car[b])
                                && fabs (360.0 * e_car[b]) > PI_EXC_DEG[x]);
          d->exc_win_frac[x]
              = win_frac (mark, e_car, e_cod, n_blk, w1, blk_s, d->t_track_s);
          d->exc_mem_frac[x]
              = win_frac (mark, e_car, e_cod, n_blk, wm, blk_s, d->t_track_s);
        }
      free (mark);
      d->coast_mem_frac
          = win_frac (coast, e_car, e_cod, n_blk, wm, blk_s, d->t_track_s);
    }
    for (size_t b = 0; b < n_blk; b++)
      {
        const double t = (double)b * blk_s;
        if (isnan (e_car[b]))
          continue;
        const int slip = cross[b];
        if (t < d->t_track_s + 0.5)
          {
            pk = fmax (pk, fabs (360.0 * e_car[b]));
            d->ho_slips += (size_t)slip;
          }
        if (t >= d->t_track_s + 1.0 && t < d->t_track_s + 1.5)
          spk = fmax (spk, fabs (360.0 * e_car[b]));
        if (t >= d->t_track_s + 1.0)
          {
            n_all++;
            if (fabs (e_cod[b]) > 0.5f)
              { /* off the emitter: not this loop's stress */
                n_off++;
                continue;
              }
            d->car_slips += (size_t)slip;
            sc += e_car[b];
            sc2 += (double)e_car[b] * e_car[b];
            sk += e_cod[b];
            sk2 += (double)e_cod[b] * e_cod[b];
            n++;
          }
      }
    d->off_code_frac   = n_all ? (double)n_off / (double)n_all : NAN;
    d->ho_peak_deg     = isnan (d->t_track_s) ? NAN : pk;
    d->steady_peak_deg = isnan (d->t_track_s) ? NAN : spk;
    const double mc = n ? sc / (double)n : NAN, mk = n ? sk / (double)n : NAN;
    d->car_mean_deg = 360.0 * mc;
    d->car_sd_deg
        = n ? 360.0 * sqrt (fmax (sc2 / (double)n - mc * mc, 0.0)) : NAN;
    d->code_mean_chip = mk;
    d->code_sd_chip   = n ? sqrt (fmax (sk2 / (double)n - mk * mk, 0.0)) : NAN;
  }

  if (hist)
    {
      const size_t c = (size_t)((dp_hist_t (*)[2][PQ_N])cell_hist - pi_hist);
      DP_REQUIRE (c < PI_CELLS);
      pi_rule[c][0] = dp_async_dsss_receiver_get_sym_lock_rule (rx);
      pi_rule[c][1] = dp_async_dsss_receiver_get_car_lock_rule (rx);
      pi_rule[c][2] = dp_async_dsss_receiver_get_code_lock_rule (rx);
      pi_rule[c][3] = dp_async_dsss_receiver_get_demod_lock_rule (rx);
      pi_rule[c][4] = dp_async_dsss_receiver_get_demod_sync_lock_rule (rx);
    }

  const size_t trans0 = pi_n_trans;
  /* Every change of the two flags after each was sustained, with its
     attribution (trans_t). Enumerated with the same counter the row's
     totals come from: each call from the last change, whose value is then
     the reference. */
  if (itlm)
    {
      const uint8_t          *fl[2]  = { f_sym, f_cod };
      const int64_t           sus[2] = { s_sym, s_cod };
      const float            *vs[2]  = { v_sym, v_cod };
      const size_t            tot[2] = { d->sym_tog, d->code_tog };
      const dp_lockdet_rule_t rl[2]
          = { dp_async_dsss_receiver_get_sym_lock_rule (rx),
              dp_async_dsss_receiver_get_code_lock_rule (rx) };
      const size_t w1 = (size_t)(1.0 / blk_s), wh = (size_t)(0.5 / blk_s);
      for (int f = 0; f < 2; f++)
        {
          if (sus[f] < 0)
            continue;
          size_t  from = (size_t)sus[f], seen = 0;
          int64_t at = -1;
          while (dp_ber_lock_transitions (fl[f], data, n_blk, from, &at) > 0
                 && at >= 0)
            {
              const size_t k  = (size_t)at;
              trans_t      tr = { 0 };
              tr.fc = d->fc, tr.esn0 = d->esn0, tr.rate = d->rate;
              tr.seed = seed, tr.flag = f, tr.up = fl[f][k] != 0;
              tr.t_s     = (double)k * blk_s;
              tr.rule    = rl[f];
              tr.on_code = !isnan (e_cod[k]) && fabs (e_cod[k]) <= 0.5f;
              tr.stat    = NAN;
              for (size_t j = k + 1; j-- > 0 && isnan (tr.stat);)
                tr.stat = vs[f][j];
              double mx = 0.0, s2 = 0.0, kx = 0.0;
              size_t m = 0;
              for (size_t j = k > w1 ? k - w1 : 0; j < k; j++)
                {
                  if (isnan (e_car[j]))
                    continue;
                  tr.car_slips += cross[j];
                  mx = fmax (mx, fabs (360.0 * e_car[j]));
                  s2 += 360.0 * 360.0 * (double)e_car[j] * e_car[j];
                  kx = fmax (kx, fabs ((double)e_cod[j]));
                  m++;
                }
              tr.car_max_deg = m ? mx : NAN;
              {
                const size_t wm
                    = (size_t)fmax (1.0, round (d->lock_mem_s / blk_s));
                double mm = 0.0;
                size_t q  = 0;
                for (size_t j = k > wm ? k - wm : 0; j < k; j++)
                  if (!isnan (e_car[j]))
                    {
                      mm = fmax (mm, fabs (360.0 * e_car[j]));
                      q++;
                    }
                tr.car_max_mem_deg = q ? mm : NAN;
                for (size_t j = k > wm ? k - wm : 0; j <= k; j++)
                  tr.coast_mem |= coast[j];
                for (size_t j = k > w1 ? k - w1 : 0; j <= k; j++)
                  tr.coast_1s |= coast[j];
              }
              tr.car_rms_deg   = m ? sqrt (s2 / (double)m) : NAN;
              tr.code_max_chip = m ? kx : NAN;
              tr.evm_db        = NAN;
              tr.draw_evm_db   = NAN; /* the draw's, once it is scored */
              const size_t a   = k > wh ? k - wh : 0;
              const size_t z   = k + wh < n_blk ? k + wh : n_blk - 1;
              if (s_end[a] != SIZE_MAX && s_end[z] != SIZE_MAX
                  && s_gen[a] == gen && s_gen[z] == gen && s_end[z] > s_end[a]
                  && s_end[z] <= ns)
                tr.evm_db
                    = dp_test_evm_db_hard_range (syms, s_end[a], s_end[z], 2);
              if (pi_n_trans == pi_cap_trans)
                {
                  pi_cap_trans = pi_cap_trans ? 2 * pi_cap_trans : 256;
                  pi_trans
                      = realloc (pi_trans, pi_cap_trans * sizeof *pi_trans);
                  DP_REQUIRE (pi_trans != NULL);
                }
              pi_trans[pi_n_trans++] = tr;
              seen++;
              from = k;
            }
          DP_CHECK_MSG (seen == tot[f], "every counted change is attributed");
        }
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
                    && fabs (d->end_resid_hz) >= 0.25 * DP_S415_SYM;

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
      const size_t n_truth = (size_t)(secs * DP_S415_SYM) + 1000;
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
              d->aligned = 1;
              /* A slip is a different alignment at the end; an end the
                 sync could not place is its own verdict (end_unsynced),
                 not a slip: dp_ber_sync has refused a clean, aligned end
                 at 20 dB (the error count from the start's alignment was
                 0 of 26,022). */
              d->end_unsynced = !end.ok;
              d->slipped
                  = end.ok
                    && (end.lag != sy.lag || cos (end.phase - sy.phase) < 0.0);
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
  for (size_t i = trans0; i < pi_n_trans; i++)
    {
      pi_trans[i].draw_evm_db = d->evm_db;
      pi_trans[i].false_lock  = d->false_lock;
    }
  if (cell_hist)
    {
      for (int v = 0; v < 2; v++)
        {
          dp_hist_t *into = cell_hist + ((d->false_lock || v) ? PQ_N : 0);
          for (int q = 0; q < PQ_N; q++)
            {
              into[q].under += draw_hist[v][q].under;
              into[q].over += draw_hist[v][q].over;
              for (size_t k = 0; k < draw_hist[v][q].nb; k++)
                into[q].c[k] += draw_hist[v][q].c[k];
            }
        }
    }
  /* Works: both flags sustained and never changed over the data, not a
     false lock, and the data good: a slip, or an end the meter could not
     sync (#3131), fails it whatever the flags say. */
  d->works = s_sym >= 0 && s_cod >= 0 && d->sym_tog == 0 && d->code_tog == 0
             && !d->false_lock && !d->slipped && !d->end_unsynced
             && d->car_tog == 0 && d->dem_tog == 0 && d->syn_tog == 0
             && d->car_up_end && d->dem_up_end && d->syn_up_end;
  d->data_ended = dp_air_data_ended (&air);

  if (itlm)
    {
      (void)dp_async_dsss_receiver_set_telemetry (rx, NULL, NULL, 1);
      dp_tlm_destroy (itlm);
    }
  free (recs);
  free (e_car);
  free (e_cod);
  free (v_sym);
  free (v_cod);
  free (s_end);
  free (s_gen);
  free (cross);
  free (coast);
  free (f_sym);
  free (f_cod);
  for (int k = 0; k < 3; k++)
    free (f_x[k]);
  free (data);
  free (x);
  free (syms);
  free (tmp);
  free (pay);
  dp_air_free (&air);
  free (lv_truth);
  dp_ber_free (&lv_acc);
  dp_async_dsss_receiver_destroy (rx);
  return 0;
}

/* The draw row's section and header, the sweep's and a capture's. */
#define PI_ROW_HEAD                                                           \
  "# pullin\nfc_ghz,esn0_db,rate_hz_s,seed,f0_hz,"                            \
  "frame_off,ho_err_hz,ho_src,seed_err_hz,seed_code_chip,seed_t_s,"           \
  "ho_code_chip,ho_age_s,t_track_s,t_sym_s,t_code_s,sym_tog,"                 \
  "sym_tog1_s,code_tog,code_tog1_s,car_tog,demod_tog,sync_tog,car_last_s,"    \
  "demod_last_s,sync_last_s,car_up_end,demod_up_end,sync_up_end,"             \
  "end_resid_hz,false_lock,"                                                  \
  "aligned,slipped,end_unsynced,errors,scored,evm_db,m2m4_db,works,car_mean_" \
  "deg,"                                                                      \
  "car_sd_deg,code_mean_chip,code_sd_chip,car_slips,ho_slips,cross_win_frac," \
  "exc30_win_frac,exc45_win_frac,exc60_win_frac,lock_mem_s,"                  \
  "exc30_mem_frac,exc45_mem_frac,exc60_mem_frac,coast_mem_frac,"              \
  "off_code_frac,"                                                            \
  "ho_peak_deg,steady_peak_deg,base_lag_deg,base_sd_deg,base_ho_deg,"         \
  "base_row_hz,base_half_sample_chip,base_unamb_hz,base_pullin_hz,"           \
  "base_half_spacing_chip,bn_car,win_natural_frac,loop_order,acq,"            \
  "t_switch_s,slope_hz_s,rate_end_hz_s,seed_rate,t_narrow_s,"                 \
  "rate_pre_narrow_hz_s,rate_post_narrow_hz_s,slope_settled_hz_s\n"

static void
emit_row (const draw_t *d, uint32_t seed)
{
  printf (
      "%.1f,%.1f,%.0f,%u,%.1f,%zu,%.2f,%d,%.2f,%.4f,%.4f,%.4f,%.4f,%."
      "4f,%.4f,%.4f,%zu,%.4f,"
      "%zu,%.4f,%zu,%zu,%zu,%.4f,%.4f,%.4f,%d,%d,%d,%.1f,%d,%d,%d,%d,%lu,%lu,%"
      ".3f,%.3f,%d,%.3f,%.3f,%.4f,%.4f,"
      "%zu,%zu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.2f,%.2f,"
      "%.3f,%.3f,%.3f,"
      "%.3f,%.4f,%.2f,%.2f,%.4f,%.4f,%.4f,%d,%c,%.4f,%.2f,%.2f,%d,%.4f,%.2f,"
      "%.2f,%.2f\n",
      d->fc / 1e9, d->esn0, d->rate, seed, d->f0, d->frame_off, d->ho_err_hz,
      d->ho_src, d->seed_err_hz, d->seed_code_chip, d->seed_t_s,
      d->ho_code_chip, d->ho_age_s, d->t_track_s, d->t_sym_s, d->t_code_s,
      d->sym_tog, d->sym_tog1_s, d->code_tog, d->code_tog1_s, d->car_tog,
      d->dem_tog, d->syn_tog, d->car_last_s, d->dem_last_s, d->syn_last_s,
      d->car_up_end, d->dem_up_end, d->syn_up_end, d->end_resid_hz,
      d->false_lock, d->aligned, d->slipped, d->end_unsynced, d->errors,
      d->scored, d->evm_db, d->m2m4_db, d->works, d->car_mean_deg,
      d->car_sd_deg, d->code_mean_chip, d->code_sd_chip, d->car_slips,
      d->ho_slips, d->cross_win_frac, d->exc_win_frac[0], d->exc_win_frac[1],
      d->exc_win_frac[2], d->lock_mem_s, d->exc_mem_frac[0],
      d->exc_mem_frac[1], d->exc_mem_frac[2], d->coast_mem_frac,
      d->off_code_frac, d->ho_peak_deg, d->steady_peak_deg, d->base_lag_deg,
      d->base_sd_deg, d->base_ho_deg, d->base_row_hz, d->base_half_sample_chip,
      d->base_unamb_hz, d->base_pullin_hz, d->base_half_spacing_chip,
      d->bn_car, d->win_n ? (double)d->win_natural / (double)d->win_n : NAN,
      d->loop_order, d->acq, d->t_switch_s, d->slope_hz_s, d->rate_end_hz_s,
      d->seed_rate, d->t_narrow_s, d->rate_pre_narrow_hz_s,
      d->rate_post_narrow_hz_s, d->slope_settled_hz_s);
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
  size_t        works = 0, acq = 0, fl = 0, al = 0, slip = 0, unsync = 0;
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
      unsync += (size_t)(ds[i].aligned && ds[i].end_unsynced);
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
  printf (
      "  %.1f GHz %4.1f dB %+4.0f Hz/s works %2zu/%zu [%.2f, "
      "%.2f], acquired %zu, false %zu; lock %.2f/%.2f/%.2f s; aligned "
      "%zu, slipped %zu, end unsynced %zu; SER %.2e [%.2e, %.2e] (%lu/%lu), "
      "loss %.2f dB\n",
      ds[0].fc / 1e9, ds[0].esn0, ds[0].rate == 0.0 ? 0.0 : DP_S415_RATE_HZ_S,
      works, n, wl, wh, acq, fl, nt ? t[nt / 2] : NAN,
      nt ? t[(size_t)(0.9 * (double)nt)] : NAN, nt ? t[nt - 1] : NAN, al, slip,
      unsync, ci.p_hat, ci.lo, ci.hi, err, sc, loss);
}

/* 415.1-B-1 §7.4.1 in every cell. Its form depends on what the cell's
   draws can say:
     demonstrate  the whole sweep (PI_DRAWS draws): the Wilson 95% LOWER
                  limit at or above PI_ACQ_P -- the claim shown;
     regression   --check (one draw) or a shard (part of a cell): the
                  UPPER limit at or above PI_ACQ_P -- not refuted, which a
                  receiver acquiring half the time also passes most runs,
                  so the line says it cannot demonstrate the claim.
   A cell that cannot demonstrate it fails the sweep: it is a gap, recorded
   with its issue, never relabeled "not refuted". */
static void
check_acq (const draw_t *ds, size_t n, int demonstrate, int emit)
{
  size_t pn = 0;
  for (size_t i = 0; i < n; i++)
    pn += !isnan (ds[i].t_code_s) && ds[i].t_code_s <= PI_ACQ_S;
  double pl, ph;
  dp_wilson ((double)pn, (double)n, &pl, &ph);
  if (!emit)
    printf (
        "    415.1-B-1 §7.4.1: PN lock within %.0f s %zu/%zu [%.3f, "
        "%.3f] against %.2f: %s\n",
        PI_ACQ_S, pn, n, pl, ph, PI_ACQ_P,
        demonstrate
            ? (pl >= PI_ACQ_P ? "DEMONSTRATED" : "NOT DEMONSTRATED (a gap)")
            : (ph >= PI_ACQ_P ? "not refuted (a regression check: "
                                "this run cannot demonstrate the claim)"
                              : "REFUTED"));
  if (demonstrate)
    DP_CHECK_MSG (pl >= PI_ACQ_P,
                  "415.1-B-1 §7.4.1: PN lock within 20 s at probability 0.9 "
                  "is demonstrated (Wilson 95% lower limit)");
  else
    DP_CHECK_MSG (ph >= PI_ACQ_P,
                  "415.1-B-1 §7.4.1: PN lock within 20 s at probability 0.9 "
                  "is not refuted (a regression check)");
}

/* The contract's cells: 10 dB, static, the receiver as built. */
static void
check_cell (const draw_t *ds, size_t n)
{
  if (ds[0].esn0 < 10.0 || ds[0].rate != 0.0)
    return;
  unsigned long err = 0, sc = 0;
  for (size_t i = 0; i < n; i++)
    {
      DP_CHECK_MSG (ds[i].works, "at 10 dB static every draw's link works");
      DP_CHECK_MSG (ds[i].aligned, "and aligns at the start");
      DP_CHECK_MSG (!ds[i].slipped, "and holds that alignment to the end");
      DP_CHECK_MSG (!ds[i].end_unsynced, "and the meter syncs at the end");
      err += ds[i].errors;
      sc += ds[i].scored;
    }
  const dp_ber_ci_t ci = dp_ber_ci (err, sc, 0.95);
  /* The claim is resolvable only where the symbols scored can put the
     interval under the bound at all: with no errors the 95% upper end is
     about 3 / sc, so the claim needs sc > 3 / bound. At 10 dB that is
     about 3,900 symbols; at 20 dB the bound is 6.8e-24 and no run is long
     enough, so the line says so and asserts nothing. */
  const double bound
      = dp_ber_theory_ser (2, pow (10.0, (ds[0].esn0 - PI_MARGIN_DB) / 10.0));
  if (sc > 0 && 3.0 / (double)sc >= bound)
    printf ("  %.1f GHz %4.1f dB: the error-rate claim needs %.2e symbols "
            "to resolve its bound (%.2e), %lu scored: not asserted\n",
            ds[0].fc / 1e9, ds[0].esn0, 3.0 / bound, bound, sc);
  else
    DP_CHECK_MSG (sc > 0 && ci.hi < bound,
                  "its error rate is under the bound's PI_MARGIN_DB lower");
}

/* Order 3's lower gain edge at a delay: the least stable loop gain, by
   bisection on dp_state_filter_loop_stable(). Order 2 has none (0). */
static double
pi_k_low (int order, const double g[3], double delay)
{
  if (order == 2)
    return 0.0;
  double lo = 0.01, hi = 1.0;
  for (int i = 0; i < 50; i++)
    {
      const double m = 0.5 * (lo + hi);
      if (dp_state_filter_loop_stable (3, g[0], g[1], g[2], delay, m))
        hi = m;
      else
        lo = m;
    }
  return hi;
}

/* The `# design` section: see the file comment. Delays 0, 1 and 1.75
   updates span what the lag instrument measured (lag.epoch_T uniform over
   one update, the look-back adding up to 0.75 on most code-period
   updates). The run's bandwidth row is the built ASYNC_DSSS_RX_BN_CARRIER,
   or --bn-carrier's. */
static void
pi_design (const double fcs[2], const double esn0s[4])
{
  /* The use case's dynamics: the envelope on the 50 km grid and on the
     halved one, how far the maxima move, and R_env. */
  pi_pass_env_t pe[2];
  pi_pass_envelope (50.0, &pe[0]);
  pi_pass_envelope (25.0, &pe[1]);
  printf ("# pass\nstep_km,passes,f2_raw_max_hz_s2,timing_max_hz_s,"
          "prop_max_hz_s,prop_pass_h_km,prop_pass_el_deg,prop_pass_fc_ghz,"
          "prop_pass_eps,rotation,r_env_hz_s,o2_lag_at_r_env_deg,"
          "o2_lag_at_prop_max_deg,res_f2_max_hz_s2,o3_err_at_res_f2_deg");
  for (int i = 0; i < PI_PASS_BINS; i++)
    printf (",env_%.0f", (double)(i + 1) * DP_S415_SPAN_HZ / PI_PASS_BINS);
  printf ("\n");
  for (int g = 0; g < 2; g++)
    {
      const double rot = pi_pass_rotation (pe[g].v_min);
      /* The order-2 lag at the rule's bn, the larger of the carriers'. */
      double lag_per = 0.0, o3_per = 0.0;
      for (int f = 0; f < 2; f++)
        {
          dp_loop_filter_state_t lf;
          const double           T = (double)SF / (dp_s415_chip_rate (fcs[f]));
          dp_loop_filter_init (
              &lf, pi_bn_rule (pi_rho (fcs[f], PI_FLOOR_ESN0_DB)), 0.707, 1.0);
          lag_per = fmax (
              lag_per,
              360.0 * dp_state_filter_steady_error (2, lf.ki, 0.0, 2, T * T));
          dp_state_filter_state_t sf;
          dp_state_filter_init (
              &sf, pi_bn_rule (pi_rho (fcs[f], PI_FLOOR_ESN0_DB)), 1.0);
          o3_per = fmax (o3_per, 360.0
                                     * dp_state_filter_steady_error (
                                         3, sf.ki, sf.kr, 3, T * T * T));
        }
      const double r_env = pi_r_env (pe[g].timing_max, rot);
      printf ("%.0f,%d,%.3f,%.3f,%.3f,%.0f,%.1f,%.1f,%.4f,%.4f,%.0f,%.3f,"
              "%.3f,%.3f,%.5f",
              pe[g].step_km, pe[g].n_pass, pe[g].f2_max, pe[g].timing_max,
              pe[g].prop_max, pe[g].prop_h_km, pe[g].prop_el_deg,
              pe[g].prop_fc / 1e9, pe[g].prop_eps, rot, r_env, lag_per * r_env,
              lag_per * pe[g].prop_max, pe[g].res_f2_max,
              o3_per * pe[g].res_f2_max);
      for (int i = 0; i < PI_PASS_BINS; i++)
        printf (",%.3f", pe[g].env[i]);
      printf ("\n");
    }
  DP_CHECK_MSG (pe[0].n_pass > 0 && pe[0].timing_max > 0.0,
                "the pass model has passes in scope");

  /* Carrier-only pre-compensation (owner 11-17: "Assume it only corrects
     carrier - worst-case"): the code clock keeps the raw pass's Doppler.
     The search's coherent block and its stacked dwell, read from the
     receiver as the cells build it, and the code drift over each at the
     raw peak, chips. */
  printf ("# search\nfc_ghz,esn0_db,coherent_bins,n_noncoh,block_ms,"
          "dwell_ms,raw_ppm_max,code_doppler_chips_s,drift_block_chips,"
          "drift_dwell_chips\n");
  for (int f = 0; f < 2; f++)
    for (int k = 0; k < 2; k++)
      {
        const double esn0 = k ? 10.0 : 4.0;
        const double chip = dp_s415_chip_rate (fcs[f]);
        const double cn0  = dp_s415_cn0 (esn0);
        uint8_t      code[SF];
        for (size_t i = 0; i < SF; i++)
          code[i] = (uint8_t)(i & 1u);
        dp_async_dsss_receiver_state_t *rx
            = dp_s415_rx (code, fcs[f], cn0, true);
        size_t nc = 0;
        if (!rx || dp_correlator_get_n_noncoh (rx->acq, &nc) != DP_OK)
          {
            DP_CHECK_MSG (0, "the search's dwell is readable");
            dp_async_dsss_receiver_destroy (rx);
            continue;
          }
        const size_t db    = dp_async_dsss_receiver_get_coherent_bins (rx);
        const double per   = (double)SF / chip;
        const double ppm   = pe[0].raw_ppm_max[f];
        const double cdopp = chip * ppm * 1e-6;
        printf ("%.1f,%.0f,%zu,%zu,%.2f,%.2f,%.3f,%.2f,%.3f,%.3f\n",
                fcs[f] / 1e9, esn0, db, nc, 1e3 * (double)db * per,
                1e3 * (double)(db * nc) * per, ppm, cdopp,
                cdopp * (double)db * per, cdopp * (double)(db * nc) * per);
        dp_async_dsss_receiver_destroy (rx);
      }
  /* The order-3 switch, per carrier, as run_draw derives it
     (pi_switch_design). */
  printf ("# switch\nfc_ghz,T_us,rho_floor,slope,bn_track,ramp_x_rad_upd2,"
          "lag_e,psi0_deg,peak_at_track_deg,settle_at_track_s,bn_wide,"
          "peak_at_wide_deg,sd_at_wide_deg,hold_wide_s,bn_mid,hold_mid_s,"
          "settle_window_s\n");
  for (int f = 0; f < 2; f++)
    {
      pi_switch_t sw;
      pi_switch_design (fcs[f], &sw);
      printf ("%.1f,%.2f,%.4f,%.4f,%.5f,%.4e,%.4f,%.2f,%.2f,%.4f,%.5f,%.2f,"
              "%.2f,%.4f,%.5f,%.4f,%.4f\n",
              fcs[f] / 1e9, sw.T * 1e6, sw.rho, sw.g, sw.bn_track, sw.x,
              sw.lag_e, sw.psi0 * 180.0 / M_PI, sw.peak_track_deg,
              sw.settle_track_s, sw.bn_wide, sw.peak_wide_deg, sw.sd_wide_deg,
              sw.hold_s, sw.bn_mid, sw.hold_mid_s, sw.w_s);
      DP_CHECK_MSG (!isnan (sw.bn_wide),
                    "a wide order-3 bandwidth holds the switch transient");
    }
  static const double delays[3] = { 0.0, 1.0, 1.75 };
  printf ("# design\nfc_ghz,esn0_db,order,bn_source,bn,"
          "T_us,rho,kw,slope,delay,bn_delivered,lsnr_rule_db,lsnr_sq_db,"
          "k_low,lag400_deg\n");
  for (int f = 0; f < 2; f++)
    for (int e = 0; e < 4; e++)
      {
        const double chip = dp_s415_chip_rate (fcs[f]), T = (double)SF / chip;
        const double rho = pi_rho (fcs[f], esn0s[e]);
        /* One coherent sum over the code-period window: one partial. */
        const int    kw = 1;
        const double k
            = pi_slope (rho, kw, 4410u + (uint64_t)(f * 8 + e * 2 + 1));
        const double bns[2]
            = { pi_bn_rule (pi_rho (fcs[f], PI_FLOOR_ESN0_DB)),
                pi_bn_rule_on || isnan (pi_bn_car) ? ASYNC_DSSS_RX_BN_CARRIER
                                                   : pi_bn_car };
        for (int order = 2; order <= 3; order++)
          for (int b = 0; b < 2; b++)
            for (int di = 0; di < 3; di++)
              {
                double g[3];
                if (order == 2)
                  {
                    dp_loop_filter_state_t lf;
                    dp_loop_filter_init (&lf, bns[b], 0.707, 1.0);
                    g[0] = lf.kp, g[1] = lf.ki, g[2] = 0.0;
                  }
                else
                  {
                    dp_state_filter_state_t sf;
                    dp_state_filter_init (&sf, bns[b], 1.0);
                    g[0] = sf.kp, g[1] = sf.ki, g[2] = sf.kr;
                  }
                const double bd = dp_state_filter_loop_bandwidth (
                    order, g[0], g[1], g[2], delays[di], k);
                /* The page's claim (4.9): at every level and
                   measured delay, the loop is stable at the detector's
                   slope, and order 3's slope clears its lower edge. */
                DP_CHECK_MSG (!isnan (bd), "the loop is stable at the "
                                           "detector's slope");
                DP_CHECK_MSG (k > pi_k_low (order, g, delays[di]),
                              "the slope is above the lower gain edge");
                printf (
                    "%.1f,%.1f,%d,%s,%.5f,%.2f,%.4f,%d,%.4f,%.2f,"
                    "%.6f,%.2f,%.2f,%.4f,%.3f\n",
                    fcs[f] / 1e9, esn0s[e], order, b ? "run" : "rule", bns[b],
                    T * 1e6, rho, kw, k, delays[di], bd,
                    10.0 * log10 (rho / (2.0 * bns[b])),
                    10.0
                        * log10 (k * k * rho
                                 / (4.0 * bd * (1.0 + kw / (2.0 * rho)))),
                    pi_k_low (order, g, delays[di]),
                    360.0
                        * dp_state_filter_steady_error (
                            order, g[1], g[2], 2, DP_S415_RATE_HZ_S * T * T));
              }
      }
}

int
main (int argc, char **argv)
{
  int         check = 0, emit = 0, design = 0;
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
      else if (strcmp (argv[i], "--bn-carrier") == 0 && i + 1 < argc)
        {
          if (strcmp (argv[++i], "rule") == 0)
            pi_bn_rule_on = 1;
          else
            {
              pi_bn_car = strtod (argv[i], NULL);
              DP_REQUIRE (pi_bn_car > 0.0 && pi_bn_car < 0.25);
            }
        }
      else if (strcmp (argv[i], "--loop-order") == 0 && i + 1 < argc)
        {
          pi_loop_order = atoi (argv[++i]);
          DP_REQUIRE (pi_loop_order == 2 || pi_loop_order == 3);
        }
      else if (strcmp (argv[i], "--dynamics") == 0 && i + 1 < argc)
        {
          ++i;
          DP_REQUIRE (strcmp (argv[i], "pass") == 0
                      || strcmp (argv[i], "ramp") == 0);
          pi_dyn_pass = strcmp (argv[i], "pass") == 0;
        }
      else if (strcmp (argv[i], "--seed-rate") == 0)
        pi_seed_rate = 1;
      else if (strcmp (argv[i], "--acq") == 0 && i + 1 < argc)
        {
          pi_acq = argv[++i][0];
          DP_REQUIRE (pi_acq >= 'a' && pi_acq <= 'f');
        }
      else if (strcmp (argv[i], "--design") == 0)
        design = 1;
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

  static const double fcs[2] = { DP_S415_FC_LO, DP_S415_FC_HI };
  /* The sweep's four levels, and a fifth only --esn0 selects: 20 dB, a
     high-SNR cell where the code lock's statistic reads its signal term
     with the max-over-windows selection's noise bias negligible. */
  static const double esn0s[5] = { 4.0, 5.0, 7.0, 10.0, 20.0 };
  double              rates[2] = { 0.0, DP_S415_RATE_HZ_S };
  /* --dynamics pass: the moving cell holds R_env, the use case's
     conservative residual rate, from the 50 km envelope. */
  if (pi_dyn_pass)
    {
      pi_pass_env_t pe;
      pi_pass_envelope (50.0, &pe);
      pi_pass_r_env = pi_r_env (pe.timing_max, pi_pass_rotation (pe.v_min));
      rates[1]      = pi_pass_r_env;
    }
  /* A slice flag that names no cell is refused: a run that measured
     nothing must not exit as if it had. */
  {
    int ok_fc = isnan (only_fc), ok_es = isnan (only_es),
        ok_r = isnan (only_rate);
    for (size_t k = 0; k < 2; k++)
      ok_fc |= fabs (fcs[k] - only_fc) <= 1e6;
    for (size_t k = 0; k < 5; k++)
      ok_es |= fabs (esn0s[k] - only_es) <= 1e-9;
    for (size_t k = 0; k < 2; k++)
      ok_r |= fabs (rates[k] - only_rate) <= 1e-9;
    if (!ok_fc || !ok_es || !ok_r)
      {
        DP_TEST_ERR ("validate_async_dsss_receiver_pullin: --fc/--esn0/"
                     "--rate names no cell (fc 2.2 or 2.8; esn0 4, 5, 7, "
                     "10 or 20; rate 0 or %g)\n",
                     rates[1]);
        return 2;
      }
  }

  /* One named draw with its telemetry: --telemetry DIR --draw
     FC_GHZ,ESN0,RATE,SEED [--from S --to S]. The capture is the
     receiver's probes and the scene's truth over the window, beside it
     the five lock rules; the draw's row is printed as the sweep prints
     it, so the capture is of a row the record has. */
  if (tlm_dir)
    {
      double   g = 0, es = 0, rt = 0;
      unsigned sd   = 0;
      int      used = 0;
      /* Exactly four fields: a fifth (the retired carrier window) is
         refused, not read as the seed. */
      DP_REQUIRE_MSG (
          one
              && sscanf (one, "%lf,%lf,%lf,%u%n", &g, &es, &rt, &sd, &used)
                     == 4
              && one[used] == '\0',
          "--telemetry names its draw with --draw");
      /* One stem names the draw's files: the capture and its rules. */
      char stem[256], path[1100], rules[1100], bn_tag[32] = "";
      if (pi_bn_rule_on)
        (void)snprintf (bn_tag, sizeof bn_tag, "_bnrule");
      else if (!isnan (pi_bn_car))
        (void)snprintf (bn_tag, sizeof bn_tag, "_bn%g", pi_bn_car);
      if (pi_loop_order == 3)
        {
          const size_t l = strlen (bn_tag);
          (void)snprintf (bn_tag + l, sizeof bn_tag - l, "_o3%c%s", pi_acq,
                          pi_seed_rate ? "s" : "");
        }
      (void)snprintf (stem, sizeof stem,
                      "pullin_%.1fGHz_%.0fdB_%.0fHzs_s%u%s%s", g, es, rt, sd,
                      spc ? (spc == 2 ? "_spc2" : "_spcN") : "", bn_tag);
      (void)snprintf (path, sizeof path, "%s/%s.tlm", tlm_dir, stem);
      (void)snprintf (rules, sizeof rules, "%s/%s.rules.csv", tlm_dir, stem);
      const capture_t cp = { path, rules, from_s, to_s };
      draw_t          d;
      DP_REQUIRE (
          run_draw (code, g * 1e9, es, rt, sd, secs, spc, &cp, NULL, &d) == 0);
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
      printf ("%s", PI_ROW_HEAD);
      emit_row (&d, sd);
      DP_TEST_EMIT_END ("validate_async_dsss_receiver_pullin --telemetry");
      DP_TEST_END ("validate_async_dsss_receiver_pullin --telemetry");
    }
  if (design)
    {
      pi_design (fcs, esn0s);
      DP_TEST_EMIT_END ("validate_async_dsss_receiver_pullin --design");
      DP_TEST_END ("validate_async_dsss_receiver_pullin --design");
    }
  if (emit)
    printf ("%s", PI_ROW_HEAD);
  else
    printf ("pull-in: 415.1-B-1, 11 MSa/s, 3000 sym/s, %u draw(s) of %.0f s "
            "a cell; C/N0 at the antenna; works = sustained, no change over "
            "the data, not false\n",
            draws, secs);
  draw_t *ds = dp_xmalloc (draws * sizeof *ds);
  for (size_t f = check ? 1 : 0; f < 2; f++)
    for (size_t e = check ? 3 : 0; e < (isnan (only_es) ? 4u : 5u); e++)
      for (size_t r = 0; r < (check ? 1u : 2u); r++)
        {
          /* --fc / --esn0 / --rate: one slice of the grid. */
          if ((!isnan (only_fc) && fabs (fcs[f] - only_fc) > 1e6)
              || (!isnan (only_es) && fabs (esn0s[e] - only_es) > 1e-9)
              || (!isnan (only_rate) && fabs (rates[r] - only_rate) > 1e-9))
            continue;
          size_t n = 0;
          DP_REQUIRE (pi_n_cells < PI_CELLS);
          dp_hist_t *h = &pi_hist[pi_n_cells][0][0];
          for (int v = 0; v < 2; v++)
            for (int q = 0; q < PQ_N; q++)
              dp_hist_init (&pi_hist[pi_n_cells][v][q], PQ[q].lo, PQ[q].hi,
                            PQ[q].nb);
          pi_cell_key[pi_n_cells][0] = fcs[f];
          pi_cell_key[pi_n_cells][1] = esn0s[e];
          pi_cell_key[pi_n_cells][2] = rates[r];
          pi_n_cells++;
          for (uint32_t s = 1; s <= draws; s++)
            {
              if ((s - 1) % shards != shard)
                continue;
              DP_REQUIRE (run_draw (code, fcs[f], esn0s[e], rates[r], s, secs,
                                    spc, NULL, h, &ds[n])
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
          check_acq (ds, n, !check && shards == 1 && draws >= PI_DRAWS, emit);
        }
  free (ds);
  if (check)
    {
      /* The live carrier holds a 400 Hz/s ramp: at 10 dB, the receiver as
         built, the end's carrier is within the false-lock limit of the
         truth. The draw is sized so the hand-over's Doppler is past it. */
      draw_t rd;
      DP_REQUIRE (run_draw (code, DP_S415_FC_HI, 10.0, DP_S415_RATE_HZ_S, 1,
                            PI_RAMP_CHECK_S, spc, NULL, NULL, &rd)
                  == 0);
      const double lim = 0.25 * DP_S415_SYM;
      printf ("  ramp: 2.8 GHz 10 dB +/-400 Hz/s, %.0f s: live "
              "carrier %.1f Hz off the truth; the hand-over's Doppler "
              "%.0f Hz off, %.2fx the %.0f Hz limit\n",
              PI_RAMP_CHECK_S, rd.end_resid_hz, rd.end_stale_hz,
              fabs (rd.end_stale_hz) / lim, lim);
      DP_CHECK_MSG (fabs (rd.end_stale_hz) > 1.2 * lim,
                    "the draw is long enough to tell the live carrier from "
                    "the hand-over's");
      DP_CHECK_MSG (!rd.false_lock, "the live carrier holds a 400 Hz/s ramp "
                                    "at 10 dB");

      /* #3134, pinned where it moves first: the 60-draw slice at 2.8 GHz,
         10 dB, static has the code lock change over the data, after it is
         sustained, on 16 of 60 draws -- the code lock alone, the other flags
         never. Seed 5 changes earliest, 0.84 s in, so a PI_CL_CHECK_S draw
         shows it. It is RED until #3134's fix: it is in
         the known red set by name. Its control runs the same seed with the
         look count forced to 4 (the receiver sizes 3 here) and holds: the
         count, not anything else in the draw, is the cause. */
      {
        draw_t cd, cc;
        pi_code_nl = 0;
        DP_REQUIRE (run_draw (code, DP_S415_FC_HI, 10.0, 0.0, PI_CL_SEED,
                              PI_CL_CHECK_S, spc, NULL, NULL, &cd)
                    == 0);
        pi_code_nl = 4;
        DP_REQUIRE (run_draw (code, DP_S415_FC_HI, 10.0, 0.0, PI_CL_SEED,
                              PI_CL_CHECK_S, spc, NULL, NULL, &cc)
                    == 0);
        pi_code_nl = 0;
        printf ("  code lock, 2.8 GHz 10 dB static seed %u: as built "
                "(n = %zu) changes %zu times over the data; at n = %zu, "
                "%zu (#3134)\n",
                PI_CL_SEED, cd.code_nl, cd.code_tog, cc.code_nl, cc.code_tog);
        DP_CHECK_MSG (!isnan (cd.t_code_s) && cd.code_tog == 0,
                      "#3134: the code lock does not change over the data "
                      "once sustained");
        DP_CHECK_MSG (cc.code_nl == 4 && !isnan (cc.t_code_s)
                          && cc.code_tog == 0,
                      "control: at 4 looks the same draw's code lock "
                      "holds");
      }
    }
  if (emit)
    pi_design (fcs, esn0s);
  if (emit)
    {
      /* The cells' histograms, and every attributed change. The rate is
         the cell's magnitude; a draw's sign is in its row. */
      printf ("# hist\nfc_ghz,esn0_db,rate_hz_s,draws,"
              "quantity,lo,hi,count\n");
      for (size_t c = 0; c < pi_n_cells; c++)
        for (int v = 0; v < 2; v++)
          for (int q = 0; q < PQ_N; q++)
            {
              char pre[160];
              (void)snprintf (pre, sizeof pre, "%.1f,%.1f,%.0f,%s,%s",
                              pi_cell_key[c][0] / 1e9, pi_cell_key[c][1],
                              pi_cell_key[c][2], v ? "off" : "on", PQ[q].name);
              dp_hist_emit (&pi_hist[c][v][q], pre);
            }
      printf ("# rule\nfc_ghz,esn0_db,rate_hz_s,rule,"
              "up_thresh,down_thresh,n_up,n_down\n");
      for (size_t c = 0; c < pi_n_cells; c++)
        for (int k = 0; k < PI_RULES; k++)
          printf ("%.1f,%.1f,%.0f,%s,%.6g,%.6g,%u,%u\n",
                  pi_cell_key[c][0] / 1e9, pi_cell_key[c][1],
                  pi_cell_key[c][2], pi_rule_name[k], pi_rule[c][k].up_thresh,
                  pi_rule[c][k].down_thresh, pi_rule[c][k].n_up,
                  pi_rule[c][k].n_down);
      printf ("# transition\nfc_ghz,esn0_db,rate_hz_s,seed,"
              "flag,up,t_s,stat,up_thresh,down_thresh,car_max_deg,"
              "car_rms_deg,car_slips,code_max_chip,evm_db,draw_evm_db,"
              "false_lock,on_code,car_max_mem_deg,coast_mem,coast_1s\n");
      for (size_t i = 0; i < pi_n_trans; i++)
        {
          const trans_t *t = &pi_trans[i];
          printf ("%.1f,%.1f,%.0f,%u,%s,%d,%.4f,%.4g,%.4g,%.4g,%.2f,%.2f,"
                  "%zu,%.4f,%.3f,%.3f,%d,%d,%.2f,%d,%d\n",
                  t->fc / 1e9, t->esn0, t->rate, t->seed,
                  t->flag ? "code" : "sym", t->up, t->t_s, t->stat,
                  t->rule.up_thresh, t->rule.down_thresh, t->car_max_deg,
                  t->car_rms_deg, t->car_slips, t->code_max_chip, t->evm_db,
                  t->draw_evm_db, t->false_lock, t->on_code,
                  t->car_max_mem_deg, t->coast_mem, t->coast_1s);
        }
    }
  free (pi_trans);
  if (emit)
    DP_TEST_EMIT_END ("validate_async_dsss_receiver_pullin");
  DP_TEST_END ("validate_async_dsss_receiver_pullin");
}
