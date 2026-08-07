/**
 * @file dp_tlm_core.h
 * @brief Lightweight scalar telemetry taps for running DSP objects.
 *
 * A `dp_tlm_t` context lets a hot loop publish named scalar time series
 * (tracking-loop stress, AGC gain, lock metrics, ...) without perturbing the
 * signal path:
 *
 *   - **Detached (the default)**: an instrumented object holds a NULL
 *     `dp_tlm_t *`; every probe site is a single pointer load and a
 *     predicted-not-taken branch, and only at *event* rate (per recovered
 *     symbol, per gain update) — never per input sample.  Consumers who want
 *     literal zero can compile with `-DDP_TLM_DISABLE`, which turns the
 *     `DP_TLM()` probe macro into `((void) 0)`.
 *   - **Attached**: each emit is a per-probe decimation check plus one
 *     16-byte record written into a lock-free VM-mirrored SPSC ring
 *     (buffer/buffer.h).  The write never blocks and never allocates; on
 *     overrun the record is dropped and counted, so a slow (or absent)
 *     reader can never stall the DSP thread.
 *
 * @section tlm_lossless Drops are preventable, not merely countable
 * Dropping is the ring's *fallback*, not the intended steady state.  Because
 * no probe can emit more than once per input sample, a block of @c N inputs
 * emits at most `dp_tlm_probe_count() * N` records — see dp_tlm_block_bound().
 * A ring sized to that bound and drained to empty at every block boundary
 * therefore *cannot* overflow, which is what dp_tlm_capture_open()
 * (dp_tlm_capture/dp_tlm_capture_core.h) sets up for you.  Prefer a capture
 * to a hand-rolled drain loop: guessing a ring size and hoping the reader keeps up
 * is the failure mode this bound exists to retire.
 *
 * @section tlm_threading Threading contract
 * The ring is single-producer / single-consumer:
 *
 *   - All objects attached to one context must step on ONE producer thread
 *     (true of any doppler pipeline).  Use one context per pipeline/thread.
 *   - `dp_tlm_read()` may run concurrently on one consumer thread — that
 *     hand-off is the ring's whole design.
 *   - Probe registration (`dp_tlm_probe`, i.e. `obj_set_telemetry`) must
 *     complete before the producer starts stepping: the probe table is
 *     written unlocked at setup time.
 *
 * @section tlm_time Timestamps
 * Records carry a caller-maintained sample index `now` (stamp it once per
 * block from the pipeline's `dp_sample_clock_t` via `dp_tlm_set_now`).  If
 * never stamped it stays 0 and consumers index by record order — fine for
 * per-symbol series.
 *
 * @code
 *   dp_tlm_t *tlm = dp_tlm_create (1 << 14);
 *   int id = dp_tlm_probe (tlm, "agc.gain_db", 1);
 *   ...
 *   DP_TLM (tlm, id, gain_db);            // in the hot loop, per event
 *   ...
 *   dp_tlm_rec_t recs[512];
 *   size_t n = dp_tlm_read (tlm, 512, recs, 512);   // on the consumer side
 *   dp_tlm_destroy (tlm);
 * @endcode
 */

#ifndef DP_TELEMETRY_H
#define DP_TELEMETRY_H

#include "buffer/buffer.h"
#include "clib_common.h" /* DP_OK, DP_ERR_INVALID */
#include "jm_perf.h"      /* JM_FORCEINLINE */

/* 16-byte ring slots: sizeof(uint64_t)*2 per "complex sample" — exactly one
 * telemetry record each, buying the VM-mirrored contiguity, acquire/release
 * correctness and the dropped counter for free. */
DECLARE_DP_BUFFER (tlmr, uint64_t)

/**
 * @brief One telemetry sample: a probe's scalar value at sample index @c n.
 *
 * 16 bytes, 8-aligned — one ring slot.  @c value is float: ~7 significant
 * digits is ample for diagnostics (timing error, dB gains, lock metrics);
 * @c flags reserves room for a future wide-value record class.
 */
