/**
 * @file async_dsss_pool_core.h
 * @brief AsyncDsssPool -- one object holds the population: a searcher, a
 *        pool of hand-off receivers, the assigned table and the event log
 *        (docs/design/async-dsss-receiver.md section 8.2).
 *
 * The multi-emitter use case (design section 6) has one channel searching
 * without pause and one AsyncDsssReceiver per emitter, assigned once from
 * a detection and tracking until its own loss decision. This object is
 * the holder of that lifecycle, in C, with the Python face as glue:
 *
 *   - **one searcher**, `Acquisition` in continuous mode with the block
 *     coherence its code-only window buys (section 2.3) and a peak list
 *     (section 7.1), its tiles fanned across the threads the pool is given;
 *   - **`n_slots` hand-off receivers**, created idle. An idle or lost
 *     receiver consumes and discards what it is fed, so every receiver is
 *     fed every block and the feed has no per-state branch; they run
 *     across the same threads (`dp_parallel.h`);
 *   - **the assigned table**, one row per slot: the seed's coordinates,
 *     and the row's CURRENT Doppler and chip phase -- the live loop's once
 *     the receiver tracks, the seed's advanced by the clock dilation
 *     before -- refreshed before every dwell is read, because an emitter
 *     drifts between windows and the seed is the wrong key (section 9);
 *   - **the event log**, borrowed by attachment (the telemetry shape): the
 *     pool is the one component that stamps.
 *
 * One `push()` per block does, in order: feed the searcher; refresh the
 * table; drop every peak within one chip of a live row's code phase, at
 * any Doppler, as that emitter's own (the zone is the code axis alone: a
 * tracked emitter's data blocks put smeared copies of it at its own phase
 * rows away, section 12.14); for each
 * survivor, `acq_build_handoff()` and `seed()` into a free slot, or count
 * it dropped when there is none; feed every receiver; then, for each slot
 * whose receiver reports lost, or has held its slot past the maximum
 * on-air time, clear the row, `reset()` the receiver to idle and log
 * `released`. `seed()`'s own refusal on a receiver that is not idle is
 * the second guard behind the table, so a bookkeeping error cannot become
 * a double assignment. The transitions -- `seeded`, `tracking`, `degrade`,
 * `lost`, `released`, `dropped` -- are the log's annotations, at the
 * sample they happened, with the slot, the receiver's state, the
 * Doppler, the chip phase and the C/N0 staged as `doppler:<name>` fields
 * beside the label (`core:label`).
 *
 * Nothing about the waveform or the population is baked in: every number
 * is a create parameter whose default is the operating point of section
 * 6.1 (twelve slots, a peak list of sixteen, a 2 s release interval, a
 * 15 min maximum on-air time), and the searcher's and the receivers' own
 * parameters pass through untouched. A physically-coupled carrier
 * (`carrier_freq_hz` > 0) is told to the searcher as well as the
 * receivers: its hand-off advances a hit's code phase by the drift over
 * half its dwell and its coherent blocks align their epochs (#1254,
 * #1256), and the table's rows are advanced by the same dilation.
 *
 * What comes out, per slot and by index: the status record by value
 * (`status()`), and the symbols the receiver decided on this push,
 * borrowed from the pool's own buffer (`symbols()`). Nothing allocates
 * per push once a block size has been seen, the pool never exceeds
 * `n_slots`, and a released emitter still on the air is a new detection
 * at its next window into whichever slot is free -- the one re-assignment
 * the lifecycle permits. Replay and live runs produce the same records,
 * because nothing here sees a time: every stamp is a stream position.
 *
 * Not built: section 11.4's replica. The operating spread of 10 dB picks
 * the list branch (section 9), so the pool is off the searcher's push
 * path and subtracts nothing.
 *
 * Lifecycle: create, then push / status / symbols / reset as often as
 * wanted, then destroy.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import AsyncDsssPool
 * >>> from doppler.wfm import Gold
 * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
 * >>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
 * ...                      spc=2, cn0_dbhz=45.0, doppler_uncertainty=5e3,
 * ...                      n_slots=4, lost_confirm_s=0.5)
 * >>> (pool.n_slots, pool.n_assigned, pool.status(0).assigned)
 * (4, 0, 0)
 * >>> int(pool.push(np.zeros(2046, np.complex64)))   # noise-free silence
 * 0
 *
 * @endcode
 */
