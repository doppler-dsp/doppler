/**
 * @file RateConverter_core.h
 * @brief Optimal-speed rate conversion cascade.
 *
 * Selects the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase
 * Resampler stages for a given output/input sample rate ratio at creation
 * time.  All sub-stage C objects are owned by the state struct.
 *
 * Stage selection (D = 1/rate):
 *
 *   rate >= 1.0 or D < 2           [Resampler(rate)]
 *   D ~= 2^1                        [HalfbandDecimator]
 *   D ~= 2^2                        [HalfbandDecimator, HalfbandDecimator]
 *   D ~= 2^n, n>=3, D<=4096         [CIC(D)]
 *   D >= 8, non-power-of-2          [CIC(R*), Resampler correction]
 *                                    R* = nearest power-of-2 to D
 *   otherwise (2 <= D < 8, non-int) [Resampler(rate)]
 *
 * Lifecycle:
 * @code
 *   RateConverter_state_t *rc = RateConverter_create(0.1, 0);
 *   // rc->n_stages == 2: CIC(8) then Resampler(0.8)
 *   float _Complex out[512];
 *   size_t n = RateConverter_execute(rc, in, 4096, out, 512);
 *   RateConverter_destroy(rc);
 * @endcode
 */
#ifndef RATE_CONVERTER_CORE_H
#define RATE_CONVERTER_CORE_H

#include "clib_common.h"

#include <complex.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Maximum number of visible cascade stages. */
#define RC_MAX_STAGES 3

/** Stage type tags. */
typedef enum
{
  RC_STAGE_HB     = 0, /**< HalfbandDecimator (2:1, CF32) */
  RC_STAGE_CIC    = 1, /**< CIC decimator (optionally with comp FIR) */
  RC_STAGE_RESAMP = 2, /**< Polyphase Resampler (any positive rate) */
} rc_stage_t;

/**
 * @brief Cascade state -- owns all sub-stage C objects.
 *
 * Do not initialise directly; use RateConverter_create().
 */
typedef struct
{
  double         rate;                        /**< current rate ratio        */
  int            compensate;                  /**< CIC droop-comp flag       */
  int            n_stages;                    /**< active stage count        */
  rc_stage_t     stage_types[RC_MAX_STAGES];  /**< stage type per slot       */
  void          *stage_ptrs[RC_MAX_STAGES];   /**< sub-object per slot       */
  /** Ping-pong intermediate buffers, grown lazily on first execute. */
  float _Complex *bufs[2];
  size_t          buf_cap;
} RateConverter_state_t;

/**
 * @brief Create a rate converter for the given output/input rate ratio.
 *
 * @param rate       Output-to-input sample rate ratio.  Any positive float.
 * @param compensate Non-zero to append a CIC passband-droop compensating FIR
 *                   after any CIC stage.
 * @return Non-NULL on success; NULL if rate <= 0 or OOM.
 *
 * @code
 * RateConverter_state_t *rc = RateConverter_create(0.125, 0); // CIC(8)
 * assert(rc->n_stages == 1);
 * @endcode
 */
RateConverter_state_t *RateConverter_create (double rate, int compensate);

/** @brief Free all resources.  NULL is a no-op. */
void RateConverter_destroy (RateConverter_state_t *s);

/**
 * @brief Zero all sub-stage filter memories.
 *
 * Rate and stage structure are preserved.  Processing from a reset state
 * produces the same output as a freshly created converter.
 */
void RateConverter_reset (RateConverter_state_t *s);

/**
 * @brief Convert n_in samples and write results to out.
 *
 * @param s        Must be non-NULL.
 * @param in       CF32 input samples.
 * @param n_in     Number of input samples.
 * @param out      Output buffer.
 * @param max_out  Output buffer capacity in samples.
 * @return Number of output samples written.
 */
size_t RateConverter_execute (RateConverter_state_t *s,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out);

/**
 * @brief Upper bound on execute output for a standard 65536-sample block.
 *
 * Returns (size_t)(65536 * max(rate, 1.0)) + 2.  The Python extension uses
 * this to pre-allocate the output buffer on the first execute call.
 */
size_t RateConverter_execute_max_out (RateConverter_state_t *s);

/** @brief Return the current rate ratio. */
double RateConverter_get_rate (const RateConverter_state_t *s);

/**
 * @brief Change the rate; rebuilds the cascade and resets all filter state.
 *
 * Silently ignores rate <= 0.
 *
 * @param s     Must be non-NULL.
 * @param rate  New output/input rate ratio.
 */
void RateConverter_set_rate (RateConverter_state_t *s, double rate);

/**
 * @brief Write a human-readable label for stage i into buf.
 *
 * Examples: "HalfbandDecimator", "CIC(8)", "CIC(8)+FIR", "Resampler(0.8)".
 *
 * @param s    Must be non-NULL.
 * @param i    Stage index in [0, s->n_stages).
 * @param buf  Output buffer.
 * @param len  Capacity of buf in bytes.
 * @return 1 on success, 0 if i is out of range.
 */
int RateConverter_stage_label (RateConverter_state_t *s, int i,
                               char *buf, size_t len);

/**
 * @brief One-shot rate conversion — no persistent state required.
 *
 * Creates a temporary converter, converts n_in samples, destroys it.
 * Equivalent to:
 * @code
 * RateConverter_state_t *rc = RateConverter_create(rate, compensate);
 * size_t n = RateConverter_execute(rc, in, n_in, out, max_out);
 * RateConverter_destroy(rc);
 * @endcode
 *
 * Use RateConverter_create() directly when processing multiple blocks at
 * the same rate — the one-shot form resets filter memory on every call.
 *
 * @param rate       Output-to-input sample rate ratio.
 * @param compensate Non-zero to enable CIC droop compensation.
 * @param in         CF32 input samples.
 * @param n_in       Number of input samples.
 * @param out        Output buffer.
 * @param max_out    Output buffer capacity in samples.
 * @return Number of output samples written, or 0 on allocation failure.
 */
size_t RateConverter_convert (double rate, int compensate,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* RATE_CONVERTER_CORE_H */