typedef struct
{
  uint64_t n;     /**< Caller-stamped sample index (dp_tlm_set_now). */
  float    value; /**< The scalar, narrowed to float.                */
  uint16_t probe; /**< Probe id (index into the context's table).    */
  uint16_t flags; /**< Reserved; 0.                                  */
} dp_tlm_rec_t;

/* One record must fill exactly one ring slot (C99-portable assert). */
typedef char dp_tlm_rec_fits_slot[sizeof (dp_tlm_rec_t)
                                          == 2 * sizeof (uint64_t)
                                      ? 1
                                      : -1];

/** Maximum probes per context.  Registration fails once full. */
#define DP_TLM_MAX_PROBES 64
/** Maximum probe-name length including the NUL terminator. */
#define DP_TLM_NAME_MAX 32

/**
 * @brief Per-probe registry entry: name, decimation and accounting.
 *
 * @c phase counts events between emits and is producer-owned (hot path);
 * @c emitted counts records actually written (post-decimation, post-drop),
 * so a consumer can reconcile losses against the ring's dropped counter.
 */
typedef struct
{
  char     name[DP_TLM_NAME_MAX]; /**< e.g. "agc.gain_db".              */
  uint32_t decim;                 /**< Emit every decim-th event, >= 1. */
  uint32_t phase;                 /**< Producer-owned event counter.    */
  uint64_t emitted;               /**< Records written into the ring.   */
} dp_tlm_probe_t;

/** Opaque lossless capture (dp_tlm_capture_core.h); see dp_tlm_set_now. */
typedef struct dp_tlm_capture dp_tlm_capture_t;

/**
 * @brief Telemetry context: probe registry + SPSC record ring.
 *
 * Public (not opaque) because the emit path is inline; treat the fields as
 * read-only outside dp_tlm_core.c and dp_tlm_emit.
 *
 * @c capture is deliberately LAST: the emit hot path touches @c ring, @c now
 * and @c probes, and appending here leaves their cache layout untouched.
 */
typedef struct dp_tlm
{
  dp_tlmr_t     *ring;    /**< Lock-free SPSC record ring.              */
  uint64_t       now;     /**< Caller-stamped sample index for records. */
  uint32_t       n_probes;
  dp_tlm_probe_t probes[DP_TLM_MAX_PROBES];
  /** Open capture that dp_tlm_set_now() drains through; NULL when none. */
  dp_tlm_capture_t *capture;
  /**
   * Boundary drain, registered by dp_tlm_capture_open().
   *
   * A function POINTER rather than a direct call, so this translation unit
   * never references a capture symbol: the inline dp_tlm_set_now() below is
   * pulled into every TU that includes this header, and calling
   * dp_tlm_capture_block() by name would make the capture a link-time
   * dependency of everything -- an inversion, since the capture depends on
   * the ring and not the other way round. NULL when no capture is open.
   */
  int (*capture_drain) (dp_tlm_capture_t *);
} dp_tlm_t;

/**
 * @brief jm's spelling of ::dp_tlm_t.
 *
 * jm derives an object's state struct as `<component>_state_t` with no
 * override (just-makeit#797), and this type predates jm by years — it is in
 * the signature of every instrumented object's `*_set_telemetry`, so renaming
 * it is not on the table. An alias costs one line and nothing at runtime.
 *
 * Not a second type: `dp_tlm_t` remains the name to write. This exists so the
 * generated binding compiles, and it goes away when jm#797 lands `state_type`.
 */
typedef dp_tlm_t dp_tlm_state_t;

/**
 * @brief Creates a telemetry context with a ring of @p ring_records slots.
 *
 * @param ring_records Requested ring capacity in records.  MUST be a power
 *                     of 2.  Sub-page requests are rounded up to the page
 *                     minimum (buffer.h semantics) — read the authoritative
 *                     value back with dp_tlm_capacity().
 * @return New context, or NULL on invalid size / allocation failure.
 */
dp_tlm_t *dp_tlm_create (size_t ring_records);

/** @brief Destroys a context.  NULL-safe.  Detach all objects first. */
void dp_tlm_destroy (dp_tlm_t *t);