#ifndef ASYNC_DSSS_POOL_CORE_H
#define ASYNC_DSSS_POOL_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "async_dsss_receiver/async_dsss_receiver_core.h"
#include "acq/acq_core.h"
#include "dp_event_log/dp_event_log_core.h"
#include "dll/dll_core.h"
#include "costas/costas_core.h"
#include "RateConverter/RateConverter_core.h"
#include "mpsk_receiver/mpsk_receiver_core.h"
#include "cic/cic_core.h"
#include "resample/resample_core.h"
#include "psd/psd_core.h"
#include "detector/detector_core.h"
#include "detection/detection_core.h"
#include "corr2d/corr2d_core.h"
#include "fft2d/fft2d_core.h"
#include "fft/fft_core.h"
#include "dp_tlm/dp_tlm_core.h"
#include "carrier_acq/carrier_acq_core.h"
#include "resamp/resamp_core.h"
#include "hbdecim/hbdecim_core.h"
#include "dp_parallel.h"
#include "dp_state.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** The one constant of design section 6.1: the maximum on-air time of a
 *  single emitter, seconds -- the default of `max_emitter_on_time_secs`,
 *  which the false-release budget and the soak are sized against. */
#define ASYNC_DSSS_POOL_MAX_EMITTER_ON_TIME_SECS (15.0 * 60.0)

#define ASYNC_DSSS_POOL_STATE_MAGIC DP_FOURCC ('A', 'D', 'P', 'L')
#define ASYNC_DSSS_POOL_STATE_VERSION 1u

  /**
   * @brief One slot's picture, by value -- what `status()` returns.
   *
   * The seed fields are the searcher's hand-off record, verbatim, at the
   * sample the row was assigned; the live fields are the receiver's own
   * status record (async_dsss_receiver_status_t) at the last push. For an
   * unassigned slot the live fields are the idle receiver's (state
   * ASYNC_DSSS_RX_IDLE); for a slot outside `[0, n_slots)` the record is
   * zero with `state` -1.
   */
  typedef struct
  {
    size_t   slot;        /**< The slot asked for.                         */
    int      assigned;    /**< 1 while a receiver holds an emitter.         */
    int      state;       /**< The receiver's ASYNC_DSSS_RX_* state; -1 for
                               a slot that does not exist.                 */
    uint64_t seed_sample; /**< Stream position the row was assigned at.    */
    double   seed_chip_phase; /**< The seed's code phase, chips.           */
    double   seed_doppler_hz; /**< The seed's Doppler, Hz.                 */
    double   seed_cn0_dbhz;   /**< The seed's C/N0 estimate, dB-Hz.        */
    double   doppler_hz;      /**< Where the emitter is now, Hz.           */
    double   chip_phase;      /**< Live Dll code phase, chips.             */
    double   code_rate;       /**< Live Dll code rate, chips per sample.   */
    double   cn0_dbhz_est;    /**< C/N0 estimate, dB-Hz.                   */
    int      code_locked;     /**< Presence flag.                          */
    int      locked;          /**< Health flag (symbol lock).              */
    double   lock_metric;     /**< The symbol-lock statistic.              */
    uint64_t state_samples;   /**< Samples since the receiver's state was
                                   entered.                                 */
    uint64_t both_down_samples; /**< The release clock, samples.           */
    uint64_t assigned_samples;  /**< Samples since the row was assigned.   */
  } async_dsss_pool_slot_t;

  /** One row of the assigned table. Plain data: it is the blob's payload. */
  typedef struct
  {
    int      assigned;        /**< 1 while the slot holds an emitter.       */
    uint64_t seed_sample;     /**< Stream position of the assignment.       */
    double   seed_chip_phase; /**< The hand-off record, verbatim.           */
    double   seed_doppler_hz;
    double   seed_cn0_dbhz;
    double   doppler_hz; /**< The row's current coordinates: the exclusion
                              zone is keyed on these -- refreshed every push
                              from a LOCKED loop, held otherwise (#1261). */
    double   chip_phase;
    int      prev_state; /**< The receiver's state at the last push, for
                              the transitions' edges.                       */
    int      prev_code;  /**< Its lock flags at the last push.              */
    int      prev_sym;
  } async_dsss_pool_row_t;

  /**
   * @brief AsyncDsssPool state.
   *
   * Allocate with async_dsss_pool_create().
   */
  typedef struct
  {
    /* Read-back (property-backed). */
    size_t   n_slots;    /**< Receivers held; never exceeded.               */
    size_t   n_assigned; /**< Slots assigned right now.                     */
    uint64_t dropped;    /**< Detections dropped for want of a free slot.   */
    uint64_t events;     /**< Transitions since create/reset, logged or not.*/
    uint64_t samples_consumed; /**< Input samples pushed: the stream
                                    position every event is stamped at.    */

    /* Config, restored by create(), never by the blob. */
    uint8_t *code;
    size_t   code_len;
    size_t   spc;
    double   chip_rate;
    double   symbol_rate;
    double   fs;
    double   carrier_freq_hz;
    uint64_t max_on_samples; /**< max_emitter_on_time_secs in samples; 0 =
                                  never released for time.                  */
    size_t   max_peaks;
    int      threads;

    /* The children. */
    acq_state_t                  *acq;
    async_dsss_receiver_state_t **rx;   /**< n_slots, created idle.        */
    dp_pool_t                    *pool; /**< the receivers' threads          */
    dp_event_log_t               *log;  /**< borrowed; NULL = none attached */

    /* The table and the per-push outputs. */
    async_dsss_pool_row_t *rows;    /**< n_slots                            */
    acq_result_t          *hits;    /**< max_peaks, the searcher's list     */
    float _Complex        *sym_buf; /**< n_slots * sym_cap, grown on demand */
    size_t                 sym_cap; /**< per slot, symbols                  */
    size_t                *n_sym;   /**< n_slots: the last push's count     */

    /* The feed's scratch: the block every receiver sees, for the fan. */
    const float _Complex *feed_x;
    size_t                feed_n;
  } async_dsss_pool_state_t;

