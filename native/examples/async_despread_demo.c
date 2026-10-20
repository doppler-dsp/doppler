/**
 * async_despread_demo.c -- dp_dll as the despreader of a DSSS link whose data
 * clock is asynchronous to its code, unaided and aided by the carrier
 * estimate.
 *
 * This is the C twin of `src/doppler/examples/async_despread_demo.py`, which
 * runs the same link at the same two Dopplers and asserts the same physics,
 * with a figure. This file shows what the binding does for you and therefore
 * hides:
 *
 *   - dp_dll_create() takes the loop's damping and its early/late spacing
 *     too. The Python defaults, 0.707 and half a chip, are named here.
 *   - dp_dll_steps() writes into a buffer the CALLER owns.
 *     dp_dll_steps_max_out() returns 0, which means "allocate x_len": a push
 *     of one code period emits at most that many samples.
 *   - the channel takes at most DOPPLER_CHANNEL_MAX_BLOCK samples a call, and
 *     its output length varies by a sample from call to call. That variation
 *     is the dilation itself.
 *   - the telemetry is the caller's: one row per push, read from the loop's
 *     getters after the push. Nothing in the loop keeps a history.
 *
 * The motion that shifts the carrier also dilates the code clock: a carrier
 * offset f on a carrier fc is a code-rate ratio f/fc. The carrier estimate
 * that wipes the carrier, divided by fc, is the code-rate aid
 * (dp_dll_set_rate_aid()). Aided, the loop carries only what the estimate
 * missed, which lets it stay narrow.
 *
 * The checks:
 *
 *   §1  The despread output is SEGMENTS samples per code period, and every
 *       loop locks within 50 ms.
 *   §2  This run: aided, the loop holds lock at both Dopplers. Unaided, it
 *       holds this link's 0.6 ppm and loses the inflated 5 ppm.
 *   §3  A locked loop's settled filter output is the rate it carries, read
 *       high by up to one NCO step: the code NCO truncates its command to
 *       steps of SF * SPC / 2^32 chips per chip. Unaided, the loop carries
 *       the dilation; aided, nothing.
 *   §4  A locked loop's output is the data along one axis, at whatever
 *       carrier phase the data sits: its power along that axis stands above
 *       its power across it by up to the link budget's 1 + 2 * SNR per output
 *       sample. It reads below the budget because a sample straddling a data
 *       flip loses amplitude. The loop that lost the code puts out noise.
 *
 * The despread rate in §1, the step bound in §3 and the budget in §4 come
 * from the object and the link, not the draw; §3 widens the bound by a
 * quarter step either side for the noise left in the settled mean, and §4
 * takes half the budget as its line. The 50 ms first lock and §2 describe this
 * run, which is the Python twin's: the same emitter, channel, noise seed and
 * loop, so the two print the same numbers.
 *
 * Build:
 *   cmake --build build
 *   ./build/native/examples/async_despread_demo
 */
#include "doppler/dp_complex.h"
#include <doppler/awgn/awgn_core.h>
#include <doppler/detection/detection_core.h>
#include <doppler/dll/dll_core.h>
#include <doppler/doppler_channel/doppler_channel_core.h>
#include <doppler/gold/gold_core.h>
#include <doppler/lo/lo_core.h>
#include <doppler/wfm_synth/wfm_synth_core.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Setup: this scenario's numbers ─────────────────────────────────────────
 * Gold-1023 at 3.069 Mcps, 2700 bps BPSK, two samples per chip, so one code
 * period is 2,046 samples: one push. A 1500 Hz carrier offset on a 2.5 GHz
 * carrier is this link's Doppler, 0.6 ppm; 5 ppm is inflated, to show what the
 * aid is for. Es/N0 is 10 dB. */
#define SF 1023u
#define SPC 2u
#define TE (SF * SPC)
#define CHIP_RATE 3.069e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define EPOCH_S ((double)TE / FS)
#define CARRIER_HZ 2.5e9
#define CARRIER_OFFSET_HZ 1500.0
#define LINK_PPM (1e6 * CARRIER_OFFSET_HZ / CARRIER_HZ)
#define INFLATED_PPM 5.0
#define ESN0_DB 10.0
#define CN0_DBHZ (ESN0_DB + 10.0 * log10 (SYM_RATE))
#define DURATION_S 0.6
#define SEED_EMITTER 0u
#define SEED_NOISE 2u

