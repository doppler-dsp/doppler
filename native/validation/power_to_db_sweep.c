/**
 * @file power_to_db_sweep.c
 * @brief The exhaustive bound of dp_power_to_db_f32 (#2094, #2074): every
 * positive finite float32, about 2.1e9 values, against double 10*log10.
 *
 * The sampled tier (every exponent, its extremes and 2^16 mantissas each)
 * runs in `make test` from test_spectral_core.c. This is the whole set, and
 * it is a `sweep`-labelled ctest (native/validation/CMakeLists.txt labels
 * every test here), so it runs in `make test-sweep` and stays out of
 * `make test`. Both measure through native/tests/dp_power_to_db_test.h.
 *
 * It asserts the contract, at most 0.01 dB from 10*log10 at or above the
 * floor, and exactly -200 below it. It also asserts every negative float
 * reads -200, since power cannot be negative but the floor's select takes
 * one anyway. It prints the achieved worst and where it is, which the
 * measurement record quotes (docs/design/spectrogram-measurements.md, "The
 * fast dB conversion").
 *
 * Run: ./build/native/validation/validate_power_to_db_sweep
 */
#include "dp_power_to_db_test.h"

#include <stdio.h>
#include <string.h>

int
main (void)
{
  /* every positive finite float: +0 through FLT_MAX */
  dp_p2db_scan_t pos = { 0 };
  dp_p2db_scan (0x00000000u, 0x7F7FFFFFu, 1, &pos);
  /* every negative finite float, -0 included: the floor, exactly */
  dp_p2db_scan_t neg = { 0 };
  dp_p2db_scan (0x80000000u, 0xFF7FFFFFu, 1, &neg);

  float at;
  memcpy (&at, &pos.worst_bits, sizeof at);
  printf ("power_to_db_sweep: %llu positive values measured, %llu below the "
          "floor, %llu negative\n",
          (unsigned long long)pos.measured, (unsigned long long)pos.floored,
          (unsigned long long)neg.floored);
  printf ("  worst |error| %.3e dB at %.9g; floor wrong %llu + %llu\n",
          pos.worst, (double)at, (unsigned long long)pos.floor_wrong,
          (unsigned long long)neg.floor_wrong);

  /* and the scan saw every pattern: 0x7F800000 of each sign */
  const int ok = pos.worst <= 0.01 && pos.floor_wrong == 0
                 && neg.floor_wrong == 0 && neg.measured == 0
                 && pos.measured + pos.floored == 0x7F800000ull
                 && neg.floored == 0x7F800000ull;
  if (!ok)
    (void)fprintf (stderr, "power_to_db_sweep: FAILED the 0.01 dB contract "
                           "or the floor\n");
  return ok ? 0 : 1;
}
