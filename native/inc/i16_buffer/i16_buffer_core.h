/**
 * @file i16_buffer_core.h
 * @brief The int16 I/Q pair ring as the component just-makeit binds.
 *
 * The ring itself is `dp_i16_*` in buffer/buffer.h, header-only and
 * macro-stamped. This header is what makes it a jm component without
 * changing it:
 *
 *   - `i16_buffer_state_t` IS `dp_i16_t`, so the binding holds the real
 *     ring and calls the real functions -- nothing is wrapped.
 *   - #DECLARE_DP_BUFFER_VIEW stamps the element-typed face (one element
 *     per SAMPLE), which is the face a numpy array has.
 *   - The Doxygen below sits on DECLARATIONS. The macros supply the
 *     definitions, but a doc extractor reads text, not the preprocessor's
 *     output, so the per-width documentation -- and the Python examples the
 *     stub and `help()` both render -- has to be written where it can be
 *     seen. The `<obj>_get_<prop>` accessors are the one thing defined
 *     here: they are jm's naming, not the ring's.
 *
 * The two siblings (f32 / f64 / i16) are the same file over a different
 * element; a manifest template (just-makeit#1310) will say so once.
 */
#ifndef I16_BUFFER_CORE_H
#define I16_BUFFER_CORE_H

#include "clib_common.h"

#include "buffer/buffer.h"
#include "dp_interrupt_guard/dp_interrupt_guard_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief One q15 complex sample: the element the i16 ring's view hands back.
 *
 * numpy has no complex-integer dtype, so the Python face is a structured
 * array of this record -- 1-D, one element per sample, zero-copy over the
 * ring's interleaved int16 storage.
 */
typedef struct
{
  int16_t i; /**< In-phase component. */
  int16_t q; /**< Quadrature component. */
} dp_iq16_t;

/** @brief The component's state IS the ring. */
typedef dp_i16_t i16_buffer_state_t;

/**
 * @brief Lock-free SPSC ring buffer for interleaved int16 IQ pairs.
 *
 * Stores raw 16-bit integer I/Q samples as they arrive from SDR
 * hardware (e.g. RTL-SDR, HackRF) before conversion to floating
 * point.  Uses the same virtual-memory double-mapping as
 * :class:`F32Buffer` to give zero-copy, branchless access across the
 * wrap boundary.
 *
 * numpy has no complex-integer dtype, so one sample is a RECORD:
 * ``[("i", "<i2"), ("q", "<i2")]``. Both faces speak it -- :meth:`write`
 * takes a 1-D array of it and :meth:`wait` lends one -- so the ring is
 * 1-D with one element per sample, exactly like its float siblings. The
 * storage underneath is still interleaved int16 (I, Q, I, Q, ...), so
 * ``flat.view(IQ16)`` and ``view.view(np.int16)`` convert either way with
 * no copy.
 *
 * A record rather than a packed ``int32`` on purpose: both are one element
 * per sample, but ``packed + 1`` carries across the I/Q boundary and
 * increments I only, silently. A record refuses arithmetic instead.
 *
 * @param capacity How many samples the ring holds: any size from 1 up, and
 *                 :attr:`capacity` is exactly this number on every machine.
 *                 What is rounded is the MAPPING behind it -- up to a
 *                 power of two, because indexing is a mask, and up to a
 *                 whole page -- so a capacity that is not a power of two
 *                 costs some address space (under 2x) and nothing per
 *                 call.
 *
 * @code
 * >>> from doppler.buffer import I16Buffer
 * >>> import numpy as np
 * >>> buf = I16Buffer(1024)
 * >>> buf.capacity
 * 1024
 * >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
 * >>> adc = np.array([10, 20, 30, 40], dtype=np.int16)   # I, Q, I, Q
 * >>> buf.write(adc.view(IQ16))
 * True
 * >>> buf.wait(2)["q"].tolist()
 * [20, 40]
 * @endcode
 */
