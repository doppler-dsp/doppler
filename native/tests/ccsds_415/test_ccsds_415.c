/*
 * test_ccsds_415.c -- the 415.1-B-1 point's arithmetic is the same bits on
 * every compiler (doppler/ccsds/ccsds_415.h).
 */
#include "doppler/ccsds/ccsds_415.h"
#include "dp_test.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

static int
same_bits (double a, double b)
{
  return memcmp (&a, &b, sizeof a) == 0;
}

/* The chip rate is fc * 31 / (221 * 96) to the bit, at a carrier the
   compiler cannot fold. Rejects the arithmetic under fast math: written
   fc * 31 * (1 / 21216), the 2.8 GHz rate is one ulp off
   (0x1.f36b9f153c9f2p+21 against 0x1.f36b9f153c9f1p+21). */
static int
test_the_chip_rate_is_the_tables_formula (void)
{
  /* volatile: read at run time, as a program reads its carrier */
  static volatile double fcs[2] = { DP_CCSDS_415_FC_LO, DP_CCSDS_415_FC_HI };
  /* fc * 31 / 21216 as IEEE-754 doubles give it (Python's, unfused) */
  static const double want[2] = { 3214555.052790347, 4091251.8853695323 };
  for (size_t i = 0; i < 2; i++)
    {
      const double got = dp_ccsds_415_chip_rate (fcs[i]);
      if (!same_bits (got, want[i]))
        DP_TEST_ERR ("chip rate at %g: %a, want %a", fcs[i], got, want[i]);
      DP_CHECK (same_bits (got, want[i]));
    }
  DP_CHECK (fabs (dp_ccsds_415_cn0 (4.0) - 38.771212547196626) < 1e-12);
  return 0;
}

/* The scene's numbers are bit for bit the literals the spec demo and its
   Python twin used before the helper existed. The C demo now reads the
   helper; the Python twin still TYPES these (it reads the helper when
   just-makeit#2755 binds a module function returning one record, and then
   this interim pin goes with the literals). So a change to the helper that
   would leave the twin behind turns this red, as does the pair test, which
   compares the twins' printed %a lines. Rejects the fast-math arithmetic:
   under Clang's -ffast-math, start_ppm at 2.2 GHz is
   -0.0062500000000000003, not -0.0062499999999999995. */
static int
test_the_scene_is_the_twins_literals (void)
{
  static volatile double fcs[2] = { DP_CCSDS_415_FC_LO, DP_CCSDS_415_FC_HI };
  static const double    lit[2][4] = {
    { 3214555.052790347, 1.7109677419354838, -0.0062499999999999995, 0.005 },
    { 4091251.8853695323, 1.344331797235023, -0.004910714285714286,
      0.003928571428571429 },
  };
  for (size_t i = 0; i < 2; i++)
    {
      const dp_ccsds_415_scene_t s = dp_ccsds_415_scene (fcs[i], 2.0, -13.75);
      const double               got[4]
          = { s.chip_rate, s.ratio, s.start_ppm, s.rate_ppm_s };
      static const char *const name[4]
          = { "chip_rate", "ratio", "start_ppm", "rate_ppm_s" };
      for (size_t k = 0; k < 4; k++)
        {
          if (!same_bits (got[k], lit[i][k]))
            DP_TEST_ERR ("%s at %g: %a, the twins typed %a", name[k], fcs[i],
                         got[k], lit[i][k]);
          DP_CHECK (same_bits (got[k], lit[i][k]));
        }
    }
  return 0;
}

int
main (void)
{
  DP_CHECK (test_the_chip_rate_is_the_tables_formula () == 0);
  DP_CHECK (test_the_scene_is_the_twins_literals () == 0);
  DP_TEST_END ("test_ccsds_415");
}
