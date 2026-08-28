/* bench_burst_demod_core.c — one burst, end to end.
 *
 * A jm scaffold that recorded nothing until now (doppler#891), and the last
 * of the thirty to be filled along with async_dsss_receiver. Both needed a
 * real fixture rather than a block of noise: `burst_demod_demod` acquires a
 * preamble, finds a sync word, despreads a payload and checks a CRC, and
 * every one of those stages exits early on a signal that is not there. A
 * benchmark over noise would time the give-up path and report a burst
 * demodulator four times faster than it is.
 *
 * So this builds the burst the way `test_burst_demod_core.c` does --
 * ACQ_REPS repeats of an unmodulated acquisition code, a Barker-13 sync
 * word, a spread payload, a CRC, all under a carrier -- and REFUSES TO TIME
 * ANYTHING unless the demodulator returns the payload bit-for-bit with
 * number a demodulation rather than a rejection.
 *
 * Reported per burst AND per sample, because the two answer different
 * questions -- how many bursts a receiver can process per second, and
 * whether it keeps up with the sample rate feeding it.
 *
 * The rows sweep the carrier the demodulator has to find:
 *
 *   clean         no offset, prior exact -- the acquisition search converges
 *                 immediately, the floor of the cost
 *   offset        a real frequency offset with a matching prior
 *   chirp         offset plus a Doppler rate, so the rate search engages
 *
 * Timing is MIN over rounds, not mean, after a WARMUP_S settle.
 */
#include "burst_demod/burst_demod_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ACQ_SF 500
#define ACQ_REPS 5
#define DATA_SF 50
#define SPC 4
#define SYNC_LEN 13
#define PAYLOAD 64
/* What demod() returns per burst: the frame as received. */
#define FRAME_SYMS (SYNC_LEN + PAYLOAD + CRC_BITS)
#define CRC_BITS 16
#define CHIP_RATE 1.0e6
#define ITERATIONS 20
#define WARMUP_S 0.25

/* Barker-13 as 0/1 (0 -> +1, 1 -> -1), the same sync the tests pin. */
static const uint8_t SYNC[SYNC_LEN]
    = { 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0 };

static float
csign (uint8_t c)
{
  return (c & 1u) ? -1.0f : 1.0f;
}

static uint16_t
crc16 (const uint8_t *bits, size_t n)
{
  uint16_t c = 0xFFFFu;
  for (size_t i = 0; i < n; i++)
    {
      c ^= (uint16_t)((bits[i] & 1u) << 15);
      c = (c & 0x8000u) ? (uint16_t)((c << 1) ^ 0x1021u) : (uint16_t)(c << 1);
    }
  return c;
}

static size_t
put_symbol (float complex *y, size_t n, const uint8_t *dcode, uint8_t bit)
{
  float a = csign (bit);
  for (size_t c = 0; c < DATA_SF; c++)
    for (size_t k = 0; k < SPC; k++)
      y[n++] = a * csign (dcode[c]);
  return n;
}

