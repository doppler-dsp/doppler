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
 * Two ways to run it:
 *
 *   uno_q [flags]             SELF-TEST. Synthesises one second of what an
 *                             RTL-SDR would send for a tone at --offset in
 *                             noise, runs the chain, and CHECKS the result.
 *                             Every failed check exits non-zero; this is the
 *                             mode `make run` and CI use.
 *   uno_q [flags] - | FILE    LIVE. Reads cu8 from stdin or a capture file
 *                             and reports what it found. Nothing is checked
 *                             — live RF is not known in advance.
 *
 *     --fs HZ        input sample rate (default 2.4e6, an RTL-SDR's usual)
 *     --offset HZ    where the wanted channel sits relative to the tuned
 *                    centre; the DDC mixes it to DC (default 250e3)
 *     --rate R       output/input rate of the DDC (default 0.125)
 *     --n N          PSD frame length (default 1024)
 *     --seconds S    LIVE only: stop after S seconds of input (default: EOF)
 *
 * WHY THE OFFSET. An RTL-SDR puts its own DC spike at the tuned centre, and
 * U8ToF32's fast `shift` mapping adds another DC term (it reads 0.5/128 low;
 * see u8_to_f32_core.h). Tuning the dongle a few hundred kHz away from the
 * wanted signal and mixing it down digitally moves BOTH out of the channel,
 * where the DDC's filter removes them. The self-test checks exactly that:
 * the bias is present before the DDC and gone after it.
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

#include "doppler/awgn/awgn_core.h"
#include "doppler/ddc/ddc_core.h"
#include "doppler/lo/lo_core.h"
#include "doppler/psd/psd_core.h"
#include "doppler/spectral/spectral_core.h"
#include "doppler/u8_to_f32/u8_to_f32_core.h"

/* Complex samples per processing block: 64 KiB of cu8 bytes. */
#define BLOCK ((size_t)32768)

/* The self-test scene: one second of a tone in noise, well inside the
   8-bit range (peaks near 0.45 of full scale, so nothing clips). */
#define SCENE_SECONDS 1.0
#define TONE_AMP 0.25    /* complex tone amplitude; power -12.0 dBFS      */
#define NOISE_SIGMA 0.05 /* per-component std dev; complex power -23 dBFS */
#define N_PEAKS 6      /* LIVE: how many spectral peaks to list             */
#define SNR_TOL_DB 1.0 /* in-channel SNR against its prediction, dB     */
#define CHANNEL 0.1    /* |f| < CHANNEL * fs_out: the flat, in-channel band */

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

/**
 * @brief The dongle's 8-bit ADC, as a model: analog `v` to its cu8 code.
 *
 * The RTL2832U's zero sits BETWEEN codes 127 and 128, so a code covers
 * `[c - 128, c - 127) / 128` and the analog zero reads 127.5 on average —
 * which is what makes U8ToF32's `shift` read 0.5/128 low. This is a model
 * of the hardware, not a DSP stage: it exists only to give the self-test
 * the bytes a real dongle would.
 */
static uint8_t
rtl_code (float v)
{
  float c = floorf (128.0f + 128.0f * v);
  return (uint8_t)(c < 0.0f ? 0.0f : c > 255.0f ? 255.0f : c);
}

/** @brief The chain's state, shared by both modes. */
typedef struct
{
  u8_to_f32_state_t *cvt;
  ddc_state_t       *ddc;
  psd_state_t       *psd;
  float complex     *cf;    /* converted input block, BLOCK        */
  float complex     *stage; /* DDC output awaiting a whole frame   */
  size_t             stage_len, stage_cap;
  size_t             n;              /* PSD frame length                    */
  double             sum_re, sum_im; /* converted-input sums (bias check)  */
  size_t             n_in;           /* complex input samples processed    */
} chain_t;