/**
 * @brief Registers (or re-registers) a named probe.  Setup path, not hot.
 *
 * Idempotent by name: registering an existing name returns its id and
 * updates @p decim (re-attach after a reset keeps ids stable).  The
 * decimation phase is primed so the FIRST event after registration emits.
 *
 * @param t     Context.
 * @param name  Probe name, e.g. "agc.gain_db".  Must be shorter than
 *              DP_TLM_NAME_MAX.
 * @param decim Emit every decim-th event; >= 1.
 * @return Probe id (>= 0), or DP_ERR_INVALID on NULL/overlong name,
 *         decim == 0, or a full table.
 *
 * @code
 * >>> from doppler.telemetry import Telemetry
 * >>> tlm = Telemetry(1 << 12)
 * >>> tlm.probe("sync.e", decim=4)
 * 0
 * >>> tlm.probe("sync.e")     # same name: same id, decim retuned
 * 0
 * >>> tlm.probe_count
 * 1
 *
 * @endcode
 */
int dp_tlm_probe (dp_tlm_t *t, const char *name, uint32_t decim);

/**
 * @brief Looks up a probe id by name; ::DP_ERR_INVALID if unknown.
 *
 * @param t    Context.
 * @param name Probe name as passed to dp_tlm_probe().
 * @return Probe id (>= 0), or ::DP_ERR_INVALID if no such probe.
 *
 * @code
 * >>> from doppler.telemetry import Telemetry
 * >>> tlm = Telemetry(1 << 12)
 * >>> _ = tlm.probe("agc.gain_db")
 * >>> tlm.probe_id("agc.gain_db")
 * 0
 * >>> tlm.probe_id("never.registered")
 * Traceback (most recent call last):
 * KeyError: 'no probe by that name (rc=-4)'
 *
 * @endcode
 */
int dp_tlm_probe_id (const dp_tlm_t *t, const char *name);

/**
 * @brief Validating dp_tlm_emit(): refuses an id the registry never issued.
 *
 * The out-of-line twin of the inline hot-path emit, for callers whose id did
 * not come from dp_tlm_probe() on this context — in practice, a language
 * binding, where the id is whatever the caller passed.  dp_tlm_emit() checks
 * only the ARRAY bound (see its docs: checking @c n_probes there costs ~16% of
 * the decimated path), so an in-range but unregistered id reaches it and emits
 * a record against a probe nobody registered.  Here that is an error.
 *
 * C hot loops keep calling dp_tlm_emit() directly and pay nothing for this.
 *
 * @param t  Context.  NULL is rejected.
 * @param id Probe id from dp_tlm_probe() on THIS context.
 * @param v  The scalar, narrowed to float by the ring record.
 * @return ::DP_OK, or ::DP_ERR_INVALID on a NULL context or an id outside
 *         the registry.
 *
 * @code
 * >>> from doppler.telemetry import Telemetry
 * >>> tlm = Telemetry(1 << 12)
 * >>> pid = tlm.probe("rx.snr_db")
 * >>> tlm.emit(pid, 12.5)
 * >>> float(tlm.read()[0]["value"])
 * 12.5
 *
 * An id the registry never issued is refused, not written:
 *
 * >>> tlm.emit(pid + 1, 1.0)
 * Traceback (most recent call last):
 * ValueError: emit failed (rc=-4)
 *
 * @endcode
 */
int dp_tlm_emit_checked (dp_tlm_t *t, int32_t id, double v);

/**
 * @brief Retunes an EXISTING probe's decimation, by name.
 *
 * Distinct from dp_tlm_probe(), which registers on a miss: this refuses an
 * unknown name rather than quietly creating a probe nothing emits to, which
 * is what a typo in a retune call deserves.
 *
 * @param t     Context.
 * @param name  Name of an ALREADY registered probe.
 * @param decim Emit every decim-th event; >= 1.
 * @return ::DP_OK, or ::DP_ERR_INVALID on NULL, an unknown name, or
 *         @p decim == 0.
 *
 * @code
 * >>> from doppler.telemetry import Telemetry
 * >>> tlm = Telemetry(1 << 12)
 * >>> _ = tlm.probe("sync.e", decim=1)
 * >>> tlm.set_decim("sync.e", 8)      # retune the existing probe
 * >>> tlm.set_decim("typo.e", 8)      # refused, not silently created
 * Traceback (most recent call last):
 * ValueError: set_decim failed (rc=-4)
 *
 * @endcode
 */
