/**
 * @file async_dsss_receiver_private.h
 * @brief A receiver's roles inside an AsyncDsssPool: pooled (idle until
 *        seeded) and search-only (the controller's one search).
 *
 * The pool's plumbing (docs/design/async-dsss-receiver.md §5, §6), kept
 * out of async_dsss_receiver_core.h so the receiver's documented API does
 * not carry them. They are still exported: this header sits under
 * native/inc, so it is installed and its symbols are in
 * cmake/public-symbols.txt, which is what lets the pool, a separate
 * component, link to them. No caller is promised them; the pool is the one
 * user, and it sets both at create. Both values still
 * travel in the receiver's blob (v15) -- a pooled receiver restores pooled
 * and idle, a search restores its pick count -- pinned by
 * test_async_dsss_receiver_core's round trip.
 */
#ifndef ASYNC_DSSS_RECEIVER_PRIVATE_H
#define ASYNC_DSSS_RECEIVER_PRIVATE_H

#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/correlator/correlator_core.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Make a receiver a pooled one: while it holds nothing it is idle
 *        (ASYNC_DSSS_RX_IDLE), consuming samples without searching, until
 *        a controller seeds it (docs/design/async-dsss-receiver.md §6).
 *
 * A pool's receivers do not search for themselves: the controller's one
 * search finds the signals and seed() hands each to an idle receiver
 * (§5). Pooled, reset() and a release with `reacquire` return the
 * receiver to idle instead of to its own search, and seed() is taken
 * while idle as while searching. Turning it on while searching goes
 * idle at once; turning it off while idle restarts the search. A
 * receiver that holds a signal keeps it either way.
 *
 * @param state  Must be non-NULL.
 * @param on     Pooled or not.
 * @return `DP_OK`.
 */
int dp_async_dsss_receiver_set_pooled (dp_async_dsss_receiver_state_t *state,
                                       bool on);

/**
 * @brief Make a receiver a controller's search: each dwell's pick runs
 *        for ranks 0 … @p n_picks - 1 and the survivors are kept for
 *        take_picks(), and the receiver never seeds itself.
 *
 * The pool's one search (docs/design/async-dsss-receiver.md §5) is a
 * receiver in this mode, fed the same samples as the pool's receivers.
 * A dwell that decides inside a call runs the pick of §7.2 once per rank
 * against the exclusions (set_exclusions(), read at the call's first
 * sample and carried forward with it), and each surviving peak is kept
 * with its code phase carried to the sample after the call: the phase
 * seed() wants on a receiver fed the same call. A peak outside the seed
 * domain is counted in `refused_hits`, as a normal receiver counts it.
 * Picks accumulate, up to ASYNC_DSSS_RX_MAX_PEAKS, until taken. 0 is a
 * normal receiver again, its waiting picks dropped.
 *
 * @param state    Must be non-NULL.
 * @param n_picks  Ranks a dwell reports, at most ASYNC_DSSS_RX_MAX_PEAKS;
 *                 0 = off.
 * @return `DP_OK`, or `DP_ERR_INVALID` (nothing changes) past the cap.
 */
int dp_async_dsss_receiver_set_search_only (
    dp_async_dsss_receiver_state_t *state, size_t n_picks);

/**
 * @brief Take a search-only receiver's waiting picks, in the order their
 *        dwells found them (each dwell's strongest first), and clear them.
 *
 * A pick waits until taken. Taken calls after the one that found it, its
 * phase is carried to the sample after the last one fed, dilated with a
 * carrier, as a hit is carried to its hand-off; and a signal a later dwell
 * picks again (within the exclusions' one chip) replaces its waiting pick
 * with the fresher estimate, so the list holds each signal once.
 *
 * @param state         Must be non-NULL.
 * @param chip_phase    Out: each pick's code phase at the sample after
 *                      the last one fed, chips; at least @p max.
 * @param doppler_hz    Out: its Doppler estimate, Hz.
 * @param cn0_dbhz      Out: its C/N0 estimate, dB-Hz.
 * @param max           Capacity of each array.
 * @return The picks written, at most @p max; any past it are dropped.
 */
size_t dp_async_dsss_receiver_take_picks (
    dp_async_dsss_receiver_state_t *state, double *chip_phase,
    double *doppler_hz, double *cn0_dbhz, size_t max);

/**
 * @brief Whether the refine has decided: its carrier estimator is ready
 *        (and, with the fine hand-over on, the fine dwell is in), or it has
 *        given up at its block cap. The one predicate the receiver's own
 *        steps() and its period loop ask, exported for a test that stops a
 *        stream at that point (a checkpoint taken there) rather than keep a
 *        copy that drifts. No caller is promised it.
 *
 * @param state  Must be non-NULL, refining or not.
 * @return 1 when decided, else 0.
 */
int dp__async_dsss_receiver_refine_decided (
    const dp_async_dsss_receiver_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_DSSS_RECEIVER_PRIVATE_H */
