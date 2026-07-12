/**
 * @file telemetry.h
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
 *   size_t n = dp_tlm_read (tlm, recs, 512);   // on the consumer side
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

/**
 * @brief Telemetry context: probe registry + SPSC record ring.
 *
 * Public (not opaque) because the emit path is inline; treat the fields as
 * read-only outside telemetry_core.c and dp_tlm_emit.
 */
typedef struct dp_tlm
{
  dp_tlmr_t     *ring;    /**< Lock-free SPSC record ring.              */
  uint64_t       now;     /**< Caller-stamped sample index for records. */
  uint32_t       n_probes;
  dp_tlm_probe_t probes[DP_TLM_MAX_PROBES];
} dp_tlm_t;

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
 */
int dp_tlm_probe (dp_tlm_t *t, const char *name, uint32_t decim);

/** @brief Looks up a probe id by name; DP_ERR_INVALID if unknown. */
int dp_tlm_lookup (const dp_tlm_t *t, const char *name);

/** @brief Probe name for @p id, or NULL if out of range. */
const char *dp_tlm_probe_name (const dp_tlm_t *t, int id);

/** @brief Number of registered probes. */
size_t dp_tlm_probe_count (const dp_tlm_t *t);

/** @brief Authoritative ring capacity in records (post page rounding). */
size_t dp_tlm_capacity (const dp_tlm_t *t);

/**
 * @brief Drains up to @p max_recs records into @p out.  Non-blocking.
 *
 * Consumer side of the SPSC ring: safe to call from a different thread
 * than the producer.  Returns immediately with whatever is available
 * (possibly 0) — never spins.
 *
 * @return Number of records copied out.
 */
size_t dp_tlm_read (dp_tlm_t *t, dp_tlm_rec_t *out, size_t max_recs);

/** @brief Total records dropped on ring overrun (monotonic). */
uint64_t dp_tlm_dropped (const dp_tlm_t *t);

/** @brief Records written for probe @p id (post-decimation, post-drop). */
uint64_t dp_tlm_emitted (const dp_tlm_t *t, int id);

/**
 * @brief Stamps the sample index carried by subsequent records.
 *
 * Call once per block from whoever owns the pipeline's sample clock
 * (`dp_tlm_set_now (tlm, clk->n)`).  NULL-safe so pipeline glue can call
 * it unconditionally.
 */
static inline void
dp_tlm_set_now (dp_tlm_t *t, uint64_t n)
{
  if (t)
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
 */
JM_FORCEINLINE void
dp_tlm_emit (dp_tlm_t *t, int32_t id, double v)
{
  if (!t)
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
