/**
 * @file corr2d_map.h
 * @brief The one delay-Doppler block map (#2752 step 2,
 *        docs/design/acq-surface.md §18.1). Internal: acq and Corr2D
 *        compose it, nothing outside the library calls it.
 *
 * For one block of D epochs of N samples, with the epoch spectra
 * X_k = FFT_N(x_k) and a tile's template weights C:
 *
 * 1. **Slow time first, shared by every tile.** For each capture bin p, the
 *    D values w_k X_k[p] (w a rate hypothesis' per-epoch de-chirp, or 1),
 *    zero-padded to F DI points and transformed:
 *    S[p, m] = sum_k w_k X_k[p] e^{-j 2 pi k m / (F DI)}, stored bin by bin.
 * 2. **Rows per tile and slow row.** For tile q and slow row i in [0, DI),
 *    each bin j read at fine row F i + off_qj (mod F DI) of bin
 *    (j + roll_q) mod N, times the tile's weight wt_qj, padded to L and
 *    inverted (unnormalized) into output row rows_q[i]:
 *    Z_q[i, .] = IFFT_L(pad_L(S[(j + roll_q) mod N, F i + off_qj] wt_qj)).
 *
 * The weights carry the scale: 1/N on every caller (one scale). A tile with
 * no code-rate ramp has every off_qj = 0 and F = 1; its weights are the
 * template's alone. dp__dd_ramp_table() is the one home of the rounded
 * ramp's offsets and phases. With D = 1 and F = 1 there is no slow time: S
 * is X itself, read in place.
 *
 * **Threading.** Each stage fans once over the caller's pool, in chunks
 * fixed by the chunk index alone, and a chunk runs on that index's slot:
 * its own plans and scratch. Every output row is computed by one chunk with
 * the same arithmetic, so the map is bit-identical at every thread count.
 * A stage given a slot >= 0 runs whole on that slot, serially (a batch's
 * map per worker).
 */
#ifndef DP_CORR2D_MAP_H
#define DP_CORR2D_MAP_H

#include "doppler/dp_parallel.h"
#include <complex.h>
#include <stddef.h>
#include <stdint.h>

/** The ramp's fine grid: F times the padded slow-time rows (§18.1). */
#define DP__DD_MAP_FINE 4u

/** A map's shape. */
typedef struct
{
  size_t n;  /**< capture bins, the epoch length N                   */
  size_t l;  /**< lag bins of the output rows, L >= N (padded)      */
  size_t d;  /**< epochs a block, D >= 1                            */
  size_t di; /**< slow-time rows, DI >= D (DI > D interpolates)     */
  size_t f;  /**< fine-grid factor, 1 or DP__DD_MAP_FINE            */
} dp__dd_geom_t;

/** One tile: what the row stage reads for it and where it writes. */
typedef struct dp__dd_tile
{
  size_t         roll;      /**< reads bin (j + roll) mod N               */
  const int32_t *off;       /**< N fine-row offsets in [0, F DI), or NULL
                                 for all 0                                */
  const float _Complex *wt; /**< N weights, the scale included, or NULL
                                 for 1/N on every bin                     */
  const size_t *rows;       /**< DI output rows, each a row of L cells    */
} dp__dd_tile_t;

/** The map: its shape, its worker slots of plans and scratch, and the
 *  rows-first ramp's phase table. Opaque; made by dp__dd_map_create(). */
typedef struct dp__dd_map dp__dd_map_t;

/**
 * @brief A map of shape @p g with @p slots worker slots.
 *
 * Never aborts: a caller's regrid can fail cleanly.
 *
 * @param g      The shape: N >= 1, L >= N, D >= 1, DI >= D, F 1 or
 *               DP__DD_MAP_FINE.
 * @param slots  Worker slots, >= 1: the most chunks a stage fans into.
 * @return The map, or NULL when an argument is out of range or an
 *         allocation fails.
 */
dp__dd_map_t *dp__dd_map_create (const dp__dd_geom_t *g, int slots);

/**
 * @brief Free a map.
 * @param map  The map, or NULL (a no-op).
 */
void dp__dd_map_destroy (dp__dd_map_t *map);

/**
 * @brief Grow a map to @p slots worker slots.
 *
 * A failure leaves the map whole at its old count: a stage fans over at most
 * the slots it has, so the failure costs speed, never a result.
 *
 * @param map    The map.
 * @param slots  The slots wanted; fewer than it has is a no-op.
 * @return 0, or -1 when an allocation fails.
 */
int dp__dd_map_reserve (dp__dd_map_t *map, int slots);

/**
 * @brief A map's shape.
 * @param map  The map.
 * @return Its shape, owned by the map.
 */
const dp__dd_geom_t *dp__dd_map_geom (const dp__dd_map_t *map);

/**
 * @brief The slow-time output's length.
 * @param map  The map.
 * @return F DI N complex values (N when F DI = 1: S is then X itself).
 */
