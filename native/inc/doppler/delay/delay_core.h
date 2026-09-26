/**
 * @file delay_core.h
 * @brief Delay component API.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * Example:
 * @code
 * dp_delay_state_t *obj = dp_delay_create();
 * float _Complex y = delay_step(obj, 0.0f + 0.0f * I);
 * dp_delay_destroy(obj);
 * @endcode
 */
#ifndef DP_DELAY_CORE_H
#define DP_DELAY_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/dp_state.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Delay state.
   *
   * Dual-buffer circular delay line.  The backing store is a contiguous
   * allocation of 2*capacity elements: the first half is the live ring;
   * the second half mirrors it so that any window of num_taps consecutive
   * samples is always contiguous in memory (no wrap-around copy needed).
   *
   * Allocate with dp_delay_create().
   */
  typedef struct
  {
    double _Complex *buf; /* 2*capacity elements; second half mirrors first */
    size_t head;          /* write pointer; decrements mod capacity */
    size_t mask;          /* capacity - 1 (power-of-two bitmask) */
    size_t num_taps;      /* window length requested at construction */
    size_t capacity;      /* smallest power-of-two >= num_taps */
  } dp_delay_state_t;

  /**
   * @brief Create a dual-buffer circular delay line of length num_taps.
   * The internal capacity is rounded up to the next power of two so that
   * modular indexing reduces to a single bitwise AND.  Any window of
   * num_taps consecutive samples is always contiguous in the backing
   * store; no wrap-around copy is ever needed.
   *
   * @param num_taps  Number of delay taps (window length, >= 1).
   *                  Internally rounded up to the next power of two.
   * @return Heap-allocated state, or NULL on allocation failure.
   * @code
   * >>> from doppler.delay import DelayCf64
   * >>> d = DelayCf64(num_taps=3)
   * >>> d.num_taps
   * 3
   * >>> d.capacity   # next power-of-two >= 3
   * 4
   * @endcode
   */
dp_delay_state_t *dp_delay_create(size_t num_taps);

  /**
   * @brief Destroy a delay instance and release all memory.
   * Frees the internal dual buffer and the state struct itself.
   * Safe to call with a NULL pointer (no-op).  After this call the
   * pointer must not be used; the Python binding raises RuntimeError on
   * any subsequent method call.
   *
   * @param state  Heap-allocated delay state, or NULL.
   * @code
   * >>> from doppler.delay import DelayCf64
   * >>> d = DelayCf64(num_taps=2)
   * >>> d.push(1+0j)
   * >>> d.destroy()
   * >>> try:
   * ...     d.push(2+0j)
   * ... except RuntimeError as e:
   * ...     print(e)
   * destroyed
   * @endcode
   */
void dp_delay_destroy(dp_delay_state_t *state);

  /**
   * @brief Reset the delay line to its post-create state.
   * Zeroes the entire dual buffer and resets the write pointer to 0,
   * discarding all previously pushed samples.  The num_taps and capacity
   * are preserved; only the sample history is cleared.
   *
   * @param state  Must be non-NULL.
   * @code
   * >>> from doppler.delay import DelayCf64
   * >>> d = DelayCf64(num_taps=3)
   * >>> d.push(1+2j)
   * >>> d.push(3+4j)
   * >>> d.ptr().tolist()
   * [(3+4j), (1+2j), 0j]
   * >>> d.reset()
   * >>> d.ptr().tolist()
   * [0j, 0j, 0j]
   * @endcode
   */
void dp_delay_reset(dp_delay_state_t *state);

  /**
   * @brief Advance the write pointer and insert a new sample.
   * The head pointer decrements (mod capacity) before the write so that
   * `buf[head]` always holds the most recent sample.  The same value is
   * simultaneously written at `buf[head + capacity]` to keep the mirror
   * half in sync; this ensures any num_taps-length window starting at
   * head is contiguous without an extra copy.
   *
   * @param state  Must be non-NULL.
   * @param x      New complex sample to insert.
   * @code
   * >>> from doppler.delay import DelayCf64
   * >>> d = DelayCf64(num_taps=3)
   * >>> d.push(1+2j)
   * >>> d.push(3+4j)
   * >>> d.ptr().tolist()
   * [(3+4j), (1+2j), 0j]
   * @endcode
   */
void dp_delay_push(dp_delay_state_t *state, double _Complex x);

  /**
   * @brief Maximum samples dp_delay_ptr() writes for a request of n.
   * Returns min(n, num_taps) — the tight per-call bound (gh-607).
   *
   * @param state  Must be non-NULL.
   * @param n      Number of samples the matching dp_delay_ptr() call requests.
   * @return       min(n, num_taps).
   */