static size_t
build_burst (float complex *y, const uint8_t *acode, const uint8_t *dcode,
             const uint8_t *payload, double f0, double mu)
{
  size_t n = 0;
  for (size_t r = 0; r < ACQ_REPS; r++)
    for (size_t c = 0; c < ACQ_SF; c++)
      for (size_t k = 0; k < SPC; k++)
        y[n++] = csign (acode[c]); /* unmodulated preamble */
  for (size_t j = 0; j < SYNC_LEN; j++)
    n = put_symbol (y, n, dcode, SYNC[j]);
  for (size_t j = 0; j < PAYLOAD; j++)
    n = put_symbol (y, n, dcode, payload[j]);
  uint16_t crc = crc16 (payload, PAYLOAD);
  for (size_t j = 0; j < CRC_BITS; j++)
    n = put_symbol (y, n, dcode, (crc >> (CRC_BITS - 1 - j)) & 1u);

  for (size_t i = 0; i < n; i++)
    {
      double ph
          = 2.0 * M_PI * (f0 * (double)i + 0.5 * mu * (double)i * (double)i);
      y[i] *= (float)cos (ph) + (float)sin (ph) * I;
    }
  return n;
}

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile size_t sink   = 0;

  static uint8_t acode[ACQ_SF], dcode[DATA_SF], payload[PAYLOAD];
  for (size_t i = 0; i < ACQ_SF; i++)
    acode[i] = (uint8_t)((i * 2654435761u >> 13) & 1u);
  for (size_t i = 0; i < DATA_SF; i++)
    dcode[i] = (uint8_t)((i * 40503u >> 7) & 1u);
  for (size_t i = 0; i < PAYLOAD; i++)
    payload[i] = (uint8_t)((i * 7u + 3u) & 1u);

  const size_t cap
      = (ACQ_SF * ACQ_REPS + (SYNC_LEN + PAYLOAD + CRC_BITS) * DATA_SF) * SPC
        + 16;
  float complex *y    = malloc (cap * sizeof *y);
  uint8_t       *bits = malloc (FRAME_SYMS);
  if (!y || !bits)
    return 1;

  printf ("=== burst_demod benchmark ===\n");
  printf ("preamble %dx%d, data SF %d, payload %d bits, spc %d, %d rounds\n\n",
          ACQ_REPS, ACQ_SF, DATA_SF, PAYLOAD, SPC, ITERATIONS);

  /* f0 in cycles/sample, mu the Doppler rate; max_rate must admit mu. */
  /* The test's own two working cases, plus a clean floor. `max_rate` is a
     chirp-rate half-span in cycles/sample^2 -- not Hz/s. Guessing 1.0e4
     which the precondition below refused to time. */
  const double      f0s[3]    = { 0.0, 0.012, 0.012 };
  const double      priors[3] = { 0.0, 0.012, 0.0115 };
  const double      mus[3]    = { 0.0, 0.0, 6.0e-7 };
  const double      rates[3]  = { 0.0, 0.0, 1.0e-6 };
  const char *const rname[3]
      = { "demod[clean]", "demod[offset]", "demod[chirp]" };
  static double t_dm[3][ITERATIONS];
  size_t        n_samples = 0;

  for (int k = 0; k < 3; k++)
    {
      size_t n  = build_burst (y, acode, dcode, payload, f0s[k], mus[k]);
      n_samples = n;

      burst_demod_state_t *d = burst_demod_create (
          dcode, DATA_SF, SPC, CHIP_RATE, 0.0, rates[k], FRAME_SYMS, 10);
      if (!d)
        {
          (void)fprintf (stderr, "bench_burst_demod: create NULL\n");
          return 1;
        }
      burst_demod_set_preamble (d, acode, ACQ_SF, ACQ_REPS);
      burst_demod_set_sync (d, SYNC, SYNC_LEN);
      burst_demod_set_prior (d, priors[k], 0);

      /* The precondition. Every stage of this object exits early on a
         signal that is not there, so without proving a real demodulation
         first the loop below would faithfully time the give-up path. */
      memset (bits, 0, FRAME_SYMS);
      size_t nb = burst_demod_demod (d, y, n, bits, FRAME_SYMS);
      if (nb != FRAME_SYMS || memcmp (bits + SYNC_LEN, payload, PAYLOAD) != 0)
        {
          (void)fprintf (stderr,
                         "bench_burst_demod: %s did not demodulate (nb=%zu) "
                         "— the timings below would be a rejection, not a "
                         "demodulation\n",
                         rname[k], nb);
          return 1;
        }

      struct timespec w0, w1;
      clock_gettime (CLOCK_MONOTONIC, &w0);
      do
        {
          burst_demod_reset (d);
          burst_demod_set_prior (d, priors[k], 0);
          sink += burst_demod_demod (d, y, n, bits, FRAME_SYMS);
          clock_gettime (CLOCK_MONOTONIC, &w1);
        }
      while (elapsed_sec (&w0, &w1) < WARMUP_S);

      for (int r = 0; r < ITERATIONS; r++)
        {
          burst_demod_reset (d);
          burst_demod_set_prior (d, priors[k], 0);
          clock_gettime (CLOCK_MONOTONIC, &t0);
          sink += burst_demod_demod (d, y, n, bits, FRAME_SYMS);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_dm[k][r] = elapsed_sec (&t0, &t1);
        }
      jm_bench_add (&_bench, rname[k], t_dm[k], ITERATIONS, 1);
      double sec = min_sec (t_dm[k], ITERATIONS);
      printf ("  %-18s %8.3f ms/burst  %7.2f ns/sample  %7.1f bursts/s\n",
              rname[k], sec * 1e3, sec / (double)n * 1e9, 1.0 / sec);
      burst_demod_destroy (d);
    }

  printf (
      "\n  A burst is %zu samples (%.2f ms at %.0f MHz chip rate), so the\n"
      "  ns/sample column says whether a demodulator keeps up with the\n"
      "  stream feeding it and the bursts/s column says how many it can\n"
      "  retire. The chirp row costs %.2fx the clean one -- that is the\n"
      "  Doppler-rate search, and it is the price of admitting a moving\n"
      "  transmitter.\n",
      n_samples, (double)n_samples / CHIP_RATE / SPC * 1e3, CHIP_RATE / 1e6,
      min_sec (t_dm[2], ITERATIONS) / min_sec (t_dm[0], ITERATIONS));

  (void)sink;
  free (y);
  free (bits);
  jm_bench_write_json (&_bench, "burst_demod");
  return 0;
}
