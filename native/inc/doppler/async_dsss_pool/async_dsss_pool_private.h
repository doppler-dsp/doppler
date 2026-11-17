/**
 * @file async_dsss_pool_private.h
 * @brief The ghost rule's counterfactual, for the measurement that keeps the
 *        rule as it is.
 *
 * Not part of the public API. The pool's ghost rule
 * (dp_async_dsss_pool_ghost_of(), docs/design/async-dsss-receiver.md §8)
 * takes only a whole NONZERO number of lattice steps: a ghost on its
 * parent's own carrier (k = 0) is left alone, because a true emitter that
 * shares a stronger one's carrier looks the same. Showing that takes the
 * rule with k = 0 admitted, evaluated on the same rings by the same tests,
 * which is this.
 */
#ifndef DP_ASYNC_DSSS_POOL_PRIVATE_H
#define DP_ASYNC_DSSS_POOL_PRIVATE_H

#include "doppler/async_dsss_pool/async_dsss_pool_core.h"
#include <stddef.h>

/**
 * @brief dp_async_dsss_pool_ghost_of() with k = 0 admitted: whether
 *        receiver @p j would be released as receiver @p i's ghost if the
 *        rule took a carrier on its parent's own lattice point as well.
 *
 * The same window, the same code-lock test, the same stillness and lattice
 * tolerance; only k = 0 is no longer refused. The pool never calls it.
 *
 * @param state  Must be non-NULL.
 * @param i      The stronger holder.
 * @param j      The candidate.
 * @return 1 if the counterfactual rule takes @p j; 0 otherwise, and for an
 *         index past the pool, @p i equal to @p j, or a ring not yet full.
 */
int dp__async_dsss_pool_ghost_of_any_k (const dp_async_dsss_pool_state_t *state,
                                        size_t i, size_t j);

#endif /* DP_ASYNC_DSSS_POOL_PRIVATE_H */
