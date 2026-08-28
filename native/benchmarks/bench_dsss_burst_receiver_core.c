/**
 * @file bench_dsss_burst_receiver_core.c
 * @brief DsssBurstReceiver — what push() costs, split by what it had to do.
 *
 * There is no step() here to time, and jm's scaffold said so and stopped.
 * That left the component writing an empty `"benchmarks": []` array into
 * every snapshot (doppler bench-coverage-check), which reads as "measured,
 * nothing to report" rather than "not measured" -- so this replaces the
 * scaffold with the measurement the object actually has.
 *
 * `push()` is the benchable shape: samples in, decoded bursts out. Its cost
 * is not one number, because the three stages do not run equally often:
 *
 * - **search** runs on EVERY sample. It is the correlation surface plus the
 *   CFAR gate, and it sets the sustained sample rate a caller can feed.
 * - **refine + demod** run once per detection. They are a per-BURST cost,
 *   amortised over however many samples separate one burst from the next.
 *
 * A single blended figure would hide which of those a caller is paying, so
 * the two are reported separately and against the same block size:
 *
 * - `push_idle`   — noise only. The search floor: no detection fires, so
 *                   this is the price of listening.
 * - `push_burst`  — one full burst per block, decoded end to end. The
 *                   difference from `push_idle` is what a detection costs.
 *
 * Both are reported in the same units (samples/second via jm_bench_add's
 * per-call item count), so `push_idle / push_burst` is directly the
 * throughput a caller gives up at the burst rate benchmarked here. At a
 * realistic duty cycle the true cost sits between them, near `push_idle`.
 *
 * The geometry mirrors `test_dsss_burst_receiver_core.c` -- a real MLS
 * spreading code from the library's own generator, not an arithmetic
 * pattern. That is not cosmetic: a pattern code's own autocorrelation
 * sidelobes set the CFAR reference, which changes how often the refine and
 * demod stages run and therefore what this benchmark measures.
 */
#include "dsss_burst_receiver/dsss_burst_receiver_core.h"
#include "pn/pn_core.h"

#include "jm_bench.h"

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ITERATIONS 30

#define ACQ_SF 31u
#define DATA_SF 8u
#define REPS 4u
#define SPC 4u
#define PAYLOAD 32u
#define SYNC_LEN 13u
#define CHIP_RATE 1.0e6
#define CN0_DBHZ 55.0

/* One burst is REPS*ACQ_SF*SPC preamble + (SYNC_LEN+PAYLOAD+16) symbols of
 * DATA_SF*SPC each = 496 + 1952 = 2448 samples. The block is sized to hold
 * one comfortably, with noise either side. */
#define BENCH_N 8192
#define BURST_AT 2048

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

/* Deterministic Gaussian pair via Box-Muller on a small LCG. The benchmark
 * does not link the test harness, and the exact noise draw does not matter
 * to a timing measurement -- only that it is a real noise floor rather than
 * zeros, so the CFAR stage sees the arithmetic it would see in service. */
static double
bench_gauss (uint32_t *st)
{
  *st       = *st * 1664525u + 1013904223u;
  double u1 = ((*st >> 8) + 1.0) / 16777217.0;
  *st       = *st * 1664525u + 1013904223u;
  double u2 = (double)(*st >> 8) / 16777216.0;
  return sqrt (-2.0 * log (u1)) * cos (2.0 * M_PI * u2);
}

static const uint8_t *
acq_code (void)
{
  static uint8_t c[ACQ_SF];
  static int     built = 0;
  if (!built)
    {
      pn_state_t *pn = pn_create (pn_mls_poly (5), 1u, 5u, 0);
      for (size_t i = 0; i < ACQ_SF; i++)
        c[i] = pn_step (pn);
      pn_destroy (pn);
      built = 1;
    }
  return c;
}

static const uint8_t *
data_code (void)
{
  static uint8_t c[DATA_SF];
  for (size_t i = 0; i < DATA_SF; i++)
    c[i] = (uint8_t)((i * 40503u >> 7) & 1u);
  return c;
}

static const uint8_t *
sync_word (void)
{
  static uint8_t s[SYNC_LEN] = { 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0 };
  return s;
}