static inline dp_i16_t *dp_i16_create (size_t capacity);

/**
 * @brief Write IQ samples into the buffer without blocking.
 *
 * Copies the record array into the ring in a single ``memcpy``. With
 * fewer than ``len(x)`` free slots the call is **refused entirely** --
 * nothing copied, ``x`` untouched -- and :attr:`dropped` grows by
 * ``len(x)``, which is not a loss count: you still hold every sample.
 *
 * A bare int16 array is refused with ``TypeError``, flat or ``(n, 2)``:
 * it is not an array of samples. ``flat.view(IQ16)`` makes it one, with
 * no copy.
 *
 * @param state The ring. Must be non-NULL.
 * @param x IQ samples to write: 1-D, C-contiguous, dtype
 *          ``[("i", "<i2"), ("q", "<i2")]``.
 * @param x_len Length of ``x``, in samples.
 *
 * @return ``True`` if all samples were written; ``False`` if the ring had no
 *         room and the call was refused (``x`` untouched).
 *
 * @code
 * >>> from doppler.buffer import I16Buffer
 * >>> import numpy as np
 * >>> buf = I16Buffer(1024)
 * >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
 * >>> buf.write(np.array([10, 20, 30, 40], dtype=np.int16).view(IQ16))
 * True
 * >>> buf2 = I16Buffer(1024)
 * >>> buf2.write(np.zeros(1024, dtype=IQ16))
 * True
 * >>> buf2.write(np.zeros(1, dtype=IQ16))
 * False
 * @endcode
 */
static inline bool
dp_i16_write_view (dp_i16_t *state, const dp_iq16_t *x, size_t x_len);

/**
 * @brief Write as much of ``x`` as fits and say how much that was.
 *
 * The partial-write twin of :meth:`write`. Where :meth:`write`
 * refuses a block that does not fit whole, this takes the leading
 * samples that do and returns their count -- ``0`` when the ring
 * is full. It never refuses, so it never touches
 * :attr:`dropped`. It is the only way to feed a chunk larger than
 * the ring: loop, advancing by the return value, draining in
 * between.
 *
 * @param state The ring. Must be non-NULL.
 * @param x Samples to write: 1-D, C-contiguous, dtype
 *          ``[("i", "<i2"), ("q", "<i2")]``.
 * @param x_len Length of ``x``, in samples.
 *
 * @return Samples accepted, ``0 <= k <= len(x)``.  The caller still owns
 *         ``x[k:]``.
 *
 * @code
 * A chunk three times the size of the ring, fed by looping:
 *
 * >>> from doppler.buffer import I16Buffer
 * >>> import numpy as np
 * >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
 * >>> buf = I16Buffer(1024)
 * >>> cap = buf.capacity
 * >>> chunk = np.ones(3 * cap, dtype=IQ16)
 * >>> fed = 0
 * >>> while fed < len(chunk):
 * ...     fed += buf.write_some(chunk[fed:])
 * ...     _ = buf.peek(buf.available); buf.consume()
 * >>> fed == 3 * cap, buf.dropped
 * (True, 0)
 * @endcode
 */
static inline size_t
dp_i16_write_some_view (dp_i16_t *state, const dp_iq16_t *x,
                        size_t x_len);

