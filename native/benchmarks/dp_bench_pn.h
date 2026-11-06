/**
 * @file dp_bench_pn.h
 * @brief A benchmark's m-sequence reference, from the shipped generator.
 *
 * Shared by bench_correlator_core.c's short rows (#2752 step 3.1b): the
 * correlator's and the 1-D detector's each correlate a +-1 m-sequence
 * against itself delayed, so a row can check that the peak lands where it
 * was planted. One copy, so the rows cannot drift apart in how they build
 * it. A bench that includes this
 * links pn_core and cvt_core (its directory's *_extra.cmake).
 */
#ifndef DP_BENCH_PN_H
#define DP_BENCH_PN_H

#include "doppler/cvt/cvt_core.h"
#include "doppler/pn/pn_core.h"

#include <stdint.h>
#include <stdlib.h>

/**
 * @brief One period of the m-sequence of a @p bits -bit register as +-1 NRZ
 *        samples: dp_pn_create_mls, mapped by dp_bin_to_nrz.
 *
 * @param bits  Register width; the period is 2^bits - 1.
 * @param out   Receives the period's samples.
 * @param cap   Capacity of @p out.
 * @return The period, or 0 when it does not fit or a call fails.
 */
static inline size_t
dp_bench_pn_nrz (uint32_t bits, float _Complex *out, size_t cap)
{
  const size_t   n    = ((size_t)1 << bits) - 1;
  dp_pn_state_t *pn   = dp_pn_create_mls (0, 1, bits, 0);
  uint8_t       *chip = malloc (n);
  float         *nrz  = malloc (n * sizeof *nrz);
  size_t         got  = 0;
  if (pn && chip && nrz && n <= cap && dp_pn_generate (pn, n, chip, n) == n
      && dp_bin_to_nrz (chip, n, nrz, n) == n)
    {
      for (size_t i = 0; i < n; i++)
        out[i] = nrz[i];
      got = n;
    }
  dp_pn_destroy (pn);
  free (chip);
  free (nrz);
  return got;
}

#endif /* DP_BENCH_PN_H */
