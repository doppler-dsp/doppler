/**
 * @file acq_private.h
 * @brief Shared internals for acq_core.c and its test.
 *
 * Not part of the public API: nothing here is exported, and nothing here is
 * promised to a caller. It exists so a one-line rule that decides a result
 * can be unit-tested as a rule (test_acq_core.c includes it), the pattern
 * detector/det_private.h follows.
 */
#ifndef ACQ_PRIVATE_H
#define ACQ_PRIVATE_H

#include <stddef.h>

/**
 * @brief Does hypothesis @p h's statistic @p stat replace the best so far,
 *        @p best? The rule the rate search picks its winner by.
 *
 * Hypothesis 0 always wins the first place, whatever it reads: a dwell
 * whose every statistic is NaN (a power that overflows float is inf/inf)
 * would otherwise fail `stat > best` at every hypothesis, pick none, and
 * leave the search restoring the PREVIOUS dwell's winning surface. After
 * it, a hypothesis must beat the best strictly, so a NaN never displaces
 * one and ties keep the earlier.
 *
 * Unit-tested as a rule on NaN / inf / finite values, because the
 * end-to-end route to a NaN statistic (a float power overflowing) depends
 * on the compiler's code generation and differs between Release and Debug.
 */
static inline int
acq_rate_better (size_t h, float stat, float best)
{
  return h == 0 || stat > best;
}

#endif /* ACQ_PRIVATE_H */