/**
 * @brief Create a async_dsss_pool instance.
 *
 * Everything is sized once, here: the searcher with its list and its
 * threads, `n_slots` idle receivers, the table. NULL for a NULL or empty
 * code, a non-positive rate, `spc` or `n_slots` of 0, `max_peaks` outside
 * the searcher's own range, a negative `lost_confirm_s` or
 * `max_emitter_on_time_secs`, or a child that fails to open.
 *
 * @param code  Spreading code, one 0/1 chip per element.
 * @param code_len  Chips in `code`.
 * @param chip_rate  Chip rate, Hz (default: 1000000.0).
 * @param symbol_rate  Data-symbol rate, Hz (default: 1000.0).
 * @param spc  Samples per chip (default: 2).
 * @param m  PSK order of the receivers (default: 2).
 * @param cn0_dbhz  Design C/N0 for the searcher's sizing and the
 *                  receivers' (default: 55.0).
 * @param pfa  False-alarm target, the searcher's and the refine's
 *             (default: 1e-3).
 * @param pd  Detection-probability target (default: 0.9).
 * @param doppler_uncertainty  The searcher's one-sided span, Hz
 *                             (default: 100.0).
 * @param code_only_epochs  Whole code-only epochs the waveform's window
 *                          holds at any chip phase -- the block depth of
 *                          section 2.3; 1 = no window (default: 1).
 * @param doppler_rate  Doppler rate the depth is bounded against, Hz/s;
 *                      0 leaves the window as the only bound (default:
 *                      0.0).
 * @param max_peaks  The searcher's list capacity per dwell (default: 16).
 * @param n_slots  Receivers held (default: 12).
 * @param threads  Threads the receivers and the searcher's fan run across;
 *                 <= 0 picks the online core count, 1 is serial (default:
 *                 1).
 * @param carrier_freq_hz  RF carrier the Doppler is physically coupled to,
 *                         Hz, told to the searcher and every receiver; 0.0
 *                         = uncoupled (default: 0.0).
 * @param lost_confirm_s  The release rule's interval, seconds (section
 *                        10) (default: 2.0).
 * @param max_emitter_on_time_secs  Maximum on-air time of one emitter,
 *                                  seconds: a slot held longer is released
 *                                  (`reason` on_time); 0 = never (default:
 *                                  900.0, ASYNC_DSSS_POOL_MAX_EMITTER_ON_
 *                                  TIME_SECS).
 * @param segments  The receivers' live Dll segments (default: 4).
 * @param sps  The receivers' samples per symbol (default: 8).
 * @param differential  The receivers' differential demap (default: 0).
 * @param refine_max_error_db  As async_dsss_receiver_create() (default:
 *                             0.5).
 * @param refine_samples_per_symbol  As async_dsss_receiver_create()
 *                                   (default: 4).
 * @param refine_design_margin_db  As async_dsss_receiver_create() (default:
 *                                 14.0).
 * @param refine_n_fft  As async_dsss_receiver_create() (default: 64).
 * @param refine_zero_pad  As async_dsss_receiver_create() (default: 8).
 * @param refine_sequential  As async_dsss_receiver_create() (default:
 *                           false).
 * @param refine_max_n_blocks  As async_dsss_receiver_create() (default:
 *                             100000).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call async_dsss_pool_destroy() when done.
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import AsyncDsssPool
 * >>> from doppler.wfm import Gold
 * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
 * >>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
 * ...                      spc=2, cn0_dbhz=45.0, doppler_uncertainty=5e3,
 * ...                      n_slots=4, lost_confirm_s=0.5)
 * >>> (pool.n_slots, pool.n_assigned, pool.coherent_bins)
 * (4, 0, 1)
 * >>> round(pool.doppler_res_hz)          # one Doppler row of the searcher
 * 4888
 *
 * @endcode
 */