static int
chain_init (chain_t *c, double fs, double offset, double rate, size_t n)
{
  memset (c, 0, sizeof *c);
  c->n   = n;
  c->cvt = u8_to_f32_create (U8_TO_F32_SHIFT);
  c->ddc = ddc_create (-offset / fs, rate);
  /* Hann window, no padding, 0 dBFS = amplitude 1.0, linear mean. */
  c->psd       = psd_create (n, fs * rate, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
  c->cf        = malloc (BLOCK * sizeof *c->cf);
  c->stage_cap = ddc_execute_max_out (c->ddc, BLOCK) + n;
  c->stage     = malloc (c->stage_cap * sizeof *c->stage);
  return c->cvt && c->ddc && c->psd && c->cf && c->stage;
}

static void
chain_free (chain_t *c)
{
  u8_to_f32_destroy (c->cvt);
  ddc_destroy (c->ddc);
  psd_destroy (c->psd);
  free (c->cf);
  free (c->stage);
}

/**
 * @brief Push `pairs` complex samples of cu8 through the chain.
 *
 * The PSD consumes whole frames only, so DDC output accumulates in a stage
 * buffer and the remainder carries into the next block: the spectrum sees
 * one unbroken stream however the input was chunked.
 */
static void
chain_push (chain_t *c, const uint8_t *cu8, size_t pairs)
{
  u8_to_f32_steps (c->cvt, cu8, (float *)c->cf, 2 * pairs);
  for (size_t i = 0; i < pairs; i++)
    {
      c->sum_re += crealf (c->cf[i]);
      c->sum_im += cimagf (c->cf[i]);
    }
  c->n_in += pairs;

  c->stage_len += ddc_execute (c->ddc, c->cf, pairs, c->stage + c->stage_len,
                               c->stage_cap - c->stage_len);
  size_t whole = c->stage_len - c->stage_len % c->n;
  psd_accumulate (c->psd, c->stage, whole);
  memmove (c->stage, c->stage + whole,
           (c->stage_len - whole) * sizeof *c->stage);
  c->stage_len -= whole;
}

/** @brief Strongest bin of the averaged spectrum, as (frequency Hz, dB). */
static void
spectrum_peak (chain_t *c, double fs_out, float *db, double *f_hz,
               double *level_db)
{
  psd_psd_db (c->psd, c->n, db, c->n);
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
 * Why not psd_noise_floor(): that is the MEDIAN over the whole output band,
 * and the plain DDC's output is not white. ddc_create() uses an
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

static int
self_test (double fs, double offset, double rate, size_t n)
{
  const size_t total = (size_t)(SCENE_SECONDS * fs);
  printf ("self-test: %.3f s of cu8 at %.3f MSa/s, tone at %+.1f kHz\n",
          SCENE_SECONDS, fs / 1e6, offset / 1e3);

  /* The scene: tone + AWGN, through the dongle's ADC model, into bytes.
     Built with the library's own LO and AWGN sources. */
  float complex *x   = malloc (total * sizeof *x);
  float complex *w   = malloc (total * sizeof *w);
  uint8_t       *cu8 = malloc (2 * total);
  lo_state_t    *lo  = lo_create (offset / fs);
  awgn_state_t  *ns  = awgn_create (0x5eedu, (float)NOISE_SIGMA);
  if (!x || !w || !cu8 || !lo || !ns)
    return 1;
  lo_steps (lo, total, x, total);
  awgn_generate (ns, total, w, total);
  for (size_t i = 0; i < total; i++)
    {
      float complex v = (float)TONE_AMP * x[i] + w[i];
      cu8[2 * i]      = rtl_code (crealf (v));
      cu8[2 * i + 1]  = rtl_code (cimagf (v));
    }
  lo_destroy (lo);
  awgn_destroy (ns);
  free (x);
  free (w);

  chain_t c;
  if (!chain_init (&c, fs, offset, rate, n))
    return 1;
  const double fs_out = fs * ddc_get_rate (c.ddc);

  /* The first block carries the DDC's filter start-up; restart the average
     after it so the spectrum describes the steady state. */
  double t0   = now_s ();
  size_t done = 0;
  while (done < total)
    {
      size_t m = total - done < BLOCK ? total - done : BLOCK;
      chain_push (&c, cu8 + 2 * done, m);
      if (done == 0)
        psd_reset (c.psd);
      done += m;
    }
  double dt = now_s () - t0;

  float *db = malloc (n * sizeof *db);
  if (!db)
    return 1;
  double f_peak, peak_db;
  spectrum_peak (&c, fs_out, db, &f_peak, &peak_db);
  const double floor_db = psd_noise_floor (c.psd);
  const double chan_db  = channel_noise_db (db, n);
  const double bin_hz   = fs_out / (double)n;
  const double snr      = peak_db - chan_db;

  /* Predictions, from the scene and the chain's own parameters. In the flat
     channel around DC the DDC passes white input noise (quantization
     included) at the density it had, i.e. the share `rate` of its power per
     output-rate band; the PSD reads a tone at its power and noise at
     power * ENBW / n per bin. */
  const double q          = 1.0 / 128.0;
  const double p_noise_in = 2.0 * NOISE_SIGMA * NOISE_SIGMA + 2.0 * q * q / 12;
  const double bin_noise  = p_noise_in * rate * c.psd->enbw / (double)n;
  const double snr_pred
      = 10.0 * log10 (TONE_AMP * TONE_AMP) - 10.0 * log10 (bin_noise);
  const double bias    = -0.5 / 128.0;
  const double bias_se = NOISE_SIGMA / sqrt ((double)total);
  const double mean_re = c.sum_re / (double)c.n_in;
  const double mean_im = c.sum_im / (double)c.n_in;

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
          "noise around it\n    (%.1f dB if it had passed unfiltered)\n",
          f_bias / 1e3, bias_db, bias_local_db, bias_unfiltered_db);
  printf ("throughput: %.2f MSa/s complex in, %.1fx real time at %.2f "
          "MSa/s\n\n",
          (double)c.n_in / dt / 1e6, (double)c.n_in / dt / fs, fs / 1e6);

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
  const double fs_out = fs * ddc_get_rate (c.ddc);
  const size_t limit  = seconds > 0.0 ? (size_t)(seconds * fs) : (size_t)-1;

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

  float *db = malloc (n * sizeof *db);
  if (!db)
    return 1;
  double f_peak, peak_db;
  spectrum_peak (&c, fs_out, db, &f_peak, &peak_db);
  const double input_s = (double)c.n_in / fs;

  printf ("live: %zu samples = %.2f s of input at %.3f MSa/s\n", c.n_in,
          input_s, fs / 1e6);
  printf ("  wall %.2f s; CPU %.2f s = %.1f%% of one core at real time\n", dt,
          dc, input_s > 0 ? 100.0 * dc / input_s : 0.0);
  printf ("  converted input mean I %+.6f, Q %+.6f\n",
          c.n_in ? c.sum_re / (double)c.n_in : 0.0,
          c.n_in ? c.sum_im / (double)c.n_in : 0.0);
  printf ("  channel at %+.1f kHz -> DC, %.1f kSa/s out\n", offset / 1e3,
          fs_out / 1e3);
  printf ("  strongest bin %+.1f kHz at %.1f dBFS\n", f_peak / 1e3, peak_db);
  /* Not "noise" here: live, whatever occupies the channel is in it. */
  printf ("  mean level in the channel (|f| < %.2f fs_out, excluding DC) "
          "%.1f dBFS/bin;\n    band-wide median %.1f dBFS/bin\n",
          CHANNEL, channel_noise_db (db, n), psd_noise_floor (c.psd));
  printf ("  occupied bandwidth (99%%): %.1f kHz\n",
          psd_occupied_bw (c.psd, 0.99) / 1e3);

  /* The strongest peaks, from the library's interpolating peak finder,
     gated 10 dB above the band-wide median so noise bumps are not listed.
     With --offset 0 the one at DC is the dongle's own spike. */
  dp_peak_t pk[N_PEAKS];
  size_t    np = find_peaks_f32 (db, n, N_PEAKS,
                                 (float)psd_noise_floor (c.psd) + 10.0f, pk);
  printf ("  strongest peaks (relative to the tuned centre + offset):\n");
  for (size_t i = 0; i < np; i++)
    printf ("    %+8.1f kHz  %6.1f dBFS\n",
            (double)pk[i].freq_norm * fs_out / 1e3, pk[i].amplitude_db);

  free (db);
  free (buf);
  chain_free (&c);
  return 0;
}

static void
usage (const char *argv0)
{
  (void)fprintf (stderr,
                 "usage: %s [--fs HZ] [--offset HZ] [--rate R] [--n N]\n"
                 "          [--seconds S] [- | FILE]\n"
                 "  no input: self-test (synthetic cu8, checked)\n"
                 "  - / FILE: live cu8 from stdin or a capture file\n",
                 argv0);
}

int
main (int argc, char **argv)
{
  double      fs = 2.4e6, offset = 250e3, rate = 0.125, seconds = 0.0;
  size_t      n     = 1024;
  const char *input = NULL;

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
  return input ? live (input, fs, offset, rate, n, seconds)
               : self_test (fs, offset, rate, n);
}
