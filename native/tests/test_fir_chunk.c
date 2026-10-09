/**
 * @file test_fir_chunk.c
 * @brief dp_fir is chunk-invariant: splitting a stream anywhere gives the
 *        bytes of the unsplit run (dp_chunk_inv.h), real and complex taps.
 *
 * Built once per instruction set the kernel has a distinct path for (see
 * fir_extra.cmake), because the failure this pins -- #1893 -- lives only in
 * the vector paths. The portable build has no FMA and no wide vector, so a
 * test linked against it passes with or without the defect.
 *
 *   FIR_REQUIRE_AVX2    compiled -mavx2 -mfma    (the 4-complex-lane path)
 *   FIR_REQUIRE_AVX512  compiled -mavx512f/dq    (8 lanes + the complex-tap
 *                                                 permute path)
 *
 * A build whose CPU lacks the extension SKIPs (exit 77, which ctest reports
 * as Skipped, not Passed): a binary that cannot run its kernel proved
 * nothing, and the gap should be visible rather than green.
 */
#include "doppler/fir/fir_core.h"
#include "dp_chunk_inv.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>

#if defined(FIR_REQUIRE_AVX512)
#define FIR_ISA "avx512"
#elif defined(FIR_REQUIRE_AVX2)
#define FIR_ISA "avx2"
#else
#define FIR_ISA "portable"
#endif

enum
{
  L = 1500
};

typedef struct
{
  const float _Complex *taps;
  size_t                n;
  int                   real;
} cfg_t;

static void *
make (void *arg)
{
  const cfg_t *c = (const cfg_t *)arg;
  if (c->real)
    {
      float rt[64];
      for (size_t i = 0; i < c->n; i++)
        rt[i] = crealf (c->taps[i]);
      return dp_fir_create_real (rt, c->n);
    }
  return dp_fir_create (c->taps, c->n);
}

static void
unmake (void *o)
{
  dp_fir_destroy ((dp_fir_state_t *)o);
}

static size_t
run (void *o, const void *in, size_t n, void *out, size_t cap)
{
  (void)cap;
  return dp_fir_execute ((dp_fir_state_t *)o, (const float _Complex *)in, n,
                         (float _Complex *)out);
}

static int
supported (void)
{
#if defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__))
#if defined(FIR_REQUIRE_AVX512)
  return __builtin_cpu_supports ("avx512f")
         && __builtin_cpu_supports ("avx512dq")
         && __builtin_cpu_supports ("fma");
#elif defined(FIR_REQUIRE_AVX2)
  return __builtin_cpu_supports ("avx2") && __builtin_cpu_supports ("fma");
#endif
#endif
  return 1;
}

int
main (void)
{
  if (!supported ())
    {
      printf ("SKIP test_fir_chunk (%s): this CPU lacks the extension\n",
              FIR_ISA);
      return 77;
    }

  static float _Complex in[L], taps[64];
  uint32_t st = 12345u;
  for (size_t i = 0; i < L; i++)
    in[i] = dp_cgauss (&st);
  for (size_t i = 0; i < 64; i++)
    taps[i] = dp_cgauss (&st);

  /* 1 (no delay line), 2, 7 (the state test's), 8/9 around a vector width,
   * and 33. Each is run with real and with complex taps. */
  static const size_t lens[] = { 1, 2, 7, 8, 9, 33 };
  for (size_t k = 0; k < sizeof lens / sizeof *lens; k++)
    for (int real = 0; real <= 1; real++)
      {
        cfg_t c = { taps, lens[k], real };
        char  name[64];
        snprintf (name, sizeof name, "dp_fir execute (%s taps, %zu taps)",
                  real ? "real" : "complex", lens[k]);
        dp_ci_spec_t spec = {
          .name     = name,
          .create   = make,
          .destroy  = unmake,
          .process  = run,
          .arg      = &c,
          .in_size  = sizeof (float _Complex),
          .out_size = sizeof (float _Complex),
          .out_cap  = L,
        };
        DP_CHECK (dp_chunk_invariance (&spec, in, L) == 0);
      }

  DP_TEST_END ("test_fir_chunk (" FIR_ISA ")");
}
