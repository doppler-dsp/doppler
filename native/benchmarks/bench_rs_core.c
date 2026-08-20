/* bench_rs_core.c — Reed-Solomon: encode, verify, and decode under load.
 *
 * An RS decoder runs more of its algorithm the more errors it finds, so the
 * question a caller has is how much more, and the answer here turned out to
 * be less than the shape of the code suggests -- 1.33x from a clean codeword
 * to a fully-loaded one, because syndrome computation dominates and the
 * Berlekamp-Massey / Chien / Forney chain it gates is the smaller half:
 *
 *   encode          rs_encode -- k systematic symbols through g(x)
 *   verify          rs_codeword_ok -- syndromes only, the CLEAN path
 *   decode[e=0]     rs_decode on a clean codeword: syndromes, all zero,
 *                   return. What a good link runs almost every frame.
 *   decode[e=8]     half the correction capability used
 *   decode[e=16]    E errors, the most the code can correct: full
 *                   Berlekamp-Massey, Chien search and Forney evaluation
 *
 * That spread is the measurement worth having, and nothing in the tree had
 * said what it is. `verify` is in the list to locate the boundary: it is
 * syndromes and nothing else, so its distance from `decode[e=0]` is the
 * cost of the decision to correct, and its distance from `decode[e=16]` is
 * the cost of correcting.
 *
 * The code is CCSDS 131.0-B-3's outer code -- RS(255,223) over GF(2^8),
 * E=16 -- with the parameters written out locally rather than taken from
 * ccsds_tm, because rs's CMakeLists deliberately does not link it: the code
 * family is not CCSDS's, and this file measures the arithmetic, not the
 * standard's pick of it. (`native/src/ccsds_tm/rs.c` holds the same numbers
 * as CCSDS_TM_RS; they are its configuration of this component.)
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "jm_bench.h"
#include "rs/rs_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Codewords per round: enough that per-call overhead is invisible against
   the ~microsecond a 255-symbol decode takes. */
#define BENCH_N 256
#define ITERATIONS 100

static const rs_code_t CODE = { .symbol_bits = 8,
                                .field_poly  = 0x87u,
                                .nroots      = 32u,
                                .first_root  = 112u,
                                .root_stride = 11u };

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

static void
report (const char *name, const double *t, unsigned k)
{
  double s = min_sec (t, ITERATIONS) / (double)BENCH_N;
  printf ("  %-16s %8.3f us/codeword  %7.2f Mbyte/s (info)\n", name, s * 1e6,
          (double)k / s / 1e6);
}

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  rs_t            rs;

  if (!rs_init (&rs, &CODE))
    return 1;

  const unsigned n = rs.n, k = rs.k, e = rs.e;

  uint8_t *info  = malloc (k);
  uint8_t *clean = malloc (n);
  uint8_t *work  = malloc (n);
  if (!info || !clean || !work)
    return 1;

  uint32_t lfsr = 0x1D0Fu;
  for (unsigned i = 0; i < k; i++)
    {
      lfsr    = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      info[i] = (uint8_t)(lfsr & 0xFFu);
    }
  memcpy (clean, info, k);
  rs_encode (&rs, info, clean + k);

  printf ("=== rs benchmark ===\n");
  printf ("RS(%u,%u) over GF(2^%u), E=%u, %d codewords/round, %d rounds\n\n",
          n, k, CODE.symbol_bits, e, BENCH_N, ITERATIONS);

  static double t_enc[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        rs_encode (&rs, info, work + k);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_enc[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "encode", t_enc, ITERATIONS, BENCH_N);
  report ("encode", t_enc, k);

  static double t_ok[ITERATIONS];
  volatile int  sink = 0;
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        sink += rs_codeword_ok (&rs, clean);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_ok[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "verify", t_ok, ITERATIONS, BENCH_N);
  report ("verify", t_ok, k);

  /* Error counts: none, half the capability, all of it. `rs_decode` mutates
     its codeword, so each timed call gets a fresh copy -- the memcpy is
     inside the loop and therefore inside the measurement, which is honest:
     a real caller also has to get the codeword from somewhere. It is 255
     bytes against a microsecond of field arithmetic. */
  const unsigned    counts[3] = { 0u, e / 2u, e };
  static double     t_dec[3][ITERATIONS];
  const char *const names[3]
      = { "decode[e=0]", "decode[e=8]", "decode[e=16]" };

  for (int c = 0; c < 3; c++)
    {
      uint8_t *corrupt = malloc (n);
      if (!corrupt)
        return 1;
      memcpy (corrupt, clean, n);
      /* Spread the errors across the codeword rather than clustering them:
         a burst inside one syndrome's reach is not the general case. The
         stride must not wrap -- `j * 17 % n` put j=0 and j=15 on the same
         symbol, and two XORs of 0x5A cancel, so the "16 error" case was
         silently a 14-error one. `j * 15` tops out at 225 < n. */
      for (unsigned j = 0; j < counts[c]; j++)
        corrupt[(size_t)j * 15u] ^= 0x5Au;

      /* Decode ONE copy before timing any, and hold it to the contract:
         `counts[c]` symbols corrected and a codeword identical to the
         original. Without this the loop below happily times the -1
         bail-out path, which is FASTER than a real correction and reads
         as a suspiciously cheap decoder rather than as a broken bench. */
      memcpy (work, corrupt, n);
      int rc = rs_decode (&rs, work);
      if (rc != (int)counts[c] || memcmp (work, clean, n) != 0)
        {
          (void)fprintf (stderr,
                         "bench_rs: %s did not decode (rc=%d, expected %u) — "
                         "the timings below would measure the failure path\n",
                         names[c], rc, counts[c]);
          return 1;
        }

      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          for (int i = 0; i < BENCH_N; i++)
            {
              memcpy (work, corrupt, n);
              sink += rs_decode (&rs, work);
            }
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_dec[c][r] = elapsed_sec (&t0, &t1);
        }
      jm_bench_add (&_bench, names[c], t_dec[c], ITERATIONS, BENCH_N);
      report (names[c], t_dec[c], k);
      free (corrupt);
    }

  printf ("\n  worst case / clean = %.2fx, and `verify` costs what a clean\n"
          "  decode costs: the %u syndromes over %u symbols dominate, and\n"
          "  the correction they gate is the smaller half. So provisioning\n"
          "  on the clean path is nearly right here -- which is a measured\n"
          "  result for THIS code, not a property of RS. A shorter code or\n"
          "  a wider E moves the balance; re-measure before assuming it.\n",
          min_sec (t_dec[2], ITERATIONS) / min_sec (t_dec[0], ITERATIONS),
          CODE.nroots, n);

  /* First-run caveat, worth stating where the numbers are read: a cold
     process reads high (27.6 us here against 19.4 us on the next two runs,
     same binary) while the CPU ramps. MIN over rounds cannot see it -- every
     round in that process is equally cold. `make bench-interleaved` takes
     the per-benchmark best across K passes, which does. */

  (void)sink;
  free (info);
  free (clean);
  free (work);
  jm_bench_write_json (&_bench, "rs");
  return 0;
}
