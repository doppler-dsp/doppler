/*
 * doppler Spectrum Analyzer
 *
 * Receives complex IQ samples via ZMQ PUB/SUB, computes the power spectrum
 * using the doppler FFT engine (FFTW-backed), and renders an ASCII waterfall
 * with properly scaled frequency and power axes.
 *
 * Usage:
 *   spectrum_analyzer [endpoint [fft_size]]
 *
 * Defaults:
 *   endpoint  tcp://localhost:5555
 *   fft_size  1024
 *
 * Key features:
 *   - FFTW-backed FFT via dp_fft1d_execute (no slow O(N^2) DFT)
 *   - Hann window applied before FFT to reduce spectral leakage
 *   - fftshift: DC bin at centre of display
 *   - dBFS power axis with configurable reference / dynamic range
 *   - Frequency axis with MHz labels at left, centre, and right
 *   - Dashboard-style display: clears and redraws each frame
 *   - Handles both CF64 and CI32 sample types from the transmitter
 */

#include "dp/fft.h"
#include <doppler.h>

#include <complex.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* -- Display geometry --------------------------------------------------- */
#define DISP_WIDTH 72  /* characters across the spectrum plot   */
#define DISP_HEIGHT 18 /* rows of amplitude bars                */

/* -- Power axis range (dBFS) -------------------------------------------- */
#define DB_TOP 10.0     /* top of display (dBFS)                 */
#define DB_BOTTOM -90.0 /* bottom of display (dBFS)              */
#define DB_RANGE (DB_TOP - DB_BOTTOM)

/* -- Valid FFT size range ----------------------------------------------- */
#define FFT_MIN 64
#define FFT_MAX 8192

static volatile int keep_running = 1;

static void
signal_handler (int signum)
{
  (void)signum;
  keep_running = 0;
}

/* -- Hann window (raised cosine) ---------------------------------------- */
static void
apply_hann (double complex *buf, size_t N)
{
  for (size_t i = 0; i < N; i++)
    {
      double w = 0.5 * (1.0 - cos (2.0 * M_PI * (double)i / (double)(N - 1)));
      buf[i] *= w;
    }
}

/*
 * fftshift in-place: swap lower and upper halves so that DC is centred.
 * Works for even N (all power-of-two and even sizes).
 */
static void
fftshift (double *mag, size_t N)
{
  size_t half = N / 2;
  double tmp;
  for (size_t i = 0; i < half; i++)
    {
      tmp = mag[i];
      mag[i] = mag[i + half];
      mag[i + half] = tmp;
    }
}

/*
 * Compute power spectrum (dBFS) from a complex FFT output buffer.
 *   out[k] = 20 * log10( |X[k]| / N )   (coherent power, normalised to 0 dBFS
 *                                         for a full-scale complex sinusoid)
 */
static void
power_db (const double complex *fft_out, double *db, size_t N)
{
  double norm = 1.0 / (double)N;
  for (size_t k = 0; k < N; k++)
    {
      double mag = cabs (fft_out[k]) * norm;
      db[k] = 20.0 * log10 (mag + 1e-12);
    }
}

/*
 * Render the spectrum as a filled ASCII bar chart.
 *
 * The display maps DISP_WIDTH columns across the FFT bins (after fftshift)
 * and DISP_HEIGHT rows linearly from DB_BOTTOM (row 0) to DB_TOP (row H-1).
 * Each column is filled solid from the baseline up to its power level.
 *
 * Frequency axis shows left edge, centre, and right edge in MHz.
 * Power axis shows dB labels at top, middle, and bottom.
 */