/**
 * @brief Block until ``n`` samples are available, then lend a zero-copy view.
 *
 * Spins with the GIL released until the producer has written at
 * least ``n`` samples.  Returns a 1-D record array directly into the
 * double-mapped ring: ``view["i"]`` is the I channel, ``view["q"]`` the
 * Q channel, each a strided int16 view with no copy.  Caller must call
 * :meth:`consume` before the next ``wait``.
 *
 * @param state The ring. Must be non-NULL.
 * @param n Number of IQ sample pairs to wait for.
 *
 * @return Zero-copy view of the next ``n`` samples, one record each.
 *
 * @throws EOFError The producer called :meth:`close` and fewer than ``n``
 *                  samples remain.  The tail is drained and no more is coming,
 *                  so the wait ends rather than blocking forever.
 * @throws KeyboardInterrupt Somebody asked this process to stop, through a
 *         :class:`doppler.interrupt.Interrupt` guard -- from any module:
 *         the flag is process-wide. Without a guard the spin checks for
 *         no signals at all.
 *
 * @code
 * >>> from doppler.buffer import I16Buffer
 * >>> import numpy as np
 * >>> buf = I16Buffer(1024)
 * >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
 * >>> buf.write(np.array([10, 20, 30, 40], dtype=np.int16).view(IQ16))
 * True
 * >>> view = buf.wait(2)
 * >>> view.dtype
 * dtype([('i', '<i2'), ('q', '<i2')])
 * >>> view.shape
 * (2,)
 * >>> view.tolist()
 * [(10, 20), (30, 40)]
 * >>> buf.consume()
 * @endcode
 */
static inline dp_iq16_t *dp_i16_wait_view (dp_i16_t *state, size_t n);

/**
 * @brief :meth:`wait` that never blocks: a view, or None for not yet.
 *
 * The single-threaded consumer's read. :meth:`wait` spins until a
 * producer on *another* thread delivers, so a caller that is its
 * own producer would deadlock in it; ``peek`` answers at once
 * instead. When ``n`` samples are buffered it returns the same
 * zero-copy, always-contiguous view :meth:`wait` would
 * (1-D, one ``(i, q)`` record per sample); otherwise it returns ``None``.
 *
 * ``None`` means **not yet** and nothing else. The two conditions
 * no amount of waiting can cure are raised, exactly as
 * :meth:`wait` raises them, so a poll loop cannot mistake either
 * for a slow producer.
 *
 * Peeking does not consume. Follow it with :meth:`consume`; a
 * ``consume(k)`` with ``k < n`` advances by a hop smaller than
 * the frame, which is how overlapped frames are read.
 *
 * @param state The ring. Must be non-NULL.
 * @param n Number of samples wanted.  Must be positive and not larger than
 *          :attr:`capacity`.
 *
 * @return Zero-copy view of the next ``n`` samples, or ``None`` when fewer
 *         than ``n`` have been written so far.
 *
 * @throws EOFError The ring is closed and fewer than ``n`` samples remain: the
 *                  rest is never coming.
 * @throws ValueError ``n`` exceeds :attr:`capacity` (or is not positive), so
 *                    no producer could ever satisfy it.
 *
 * @code
 * >>> from doppler.buffer import I16Buffer
 * >>> import numpy as np
 * >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
 * >>> buf = I16Buffer(1024)
 * >>> buf.peek(4) is None
 * True
 * >>> buf.write_some(np.ones(8, dtype=IQ16))
 * 8
 * >>> buf.peek(4).shape
 * (4,)
 * >>> buf.consume(2)
 * >>> buf.available
 * 6
 * >>> buf.close()
 * >>> buf.peek(8)
 * Traceback (most recent call last):
 *     ...
 * EOFError: end of stream: the producer closed the ring
 * @endcode
 */
static inline dp_iq16_t *dp_i16_peek_view (dp_i16_t *state, size_t n);