int dp_tlm_set_decim (dp_tlm_t *t, const char *name, uint32_t decim);

/** @brief Probe name for @p id, or NULL if out of range. */
const char *dp_tlm_probe_name (const dp_tlm_t *t, int id);

/** @brief Number of registered probes. */
size_t dp_tlm_probe_count (const dp_tlm_t *t);

/** @brief Authoritative ring capacity in records (post page rounding). */
size_t dp_tlm_capacity (const dp_tlm_t *t);

/**
 * @brief Probe id at registry slot @p i.  Always @p i — ids ARE slots.
 *
 * Exists so a `{name: id}` mapping can be built from the plain-C triple
 * (dp_tlm_probe_count, dp_tlm_probe_name, this) without the caller needing
 * to know that the identity holds.
 */
int dp_tlm_probe_id_at (const dp_tlm_t *t, size_t i);

/**
 * @brief Records this context can emit while processing @p block_samples
 *        inputs — the number that makes drops preventable.
 *
 * `probe_count * block_samples`, and that is a genuine upper bound rather
 * than an estimate: **no probe can emit more than once per input sample.**
 * Verified across every object with a `*_set_telemetry` — the interpolating
 * ones are not counterexamples, because a cascade that produces several
 * outputs from one input collapses them into a single `emitted |=` strobe
 * (ratesync_core.h, mpsk_receiver_core.h), so one input yields at most one
 * flush.  Each probe belongs to exactly one object, so summing over objects
 * is just the context-wide probe count.
 *
 * Size a ring to this and drain it to empty every block and the ring cannot
 * overflow — no scheduling assumption, no safety factor.  Registering more
 * probes raises the bound, which is why a capture re-checks it at each
 * boundary.
 *
 * @return The bound, or 0 for a NULL context / zero block / no probes.
 *         Saturates at SIZE_MAX rather than wrapping.
 */
size_t dp_tlm_block_bound (const dp_tlm_t *t, size_t block_samples);

/**
 * @brief Records currently readable, without consuming them.
 *
 * The consumer-side head/tail snapshot.  Safe to call from the consumer
 * thread while the producer runs: the true count can only GROW after the
 * snapshot, so the value is a lower bound and never over-reports.
 */
size_t dp_tlm_avail (const dp_tlm_t *t);

/**
 * @brief Replaces the ring with one holding at least @p records.
 *
 * Rounds @p records up to a power of two (buffer.h requires it) and then to
 * the page minimum.  A no-op returning ::DP_OK when the ring is already big
 * enough, so it is cheap to call speculatively at every boundary.
 *
 * @warning **Destroys whatever the ring holds** and is unsynchronised with
 * the producer.  Legal only where the producer is quiescent AND the ring has
 * been drained — i.e. a block boundary.  dp_tlm_capture_block() is the only
 * caller that needs it; call it yourself only if you own the same guarantee.
 *
 * @return ::DP_OK, or ::DP_ERR_INVALID on NULL / allocation failure (in
 *         which case the existing ring is left intact).
 */
int dp_tlm_resize (dp_tlm_t *t, size_t records);

/**
 * @brief Context-wide counters, snapshotted together.
 *
 * A by-value record rather than a dict so the whole thing crosses a language
 * boundary as one value.  Per-probe detail is not in here on purpose: it is
 * dp_tlm_probe_name() + dp_tlm_emitted(), which stay the SSOT for it.
 */
typedef struct
{
  uint64_t dropped;  /**< Records lost to ring overrun (monotonic).      */
  uint64_t emitted;  /**< Records written, summed over every probe.      */
  size_t   capacity; /**< Ring capacity in records.                      */
  size_t   probes;   /**< Registered probes.                             */
} dp_tlm_stats_t;

