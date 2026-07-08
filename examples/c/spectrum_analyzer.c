/*
 * spectrum_analyzer.c — ASCII real-time spectrum analyzer.
 *
 * Receives CF64 or CI32 samples via NATS PUB/SUB, computes the power
 * spectrum using doppler's FFT engine, and renders an ASCII waterfall
 * with Hann windowing, fftshift (DC centred), and dBFS power axis.
 * Requires a running nats-server (e.g. `nats-server -js`).
 *
 * Usage:
 *   spectrum_analyzer [endpoint [fft_size]]
 *   spectrum_analyzer                              # 127.0.0.1:4222/iq,
 * 1024-pt spectrum_analyzer nats://broker.example:4222/iq spectrum_analyzer
 * nats://127.0.0.1:4222/iq 2048
 *
 * Press Ctrl+C to stop.
 *
 * Build:
 *   make build
 *   ./build/native/examples/spectrum_analyzer
 */

#include <doppler.h>
#include <fft/fft_core.h>
#include <stream/stream.h>

#include <complex.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define dp_usleep(us) Sleep ((DWORD)((us) / 1000))
#else
#include <unistd.h>
#define dp_usleep(us) usleep ((useconds_t)(us))
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DISP_WIDTH 72
#define DISP_HEIGHT 18
#define DB_TOP 10.0
#define DB_BOTTOM -90.0
#define DB_RANGE (DB_TOP - DB_BOTTOM)
#define FFT_MIN 64
#define FFT_MAX 8192

static volatile int keep_running = 1;

static void
signal_handler (int signum)
{
  (void)signum;
  keep_running = 0;
}

static void
apply_hann (double _Complex *buf, size_t N)
{
  for (size_t i = 0; i < N; i++)
    {
      double w = 0.5 * (1.0 - cos (2.0 * M_PI * (double)i / (double)(N - 1)));
      buf[i] *= w;
    }
}

/* Swap lower and upper halves in-place (DC to centre). */
static void
fftshift (double *mag, size_t N)
{
  size_t half = N / 2;
  for (size_t i = 0; i < half; i++)
    {
      double t      = mag[i];
      mag[i]        = mag[i + half];
      mag[i + half] = t;
    }
}

static void
power_db (const double _Complex *fft_out, double *db, size_t N)
{
  double norm = 1.0 / (double)N;
  for (size_t k = 0; k < N; k++)
    db[k] = 20.0 * log10 (cabs (fft_out[k]) * norm + 1e-12);
}

static void
render_spectrum (const double *db, size_t N, double sample_rate,
                 double center_freq, uint64_t frame)
{
  /* Map N bins to DISP_WIDTH columns (peak-hold per column). */
  double col_db[DISP_WIDTH];
  for (int c = 0; c < DISP_WIDTH; c++)
    {
      size_t b0 = (size_t)((double)c / DISP_WIDTH * (double)N);
      size_t b1 = (size_t)((double)(c + 1) / DISP_WIDTH * (double)N);
      if (b1 <= b0)
        b1 = b0 + 1;
      if (b1 > N)
        b1 = N;
      double peak = db[b0];
      for (size_t k = b0 + 1; k < b1; k++)
        if (db[k] > peak)
          peak = db[k];
      col_db[c] = peak;
    }

  double peak_db  = col_db[0];
  int    peak_col = 0;
  for (int c = 1; c < DISP_WIDTH; c++)
    if (col_db[c] > peak_db)
      {
        peak_db  = col_db[c];
        peak_col = c;
      }

  double f_left   = (center_freq - sample_rate * 0.5) / 1e6;
  double f_centre = center_freq / 1e6;
  double f_right  = (center_freq + sample_rate * 0.5) / 1e6;
  double peak_mhz
      = f_centre
        + (((double)peak_col / DISP_WIDTH) - 0.5) * (sample_rate / 1e6);

  printf ("\033[2J\033[H");
  printf ("doppler Spectrum Analyzer\n==========================\n");
  printf ("  Fc: %10.4f MHz   BW: %.3f MHz   FFT: %zu   Frame: %llu\n",
          f_centre, sample_rate / 1e6, N, (unsigned long long)frame);
  printf ("  Peak: %+.1f dBFS @ %.4f MHz\n\n", peak_db, peak_mhz);

  for (int row = DISP_HEIGHT - 1; row >= 0; row--)
    {
      double row_db = DB_BOTTOM + DB_RANGE * (double)row / (DISP_HEIGHT - 1);
      int    label
          = (row == DISP_HEIGHT - 1) || (row == DISP_HEIGHT / 2) || (row == 0);
      printf ("|");
      for (int c = 0; c < DISP_WIDTH; c++)
        putchar (col_db[c] >= row_db ? (c == peak_col ? '|' : '#') : ' ');
      printf ("|");
      if (label)
        printf (" %+5.0f dBFS", row_db);
      putchar ('\n');
    }

  /* Frequency axis */
  printf ("+");
  for (int c = 0; c < DISP_WIDTH; c++)
    putchar ('-');
  printf ("+\n");

  char lbl_l[24], lbl_c[24], lbl_r[24];
  snprintf (lbl_l, sizeof lbl_l, "%.3f MHz", f_left);
  snprintf (lbl_c, sizeof lbl_c, "%.4f MHz", f_centre);
  snprintf (lbl_r, sizeof lbl_r, "%.3f MHz", f_right);
  int len_l = (int)strlen (lbl_l);
  int len_c = (int)strlen (lbl_c);
  int len_r = (int)strlen (lbl_r);

  int pos_c = 1 + DISP_WIDTH / 2 - len_c / 2;
  int pos_r = 1 + DISP_WIDTH - len_r + 1;

  char line[DISP_WIDTH + 64];
  memset (line, ' ', sizeof line);
  line[sizeof line - 1] = '\0';
  memcpy (line + 1, lbl_l, (size_t)len_l);
  if (pos_c + len_c < (int)sizeof line)
    memcpy (line + pos_c, lbl_c, (size_t)len_c);
  if (pos_r + len_r < (int)sizeof line)
    memcpy (line + pos_r, lbl_r, (size_t)len_r);
  line[pos_r + len_r] = '\0';
  printf ("%s\n\n  Press Ctrl+C to stop.\n", line);
  fflush (stdout);
}