static void
render_spectrum (const double *db, size_t N, double sample_rate,
                 double center_freq, uint64_t frame_count)
{
  /*
   * Map N FFT bins to DISP_WIDTH display columns.
   *
   * Each column takes the PEAK bin value within its bin range so that a
   * narrow spectral line (e.g. a single-tone CW signal) always appears as
   * one bright column at the correct frequency position rather than being
   * smeared or attenuated by averaging.
   */
  double col_db[DISP_WIDTH];
  for (int c = 0; c < DISP_WIDTH; c++)
    {
      size_t bin_start = (size_t)((double)c / DISP_WIDTH * (double)N);
      size_t bin_end = (size_t)((double)(c + 1) / DISP_WIDTH * (double)N);
      if (bin_end <= bin_start)
        bin_end = bin_start + 1;
      if (bin_end > N)
        bin_end = N;

      double peak = db[bin_start];
      for (size_t k = bin_start + 1; k < bin_end; k++)
        if (db[k] > peak)
          peak = db[k];
      col_db[c] = peak;
    }

  /* Find peak for status line */
  double peak_db = col_db[0];
  int peak_col = 0;
  for (int c = 1; c < DISP_WIDTH; c++)
    {
      if (col_db[c] > peak_db)
        {
          peak_db = col_db[c];
          peak_col = c;
        }
    }
  double peak_freq_mhz = (center_freq / 1e6)
                         + (((double)peak_col / (double)DISP_WIDTH) - 0.5)
                               * (sample_rate / 1e6);

  /* Frequency extents */
  double f_left_mhz = (center_freq - sample_rate * 0.5) / 1e6;
  double f_centre_mhz = center_freq / 1e6;
  double f_right_mhz = (center_freq + sample_rate * 0.5) / 1e6;

  /* -- Clear and header ------------------------------------------------ */
  printf ("\033[2J\033[H");
  printf ("doppler Spectrum Analyzer\n");
  printf ("==============================\n");
  printf ("  Fc: %10.4f MHz   BW: %.3f MHz   FFT: %zu pts   Frame: %llu\n",
          f_centre_mhz, sample_rate / 1e6, N, (unsigned long long)frame_count);
  printf ("  Peak: %+.1f dBFS @ %.4f MHz\n\n", peak_db, peak_freq_mhz);

  /* -- Spectrum rows (top = DB_TOP, bottom = DB_BOTTOM) ---------------- */
  for (int row = DISP_HEIGHT - 1; row >= 0; row--)
    {
      /* dB threshold for this row */
      double row_db
          = DB_BOTTOM + DB_RANGE * (double)row / (double)(DISP_HEIGHT - 1);

      /* dB label on right side at top, middle, and bottom rows */
      int label_row
          = (row == DISP_HEIGHT - 1) || (row == DISP_HEIGHT / 2) || (row == 0);

      printf ("|");
      for (int c = 0; c < DISP_WIDTH; c++)
        {
          putchar (col_db[c] >= row_db ? (c == peak_col ? '|' : '#') : ' ');
        }
      printf ("|");

      if (label_row)
        {
          printf (" %+5.0f dBFS", row_db);
        }
      putchar ('\n');
    }

  /* -- Frequency axis -------------------------------------------------- */
  /* Bottom border */
  printf ("+");
  for (int c = 0; c < DISP_WIDTH; c++)
    putchar ('-');
  printf ("+\n");

  /* Three-point frequency label: left, centre, right */
  /* Centre label starts at column DISP_WIDTH/2 - len/2 */
  char lbl_l[24], lbl_c[24], lbl_r[24];
  snprintf (lbl_l, sizeof (lbl_l), "%.3f MHz", f_left_mhz);
  snprintf (lbl_c, sizeof (lbl_c), "%.4f MHz", f_centre_mhz);
  snprintf (lbl_r, sizeof (lbl_r), "%.3f MHz", f_right_mhz);

  int len_l = (int)strlen (lbl_l);
  int len_c = (int)strlen (lbl_c);
  int len_r = (int)strlen (lbl_r);

  /* Column positions (1-based, accounting for the leading '|') */
  int pos_c_start = 1 + DISP_WIDTH / 2 - len_c / 2; /* centre label */
  int pos_r_start = 1 + DISP_WIDTH - len_r + 1;     /* right label  */

  /* Build the label line into a fixed buffer */
  char line[DISP_WIDTH + 64];
  memset (line, ' ', sizeof (line));
  line[sizeof (line) - 1] = '\0';

  /* Place left label at col 1 */
  memcpy (line + 1, lbl_l, (size_t)len_l);
  /* Place centre label */
  if (pos_c_start + len_c < (int)sizeof (line))
    memcpy (line + pos_c_start, lbl_c, (size_t)len_c);
  /* Place right label */
  if (pos_r_start + len_r < (int)sizeof (line))
    memcpy (line + pos_r_start, lbl_r, (size_t)len_r);

  /* Trim trailing spaces */
  int end = pos_r_start + len_r;
  if (end >= (int)sizeof (line))
    end = (int)sizeof (line) - 1;
  line[end] = '\0';

  printf ("%s\n", line);
  printf ("\n  Press Ctrl+C to stop.\n");
  fflush (stdout);
}

static void
print_usage (const char *prog)
{
  printf ("Usage: %s [endpoint [fft_size]]\n", prog);
  printf ("\n");
  printf ("  endpoint  ZMQ SUB connect address  (default: "
          "tcp://localhost:5555)\n");
  printf ("  fft_size  FFT size in points, %d..%d  (default: 1024)\n", FFT_MIN,
          FFT_MAX);
  printf ("\n");
  printf ("Subscribes to a doppler transmitter and renders a live ASCII\n");
  printf ("power spectrum with Hann windowing, fftshift (DC centred), and\n");
  printf ("dBFS power axis.  Handles CF64 and CI32 sample types.\n");
  printf ("\n");
  printf ("Display: %d columns x %d rows, %.0f to %.0f dBFS range\n",
          DISP_WIDTH, DISP_HEIGHT, DB_BOTTOM, DB_TOP);
  printf ("\n");
  printf ("Examples:\n");
  printf ("  %s                             # localhost:5555, 1024-pt FFT\n",
          prog);
  printf ("  %s tcp://192.168.1.10:5555    # remote host\n", prog);
  printf ("  %s tcp://localhost:5555 2048  # larger FFT for more resolution\n",
          prog);
  printf ("\n");
  printf ("Press Ctrl+C to stop.\n");
}