/**
 * @brief Snapshots the context's counters.  Zeroed for a NULL context.
 *
 * @param t Context, or NULL for an all-zero record.
 * @return The four counters as one ::dp_tlm_stats_t value.
 *
 * @code
 * >>> from doppler.telemetry import Telemetry
 * >>> tlm = Telemetry(1 << 12)
 * >>> pid = tlm.probe("agc.gain_db")
 * >>> tlm.emit(pid, -3.5)
 * >>> tlm.stats()
 * doppler.telemetry.TelemetryStats(dropped=0, emitted=1, capacity=4096, probes=1)
 * >>> tlm.stats().emitted
 * 1
 *
 * @endcode
 */
dp_tlm_stats_t dp_tlm_stats (const dp_tlm_t *t);

/**
 * @brief Upper bound on what dp_tlm_read() can return right now.
 *
 * Simply the available count: a caller sizing a destination cannot know the
 * request will be smaller, and jm's generated binding allocates this much,
 * reads, then resizes to what actually came back.
 */
size_t dp_tlm_read_max_out (dp_tlm_t *t);

/**
 * @brief Drains records into @p out.  Non-blocking.
 *
 * Consumer side of the SPSC ring: safe to call from a different thread than
 * the producer.  Returns immediately with whatever is available (possibly 0)
 * — never spins.
 *
 * @param t        Context.
 * @param n        Records wanted; 0 means "everything available".
 * @param out      Destination.
 * @param max_out  Capacity of @p out, in records.
 *
 * @c n and @c max_out are separate because the binding allocates @p out from
 * dp_tlm_read_max_out() and then resizes to what came back — so the request
 * and the buffer are genuinely two numbers, and the read is clamped to the
 * smaller.
 *
 * @return Number of records copied out.
 *
 * @code
 * >>> from doppler.telemetry import Telemetry
 * >>> tlm = Telemetry(1 << 12)
 * >>> eid = tlm.probe("sync.e")
 * >>> for i in range(5):
 * ...     tlm.emit(eid, i / 10)
 * >>> recs = tlm.read(2)          # take two
 * >>> recs.shape, recs.dtype.names
 * ((2,), ('n', 'value', 'probe', 'flags'))
 * >>> tlm.read().shape            # 0 means "everything left"
 * (3,)
 * >>> tlm.read().shape            # drained
 * (0,)
 *
 * @endcode
 */
size_t dp_tlm_read (dp_tlm_t *t, size_t n, dp_tlm_rec_t *out,
                    size_t max_out);

/** @brief Total records dropped on ring overrun (monotonic). */
uint64_t dp_tlm_dropped (const dp_tlm_t *t);

/**
 * @brief Records written for probe @p id (post-decimation, post-drop).
 *
 * Reconcile against dp_tlm_dropped() to account for losses: what a probe
 * emitted is what reached the ring, not what the call sites offered it.
 *
 * @param t  Context.
 * @param id Probe id from dp_tlm_probe().
 * @return Records written for that probe, 0 for an unknown id.
 *
 * @code
 * >>> from doppler.telemetry import Telemetry
 * >>> tlm = Telemetry(1 << 12)
 * >>> eid = tlm.probe("sync.e", decim=2)
 * >>> for i in range(4):
 * ...     tlm.emit(eid, i / 10)
 * >>> tlm.emitted(eid)            # decim=2: half the events
 * 2
 * >>> tlm.dropped
 * 0
 *
 * @endcode
 */
uint64_t dp_tlm_emitted (const dp_tlm_t *t, int id);

/**
 * @brief Stamps the sample index carried by subsequent records, and — when a
 *        capture is open — closes out the block just finished.
 *
 * Call once per block from whoever owns the pipeline's sample clock
 * (`dp_tlm_set_now (tlm, clk->n)`).  NULL-safe so pipeline glue can call it
 * unconditionally.
 *
 * Callers already place this at the top of the block loop, *before* stepping,
 * which makes it exactly the boundary a lossless capture needs: delegating
 * here drains the PREVIOUS block, leaving the ring empty as the next one
 * starts.  That is the invariant dp_tlm_block_bound() is sized against, so an
 * existing `set_now / steps / read` loop becomes lossless by opening a
 * capture and changing nothing else.
 *
 * With no capture open the behaviour is byte-identical to a bare assignment.
 * The delegation is a cold branch on a per-block call, never a per-sample
 * one, so it is nowhere near the hot loops dp_tlm_emit() cares about.
 *
 * @param t Context; NULL is a no-op.
 * @param n Sample index stamped into every subsequent record.
 *
 * @code
 * >>> from doppler.telemetry import Telemetry
 * >>> tlm = Telemetry(1 << 12)
 * >>> pid = tlm.probe("agc.gain_db")
 * >>> tlm.set_now(1000)           # top of the block, before stepping
 * >>> tlm.emit(pid, -3.5)
 * >>> rec = tlm.read()[0]
 * >>> int(rec["n"]), float(rec["value"])
 * (1000, -3.5)
 *
 * @endcode
 */