/* The despreader. */
#define BN 0.002    /* narrow, for a quiet code phase */
#define ZETA 0.707  /* the loop's damping */
#define SPACING 0.5 /* early/late tap offset, chips */
#define SEGMENTS 4u /* despread samples per code period */
#define PFA 1e-3    /* the lock detector's false-alarm rate per decision */
#define PD 0.99     /* its detection rate per decision, at the C/N0 */

/* The code NCO's rate step, chips per chip: one phase increment of its
   32-bit accumulator, which turns once per code period. */
#define NCO_STEP ((double)TE / 4294967296.0)

/** @brief One push's telemetry, read from the loop after the push. */
struct row
{
  int    locked;     /* the code-lock flag */
  double lock_stat;  /* the statistic it thresholds */
  double filter_out; /* code_rate - 1: the loop filter output */
  size_t samples;    /* despread samples the push emitted */
};

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

/** @brief The link at a Doppler: the shipped continuous-DSSS synth on
 *  nominal frequency with its own PRBS payload, through a DopplerChannel,
 *  plus noise.
 *
 *  Writes the received stream into @p x, which holds @p cap samples, and
 *  returns its length, or 0 if a source fails to open. The channel's carrier
 *  offset and delay go to @p offset_hz and @p delay: they stand in for the
 *  carrier estimate an upstream carrier loop hands over and the code phase
 *  acquisition hands over. */
static size_t
receive (const uint8_t *code, double doppler_ppm, float complex *x, size_t cap,
         double *offset_hz, double *delay)
{
  const size_t n = (size_t)(DURATION_S / EPOCH_S) * TE;
  /* The emitter, named in dp_wfm_synth_create()'s order: wfm.Synth's own
     defaults, so the payload is the one the Python twin sends. */
  const int             type = WFM_SYNTH_DSSS;
  const double          freq = 0.0;                 /* the channel adds it */
  const double          snr  = WFM_SYNTH_SNR_CLEAN; /* noise is added below */
  const int             snr_mode  = 0;              /* auto */
  const uint32_t        seed      = SEED_EMITTER;
  const int             sps       = (int)SPC; /* samples per chip */
  const int             pn_length = 15;       /* the PRBS register */
  const uint64_t        pn_poly   = 0;        /* its maximal-length default */
  const int             lfsr      = 0;        /* Galois */
  const double          f_end     = 0.0;
  dp_wfm_synth_state_t *syn
      = dp_wfm_synth_create (type, FS, freq, snr, snr_mode, seed, sps,
                             pn_length, pn_poly, lfsr, f_end);
  dp_doppler_channel_state_t *ch
      = dp_doppler_channel_create (FS, CARRIER_HZ, doppler_ppm, 0.0);
  dp_awgn_state_t *awg = dp_awgn_create (
      SEED_NOISE, dp_awgn_amplitude_for_snr (
                      (float)(ESN0_DB - 10.0 * log10 (FS / SYM_RATE)), 1.0f));
  float complex *tx  = malloc (cap * sizeof *tx);
  size_t         got = 0;
  if (syn && ch && awg && tx && n <= cap
      && dp_wfm_synth_set_dsss_cont (syn, code, SF, CHIP_RATE / SYM_RATE,
                                     WFM_DSSS_DATA_PRBS, NULL, 0)
             == 0)
    {
      dp_wfm_synth_steps (syn, tx, n);
      /* The channel takes at most DOPPLER_CHANNEL_MAX_BLOCK samples a call. */
      for (size_t i = 0; i < n; i += DOPPLER_CHANNEL_MAX_BLOCK)
        {
          size_t blk = n - i < DOPPLER_CHANNEL_MAX_BLOCK
                           ? n - i
                           : DOPPLER_CHANNEL_MAX_BLOCK;
          got += dp_doppler_channel_execute (ch, tx + i, blk, x + got,
                                             cap - got);
        }
      /* The transmit buffer is spent; it holds the noise now. */
      dp_awgn_generate (awg, got, tx, cap);
      for (size_t i = 0; i < got; i++)
        x[i] += tx[i];
      *offset_hz = dp_doppler_channel_get_offset_hz (ch);
      *delay     = dp_doppler_channel_get_delay_samples (ch);
    }
  free (tx);
  dp_awgn_destroy (awg);
  dp_doppler_channel_destroy (ch);
  dp_wfm_synth_destroy (syn);
  return got;
}