async_dsss_pool_state_t *async_dsss_pool_create(const uint8_t *code, size_t code_len, double chip_rate, double symbol_rate, size_t spc, int m, double cn0_dbhz, double pfa, double pd, double doppler_uncertainty, size_t code_only_epochs, double doppler_rate, size_t max_peaks, size_t n_slots, int threads, double carrier_freq_hz, double lost_confirm_s, double max_emitter_on_time_secs, size_t segments, size_t sps, int differential, double refine_max_error_db, size_t refine_samples_per_symbol, double refine_design_margin_db, size_t refine_n_fft, size_t refine_zero_pad, bool refine_sequential, size_t refine_max_n_blocks);

/**
 * @brief Destroy a async_dsss_pool instance and release all memory.
 * @param state  May be NULL.
 */
void async_dsss_pool_destroy(async_dsss_pool_state_t *state);

/**
 * @brief Release every slot and start over: the searcher reset, every
 *        receiver back to idle, the table cleared, the counters zeroed.
 *
 * The attached log stays attached and nothing is logged -- a reset is the
 * holder's decision, not an emitter's transition.
 *
 * @param state  Must be non-NULL.
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import AsyncDsssPool
 * >>> from doppler.wfm import Gold
 * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
 * >>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
 * ...                      spc=2, cn0_dbhz=45.0, n_slots=2)
 * >>> _ = pool.push(np.zeros(2046, np.complex64))
 * >>> pool.samples_consumed
 * 2046
 * >>> pool.reset()
 * >>> (pool.samples_consumed, pool.n_assigned, pool.events)
 * (0, 0, 0)
 *
 * @endcode
 */
void async_dsss_pool_reset(async_dsss_pool_state_t *state);

/**
 * @brief One block of raw cf32 samples through the population.
 *
 * In order: the searcher; the table refreshed; every peak within one chip
 * of a live row's code phase, at any Doppler, dropped as that emitter's
 * own; each
 * survivor seeded into a free slot or counted dropped; every receiver
 * fed, across the pool's threads; every receiver that reports lost, or
 * has held its slot past the maximum on-air time, released. Every
 * transition goes to the attached log at the sample it happened. Accepts
 * any block size: a hit decided inside the block is referred to the
 * block's start before it seeds (the receiver is fed the whole block),
 * on the dilated clock when the carrier is known.
 *
 * @param state  Must be non-NULL.
 * @param x  Input samples.
 * @param x_len  Samples in @p x.
 * @return Receivers assigned after this push.
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import AsyncDsssPool
 * >>> from doppler.wfm import Gold
 * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
 * >>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
 * ...                      spc=2, cn0_dbhz=45.0, n_slots=2)
 * >>> int(pool.push(np.zeros(4 * 2046, np.complex64)))  # silence: no one
 * 0
 * >>> pool.samples_consumed                  # the stream position
 * 8184
 *
 * @endcode
 */
size_t async_dsss_pool_push(async_dsss_pool_state_t *state, const float _Complex *x, size_t x_len);

/**
 * @brief One slot's picture, by value (async_dsss_pool_slot_t).
 *
 * Allocation-free: the row plus the receiver's own status record. A slot
 * outside `[0, n_slots)` returns a zero record with `state` -1.
 *
 * @param state  Must be non-NULL.
 * @param slot  The slot.
 * @return The record.
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import AsyncDsssPool
 * >>> from doppler.wfm import Gold
 * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
 * >>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
 * ...                      spc=2, cn0_dbhz=45.0, n_slots=2)
 * >>> r = pool.status(1)
 * >>> (r.slot, r.assigned, r.state)         # idle: 3, nothing assigned
 * (1, 0, 3)
 * >>> pool.status(2).state                  # no such slot
 * -1
 *
 * @endcode
 */
async_dsss_pool_slot_t async_dsss_pool_status(async_dsss_pool_state_t *state, size_t slot);

