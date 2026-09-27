/* uno-q — an RTL-SDR receive front end on a small ARM board.
 *
 * The first stages of a receiver fed by an RTL-SDR: take its native `cu8`
 * bytes, bring one channel to DC, decimate, and look at the spectrum.
 *
 *     cu8 bytes -> U8ToF32 -> DDC (mix by -offset, decimate by rate) -> PSD
 *
 * Written for, and measured on, an Arduino UNO Q (Qualcomm QRB2210, four
 * Cortex-A53-class cores), but nothing in it is board-specific: it builds and
 * runs anywhere doppler does, which is how CI checks it.
 *
 * Three ways to run it:
 *
 *   uno_q [flags]             SELF-TEST. Synthesises one second of what an
 *                             RTL-SDR would send for a tone at --offset in
 *                             noise, runs the chain, and CHECKS the result.
 *                             Every failed check exits non-zero; this is the
 *                             mode `make run` and CI use.
 *   uno_q [flags] - | FILE    LIVE. Reads cu8 from stdin or a capture file
 *                             and reports what it found. Nothing is checked
 *                             — live RF is not known in advance.
 *   uno_q [flags] --nats URL  NATS. Receives ci8 frames that uno_q_pub
 *                             published (built only when the doppler install
 *                             has its stream component). Counts lost and
 *                             duplicated frames from the wire header, then
 *                             runs the same chain.
 *
 *     --fs HZ        input sample rate (default 2.4e6; NATS: from the header)
 *     --offset HZ    where the wanted channel sits relative to the tuned
 *                    centre; the DDC mixes it to DC (default 250e3)
 *     --rate R       output/input rate of the DDC (default 0.125)
 *     --n N          PSD frame length (default 1024)
 *     --seconds S    LIVE/NATS: stop after S seconds of input (default: end)
 *     --pattern P    NATS: `sub` (default; core pub/sub, at-most-once) or
 *                    `pull` (JetStream work queue, at-least-once)
 *     --check        NATS: apply the self-test's checks to what arrived
 *                    (meaningful when `uno_q_pub --synthetic` sent it)
 *     --expect-frames N  NATS: fail unless exactly N frames were received
 *     --stall-ms MS  NATS: pause once, mid-stream, for MS milliseconds —
 *                    what a busy consumer does. Neither pattern may lose a
 *                    frame to it: doppler's subscriber queues without limit
 *                    (so pub/sub loss comes from a lost connection or a
 *                    link slower than the stream, not a slow reader), and
 *                    a pull consumer's frames wait on the broker
 *
 * WHY THE OFFSET. An RTL-SDR puts its own DC spike at the tuned centre, and
 * U8ToF32's fast `shift` mapping adds another DC term (it reads 0.5/128 low;
 * see u8_to_f32_core.h). Tuning the dongle a few hundred kHz away from the
 * wanted signal and mixing it down digitally moves BOTH out of the channel,
 * where the DDC's filter removes them. The self-test checks exactly that:
 * the bias is present before the DDC and gone after it.
 *
 * LOSS. Neither librtlsdr nor rtl_tcp reports dropped samples. doppler's
 * wire header numbers every frame, so over NATS a missing frame is a gap in
 * `sequence` and a redelivered one is a repeat — both counted exactly. The
 * end-of-stream frame takes the next number, so loss at the tail shows too.
 *
 * Measurement comes from the library: PSD (a Welch averager) supplies the
 * spectrum, its noise floor and the in-band SNR. Every number printed is
 * measured on the machine that runs it and carries its units; throughput
 * is printed and never asserted, because it is a property of the machine.
 */
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "doppler/ddc/ddc_core.h"
#include "doppler/i8_to_f32/i8_to_f32_core.h"
#include "doppler/psd/psd_core.h"
#include "doppler/spectral/spectral_core.h"
#include "doppler/u8_to_f32/u8_to_f32_core.h"
#ifdef UNO_Q_NATS
#include "doppler/stream/stream.h"
#endif

#include "scene.h"

/* Complex samples per processing block: 64 KiB of cu8 bytes. */
#define BLOCK ((size_t)32768)