/**
 * @brief Release ``n`` samples back to the producer.
 *
 * Advances the consumer tail pointer by ``n``, making that space
 * available for the producer to overwrite, and ends the loan: the view
 * a :meth:`wait` or :meth:`peek` lent must not be used afterwards.  If
 * ``n`` is omitted it is the count of that outstanding view, so the
 * number is written once.  ``n`` smaller than the view is how overlapped
 * frames are read: release a hop, keep the rest.
 *
 * @param state The ring. Must be non-NULL.
 * @param n Number of samples to release.  Defaults to the count of the
 *          outstanding :meth:`wait` / :meth:`peek` view.
 *
 * @return DP_OK, or DP_ERR_INVALID when ``n`` exceeds what is readable.
 *
 * @throws ValueError ``n`` exceeds :attr:`available`. Nothing is released:
 *         past that point the ring's counts would stop describing it.
 * @throws RuntimeError ``n`` was omitted and nothing is outstanding -- no view
 *         was lent since the last release, so there is no count to default
 *         to.
 *
 * @code
 * >>> from doppler.buffer import I16Buffer
 * >>> import numpy as np
 * >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
 * >>> buf = I16Buffer(1024)
 * >>> buf.write(np.array([1, 2, 3, 4], dtype=np.int16).view(IQ16))
 * True
 * >>> _ = buf.wait(2)
 * >>> buf.consume()
 * @endcode
 */
static inline int dp_i16_consume (dp_i16_t *state, size_t n);

/**
 * @brief Say that no more data is coming.
 *
 * The producer's half of end of stream.  Until this exists a
 * consumer cannot tell a slow producer from a finished one --
 * both look like an empty ring -- so :meth:`wait` had nothing to
 * do but spin.  Call it once, after the last write.
 *
 * Release ordering: every sample written before this is visible
 * to a consumer that observes the flag.  Closing does not discard
 * what was already written; :meth:`wait` keeps returning batches
 * until the ring is drained, and only then raises ``EOFError``.
 *
 * See ``docs/design/io-termination.md`` for the one termination
 * contract shared with the network and disk transports.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.buffer import I16Buffer
 * >>> buf = I16Buffer(1024)
 * >>> buf.close()
 * >>> buf.closed
 * True
 * >>> buf.wait(4)
 * Traceback (most recent call last):
 *     ...
 * EOFError: end of stream: the producer closed the ring
 * @endcode
 */
static inline void dp_i16_close (dp_i16_t *state);

/**
 * @brief Empty the ring and reopen it.
 *
 * Discards everything buffered, and clears :attr:`closed` so the
 * same ring can carry a second stream -- without it, reuse after
 * :meth:`close` means destroying and re-mapping.  :attr:`dropped`
 * is a lifetime count and is kept.
 *
 * Not safe against a concurrent producer or consumer: it moves
 * both ends of the ring.  Call it only when both sides are idle.
 *
 * @code
 * >>> from doppler.buffer import I16Buffer
 * >>> import numpy as np
 * >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
 * >>> buf = I16Buffer(1024)
 * >>> buf.write_some(np.ones(8, dtype=IQ16))
 * 8
 * >>> buf.close()
 * >>> buf.reset()
 * >>> buf.available, buf.closed
 * (0, False)
 * @endcode
 */
static inline void dp_i16_reset (dp_i16_t *state);

/**
 * @brief Unmap the buffer and free the underlying struct.
 *
 * Releases both virtual-address views and frees the C struct.
 * Safe to call more than once; subsequent calls are no-ops.
 *
 * @code
 * >>> from doppler.buffer import I16Buffer
 * >>> buf = I16Buffer(1024)
 * >>> buf.destroy()
 * @endcode
 */
static inline void dp_i16_destroy (dp_i16_t *state);

/**
 * @brief Buffer capacity in IQ sample pairs.
 *
 * Read-only.  Exactly the number passed to the constructor, whatever
 * the machine's page size; the mapping behind it is larger when that
 * number is not a power of two or spans less than a page, and that
 * slack is never room.
 *
 * @code
 * >>> from doppler.buffer import I16Buffer
 * >>> I16Buffer(1024).capacity, I16Buffer(1000).capacity
 * (1024, 1000)
 * @endcode
 */
static inline size_t
i16_buffer_get_capacity (const i16_buffer_state_t *state)
{
  return state->capacity;
}