size_t dp__dd_map_fine_len (const dp__dd_map_t *map);

/**
 * @brief Epoch spectra: X[k N .. k N + N) = FFT_N(x[k N ..]) for k < D.
 * @param map   The map.
 * @param pool  The pool to fan over epochs on, or NULL (serial).
 * @param slot  < 0 to fan; else run whole on this slot.
 * @param x     D epochs of N samples.
 * @param X     D x N spectra, epoch-major.
 */
void dp__dd_map_forward (dp__dd_map_t *map, dp_pool_t *pool, int slot,
                         const float _Complex *x, float _Complex *X);

/**
 * @brief Slow time first, stage 1: S[p F DI + m] for every bin p and fine
 *        row m, bin-major.
 *
 * Not called when F DI = 1 (S is then X).
 *
 * @param map   The map.
 * @param pool  The pool to fan over bin chunks on, or NULL (serial).
 * @param slot  < 0 to fan; else run whole on this slot.
 * @param X     D x N spectra, epoch-major.
 * @param w     D per-epoch weights (a rate's de-chirp), or NULL for 1.
 * @param S     F DI N values.
 */
void dp__dd_map_slow (dp__dd_map_t *map, dp_pool_t *pool, int slot,
                      const float _Complex *X, const float _Complex *w,
                      float _Complex *S);

/**
 * @brief Slow time first, stage 2: every tile's DI rows, gathered from S,
 *        padded to L and inverted onto the rows each tile names.
 * @param map      The map.
 * @param pool     The pool to fan over the tiles' rows on, or NULL.
 * @param slot     < 0 to fan; else run whole on this slot.
 * @param S        The slow-time output (X when F DI = 1).
 * @param tiles    The tiles.
 * @param n_tiles  How many.
 * @param out      The surface, rows of L.
 */
void dp__dd_map_rows (dp__dd_map_t *map, dp_pool_t *pool, int slot,
                      const float _Complex *S, const dp__dd_tile_t *tiles,
                      size_t n_tiles, float _Complex *out);

/**
 * @brief One row from one epoch's spectrum: the row stage at
 *        D = DI = F = 1.
 *
 * A caller's exact per-epoch weights (an exact ramp folded into @p wt) run
 * through the same code.
 *
 * @param map   The map.
 * @param slot  The slot to run on.
 * @param X     The epoch's N bins.
 * @param roll  Reads bin (j + roll) mod N.
 * @param wt    N weights, the scale included, or NULL for 1/N.
 * @param row   L values.
 */
void dp__dd_map_row1 (dp__dd_map_t *map, int slot, const float _Complex *X,
                      size_t roll, const float _Complex *wt,
                      float _Complex *row);

/**
 * @brief The rounded code-rate ramp of a tile, the one home of its rule.
 *
 * r_j = lround(F DI dq j' / N), j' the signed bin (dp_fftfreq_index);
 * off[j] = r_j wrapped into [0, F DI); wt[j] = base[j] e^{+j 2 pi c r_j /
 * (F DI)}, c = (D - 1)/2 the block's middle.
 *
 * @param g     The map's shape.
 * @param dq    The tile's dilation, samples an epoch.
 * @param base  The tile's unramped weights (template and scale), N values.
 * @param off   N fine-row offsets.
 * @param wt    N weights.
 */
void dp__dd_ramp_table (const dp__dd_geom_t *g, double dq,
                        const float _Complex *base, int32_t *off,
                        float _Complex *wt);

/* ── Two schedules of the one map ─────────────────────────────────────────
 *
 * The same function computed in either order (docs/design/acq-surface.md
 * §18.1): slow time first (the stages above: shared slow time on the fine
 * grid, then every tile's rows gathered from it), or rows first (each
 * epoch's product inverted, with the rounded ramp as a per-epoch phase,
 * then slow time down every lag column). They differ by float rounding
 * alone. dp__dd_schedule() is the one rule that picks between them. */

/** Slow time first: shared slow time, then every tile's rows. */
#define DP__DD_SLOW_FIRST 0
/** Rows first: every epoch inverted, then slow time down each column. */
#define DP__DD_ROWS_FIRST 1

/**
 * @brief The schedule a map runs: rows first only when it counts clearly
 *        fewer transforms, n log2 n each.
 *
 * Slow first costs, per rate, N transforms of F DI and W DI inverses of L.
 * Rows first costs W D inverses of L once -- not counted for a @p fused
 * caller, which runs them inside a frame stage it runs anyway, on rows
 * still in cache -- and, per rate, W L transforms of DI. Rows first runs
 * only when it counts more than 1/16 fewer (the count's resolution); a
 * tie within it goes to slow first, which measured faster at every
 * near-equal count on acq's working sets
 * (docs/design/acq-surface-measurements.md §18.6). One slow row (DI = 1) is
 * rows first: there is no slow time to order.
 *
 * @param g        The map's shape.
 * @param n_tiles  W, the tiles one slow time serves.
 * @param n_rates  R, the rate hypotheses.
 * @param fused    1 when the caller inverts each epoch inside a frame
 *                 stage it already runs (DelayDoppler's row stage), else 0.
 * @return DP__DD_SLOW_FIRST or DP__DD_ROWS_FIRST.
 */
