/**
 * @file delay_core.h
 * @brief Delay component API.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * Example:
 * @code
 * delay_state_t *obj = delay_create();
 * float complex y = delay_step(obj, 0.0f + 0.0f * I);
 * delay_destroy(obj);
 * @endcode
 */
#ifndef DELAY_CORE_H
#define DELAY_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
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
   * Allocate with delay_create().
   */
  typedef struct
  {
    double _Complex *buf; /* 2*capacity elements; second half mirrors first */
    size_t head;          /* write pointer; decrements mod capacity */
    size_t mask;          /* capacity - 1 (power-of-two bitmask) */
    size_t num_taps;      /* window length requested at construction */
    size_t capacity;      /* smallest power-of-two >= num_taps */
  } delay_state_t;

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
delay_state_t *delay_create(size_t num_taps);

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
void delay_destroy(delay_state_t *state);

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
void delay_reset(delay_state_t *state);

  /**
   * @brief Advance the write pointer and insert a new sample.
   * The head pointer decrements (mod capacity) before the write so that
   * buf[head] always holds the most recent sample.  The same value is
   * simultaneously written at buf[head + capacity] to keep the mirror
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
void delay_push(delay_state_t *state, double complex x);

  /**
   * @brief Return the maximum output capacity for delay_ptr().
   * Returns num_taps; the Python binding uses this to pre-allocate the
   * output buffer before calling delay_ptr().
   *
   * @param state  Must be non-NULL.
   * @return       num_taps (maximum samples delay_ptr() can write).
   */
size_t delay_ptr_max_out(delay_state_t *state);

  /**
   * @brief Return a zero-copy view of the n most recent samples.
   * Copies at most min(n, num_taps) samples starting from buf[head] into
   * out.  Because the dual-buffer layout guarantees contiguity, this is a
   * single memcpy of up to num_taps elements; no wrap-around logic is
   * needed.  The Python binding returns a NumPy array backed directly by
   * the pre-allocated output buffer (base object is the DelayCf64 itself).
   *
   * @param state  Must be non-NULL.
   * @param n      Number of samples to copy; clamped to num_taps.
   * @param out    Output buffer; must hold at least min(n, num_taps) elements.
   * @return       Number of samples written.
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
size_t delay_ptr(delay_state_t *state, size_t n, double complex *out);

  /**
   * @brief Return the maximum output capacity for delay_push_ptr().
   * Returns num_taps; the Python binding uses this to pre-allocate the
   * output buffer before calling delay_push_ptr().
   *
   * @param state  Must be non-NULL.
   * @return       num_taps (number of samples delay_push_ptr() will write).
   */
size_t delay_push_ptr_max_out(delay_state_t *state);

  /**
   * @brief Atomically push a sample and snapshot the current window.
   * Equivalent to calling delay_push() then delay_ptr(num_taps), but
   * avoids the overhead of a second function call.  Always writes exactly
   * num_taps samples to out.  The Python binding returns a NumPy array
   * backed by the pre-allocated push_ptr output buffer.
   *
   * @param state  Must be non-NULL.
   * @param x      New complex sample to insert.
   * @param out    Output buffer; must hold at least num_taps elements.
   * @return       num_taps (always equal to the window length).
   * @code
   * >>> from doppler.delay import DelayCf64
   * >>> d = DelayCf64(num_taps=3)
   * >>> d.push_ptr(1+0j).tolist()
   * [(1+0j), 0j, 0j]
   * >>> d.push_ptr(2+0j).tolist()
   * [(2+0j), (1+0j), 0j]
   * @endcode
   */
  size_t delay_push_ptr (delay_state_t *state, double complex x,
                         double complex *out);

  /**
   * @brief Alias for delay_push(); insert a sample without reading back.
   * Provided for API symmetry with write-then-read patterns where the
   * caller wants to decouple sample ingestion from window inspection.
   * Internally delegates to delay_push() with no additional overhead.
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
void delay_write(delay_state_t *state, double complex x);

size_t delay_push_ptr(delay_state_t *state, double complex x, double complex *out);
#ifdef __cplusplus
}
#endif

#endif /* DELAY_CORE_H */
