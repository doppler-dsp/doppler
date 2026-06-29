#include "burst_demod/burst_demod_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

#define ACQ_SF 500
#define ACQ_REPS 5
#define DATA_SF 50
#define SPC 4
#define SYNC_LEN 13
#define PAYLOAD 64
#define CRC_BITS 16
#define CHIP_RATE 1.0e6

/* Barker-13 as 0/1 (0 -> +1, 1 -> -1). */
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

/* Append one BPSK data symbol (bit -> +/-1) spread by data_code. */
static size_t
put_symbol (float complex *y, size_t n, const uint8_t *dcode, uint8_t bit)
{
  float a = csign (bit);
  for (size_t c = 0; c < DATA_SF; c++)
    for (size_t k = 0; k < SPC; k++)
      y[n++] = a * csign (dcode[c]);
  return n;
}

/* Build preamble (5x500 unmod) + frame (sync|payload|crc), then apply the
 * carrier exp(j2π(f0·n + ½μ·n²)). Returns total sample count. */
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

static int
run_case (const char *name, double f0, double f0_prior, double mu,
          double max_rate)
{
  int _fails = 0;
  /* Codes + payload (deterministic). */
  uint8_t acode[ACQ_SF], dcode[DATA_SF], payload[PAYLOAD];
  for (size_t i = 0; i < ACQ_SF; i++)
    acode[i] = (uint8_t)((i * 2654435761u >> 13) & 1u);
  for (size_t i = 0; i < DATA_SF; i++)
    dcode[i] = (uint8_t)((i * 40503u >> 7) & 1u);
  for (size_t i = 0; i < PAYLOAD; i++)
    payload[i] = (uint8_t)((i * 7u + 3u) & 1u);

  size_t cap
      = (ACQ_SF * ACQ_REPS + (SYNC_LEN + PAYLOAD + CRC_BITS) * DATA_SF) * SPC
        + 16;
  float complex *y = malloc (cap * sizeof *y);
  size_t         n = build_burst (y, acode, dcode, payload, f0, mu);

  burst_demod_state_t *d = burst_demod_create (dcode, DATA_SF, SPC, CHIP_RATE,
                                               0.0, max_rate, PAYLOAD, 10);
  CHECK (d != NULL);
  burst_demod_set_preamble (d, acode, ACQ_SF, ACQ_REPS);
  burst_demod_set_sync (d, SYNC, SYNC_LEN);
  burst_demod_set_prior (d, f0_prior, 0);

  uint8_t bits[PAYLOAD];
  size_t  nb = burst_demod_demod (d, y, n, bits, PAYLOAD);
  CHECK (nb == PAYLOAD);
  CHECK (d->frame_valid == 1);
  size_t errs = 0;
  for (size_t i = 0; i < PAYLOAD; i++)
    if (bits[i] != payload[i])
      errs++;
  CHECK (errs == 0);

  printf ("  %-10s f0=%.4f(prior %.4f) mu=%.2e | est f=%.1fHz r=%.2eHz/s "
          "snr=%.0f off=%zu valid=%d errs=%zu\n",
          name, f0, f0_prior, mu, d->est_freq_hz, d->est_rate_hz,
          d->est_snr_db, d->frame_offset, d->frame_valid, errs);
  burst_demod_destroy (d);
  free (y);
  return _fails;
}

/* Guard / error / clamp paths the happy-path cases never reach. */
static int
run_edge_cases (void)
{
  int     _fails = 0;
  uint8_t dc[DATA_SF], ac[ACQ_SF];
  for (size_t i = 0; i < DATA_SF; i++)
    dc[i] = (uint8_t)(i & 1u);
  for (size_t i = 0; i < ACQ_SF; i++)
    ac[i] = (uint8_t)(i & 1u);

  /* Argument validation → NULL (each clause of the create guard). */
  CHECK (burst_demod_create (NULL, DATA_SF, SPC, CHIP_RATE, 0, 0, PAYLOAD, 10)
         == NULL);
  CHECK (burst_demod_create (dc, 0, SPC, CHIP_RATE, 0, 0, PAYLOAD, 10)
         == NULL);
  CHECK (burst_demod_create (dc, DATA_SF, 0, CHIP_RATE, 0, 0, PAYLOAD, 10)
         == NULL);
  CHECK (burst_demod_create (dc, DATA_SF, SPC, 0.0, 0, 0, PAYLOAD, 10)
         == NULL);
  CHECK (burst_demod_create (dc, DATA_SF, SPC, CHIP_RATE, 0, -1.0, PAYLOAD, 10)
         == NULL);
  CHECK (burst_demod_create (dc, DATA_SF, SPC, CHIP_RATE, 0, 0, PAYLOAD, 0)
         == NULL);

  burst_demod_destroy (NULL); /* no-op on NULL */

  burst_demod_state_t *d
      = burst_demod_create (dc, DATA_SF, SPC, CHIP_RATE, 0, 0, PAYLOAD, 10);
  CHECK (d != NULL);
  burst_demod_set_preamble (d, NULL, 0, 0); /* guard: ignored */
  burst_demod_set_sync (d, NULL, 0);        /* guard: ignored */
  burst_demod_set_preamble (d, ac, ACQ_SF, ACQ_REPS);
  burst_demod_set_preamble (d, ac, ACQ_SF,
                            ACQ_REPS); /* re-arm: frees old ppe */
  burst_demod_set_sync (d, SYNC, SYNC_LEN);

  /* Too-short input → clean failure (0 symbols, frame invalid). */
  float complex tiny[8] = { 0 };
  uint8_t       eb[PAYLOAD];
  burst_demod_set_prior (d, 0.0, 0);
  CHECK (burst_demod_demod (d, tiny, 8, eb, PAYLOAD) == 0);
  CHECK (d->frame_valid == 0);
  burst_demod_destroy (d);

  /* est_segments > acq_sf forces the per-segment chip clamp (Lseg >= 1). */
  burst_demod_state_t *d2 = burst_demod_create (dc, DATA_SF, SPC, CHIP_RATE, 0,
                                                0, PAYLOAD, ACQ_SF + 100);
  CHECK (d2 != NULL);
  burst_demod_set_preamble (d2, ac, ACQ_SF, 1);
  burst_demod_destroy (d2);
  return _fails;
}

int
main (void)
{
  int _fails = 0;
  _fails += run_edge_cases ();

  /* Near-static Doppler (negligible rate): max_rate = 0, single-FFT estimate.
   */
  _fails += run_case ("static", 0.012, 0.012, 0.0, 0.0);

  /* LEO: a real chirp, coarse prior slightly off; the 2-D estimate recovers
   * the residual Doppler + rate and dechirps before despreading. */
  _fails += run_case ("leo", 0.012, 0.0115, 6.0e-7, 1.0e-6);

  if (_fails)
    {
      fprintf (stderr, "test_burst_demod_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_burst_demod_core PASSED\n");
  return 0;
}