#define SCENE_SECONDS 1.0 /* length of the self-test scene                */
#define N_PEAKS 6         /* LIVE: how many spectral peaks to list        */
#define SNR_TOL_DB 1.0    /* in-channel SNR against its prediction, dB     */
#define CHANNEL 0.1 /* |f| < CHANNEL * fs_out: the flat, in-channel band */

static int failures = 0;

/** @brief Report one named check; any failure sets the exit status. */
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

/** @brief Process CPU seconds, to report load as a share of one core. */
static double
cpu_s (void)
{
  return (double)clock () / CLOCKS_PER_SEC;
}

/* ── Sequence accounting ───────────────────────────────────────────────────
 */

/**
 * @brief Frame bookkeeping from the wire header's `sequence`.
 *
 * The first frame sets the baseline — a subscriber that joins a running
 * stream has not LOST the frames before it joined — and from then on a jump
 * forward is a gap of that many frames and a number already seen is a
 * duplicate (JetStream redelivery; PUB/SUB never repeats one).
 */
typedef struct
{
  int      started;
  uint64_t first, next;
  uint64_t frames, gaps, dups;
} seq_t;

/** @brief Account one frame; 1 if it is new data, 0 if a duplicate. */
static int
seq_account (seq_t *s, uint64_t seq)
{
  if (!s->started)
    {
      s->started = 1;
      s->first   = seq;
      s->next    = seq;
    }
  if (seq < s->next)
    {
      s->dups++;
      return 0;
    }
  s->gaps += seq - s->next;
  s->next = seq + 1;
  s->frames++;
  return 1;
}

/* ── The chain ─────────────────────────────────────────────────────────────
 */

/** @brief The chain's state, shared by every mode. */
typedef struct
{
  dp_u8_to_f32_state_t *cvt_u8; /* cu8 in (stdin, file, self-test)      */
  dp_i8_to_f32_state_t *cvt_i8; /* ci8 in (NATS)                        */
  dp_ddc_state_t       *ddc;
  dp_psd_state_t       *psd;
  float complex        *cf;    /* converted input block, BLOCK        */
  float complex        *stage; /* DDC output awaiting a whole frame   */
  size_t                stage_len, stage_cap;
  size_t                n; /* PSD frame length                    */
  double sum_re, sum_im;   /* converted-input sums (bias check)  */
  size_t n_in;             /* complex input samples processed    */
} chain_t;