/**
 * @brief Samples written but not yet consumed.
 *
 * The largest ``n`` for which :meth:`wait` is guaranteed to return
 * without spinning.  Read this rather than tracking the count
 * yourself: :meth:`wait` has no timeout and no short return, so
 * asking for more than has been written spins until the producer
 * catches up -- forever, if there is no producer.
 *
 * Read from the consumer side this is a *lower* bound.  A producer
 * on another thread can only increase it, so a block sized from it
 * is always safe; it may simply be smaller than what has landed by
 * the time :meth:`wait` runs.
 *
 * @code
 * >>> from doppler.buffer import I16Buffer
 * >>> import numpy as np
 * >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
 * >>> buf = I16Buffer(1024)
 * >>> buf.available
 * 0
 * >>> _ = buf.write(np.zeros(100, dtype=IQ16))
 * >>> buf.available
 * 100
 * >>> _ = buf.wait(64); buf.consume(64)
 * >>> buf.available
 * 36
 * @endcode
 */
static inline size_t
i16_buffer_get_available (const i16_buffer_state_t *state)
{
  return dp_i16_available (state);
}

/**
 * @brief Free room in samples: the largest :meth:`write` sure to fit.
 *
 * ``capacity - available``, read in one place so callers stop
 * deriving it.  Read from the producer side it is a *lower*
 * bound: a consumer on another thread can only increase it, so a
 * block sized from it is always accepted.
 *
 * @code
 * >>> from doppler.buffer import I16Buffer
 * >>> import numpy as np
 * >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
 * >>> buf = I16Buffer(1024)
 * >>> buf.space == buf.capacity
 * True
 * >>> buf.write_some(np.ones(8, dtype=IQ16))
 * 8
 * >>> buf.capacity - buf.space
 * 8
 * @endcode
 */
static inline size_t
i16_buffer_get_space (const i16_buffer_state_t *state)
{
  return dp_i16_space (state);
}

/**
 * @brief Cumulative IQ sample pairs in REFUSED writes -- not pairs lost.
 *
 * **Not a count of lost data.** :meth:`write` is all-or-nothing:
 * with no room it copies nothing, leaves the caller's array
 * untouched and refuses the call -- and this counter is then
 * incremented by the length of that refused call, not by 1 and not
 * by anything actually lost.
 *
 * So a producer that spins on :meth:`write` until it succeeds, the
 * obvious way to apply backpressure, inflates this while losing
 * nothing: a 60,000-pair run written that way reported 5,960,438.
 * Samples are lost only when the caller *discards* them, which is
 * what ignoring the return value does. Wait for room if you want
 * this to mean what it sounds like.
 *
 * @code
 * >>> from doppler.buffer import I16Buffer
 * >>> import numpy as np
 * >>> IQ16 = np.dtype([("i", "<i2"), ("q", "<i2")])
 * >>> buf = I16Buffer(1024)
 * >>> buf.dropped
 * 0
 * >>> buf.write(np.zeros(1024, dtype=IQ16))
 * True
 * >>> buf.write(np.zeros(3, dtype=IQ16))
 * False
 * >>> buf.dropped
 * 1
 * @endcode
 */
static inline size_t
i16_buffer_get_dropped (const i16_buffer_state_t *state)
{
  return state->dropped;
}

/**
 * @brief ``True`` once the producer has called :meth:`close`.
 *
 * The consumer's half of end of stream: it distinguishes "the
 * producer is slow" from "the producer has finished", which an
 * empty ring alone cannot.
 *
 * @code
 * >>> from doppler.buffer import I16Buffer
 * >>> buf = I16Buffer(1024)
 * >>> buf.closed
 * False
 * >>> buf.close()
 * >>> buf.closed
 * True
 * @endcode
 */
static inline bool
i16_buffer_get_closed (const i16_buffer_state_t *state)
{
  return dp_i16_closed (state);
}

DECLARE_DP_BUFFER_VIEW (i16, int16_t, dp_iq16_t)

#ifdef __cplusplus
}
#endif

#endif /* I16_BUFFER_CORE_H */