int dp__dd_schedule (const dp__dd_geom_t *g, size_t n_tiles, size_t n_rates,
                     int fused);

/**
 * @brief Override the rule on a map, for tests that run both schedules.
 * @param map       The map.
 * @param schedule  DP__DD_SLOW_FIRST, DP__DD_ROWS_FIRST, or -1 for the rule.
 */
void dp__dd_map_force (dp__dd_map_t *map, int schedule);

/**
 * @brief The schedule a map runs.
 * @param map      The map.
 * @param n_tiles  W.
 * @param n_rates  R.
 * @param fused    As dp__dd_schedule()'s.
 * @return The forced schedule, else dp__dd_schedule()'s.
 */
int dp__dd_map_pick (const dp__dd_map_t *map, size_t n_tiles, size_t n_rates,
                     int fused);

/**
 * @brief Rows first, stage 1: tile q's epoch k rolled, weighted, ramped by
 *        e^{-j 2 pi k off_j / (F DI)}, padded and inverted.
 * @param map      The map.
 * @param pool     The pool to fan over (tile, epoch) on, or NULL.
 * @param slot     < 0 to fan; else run whole on this slot.
 * @param X        D x N spectra, epoch-major.
 * @param tiles    The tiles.
 * @param n_tiles  How many.
 * @param blk      n_tiles x D x L: tile q's epoch k at (q D + k) L.
 */
void dp__dd_map_epochs (dp__dd_map_t *map, dp_pool_t *pool, int slot,
                        const float _Complex *X, const dp__dd_tile_t *tiles,
                        size_t n_tiles, float _Complex *blk);

/**
 * @brief Rows first, stage 1 for one epoch: the epoch stage itself, for a
 *        caller that inverts each epoch while it is in cache.
 * @param map   The map.
 * @param slot  The slot to run on.
 * @param x     The epoch's N bins.
 * @param k     Its index in the block (the ramp's phase).
 * @param tl    The tile.
 * @param dst   L values.
 */
void dp__dd_map_epoch1 (dp__dd_map_t *map, int slot, const float _Complex *x,
                        size_t k, const dp__dd_tile_t *tl,
                        float _Complex *dst);

/**
 * @brief Rows first, stage 2: slow time down every lag column of every
 *        tile's block, zero-padded to DI, onto the tile's rows.
 * @param map      The map.
 * @param pool     The pool to fan over (tile, column chunk) on, or NULL.
 * @param slot     < 0 to fan; else run whole on this slot.
 * @param blk      The blocks dp__dd_map_epochs() made.
 * @param w        D per-epoch weights, or NULL for 1.
 * @param tiles    The tiles.
 * @param n_tiles  How many.
 * @param out      The surface, rows of L.
 */
void dp__dd_map_columns (dp__dd_map_t *map, dp_pool_t *pool, int slot,
                         const float _Complex *blk, const float _Complex *w,
                         const dp__dd_tile_t *tiles, size_t n_tiles,
                         float _Complex *out);

/** Called with each rate's map, in rate order; @p out is reused. */
typedef void (*dp__dd_fold_fn) (void *ctx, size_t r,
                                const float _Complex *out);

/**
 * @brief The scratch dp__dd_map_run() needs.
 * @param map      The map.
 * @param n_tiles  W.
 * @param n_rates  R.
 * @return Complex values: the fine slow time (slow first) or the tiles'
 *         blocks (rows first); 0 when it needs none.
 */
size_t dp__dd_map_work_len (const dp__dd_map_t *map, size_t n_tiles,
                            size_t n_rates);

/**
 * @brief The whole map, for every rate, in the schedule the rule picks.
 * @param map      The map.
 * @param pool     The pool to fan each stage on, or NULL.
 * @param slot     < 0 to fan; else run whole on this slot.
 * @param X        D x N epoch spectra.
 * @param rates    R x D per-epoch weights, or NULL.
 * @param n_rates  R (0 counts as 1).
 * @param tiles    The tiles.
 * @param n_tiles  How many.
 * @param work     dp__dd_map_work_len() values.
 * @param out      The surface, rows of L as the tiles name them.
 * @param fold     Called once per rate with @p out.
 * @param ctx      Passed to @p fold.
 * @return The schedule it ran.
 */
int dp__dd_map_run (dp__dd_map_t *map, dp_pool_t *pool, int slot,
                    const float _Complex *X, const float _Complex *rates,
                    size_t n_rates, const dp__dd_tile_t *tiles, size_t n_tiles,
                    float _Complex *work, float _Complex *out,
                    dp__dd_fold_fn fold, void *ctx);

#endif /* DP_CORR2D_MAP_H */