static const uint8_t *
payload_bits (void)
{
  static uint8_t p[PAYLOAD];
  for (size_t i = 0; i < PAYLOAD; i++)
    p[i] = (uint8_t)((i * 7u + 3u) & 1u);
  return p;
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

/** @brief One burst: preamble, sync, payload, CRC-16, at zero carrier. */
static size_t
build_burst (float complex *y)
{
  const uint8_t *acode = acq_code (), *dcode = data_code ();
  const uint8_t *sy = sync_word (), *pl = payload_bits ();
  size_t         n = 0;
  for (size_t r = 0; r < REPS; r++)
    for (size_t c = 0; c < ACQ_SF; c++)
      for (size_t k = 0; k < SPC; k++)
        y[n++] = csign (acode[c]);
  for (size_t j = 0; j < SYNC_LEN; j++)
    n = put_symbol (y, n, dcode, sy[j]);
  for (size_t j = 0; j < PAYLOAD; j++)
    n = put_symbol (y, n, dcode, pl[j]);
  uint16_t crc = crc16 (pl, PAYLOAD);
  for (size_t j = 0; j < 16u; j++)
    n = put_symbol (y, n, dcode, (uint8_t)((crc >> (15u - j)) & 1u));
  return n;
}

static void
fill_noise (float complex *x, size_t n, double sigma, uint32_t seed)
{
  uint32_t st = seed;
  for (size_t i = 0; i < n; i++)
    {
      /* Named locals: two bench_gauss() calls in one expression would be
         ordered by the compiler, and gcc and clang differ. */
      float re = (float)(sigma * bench_gauss (&st));
      float im = (float)(sigma * bench_gauss (&st));
      x[i]     = re + im * I;
    }
}

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static dsss_burst_receiver_state_t *
make_rx (void)
{
  return dsss_burst_receiver_create (
      acq_code (), ACQ_SF, data_code (), DATA_SF, sync_word (), SYNC_LEN, REPS,
      SPC, CHIP_RATE, PAYLOAD, CN0_DBHZ, 0.0, 1e-3, 0.9, 0.0, 0.0, 10);
}

/**
 * @brief Time ITERATIONS blocks of @p x through a fresh receiver.
 *
 * The receiver is rebuilt per repeat so every repeat starts from the same
 * state: push() carries look-back history and a suppression window, so
 * feeding the same burst thirty times into one instance would measure the
 * dedup path from the second repeat onward rather than the decode.
 *
 * @return Bursts decoded across all repeats -- printed, so a run that
 *         silently stopped detecting is visible rather than fast.
 */
static size_t
time_push (const float complex *x, const char *name, jm_bench_t *bench)
{
  double          times[ITERATIONS];
  size_t          decoded = 0;
  struct timespec t0, t1;

  dsss_burst_receiver_state_t *probe = make_rx ();
  size_t cap = dsss_burst_receiver_push_max_out (probe, BENCH_N);
  dsss_burst_receiver_destroy (probe);
  uint8_t *out = malloc (cap ? cap : 1u);
  if (!out)
    return 0;

  for (int r = 0; r < ITERATIONS; r++)
    {
      dsss_burst_receiver_state_t *rx = make_rx ();
      clock_gettime (CLOCK_MONOTONIC, &t0);
      size_t n = dsss_burst_receiver_push (rx, x, BENCH_N, out, cap);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[r] = elapsed_sec (&t0, &t1);
      decoded += n / PAYLOAD;
      dsss_burst_receiver_destroy (rx);
    }

  jm_bench_add (bench, name, times, ITERATIONS, BENCH_N);
  free (out);
  return decoded;
}

int
main (void)
{
  static float complex idle[BENCH_N];
  static float complex hit[BENCH_N];
  static float complex burst[4096];

  jm_bench_t _bench = { 0 };

  printf ("=== dsss_burst_receiver benchmark ===\n");
  printf ("push(): search runs per sample, refine+demod run per burst\n");
  printf ("block = %d samples, %d iterations, code = MLS-%u, spc = %u\n\n",
          BENCH_N, ITERATIONS, ACQ_SF, SPC);

  fill_noise (idle, BENCH_N, 0.1, 12345u);
  fill_noise (hit, BENCH_N, 0.1, 12345u);
  size_t nb = build_burst (burst);
  for (size_t i = 0; i < nb && BURST_AT + i < BENCH_N; i++)
    hit[BURST_AT + i] += burst[i];

  size_t n_idle  = time_push (idle, "push_idle", &_bench);
  size_t n_burst = time_push (hit, "push_burst", &_bench);

  printf ("  push_idle : %zu burst(s) decoded over %d block(s)\n", n_idle,
          ITERATIONS);
  printf ("  push_burst: %zu burst(s) decoded over %d block(s)\n", n_burst,
          ITERATIONS);
  if (n_burst == 0)
    printf ("  WARNING: push_burst decoded nothing — the timing below is a\n"
            "           search-only figure, not the decode path it names.\n");
  printf ("\n");

  jm_bench_write_json (&_bench, "dsss_burst_receiver");
  return 0;
}
