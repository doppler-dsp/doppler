/**
 * @file dp_preamble_test.h
 * @brief A PN code as preamble SAMPLES, for the burst engines' tests.
 *
 * The burst constructors -- acq_create_burst(), burst_acq_create(),
 * burst_capture_create() -- take a preamble as its samples (doppler#1470):
 * one period, at `fs`. A PN code is one such preamble, built the way a
 * caller builds it: chips mapped by bin_to_nrz(), the library's one
 * chip -> +-1 rule, each held `spc` samples, at `fs = chip_rate * spc`.
 *
 * One builder, so a test cannot quietly grow its own mapping: before this
 * the chip -> sign rule was written inline dozens of times across these
 * files, each free to disagree with the library's.
 *
 * Link `cvt` (bin_to_nrz) into any test that includes this.
 */
#ifndef DP_PREAMBLE_TEST_H
#define DP_PREAMBLE_TEST_H

#include "cvt/cvt_core.h"
#include "util/util_core.h"
#include <complex.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @brief One period of @p code, mapped by bin_to_nrz() and held @p spc
 *        samples a chip: `code_len * spc` samples, heap-allocated.
 *
 * @param code      PN chips (0/1), length @p code_len.
 * @param code_len  Chips in one period (>= 1).
 * @param spc       Samples per chip (>= 1).
 * @return The samples; the caller frees them.
 */
static inline float _Complex *
dp_code_preamble (const uint8_t *code, size_t code_len, size_t spc)
{
  float          *nrz = dp_xmalloc (code_len * sizeof *nrz);
  float _Complex *pre = dp_xmalloc (code_len * spc * sizeof *pre);
  (void)bin_to_nrz (code, code_len, nrz, code_len);
  for (size_t i = 0; i < code_len * spc; i++)
    pre[i] = nrz[i / spc];
  free (nrz);
  return pre;
}

#endif /* DP_PREAMBLE_TEST_H */