int
main (int argc, char *argv[])
{
  const char *endpoint = "nats://127.0.0.1:4222/iq";
  size_t      fft_size = 1024;

  if (argc > 1
      && (strcmp (argv[1], "--help") == 0 || strcmp (argv[1], "-h") == 0))
    {
      printf ("Usage: %s [endpoint [fft_size]]\n\n", argv[0]);
      printf ("  endpoint  NATS SUB endpoint (default: "
              "nats://127.0.0.1:4222/iq)\n");
      printf ("  fft_size  %d..%d         (default: 1024)\n", FFT_MIN,
              FFT_MAX);
      printf ("\nPress Ctrl+C to stop.\n");
      return 0;
    }
  if (argc > 1)
    endpoint = argv[1];
  if (argc > 2)
    {
      fft_size = (size_t)atoi (argv[2]);
      if (fft_size < FFT_MIN || fft_size > FFT_MAX)
        {
          fprintf (stderr, "fft_size must be %d..%d\n", FFT_MIN, FFT_MAX);
          return 1;
        }
    }

  signal (SIGINT, signal_handler);
  signal (SIGTERM, signal_handler);

  /* Forward FFT plan (sign = +1 = forward). */
  fft_state_t *fft = fft_create (fft_size, +1, 1);
  if (!fft)
    {
      fputs ("fft_create failed\n", stderr);
      return 1;
    }

  dp_sub_t *ctx = dp_sub_create (endpoint);
  if (!ctx)
    {
      fprintf (stderr, "Failed to create subscriber on %s\n", endpoint);
      fft_destroy (fft);
      return 1;
    }

  double _Complex *win_buf = malloc (fft_size * sizeof (double _Complex));
  double _Complex *fft_out = malloc (fft_size * sizeof (double _Complex));
  double          *db_buf  = malloc (fft_size * sizeof (double));
  if (!win_buf || !fft_out || !db_buf)
    {
      fputs ("malloc failed\n", stderr);
      free (win_buf);
      free (fft_out);
      free (db_buf);
      dp_sub_destroy (ctx);
      fft_destroy (fft);
      return 1;
    }

  printf ("doppler Spectrum Analyzer\n  Endpoint: %s\n  FFT size: %zu\n"
          "  Waiting for samples...\n",
          endpoint, fft_size);
  fflush (stdout);

  uint64_t frame = 0;

  while (keep_running)
    {
      dp_msg_t   *msg = NULL;
      dp_header_t hdr;

      if (dp_sub_recv (ctx, &msg, &hdr) != DP_OK)
        continue;

      size_t           n    = dp_msg_num_samples (msg);
      dp_sample_type_t type = dp_msg_sample_type (msg);
      void            *raw  = dp_msg_data (msg);
      size_t           N    = n < fft_size ? n : fft_size;

      if (type == CF64)
        {
          const double _Complex *src = (const double _Complex *)raw;
          for (size_t i = 0; i < N; i++)
            win_buf[i] = src[i];
        }
      else if (type == CI32)
        {
          const int32_t *src = (const int32_t *)raw;
          const double   sc  = 1.0 / 2147483647.0;
          for (size_t i = 0; i < N; i++)
            win_buf[i]
                = (src[2 * i] * sc) + (src[2 * i + 1] * sc) * _Complex_I;
        }
      else
        {
          dp_msg_free (msg);
          continue;
        }

      for (size_t i = N; i < fft_size; i++)
        win_buf[i] = 0.0;
      dp_msg_free (msg);

      apply_hann (win_buf, fft_size);
      fft_execute_cf64 (fft, win_buf, fft_size, fft_out);
      power_db (fft_out, db_buf, fft_size);
      fftshift (db_buf, fft_size);

      render_spectrum (db_buf, fft_size, hdr.sample_rate, hdr.center_freq,
                       ++frame);

      dp_usleep (100000); /* ~10 fps cap */
    }

  printf ("\n\nStopped after %llu frames.\n", (unsigned long long)frame);
  free (win_buf);
  free (fft_out);
  free (db_buf);
  dp_sub_destroy (ctx);
  fft_destroy (fft);
  return 0;
}
