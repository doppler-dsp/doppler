/**
 * @file carrier_acq_private.h
 * @brief The carrier acquisition's derived detection threshold.
 *
 * Not part of the public API. A composition that knows its template's
 * statistic is the one the derivation covers (AsyncDsssReceiver's squared
 * tone estimator, docs/design/async-dsss-receiver.md §4.7) switches its
 * CarrierAcquisition to it; every other caller keeps the default rule.
 */
#ifndef DP_CARRIER_ACQ_PRIVATE_H
#define DP_CARRIER_ACQ_PRIVATE_H

#include "doppler/carrier_acq/carrier_acq_core.h"
#include <stddef.h>

/**
 * @brief Test this estimator against its derived threshold from now on.
 *
 * The threshold is the ratio eta at which the null probability that the
 * correlation's peak over its median exceeds eta equals the configured pfa:
 * each lag a chi-square of thr_nu1 * n_blocks degrees of freedom, the peak
 * over the nfft circular lags by Rice's upcrossing formula, the median a
 * sample over thr_neff independent lags. Every input is the window's and
 * the template's, derived at create; nothing is fitted. The rule travels in
 * the blob, so a restored estimator keeps it.
 *
 * @param ca  Must be non-NULL.
 */
void dp__carrier_acq_derive_threshold (dp_carrier_acq_state_t *ca);

/**
 * @brief The derived ratio threshold at @p n_blocks blocks (> 0).
 *
 * What the derived rule tests against after that many blocks, exposed for
 * the tests that measure its realized false-alarm rate.
 *
 * @param ca        Must be non-NULL; its null statistics are computed at
 *                  create.
 * @param n_blocks  Blocks integrated, > 0.
 * @return          The peak-over-median ratio a dwell of @p n_blocks must
 *                  exceed to declare at the configured pfa.
 */
double dp__carrier_acq_derived_threshold (const dp_carrier_acq_state_t *ca,
                                          size_t n_blocks);

#endif /* DP_CARRIER_ACQ_PRIVATE_H */