/** @brief Wipe the carrier, then despread with a Dll, one code period per
 *  push, and read the loop after each push into @p rows.
 *
 *  One carrier estimate does two jobs: it wipes the carrier, and divided by
 *  the carrier frequency it is the code-rate aid. The loop starts at the code
 *  phase acquisition reports, to one sample; the code phase runs opposite to
 *  the delay, so a code L chips late starts the loop at SF - L. The lock
 *  detector is sized for the link, as AsyncDsssReceiver sizes its own: looks
 *  coherent over the transition-free part of a symbol, as many as the C/N0
 *  needs for PD, and a drop count that holds false drops under 1e-6 per
 *  decision. Writes the lock threshold to @p eta, and the despread output --
 *  SEGMENTS samples per code period, the data with the code removed -- to
 *  @p stream, which holds @p stream_cap samples, its length to
 *  @p stream_len. Returns 0, or 1 if the loop cannot be built or
 *  configured. */
static int
track (const uint8_t *code, const float complex *x, size_t n_push,
       double carrier_estimate_hz, double delay, int aided, struct row *rows,
       double *eta, float complex *stream, size_t stream_cap,
       size_t *stream_len)
{
  /* Named in dp_dll_create()'s order, so an arity mismatch shows as a line. */
  const size_t    code_len  = SF;
  const size_t    spc       = SPC;
  const double    init_chip = (double)SF - floor (delay) / (double)SPC;
  const double    bn        = BN;
  const double    zeta      = ZETA;
  const double    spacing   = SPACING;
  const size_t    segments  = SEGMENTS;
  dp_dll_state_t *dll      = dp_dll_create (code, code_len, spc, init_chip, bn,
                                            zeta, spacing, segments);
  dp_lo_state_t  *lo       = dp_lo_create (-carrier_estimate_hz / FS);
  float complex  *wiped    = malloc ((size_t)TE * sizeof *wiped);
  float complex  *despread = malloc ((size_t)TE * sizeof *despread);
  CHECK (dll && lo && wiped && despread, "the loop and its buffers open");

  CHECK (dp_dll_set_symbol_period (dll, SEGMENTS * CHIP_RATE
                                            / ((double)SF * SYM_RATE))
             == DP_OK,
         "the symbol period is taken");
  const size_t look    = dp_dll_get_symbol_window (dll) * (TE / SEGMENTS);
  const int    n_looks = dp_det_n_noncoh (dp_det_cn0_to_snr (CN0_DBHZ, FS),
                                          (int)look, PD, PFA, 4000);
  CHECK (n_looks >= 1, "the C/N0 supports a lock decision");
  CHECK (dp_dll_configure_lock (dll, PFA, (size_t)n_looks, 0.0) == DP_OK,
         "the lock detector configures");
  CHECK (dp_dll_set_lock_verify (
             dll, (uint32_t)dp_det_verify_count (PFA, PFA * 1e-3),
             (uint32_t)dp_det_verify_count (1.0 - PD, 1e-6))
             == DP_OK,
         "the verify counts are taken");
  if (aided)
    dp_dll_set_rate_aid (dll, carrier_estimate_hz / CARRIER_HZ);
  *eta = dp_det_threshold_noncoherent (PFA, n_looks);

  *stream_len = 0;
  for (size_t k = 0; k < n_push; k++)
    {
      dp_lo_steps (lo, TE, wiped, TE);
      for (size_t i = 0; i < TE; i++)
        wiped[i] *= x[k * TE + i];
      size_t m = dp_dll_steps (dll, wiped, TE, despread, TE);
      for (size_t i = 0; i < m && *stream_len < stream_cap; i++)
        stream[(*stream_len)++] = despread[i];
      rows[k].locked     = dp_dll_get_locked (dll);
      rows[k].lock_stat  = dp_dll_get_lock_stat (dll);
      rows[k].filter_out = dp_dll_get_code_rate (dll) - 1.0;
      rows[k].samples    = m;
    }

  /* The loop owns its internals; destroy() frees them. The buffers are the
     caller's, and the caller frees them. */
  dp_dll_destroy (dll);
  dp_lo_destroy (lo);
  free (despread);
  free (wiped);
  return 0;
}