static inline void
dp_tlm_set_now (dp_tlm_t *t, uint64_t n)
{
  if (!t)
    return;
  if (t->capture_drain)
    t->capture_drain (t->capture);
  t->now = n;
}

/**
 * @brief Records one scalar for probe @p id.  The hot-path primitive.
 *
 * Detached (@p t NULL) this is one branch — the entire disabled cost.
 * Attached: bump the probe's decimation phase, and on the decim-th event
 * write one 16-byte record (value narrowed to float, stamped with the
 * context's current @c now).  Never blocks, never allocates; on ring
 * overrun the record is dropped and counted.
 *
 * @p id must come from a successful dp_tlm_probe() on this context —
 * an object's set_telemetry fails the whole attach otherwise.
 *
 * The bound checked here is the ARRAY's, not the registry's.  @c probes is a
 * fixed DP_TLM_MAX_PROBES array, so the unguarded indexing this used to do
 * turned any out-of-range id into an out-of-bounds write — reachable from a
 * language binding, where the id is whatever the caller passed, and
 * `Telemetry.emit(1000000, 1.0)` segfaulted the interpreter.  Comparing
 * against the compile-time constant (unsigned, so a negative id fails it too)
 * needs no memory and measures free.  Comparing against @c n_probes instead
 * would also reject an in-range-but-unregistered id, but it loads a field on
 * the early-return path and cost ~16% of the decimated case
 * (bench_telemetry_core, ABBA-interleaved) — so *that* check belongs at the
 * binding boundary, where the id is untrusted, not in the hot loop, where the
 * caller holds an id dp_tlm_probe() gave it.
 *
 * @param t  Context; NULL is a no-op (the detached case).
 * @param id Probe id from dp_tlm_probe() on THIS context.
 * @param v  The scalar, narrowed to float by the ring record.
 *
 * The Python face binds dp_tlm_emit_checked() instead, which additionally
 * refuses an id the registry never issued — see its docs for why the hot
 * path does not.
 *
 * @code
 * >>> from doppler.telemetry import Telemetry
 * >>> tlm = Telemetry(1 << 12)
 * >>> pid = tlm.probe("rx.snr_db")
 * >>> tlm.emit(pid, 12.5)
 * >>> float(tlm.read()[0]["value"])
 * 12.5
 *
 * An id the registry never issued is refused, not written:
 *
 * >>> tlm.emit(pid + 1, 1.0)
 * Traceback (most recent call last):
 * ValueError: emit failed (rc=-4)
 *
 * @endcode
 */
JM_FORCEINLINE void
dp_tlm_emit (dp_tlm_t *t, int32_t id, double v)
{
  if (!t || (uint32_t) id >= DP_TLM_MAX_PROBES)
    return;
  dp_tlm_probe_t *p = &t->probes[id];
  if (++p->phase < p->decim)
    return;
  p->phase = 0;
  dp_tlm_rec_t r = { t->now, (float) v, (uint16_t) id, 0u };
  if (dp_tlmr_write (t->ring, (const uint64_t *) &r, 1))
    p->emitted++;
}

/**
 * @def DP_TLM(ctx, id, v)
 * @brief Probe-site wrapper around dp_tlm_emit().
 *
 * Instrumented hot loops use this form so a consumer building with
 * `-DDP_TLM_DISABLE` compiles every probe site out entirely.
 */
#ifndef DP_TLM_DISABLE
#define DP_TLM(ctx, id, v) dp_tlm_emit ((ctx), (id), (v))
#else
#define DP_TLM(ctx, id, v) ((void) 0)
#endif

#endif /* DP_TELEMETRY_H */
