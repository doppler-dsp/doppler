/**
 * dp_power_to_db_test.h — the one measurement of dp_power_to_db_f32's error,
 * shared by the sampled check in `make test` (test_spectral_core.c) and the
 * exhaustive sweep over every float32 (native/validation/power_to_db_sweep.c,
 * `make test-sweep`), so the two tiers cannot measure differently (#2094).
 *
 * Values are converted in blocks through the real call, so the vectorized
 * body and its remainder are what is measured, not a scalar copy. A value at
 * or above the floor (1e-20) is judged against double-precision 10*log10;
 * a value below it (0, a subnormal, a negative value) must read exactly
 * -200.0f, so it is counted, not measured.
 */
#ifndef DP_POWER_TO_DB_TEST_H
#define DP_POWER_TO_DB_TEST_H

#include "doppler/spectral/spectral_core.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

/** @brief What a scan found. */
typedef struct
{
  double   worst;       /**< max |error| in dB, over values >= the floor */
  uint32_t worst_bits;  /**< the float (as bits) where it was found      */
  uint64_t measured;    /**< values judged against 10*log10             */
  uint64_t floored;     /**< values below the floor                     */
  uint64_t floor_wrong; /**< of those, ones that did not read -200 exactly */
} dp_p2db_scan_t;

#define DP_P2DB_BLOCK 4096u

/** @brief Judge one block of float bit patterns already converted. */
static inline void
dp_p2db_judge (const float *x, const float *db, size_t n, dp_p2db_scan_t *s)
{
  for (size_t i = 0; i < n; i++)
    {
      if (!(x[i] >= 1e-20f))
        {
          s->floored++;
          s->floor_wrong += db[i] != -200.0f;
          continue;
        }
      const double e = fabs ((double)db[i] - 10.0 * log10 ((double)x[i]));
      s->measured++;
      if (e > s->worst)
        {
          s->worst = e;
          memcpy (&s->worst_bits, &x[i], sizeof x[i]);
        }
    }
}

/**
 * @brief Scan the float bit patterns lo, lo + step, ... up to hi inclusive
 *        (finite values only: a NaN or Inf pattern is skipped).
 */
static inline void
dp_p2db_scan (uint32_t lo, uint32_t hi, uint32_t step, dp_p2db_scan_t *s)
{
  static float x[DP_P2DB_BLOCK], db[DP_P2DB_BLOCK];
  size_t       k = 0;
  for (uint64_t b = lo; b <= hi; b += step)
    {
      const uint32_t bits = (uint32_t)b;
      if ((bits & 0x7F800000u) == 0x7F800000u)
        continue; /* NaN or Inf: unspecified, not measured */
      memcpy (&x[k], &bits, sizeof bits);
      if (++k == DP_P2DB_BLOCK)
        {
          dp_power_to_db_f32 (x, k, db);
          dp_p2db_judge (x, db, k, s);
          k = 0;
        }
    }
  if (k)
    {
      dp_power_to_db_f32 (x, k, db);
      dp_p2db_judge (x, db, k, s);
    }
}

#endif /* DP_POWER_TO_DB_TEST_H */