size_t dp_delay_ptr_max_out(dp_delay_state_t *state, size_t n);

  /**
   * @brief Snapshot the n most recent samples.
   * Copies at most min(n, num_taps) samples starting from `buf[head]` into
   * out.  Because the dual-buffer layout guarantees contiguity, this is a
   * single memcpy of up to num_taps elements; no wrap-around logic is
   * needed.  The Python binding returns an independent NumPy array per
   * call, so an earlier snapshot is never overwritten by a later one; pass
   * `out=` to fill a caller-owned buffer instead of allocating.
   *
   * @param state  Must be non-NULL.
   * @param n      Number of samples to copy; clamped to num_taps.
   * @param out    Output buffer; must hold at least max_out elements.
   * @param max_out Capacity of @p out in samples.  Normally num_taps (what
   *               dp_delay_ptr_max_out() reports); a smaller value truncates
   *               the snapshot instead of overrunning the buffer.
   * @return       min(n, num_taps, max_out) samples.
   * @code
   * >>> from doppler.delay import DelayCf64
   * >>> d = DelayCf64(num_taps=3)
   * >>> d.push(1+0j)
   * >>> d.push(2+0j)
   * >>> y = d.ptr()
   * >>> y.tolist()
   * [(2+0j), (1+0j), 0j]
   * >>> y.dtype
   * dtype('complex128')
   * >>> y.shape
   * (3,)
   * @endcode
   */
size_t dp_delay_ptr(dp_delay_state_t *state, size_t n, double _Complex *out, size_t max_out);

  /**
   * @brief Return the maximum output capacity for dp_delay_push_ptr().
   * Returns num_taps; the Python binding sizes each call's output array
   * with it, and checks a caller's `out=` buffer against it.
   *
   * @param state  Must be non-NULL.
   * @return       num_taps (number of samples dp_delay_push_ptr() will write).
   */
size_t dp_delay_push_ptr_max_out(dp_delay_state_t *state);

  /**
   * @brief Atomically push a sample and snapshot the current window.
   * Equivalent to calling dp_delay_push() then dp_delay_ptr(num_taps), but
   * avoids the overhead of a second function call.  Always writes exactly
   * num_taps samples to out.  The Python binding returns an independent
   * NumPy array per call; pass `out=` to reuse one buffer across pushes.
   *
   * @param state  Must be non-NULL.
   * @param x      New complex sample to insert.
   * @param out    Output buffer; must hold at least max_out elements.
   * @param max_out Capacity of @p out in samples.  Normally num_taps.  The
   *               push happens either way -- the ring is a running window
   *               and cannot be left un-advanced -- but a smaller capacity
   *               truncates the snapshot that is handed back.
   * @return       min(num_taps, max_out) samples.
   * @code
   * >>> from doppler.delay import DelayCf64
   * >>> d = DelayCf64(num_taps=3)
   * >>> d.push_ptr(1+0j).tolist()
   * [(1+0j), 0j, 0j]
   * >>> d.push_ptr(2+0j).tolist()
   * [(2+0j), (1+0j), 0j]
   * @endcode
   */
size_t dp_delay_push_ptr(dp_delay_state_t *state, double _Complex x,
                      double _Complex *out, size_t max_out);

  /**
   * @brief Alias for dp_delay_push(); insert a sample without reading back.
   * Provided for API symmetry with write-then-read patterns where the
   * caller wants to decouple sample ingestion from window inspection.
   * Internally delegates to dp_delay_push() with no additional overhead.
   *
   * @param state  Must be non-NULL.
   * @param x      New complex sample to insert.
   * @code
   * >>> from doppler.delay import DelayCf64
   * >>> d = DelayCf64(num_taps=2)
   * >>> d.write(5+6j)
   * >>> d.ptr().tolist()
   * [(5+6j), 0j]
   * @endcode
   */
void dp_delay_write(dp_delay_state_t *state, double _Complex x);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Field-wise: pack running ring buffer + head; capacity/mask/num_taps restored by create. */
#define DELAY_STATE_MAGIC DP_FOURCC ('D','L','A','Y')
#define DELAY_STATE_VERSION 1u
size_t dp_delay_state_bytes (const dp_delay_state_t *state);
void dp_delay_get_state (const dp_delay_state_t *state, void *blob);
int dp_delay_set_state (dp_delay_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DELAY_CORE_H */