/** @brief The per-slot symbol capacity `symbols()` can return -- grown
 *  with the largest block pushed so far (0 before the first push). */
size_t async_dsss_pool_symbols_max_out(async_dsss_pool_state_t *state);

/**
 * @brief The symbols slot @p slot's receiver decided on the last push().
 *
 * Copied from the pool's own buffer, which the next push() overwrites.
 * Empty while the slot is idle, refining or lost, and for a slot outside
 * `[0, n_slots)`.
 *
 * @param state  Must be non-NULL.
 * @param slot  The slot.
 * @param out  Caller buffer.
 * @param max_out  Its capacity.
 * @return Symbols written.
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import AsyncDsssPool
 * >>> from doppler.wfm import Gold
 * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
 * >>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
 * ...                      spc=2, cn0_dbhz=45.0, n_slots=2)
 * >>> _ = pool.push(np.zeros(2046, np.complex64))
 * >>> pool.symbols(0).shape                 # idle: nothing decided
 * (0,)
 *
 * @endcode
 */
size_t async_dsss_pool_symbols(async_dsss_pool_state_t *state, size_t slot, float _Complex *out, size_t max_out);

/**
 * @brief Attach the run's event log (design section 8.1); NULL detaches.
 *
 * Borrowed, never owned: the holder opens, finalizes and closes it. From
 * now on every transition is appended at the sample it happened, with
 * `slot`, `state`, `doppler_hz`, `chip_phase` and `cn0_dbhz` staged as
 * `doppler:<name>` fields beside the label (`core:label`) and, on `released`, `reason` (`lost` or
 * `on_time`). A log that has already failed keeps failing (its error is
 * sticky); the pool counts the transition either way.
 *
 * @param state  Must be non-NULL.
 * @param log  The log, or NULL.
 * @return `DP_OK`.
 * @code
 * >>> import os, tempfile
 * >>> import numpy as np
 * >>> from doppler.dsss import AsyncDsssPool
 * >>> from doppler.telemetry import EventLog
 * >>> from doppler.wfm import Gold
 * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
 * >>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
 * ...                      spc=2, cn0_dbhz=45.0, n_slots=2)
 * >>> log = EventLog(os.path.join(tempfile.mkdtemp(), "run.events"))
 * >>> pool.set_event_log(log)               # attached: transitions go here
 * >>> _ = pool.push(np.zeros(2046, np.complex64))
 * >>> pool.set_event_log(None)              # detached
 * >>> log.close()
 *
 * @endcode
 */
int async_dsss_pool_set_event_log(async_dsss_pool_state_t *state, dp_event_log_t * log);

/**
 * @brief Floor every receiver's refine dwell at @p n_blocks
 *        (async_dsss_receiver_set_refine_min_blocks(); design section
 *        12.16, #1265).
 *
 * Forwarded to all `n_slots` receivers; each applies it to the next
 * refine chain it builds, so a slot already refining keeps its dwell.
 * The receivers' default is 7 blocks. Config, not running state.
 *
 * @param state     Must be non-NULL.
 * @param n_blocks  The floor, blocks; 0 removes it.
 * @return `DP_OK`.
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import AsyncDsssPool
 * >>> from doppler.wfm import Gold
 * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
 * >>> pool = AsyncDsssPool(code, chip_rate=5e6, symbol_rate=2700.0,
 * ...                      spc=2, cn0_dbhz=45.0, n_slots=2)
 * >>> pool.refine_min_blocks
 * 7
 * >>> pool.set_refine_min_blocks(12)
 * >>> pool.refine_min_blocks
 * 12
 *
 * @endcode
 */
int async_dsss_pool_set_refine_min_blocks(async_dsss_pool_state_t *state, size_t n_blocks);

  /* ── Serializable state (docs/design/state-serialization.md) ──────────
   * A composition: the pool's own counters and the table, then the
   * searcher's blob and every receiver's, each self-validating. Config
   * (the geometry, the slot count, the carrier, the attached log) is
   * restored by create(), not the blob; a blob from a pool of another
   * slot count is rejected. The symbol buffer is scratch -- the last
   * push's symbols do not survive a hand-off. */
  size_t async_dsss_pool_state_bytes (const async_dsss_pool_state_t *state);
  void   async_dsss_pool_get_state (const async_dsss_pool_state_t *state,
                                    void                          *blob);
  int    async_dsss_pool_set_state (async_dsss_pool_state_t *state,
                                    const void              *blob);

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_DSSS_POOL_CORE_H */