/* -- main --------------------------------------------------------------- */
int
main (int argc, char *argv[])
{
  const char *endpoint = "tcp://localhost:5555";
  size_t fft_size = 1024;

  if (argc > 1
      && (strcmp (argv[1], "--help") == 0 || strcmp (argv[1], "-h") == 0))
    {
      print_usage (argv[0]);
      return 0;
    }
  if (argc > 1)
    endpoint = argv[1];
  if (argc > 2)
    {
      fft_size = (size_t)atoi (argv[2]);
      if (fft_size < FFT_MIN || fft_size > FFT_MAX)
        {
          fprintf (stderr, "FFT size must be between %d and %d\n", FFT_MIN,
                   FFT_MAX);
          fprintf (stderr, "Run '%s --help' for usage.\n", argv[0]);
          return 1;
        }
    }

  signal (SIGINT, signal_handler);
  signal (SIGTERM, signal_handler);

  /* Initialise FFTW plan for 1-D complex FFT of fft_size points */
  dp_fft_global_setup (&fft_size, 1, -1, /* FFTW_FORWARD */
                       1,                /* single thread */
                       "measure",        /* planner effort */
                       NULL);            /* no wisdom file */

  /* Create subscriber */
  dp_sub *ctx = dp_sub_create (endpoint);
  if (!ctx)
    {
      fprintf (stderr, "Failed to create subscriber on %s\n", endpoint);
      return 1;
    }

  /* Allocate working buffers (no heap allocation in the hot loop) */
  double complex *win_buf = malloc (fft_size * sizeof (double complex));
  double complex *fft_out = malloc (fft_size * sizeof (double complex));
  double *db_buf = malloc (fft_size * sizeof (double));

  if (!win_buf || !fft_out || !db_buf)
    {
      fprintf (stderr, "Failed to allocate working buffers\n");
      free (win_buf);
      free (fft_out);
      free (db_buf);
      dp_sub_destroy (ctx);
      return 1;
    }

  printf ("doppler Spectrum Analyzer\n");
  printf ("  Endpoint : %s\n", endpoint);
  printf ("  FFT size : %zu\n", fft_size);
  printf ("  Waiting for samples...\n");
  fflush (stdout);

  uint64_t frame_count = 0;

  while (keep_running)
    {
      dp_msg_t *msg = NULL;
      dp_header_t header;

      int rc = dp_sub_recv (ctx, &msg, &header);
      if (rc != DP_OK)
        {
          /* timeout or transient error -- keep looping */
          continue;
        }

      size_t num_samples = dp_msg_num_samples (msg);
      dp_sample_type_t sample_type = dp_msg_sample_type (msg);
      void *raw_samples = dp_msg_data (msg);

      /* -- Convert incoming samples into the windowed complex buffer -- */
      size_t N = num_samples < fft_size ? num_samples : fft_size;

      if (sample_type == DP_CF64)
        {
          const dp_cf64_t *src = (const dp_cf64_t *)raw_samples;
          for (size_t i = 0; i < N; i++)
            win_buf[i] = src[i].i + I * src[i].q;
        }
      else if (sample_type == DP_CI32)
        {
          const dp_ci32_t *src = (const dp_ci32_t *)raw_samples;
          const double scale = 1.0 / 2147483647.0;
          for (size_t i = 0; i < N; i++)
            win_buf[i] = (src[i].i * scale) + I * (src[i].q * scale);
        }
      else
        {
          /* Unsupported type -- skip */
          dp_msg_free (msg);
          continue;
        }

      /* Zero-pad if we got fewer samples than fft_size */
      for (size_t i = N; i < fft_size; i++)
        win_buf[i] = 0.0;

      dp_msg_free (msg);

      /* -- Apply Hann window, run FFT, compute power, shift ----------- */
      apply_hann (win_buf, fft_size);
      dp_fft1d_execute (win_buf, fft_out);
      power_db (fft_out, db_buf, fft_size);
      fftshift (db_buf, fft_size);

      frame_count++;
      render_spectrum (db_buf, fft_size, header.sample_rate,
                       header.center_freq, frame_count);

      /* ~10 fps refresh cap */
      usleep (100000);
    }

  printf ("\n\nSpectrum analyzer stopped after %llu frames.\n",
          (unsigned long long)frame_count);

  free (win_buf);
  free (fft_out);
  free (db_buf);
  dp_sub_destroy (ctx);
  return 0;
}