static int
chain_init (chain_t *c, double fs, double offset, double rate, size_t n)
{
  memset (c, 0, sizeof *c);
  c->n      = n;
  c->cvt_u8 = dp_u8_to_f32_create (U8_TO_F32_SHIFT);
  /* ci8 at scale 128 is bit-identical to cu8 through U8ToF32's `shift`:
     the publisher sends (x - 128), and #1568's tests pin the equivalence. */
  c->cvt_i8 = dp_i8_to_f32_create (128.0f);
  c->ddc    = dp_ddc_create (-offset / fs, rate);
  /* Hann window, no padding, 0 dBFS = amplitude 1.0, linear mean. */
  c->psd       = dp_psd_create (n, fs * rate, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
  c->cf        = malloc (BLOCK * sizeof *c->cf);
  c->stage_cap = dp_ddc_execute_max_out (c->ddc, BLOCK) + n;
  c->stage     = malloc (c->stage_cap * sizeof *c->stage);
  return c->cvt_u8 && c->cvt_i8 && c->ddc && c->psd && c->cf && c->stage;
}

static void
chain_free (chain_t *c)
{
  dp_u8_to_f32_destroy (c->cvt_u8);
  dp_i8_to_f32_destroy (c->cvt_i8);
  dp_ddc_destroy (c->ddc);
  dp_psd_destroy (c->psd);
  free (c->cf);
  free (c->stage);
}

/**
 * @brief Push `pairs` converted complex samples (in c->cf) through the DDC
 * and into the PSD.
 *
 * The PSD consumes whole frames only, so DDC output accumulates in a stage
 * buffer and the remainder carries into the next block: the spectrum sees
 * one unbroken stream however the input was chunked.
 */
static void
chain_push_cf (chain_t *c, size_t pairs)
{
  for (size_t i = 0; i < pairs; i++)
    {
      c->sum_re += crealf (c->cf[i]);
      c->sum_im += cimagf (c->cf[i]);
    }
  c->n_in += pairs;

  c->stage_len
      += dp_ddc_execute (c->ddc, c->cf, pairs, c->stage + c->stage_len,
                         c->stage_cap - c->stage_len);
  size_t whole = c->stage_len - c->stage_len % c->n;
  dp_psd_accumulate (c->psd, c->stage, whole);
  memmove (c->stage, c->stage + whole,
           (c->stage_len - whole) * sizeof *c->stage);
  c->stage_len -= whole;
}

/** @brief cu8 in: at most BLOCK pairs per call. */
static void
chain_push (chain_t *c, const uint8_t *cu8, size_t pairs)
{
  dp_u8_to_f32_steps (c->cvt_u8, cu8, (float *)c->cf, 2 * pairs);
  chain_push_cf (c, pairs);
}

/** @brief ci8 in, any length: converted and pushed BLOCK pairs at a time. */
static void
chain_push_ci8 (chain_t *c, const int8_t *ci8, size_t pairs)
{
  for (size_t done = 0; done < pairs;)
    {
      size_t m = pairs - done < BLOCK ? pairs - done : BLOCK;
      dp_i8_to_f32_steps (c->cvt_i8, ci8 + 2 * done, (float *)c->cf, 2 * m);
      chain_push_cf (c, m);
      done += m;
    }
}

/** @brief Strongest bin of the averaged spectrum, as (frequency Hz, dB). */
static void
spectrum_peak (chain_t *c, double fs_out, float *db, double *f_hz,
               double *level_db)
{
  dp_psd_psd_db (c->psd, c->n, db, c->n);
  size_t k = 0;
  for (size_t i = 1; i < c->n; i++)
    if (db[i] > db[k])
      k = i;
  /* DC-centred: bin i is (i - n/2) / n of the output rate. */
  *f_hz     = ((double)k - (double)(c->n / 2)) * fs_out / (double)c->n;
  *level_db = db[k];
}

/**
 * @brief Mean noise per bin near DC, dB: bins 3 .. CHANNEL * n from the
 * centre, averaged in linear power, so the tone's own bins are excluded.
 *
 * Why not dp_psd_noise_floor(): that is the MEDIAN over the whole output band,
 * and the plain DDC's output is not white. dp_ddc_create() uses an
 * UNCOMPENSATED CIC (see ddc_core.h), whose droop reaches about -3.5 dB at a
 * quarter of the output rate and -12 dB near its edge, so the band-wide
 * median sits ~3.5 dB below the noise actually next to the tone. Measure the
 * noise where the signal is.
 */
static double
channel_noise_db (const float *db, size_t n)
{
  double sum = 0.0;
  size_t cnt = 0;
  for (size_t i = 0; i < n; i++)
    {
      size_t d = i > n / 2 ? i - n / 2 : n / 2 - i;
      if (d >= 3 && (double)d <= CHANNEL * (double)n)
        {
          sum += pow (10.0, db[i] / 10.0);
          cnt++;
        }
    }
  return cnt ? 10.0 * log10 (sum / (double)cnt) : 0.0;
}

/** @brief Index of the bin nearest `f_hz` in the DC-centred spectrum. */
static size_t
bin_of (size_t n, double fs_out, double f_hz)
{
  long k = lround (f_hz / fs_out * (double)n) + (long)(n / 2);
  return k < 0 ? 0 : k >= (long)n ? n - 1 : (size_t)k;
}

/**
 * @brief Mean noise per bin AROUND bin `k`, dB: bins 3 .. 12 away on either
 * side, in linear power. The comparison a spur test needs -- the level next
 * to it -- because the droop makes any one band-wide number wrong somewhere.
 */
static double
local_noise_db (const float *db, size_t n, size_t k)
{
  double sum = 0.0;
  size_t cnt = 0;
  for (long d = 3; d <= 12; d++)
    for (int sgn = -1; sgn <= 1; sgn += 2)
      {
        long i = (long)k + sgn * d;
        if (i >= 0 && i < (long)n)
          {
            sum += pow (10.0, db[i] / 10.0);
            cnt++;
          }
      }
  return cnt ? 10.0 * log10 (sum / (double)cnt) : 0.0;
}

/* ── Reports ───────────────────────────────────────────────────────────────
 */

/**
 * @brief The self-test's analysis: report the chain's result for the
 * synthetic scene and check it against predictions. Used by the self-test
 * and by `--nats ... --check`, so data that crossed the transport is held
 * to the same numbers as data built in place.
 */
static void
report_checked (chain_t *c, double fs, double offset, double rate)
{
  const size_t n      = c->n;
  const double fs_out = fs * dp_ddc_get_rate (c->ddc);
  float       *db     = malloc (n * sizeof *db);
  if (!db)
    {
      check (0, "allocate the spectrum");
      return;
    }
  double f_peak, peak_db;
  spectrum_peak (c, fs_out, db, &f_peak, &peak_db);
  const double floor_db = dp_psd_noise_floor (c->psd);
  const double chan_db  = channel_noise_db (db, n);
  const double bin_hz   = fs_out / (double)n;
  const double snr      = peak_db - chan_db;

  /* Predictions, from the scene and the chain's own parameters. In the flat
     channel around DC the DDC passes white input noise (quantization
     included) at the density it had, i.e. the share `rate` of its power per
     output-rate band; the PSD reads a tone at its power and noise at
     power * ENBW / n per bin. */
  const double q = 1.0 / 128.0;
  const double p_noise_in
      = 2.0 * SCENE_NOISE_SIGMA * SCENE_NOISE_SIGMA + 2.0 * q * q / 12;
  const double bin_noise = p_noise_in * rate * c->psd->enbw / (double)n;
  const double snr_pred  = 10.0 * log10 (SCENE_TONE_AMP * SCENE_TONE_AMP)
                           - 10.0 * log10 (bin_noise);
  const double bias      = -0.5 / 128.0;
  const double bias_se   = SCENE_NOISE_SIGMA / sqrt ((double)c->n_in);
  const double mean_re   = c->sum_re / (double)c->n_in;
  const double mean_im   = c->sum_im / (double)c->n_in;

  /* Where the bias lands after the DDC: input DC mixed by -offset, folded
     into the output band. */
  double       f_bias             = remainder (-offset, fs_out);
  const size_t k_bias             = bin_of (n, fs_out, f_bias);
  const double bias_db            = db[k_bias];
  const double bias_local_db      = local_noise_db (db, n, k_bias);
  const double bias_unfiltered_db = 10.0 * log10 (2.0 * bias * bias);

  printf ("\nconverted input (before the DDC):\n");
  printf ("  mean I %+.6f, Q %+.6f   (0.5/128 low = %+.6f)\n", mean_re,
          mean_im, bias);
  printf ("after the DDC (%.1f kSa/s out, %zu-point Hann PSD):\n",
          fs_out / 1e3, n);
  printf ("  peak %+.1f Hz at %.1f dB\n", f_peak, peak_db);
  printf ("  noise in the channel (|f| < %.2f fs_out) %.2f dB/bin; band-wide "
          "median %.2f dB/bin\n",
          CHANNEL, chan_db, floor_db);
  printf ("  SNR in the channel %.2f dB (predicted %.2f dB)\n", snr, snr_pred);
  printf ("  at %+.1f kHz, where the bias lands: %.1f dB against %.1f dB of "
          "noise around it\n    (%.1f dB if it had passed unfiltered)\n\n",
          f_bias / 1e3, bias_db, bias_local_db, bias_unfiltered_db);

  check (fabs (mean_re - bias) < 6 * bias_se
             && fabs (mean_im - bias) < 6 * bias_se,
         "the cu8 bias is present before the DDC (0.5/128 low, I and Q)");
  check (fabs (f_peak) <= bin_hz, "the tone is mixed to DC (within 1 bin)");
  check (fabs (snr - snr_pred) <= SNR_TOL_DB,
         "in-channel SNR matches the prediction (within 1 dB)");
  if (fabs (f_bias) > 3.0 * bin_hz)
    check (bias_db <= bias_local_db + 3.0,
           "the DDC filters the bias out (its image reads as noise)");
  else
    printf ("  [skip] bias image lands on the tone; pick another --offset\n");
  free (db);
}

/** @brief The live report: what arrived, what it cost, what is in it. */
static void
report_live (chain_t *c, double fs, double offset, double dt, double dc)
{
  const size_t n      = c->n;
  const double fs_out = fs * dp_ddc_get_rate (c->ddc);
  float       *db     = malloc (n * sizeof *db);
  if (!db)
    return;
  double f_peak, peak_db;
  spectrum_peak (c, fs_out, db, &f_peak, &peak_db);
  const double input_s = (double)c->n_in / fs;

  printf ("  %zu samples = %.2f s of input at %.3f MSa/s\n", c->n_in, input_s,
          fs / 1e6);
  printf ("  wall %.2f s; CPU %.2f s = %.1f%% of one core at real time\n", dt,
          dc, input_s > 0 ? 100.0 * dc / input_s : 0.0);
  printf ("  converted input mean I %+.6f, Q %+.6f\n",
          c->n_in ? c->sum_re / (double)c->n_in : 0.0,
          c->n_in ? c->sum_im / (double)c->n_in : 0.0);
  printf ("  channel at %+.1f kHz -> DC, %.1f kSa/s out\n", offset / 1e3,
          fs_out / 1e3);
  printf ("  strongest bin %+.1f kHz at %.1f dBFS\n", f_peak / 1e3, peak_db);
  /* Not "noise" here: live, whatever occupies the channel is in it. */
  printf ("  mean level in the channel (|f| < %.2f fs_out, excluding DC) "
          "%.1f dBFS/bin;\n    band-wide median %.1f dBFS/bin\n",
          CHANNEL, channel_noise_db (db, n), dp_psd_noise_floor (c->psd));
  printf ("  occupied bandwidth (99%%): %.1f kHz\n",
          dp_psd_occupied_bw (c->psd, 0.99) / 1e3);

  /* The strongest peaks, from the library's interpolating peak finder,
     gated 10 dB above the band-wide median so noise bumps are not listed.
     With --offset 0 the one at DC is the dongle's own spike. */
  dp_peak_t pk[N_PEAKS];
  size_t    np = dp_find_peaks_f32 (
      db, n, N_PEAKS, (float)dp_psd_noise_floor (c->psd) + 10.0f, pk);
  printf ("  strongest peaks (relative to the tuned centre + offset):\n");
  for (size_t i = 0; i < np; i++)
    printf ("    %+8.1f kHz  %6.1f dBFS\n",
            (double)pk[i].freq_norm * fs_out / 1e3, pk[i].amplitude_db);
  free (db);
}

/* ── Modes ─────────────────────────────────────────────────────────────────
 */

static int
self_test (double fs, double offset, double rate, size_t n)
{
  const size_t total = (size_t)(SCENE_SECONDS * fs);
  printf ("self-test: %.3f s of cu8 at %.3f MSa/s, tone at %+.1f kHz\n",
          SCENE_SECONDS, fs / 1e6, offset / 1e3);

  uint8_t *cu8 = scene_cu8 (fs, offset, total);
  chain_t  c;
  if (!cu8 || !chain_init (&c, fs, offset, rate, n))
    return 1;

  /* The first block carries the DDC's filter start-up; restart the average
     after it so the spectrum describes the steady state. */
  double t0   = now_s ();
  size_t done = 0;
  while (done < total)
    {
      size_t m = total - done < BLOCK ? total - done : BLOCK;
      chain_push (&c, cu8 + 2 * done, m);
      if (done == 0)
        dp_psd_reset (c.psd);
      done += m;
    }
  double dt = now_s () - t0;

  report_checked (&c, fs, offset, rate);
  printf ("  throughput: %.2f MSa/s complex in, %.1fx real time at %.2f "
          "MSa/s\n",
          (double)c.n_in / dt / 1e6, (double)c.n_in / dt / fs, fs / 1e6);

  /* The loss accounting NATS mode relies on, held to a scripted sequence:
     frame 3 missing, frame 4 delivered twice. */
  seq_t                 s        = { 0 };
  static const uint64_t script[] = { 0, 1, 2, 4, 4, 5 };
  for (size_t i = 0; i < sizeof script / sizeof script[0]; i++)
    seq_account (&s, script[i]);
  check (s.frames == 5 && s.gaps == 1 && s.dups == 1,
         "loss accounting counts a skipped frame and a repeated one");

  free (cu8);
  chain_free (&c);
  printf ("%s\n", failures ? "FAILED" : "all checks passed");
  return failures ? 1 : 0;
}

static int
live (const char *path, double fs, double offset, double rate, size_t n,
      double seconds)
{
  FILE *in = strcmp (path, "-") == 0 ? stdin : fopen (path, "rb");
  if (!in)
    {
      perror (path);
      return 1;
    }
  chain_t  c;
  uint8_t *buf = malloc (2 * BLOCK);
  if (!buf || !chain_init (&c, fs, offset, rate, n))
    return 1;
  const size_t limit = seconds > 0.0 ? (size_t)(seconds * fs) : (size_t)-1;

  double t0 = now_s (), c0 = cpu_s ();
  size_t have = 0; /* bytes in buf, carried across reads for I/Q pairing */
  while (c.n_in < limit)
    {
      size_t got = fread (buf + have, 1, 2 * BLOCK - have, in);
      if (got == 0)
        break;
      have += got;
      size_t pairs = have / 2;
      if (pairs > limit - c.n_in)
        pairs = limit - c.n_in;
      chain_push (&c, buf, pairs);
      memmove (buf, buf + 2 * pairs, have - 2 * pairs);
      have -= 2 * pairs;
    }
  double dt = now_s () - t0, dc = cpu_s () - c0;
  if (in != stdin)
    (void)fclose (in);

  printf ("live:\n");
  report_live (&c, fs, offset, dt, dc);
  free (buf);
  chain_free (&c);
  return 0;
}

#ifdef UNO_Q_NATS
/** @brief Sleep `ms` milliseconds. */
static void
sleep_ms (long ms)
{
  struct timespec t = { ms / 1000, (ms % 1000) * 1000000L };
  nanosleep (&t, NULL);
}

static int
nats_receive (const char *url, int pull, double fs_flag, double offset,
              double rate, size_t n, double seconds, int do_check,
              long expect_frames, long stall_ms)
{
  dp_sub_t  *sub = NULL;
  dp_pull_t *pl  = NULL;
  if (pull)
    pl = dp_pull_create (url);
  else
    sub = dp_sub_create (url);
  if (!sub && !pl)
    {
      (void)fprintf (stderr, "cannot connect to %s\n", url);
      return 1;
    }
  /* A timeout, not a hang: PUB/SUB can lose the end-of-stream frame like
     any other, so silence must end the run too. */
  if (pull)
    dp_pull_set_timeout (pl, 3000);
  else
    dp_sub_set_timeout (sub, 3000);
  printf ("ready: %s (%s)\n", url,
          pull ? "JetStream pull, at-least-once" : "pub/sub, at-most-once");
  (void)fflush (stdout);

  chain_t  c;
  int      have_chain = 0, stalled = 0, eos = 0, hit_limit = 0;
  uint64_t eos_seq = 0;
  seq_t    s       = { 0 };
  double   fs = fs_flag, fc = 0.0;
  uint64_t ts_first = 0, ts_last = 0, last_frame_samples = 0;
  double   t0 = 0.0, c0 = 0.0, waited = 0.0;
  /* A dropped connection (a broker restart, a Wi-Fi blip) is DP_ERR_RECV,
     and it is transient: the client reconnects, and a durable pull consumer
     picks up where it stopped. Retry it, counted, for a bounded time. */
  unsigned long recv_errors = 0;
  double        err_since   = 0.0;

  for (;;)
    {
      dp_msg_t   *msg = NULL;
      dp_header_t h;
      memset (&h, 0, sizeof h); /* EOS: 0, not garbage, if it isn't filled */
      int rc
          = pull ? dp_pull_recv (pl, &msg, &h) : dp_sub_recv (sub, &msg, &h);
      if (rc == DP_ERR_EOF)
        {
          eos     = 1;
          eos_seq = h.sequence;
          break;
        }
      if (rc == DP_ERR_TIMEOUT)
        {
          if (s.frames)
            break; /* the stream went quiet: end of input */
          if ((waited += 3.0) >= 30.0)
            {
              (void)fprintf (stderr, "no frames on %s in %.0f s\n", url,
                             waited);
              return 1;
            }
          continue;
        }
      if (rc == DP_ERR_RECV)
        {
          recv_errors++;
          if (err_since == 0.0)
            err_since = now_s ();
          if (now_s () - err_since >= 30.0)
            {
              (void)fprintf (stderr, "no connection to %s for 30 s\n", url);
              return 1;
            }
          sleep_ms (200);
          continue;
        }
      if (rc != DP_OK)
        {
          (void)fprintf (stderr, "receive failed: %d\n", rc);
          return 1;
        }
      err_since = 0.0;
      if (h.format != CI8)
        {
          (void)fprintf (stderr, "expected ci8 frames, got format 0x%04x\n",
                         h.format);
          dp_msg_free (msg);
          return 1;
        }
      if (!have_chain)
        {
          if (h.sample_rate > 0.0)
            fs = h.sample_rate;
          fc = h.center_freq;
          /* main() checked --offset against --fs; the stream's own rate
             is the one the DDC normalises by, so check it again here. */
          if (fabs (offset) >= fs / 2.0)
            {
              (void)fprintf (stderr,
                             "--offset %.1f Hz is outside the stream's "
                             "+/- %.1f Hz (sample_rate %.1f)\n",
                             offset, fs / 2.0, fs);
              dp_msg_free (msg);
              return 2;
            }
          if (!chain_init (&c, fs, offset, rate, n))
            {
              (void)fprintf (stderr, "could not build the chain\n");
              dp_msg_free (msg);
              return 1;
            }
          have_chain = 1;
          t0         = now_s ();
          c0         = cpu_s ();
        }
      if (seq_account (&s, h.sequence))
        {
          if (!ts_first)
            ts_first = h.timestamp_ns;
          ts_last            = h.timestamp_ns;
          last_frame_samples = h.num_samples;
          chain_push_ci8 (&c, (const int8_t *)dp_msg_data (msg),
                          h.num_samples);
          if (s.frames == 1)
            dp_psd_reset (c.psd); /* drop the DDC start-up, as the self-test */
        }
      if (pull)
        dp_msg_ack (msg); /* a duplicate is acked too: it is handled */
      dp_msg_free (msg);

      if (stall_ms > 0 && !stalled && s.frames >= 8)
        {
          stalled = 1;
          sleep_ms (stall_ms);
        }
      if (seconds > 0.0 && (double)c.n_in >= seconds * fs)
        {
          hit_limit = 1;
          break;
        }
    }
  double dt = now_s () - t0, dc = cpu_s () - c0;

  /* Frames the sender numbered but we never saw: gaps inside the stream,
     plus any missing between the last one and the end-of-stream frame. */
  uint64_t tail = eos && eos_seq > s.next ? eos_seq - s.next : 0;
  double   span = (double)(ts_last - ts_first) * 1e-9;
  double   src_rate
      = span > 0 ? (double)(c.n_in - last_frame_samples) / span : 0.0;

  printf ("nats (%s): %s\n", pull ? "pull" : "sub", url);
  printf (
      "  frames %llu (first sequence %llu), lost %llu in the stream + %llu "
      "at the tail, duplicates %llu\n",
      (unsigned long long)s.frames, (unsigned long long)s.first,
      (unsigned long long)s.gaps, (unsigned long long)tail,
      (unsigned long long)s.dups);
  if (recv_errors)
    printf ("  receive errors ridden out (connection lost and back): %lu\n",
            recv_errors);
  if (eos)
    printf ("  end-of-stream received, numbered %llu\n",
            (unsigned long long)eos_seq);
  else if (hit_limit)
    printf ("  stopped after --seconds %.3g (the stream was still live)\n",
            seconds);
  else
    printf ("  end-of-stream not seen (the stream went quiet)\n");
  if (src_rate > 0)
    printf ("  source rate from header timestamps: %.0f Sa/s (%+.0f ppm of "
            "%.0f)\n",
            src_rate, (src_rate / fs - 1.0) * 1e6, fs);
  if (fc > 0)
    printf ("  tuned to %.6f MHz\n", fc / 1e6);
  if (have_chain)
    {
      if (do_check)
        report_checked (&c, fs, offset, rate);
      else
        report_live (&c, fs, offset, dt, dc);
    }

  if (do_check || expect_frames >= 0)
    {
      check (s.gaps == 0 && tail == 0, "no frame was lost in transport");
      check (s.dups == 0, "no frame was delivered twice");
      if (expect_frames >= 0)
        check ((long)s.frames == expect_frames && s.first == 0,
               "every frame the publisher sent arrived");
    }
  if (have_chain)
    chain_free (&c);
  if (pull)
    dp_pull_destroy (pl);
  else
    dp_sub_destroy (sub);
  if (do_check || expect_frames >= 0)
    printf ("%s\n", failures ? "FAILED" : "all checks passed");
  return failures ? 1 : 0;
}
#endif /* UNO_Q_NATS */

static void
usage (const char *argv0)
{
  (void)fprintf (stderr,
                 "usage: %s [--fs HZ] [--offset HZ] [--rate R] [--n N]\n"
                 "          [--seconds S] [- | FILE]\n"
#ifdef UNO_Q_NATS
                 "       %s [...] --nats URL [--pattern sub|pull] [--check]\n"
                 "          [--expect-frames N] [--stall-ms MS]\n"
#endif
                 "  no input: self-test (synthetic cu8, checked)\n"
                 "  - / FILE: live cu8 from stdin or a capture file\n",
                 argv0
#ifdef UNO_Q_NATS
                 ,
                 argv0
#endif
  );
}

int
main (int argc, char **argv)
{
  double      fs = 2.4e6, offset = 250e3, rate = 0.125, seconds = 0.0;
  size_t      n     = 1024;
  const char *input = NULL, *nats = NULL, *pattern = "sub";
  int         do_check      = 0;
  long        expect_frames = -1, stall_ms = 0;

  for (int i = 1; i < argc; i++)
    {
      const char *a       = argv[i];
      int         has_val = i + 1 < argc;
      if (strcmp (a, "--fs") == 0 && has_val)
        fs = strtod (argv[++i], NULL);
      else if (strcmp (a, "--offset") == 0 && has_val)
        offset = strtod (argv[++i], NULL);
      else if (strcmp (a, "--rate") == 0 && has_val)
        rate = strtod (argv[++i], NULL);
      else if (strcmp (a, "--n") == 0 && has_val)
        n = (size_t)strtoul (argv[++i], NULL, 10);
      else if (strcmp (a, "--seconds") == 0 && has_val)
        seconds = strtod (argv[++i], NULL);
      else if (strcmp (a, "--nats") == 0 && has_val)
        nats = argv[++i];
      else if (strcmp (a, "--pattern") == 0 && has_val)
        pattern = argv[++i];
      else if (strcmp (a, "--check") == 0)
        do_check = 1;
      else if (strcmp (a, "--expect-frames") == 0 && has_val)
        expect_frames = strtol (argv[++i], NULL, 10);
      else if (strcmp (a, "--stall-ms") == 0 && has_val)
        stall_ms = strtol (argv[++i], NULL, 10);
      else if ((strcmp (a, "-") == 0 || a[0] != '-') && !input)
        input = a;
      else
        {
          usage (argv[0]);
          return 2;
        }
    }
  if (!(fs > 0.0) || !(rate > 0.0 && rate <= 1.0) || n < 2
      || fabs (offset) >= fs / 2.0)
    {
      (void)fprintf (stderr, "need fs > 0, 0 < rate <= 1, n >= 2 and "
                             "|offset| < fs/2\n");
      return 2;
    }
  if (nats)
    {
#ifdef UNO_Q_NATS
      int pull = strcmp (pattern, "pull") == 0;
      if (!pull && strcmp (pattern, "sub") != 0)
        {
          (void)fprintf (stderr, "--pattern is sub or pull\n");
          return 2;
        }
      return nats_receive (nats, pull, fs, offset, rate, n, seconds, do_check,
                           expect_frames, stall_ms);
#else
      (void)fprintf (stderr, "--nats needs a doppler install with its stream "
                             "component (doppler::stream)\n");
      return 2;
#endif
    }
  return input ? live (input, fs, offset, rate, n, seconds)
               : self_test (fs, offset, rate, n);
}