/** @brief The first push in @p r whose lock flag is up, or @p n if none. */
static size_t
first_lock (const struct row *r, size_t n)
{
  for (size_t i = 0; i < n; i++)
    if (r[i].locked)
      return i;
  return n;
}

/** @brief Whether every push in [lo, n) is locked. */
static int
locked_from (const struct row *r, size_t lo, size_t n)
{
  for (size_t i = lo; i < n; i++)
    if (!r[i].locked)
      return 0;
  return 1;
}

/** @brief The mean filter output over the settled half of a run. */
static double
settled_filter (const struct row *r, size_t n)
{
  double sum = 0.0;
  for (size_t i = n / 2; i < n; i++)
    sum += r[i].filter_out;
  return sum / (double)(n - n / 2);
}

int
main (void)
{
  uint8_t          code[SF];
  dp_gold_state_t *gd = dp_gold_create (934, 350, 567, 73, 10);
  CHECK (gd != NULL, "the Gold generator opens");
  dp_gold_generate (gd, SF, code, SF);
  dp_gold_destroy (gd);

  /* The capture, with room for the channel's sample of slack. */
  const size_t   n   = (size_t)(DURATION_S / EPOCH_S) * TE;
  const size_t   cap = n + 64;
  float complex *x   = malloc (cap * sizeof *x);
  struct row    *rows[2][2]; /* [link, inflated][unaided, aided] */
  float complex *stream[2][2];
  size_t         stream_len[2][2];
  size_t         n_push[2];
  const double   ppm[2] = { LINK_PPM, INFLATED_PPM };
  double         eta    = 0.0;
  CHECK (x != NULL, "the capture allocates");

  for (int d = 0; d < 2; d++)
    {
      double offset_hz = 0.0, delay = 0.0;
      size_t len = receive (code, ppm[d], x, cap, &offset_hz, &delay);
      CHECK (len > 0, "the %.1f ppm link opens", ppm[d]);
      n_push[d] = len / TE;
      for (int aided = 0; aided < 2; aided++)
        {
          /* SEGMENTS a period, and one period's more for the slack. */
          const size_t stream_cap = (n_push[d] + 1) * SEGMENTS;
          rows[d][aided]          = malloc (n_push[d] * sizeof (struct row));
          stream[d][aided] = malloc (stream_cap * sizeof (float complex));
          CHECK (rows[d][aided] && stream[d][aided], "the rows allocate");
          CHECK (track (code, x, n_push[d], offset_hz, delay, aided,
                        rows[d][aided], &eta, stream[d][aided], stream_cap,
                        &stream_len[d][aided])
                     == 0,
                 "the %.1f ppm run completes", ppm[d]);
        }
    }

  const double ms = 1e3 * EPOCH_S; /* one push, ms */

  /* §1: SEGMENTS despread samples per code period, and every loop locks. */
  size_t first[2][2];
  size_t latest = 0;
  for (int d = 0; d < 2; d++)
    for (int aided = 0; aided < 2; aided++)
      {
        const struct row *r     = rows[d][aided];
        size_t            total = 0;
        for (size_t k = 0; k < n_push[d]; k++)
          total += r[k].samples;
        CHECK (labs ((long)total - (long)(SEGMENTS * n_push[d]))
                   <= (long)SEGMENTS,
               "%zu despread samples over %zu code periods", total, n_push[d]);
        first[d][aided] = first_lock (r, n_push[d]);
        CHECK ((double)first[d][aided] * ms < 50.0,
               "%.1f ppm aided=%d: first lock at %.0f ms", ppm[d], aided,
               (double)first[d][aided] * ms);
        if (first[d][aided] > latest)
          latest = first[d][aided];
      }
  printf ("§1  %u despread samples per code period; every loop locked "
          "within %.0f ms (lock threshold %.1f)\n",
          SEGMENTS, (double)latest * ms, eta);

  /* §2, this run: aided, the loop holds both Dopplers; unaided, it holds
     this link's and loses the inflated one. */
  CHECK (locked_from (rows[0][1], first[0][1], n_push[0]),
         "this run: the aided loop lost lock at %.1f ppm", LINK_PPM);
  CHECK (locked_from (rows[1][1], first[1][1], n_push[1]),
         "this run: the aided loop lost lock at %.0f ppm", INFLATED_PPM);
  CHECK (locked_from (rows[0][0], first[0][0], n_push[0]),
         "this run: the unaided loop lost lock at %.1f ppm", LINK_PPM);
  size_t last = 0;
  for (size_t k = 0; k < n_push[1]; k++)
    if (rows[1][0][k].locked)
      last = k + 1;
  CHECK (last <= n_push[1] / 2,
         "this run: the unaided loop held the inflated Doppler");
  printf ("§2  this run: both loops held %.1f ppm; at the inflated %.0f ppm "
          "the unaided loop lost lock %.0f ms in, the aided one held\n",
          LINK_PPM, INFLATED_PPM, (double)last * ms);

  /* §3: a locked loop's settled filter output is the rate it carries, read
     high by up to one NCO step, with a quarter step either side for the
     noise left in the mean. */
  struct
  {
    int d, aided;
  } settled[3] = { { 0, 0 }, { 0, 1 }, { 1, 1 } };
  for (int i = 0; i < 3; i++)
    {
      int    d       = settled[i].d;
      int    aided   = settled[i].aided;
      double carried = aided ? 0.0 : ppm[d] * 1e-6;
      double mean    = settled_filter (rows[d][aided], n_push[d]);
      double steps   = (mean - carried) / NCO_STEP;
      CHECK (steps >= -0.25 && steps <= 1.25,
             "%.1f ppm aided=%d: filter output %+.2f ppm is %+.2f NCO steps "
             "from the %.2f ppm the loop carries",
             ppm[d], aided, 1e6 * mean, steps, 1e6 * carried);
      printf ("§3  %.1f ppm %s: settled filter output %+.2f ppm, %.2f NCO "
              "steps above the %.2f ppm the loop carries\n",
              ppm[d], aided ? "aided  " : "unaided", 1e6 * mean, steps,
              1e6 * carried);
    }

  /* §4: the output is the data along one axis while the loop holds the
     code, noise once it has lost it. The two eigenvalues of the output's
     I/Q second moments are its power along its strongest axis and across
     it, whatever carrier phase the data sits at. Per output sample the SNR
     is Es/N0 over the samples a symbol spans, so along stands above across
     by up to 1 + 2 * SNR; half that is the line. */
  const double per_symbol = SEGMENTS * CHIP_RATE / ((double)SF * SYM_RATE);
  const double budget = 1.0 + 2.0 * pow (10.0, ESN0_DB / 10.0) / per_symbol;
  for (int d = 0; d < 2; d++)
    for (int aided = 0; aided < 2; aided++)
      {
        const float complex *s  = stream[d][aided];
        const size_t         ns = stream_len[d][aided];
        double               ii = 0.0, qq = 0.0, iq = 0.0;
        for (size_t k = ns / 2; k < ns; k++)
          {
            const double i = crealf (s[k]), q = cimagf (s[k]);
            ii += i * i;
            qq += q * q;
            iq += i * q;
          }
        /* The eigenvalues of [[ii, iq], [iq, qq]]: mean +/- spread. */
        const double mean   = 0.5 * (ii + qq);
        const double spread = sqrt (0.25 * (ii - qq) * (ii - qq) + iq * iq);
        const double ratio  = (mean + spread) / (mean - spread);
        const int    holds  = !(d == 1 && aided == 0);
        CHECK ((ratio > budget / 2.0) == holds,
               "%.1f ppm aided=%d: output power along its axis against across "
               "it %.2f, against the budget's %.2f",
               ppm[d], aided, ratio, budget);
        printf ("§4  %.1f ppm %s: output power along its axis against across "
                "it %.1f (budget %.1f)\n",
                ppm[d], aided ? "aided  " : "unaided", ratio, budget);
      }

  for (int d = 0; d < 2; d++)
    for (int aided = 0; aided < 2; aided++)
      {
        free (rows[d][aided]);
        free (stream[d][aided]);
      }
  free (x);
  printf ("OK\n");
  return 0;
}
