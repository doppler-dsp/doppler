/**
 * @file async_dsss_pool_core.h
 * @brief AsyncDsssPool -- N AsyncDsssReceivers on one stream under a
 *        controller (docs/design/async-dsss-parallel.md §4).
 *
 * Every receiver is a whole AsyncDsssReceiver: it searches the full
 * Doppler range for itself, seeds itself, refines, tracks and releases.
 * The pool adds no searcher and no surface. What it adds is the
 * bookkeeping that makes N of them hold N different signals instead of
 * all piling onto the strongest:
 *
 * 1. **One block to every receiver, in parallel.** push() hands the same
 *    samples to each receiver and steps them on the `dp_parallel` workers,
 *    one receiver per task. Each receiver's own search engine runs on one
 *    thread (dp_async_dsss_receiver_set_threads() 1), so N receivers do
 *    not each start a pool of every core. A receiver touches only its own
 *    state, so the result does not depend on the thread count: one thread
 *    and N give the same bytes.
 * 2. **Then the controller, serially, at every code period of the
 *    stream.** A receiver is fed exactly one code period (`code_len *
 *    spc` samples) per call, on the grid of the pool's sample count from
 *    create or reset; push() keeps what is left past the last whole
 *    period and feeds it with the next push. The controller runs after
 *    each period. Every receiver call and every controller step is then a
 *    position in the stream, not a call boundary, so any blocking of one
 *    stream gives the same receivers, the same releases (a receiver's
 *    release clock is read once per call) and the same symbols. The cost
 *    is latency: up to one code period of a push waits for the next. It
 *    reads every receiver's status() and:
 *    - **releases the lost.** A receiver in the lost state (its release
 *      rule fired: the emitter left, or the seed never locked) is reset to
 *      searching. With `reacquire` the receiver does this itself and is
 *      never lost.
 *    - **resolves duplicates.** Two receivers that hold a signal
 *      (refining or tracking) within one chip of each other's code phase,
 *      circularly, hold the same signal. The one that keeps it is the one
 *      tracking over the one refining, then the one in its state longer
 *      (status().state_samples: the first to lock), then the higher C/N0
 *      estimate, then the lower index. The other is reset.
 *    - **releases ghosts.** A receiver can take a strong emitter's
 *      Doppler-offset sidelobe, hundreds of chips from the emitter's own
 *      phase, where the duplicate key cannot see it: it despreads the
 *      emitter's own data. Two of its traits give it away together
 *      (doppler#2529), each over the same trailing window of
 *      @ref ASYNC_DSSS_POOL_GHOST_WINDOW_S of whole code periods, both
 *      receivers tracking throughout:
 *      - its carrier moves in lockstep with a stronger holder's (a higher
 *        status().cn0_dbhz_est), a whole nonzero number k of lattice
 *        steps away: the carrier delta's mean is within
 *        @ref ASYNC_DSSS_POOL_GHOST_EPS_HZ of k times chip_rate /
 *        (2 code_len), and its standard deviation below
 *        @ref ASYNC_DSSS_POOL_GHOST_STILL_HZ. A sidelobe sits on its
 *        parent's carrier a tile away, and the carrier loop, updating
 *        once a code period from a squaring combine, reads that modulo
 *        half the epoch rate. Two emitters' carriers move apart.
 *      - its Dll holds code lock in fewer than
 *        @ref ASYNC_DSSS_POOL_GHOST_LOCK_FRAC of the window's periods: a
 *        sidelobe is not the code's triangle.
 *      Neither alone will do: a distinct emitter can co-move with another
 *      on the lattice (but holds code lock), and a true track near the
 *      floor loses code lock (but its carrier is its own).
 *      dp_async_dsss_pool_ghost_of() is the test. The ghost is reset; the
 *      search's own sidelobe rule (dp_async_dsss_receiver_set_rank()) then
 *      keeps it off that cell, but for noise lifting it past the gate.
 *    - **hands out exclusions and ranks.** Every receiver gets the code
 *      phases the OTHER holders hold as its exclusions
 *      (dp_async_dsss_receiver_set_exclusions()), so a search never takes
 *      a signal already held, and a reset loser does not take its signal
 *      back. The searching receivers get ranks 0, 1, ... in index order
 *      (dp_async_dsss_receiver_set_rank()), so searchers that decide in
 *      the same dwell take different signals. A holder's rank is set to 0,
 *      ready for its next search.
 *    Both levers take effect at a receiver's next dwell.
 * 3. **Reports.** status() is a receiver's status now, symbols() the
 *    symbols it emitted during the last push(); both are as of the last
 *    whole period fed, since the controller runs right after it, so a
 *    released receiver never reads lost.
 *
 * The receivers share the stream from create on, so their dwells share
 * one block grid (a reset receiver's engine restarts on it), and their
 * ranks line up with no command.
 *
 * Lifecycle: create, then push() any number of times, reset() at any point,
 * destroy.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import AsyncDsssPool
 * >>> from doppler.wfm import Gold
 * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
 * >>> pool = AsyncDsssPool(code, n_receivers=3, chip_rate=3.069e6,
 * ...                      symbol_rate=2700.0, spc=2,
 * ...                      doppler_uncertainty=500.0, threads=1)
 * >>> pool.n_receivers
 * 3
 * >>> pool.push(np.zeros(4096, dtype=np.complex64))   # holders: none
 * 0
 * >>> [pool.status(i).state for i in range(3)]         # all searching
 * [0, 0, 0]
 *
 * @endcode
 */
#ifndef DP_ASYNC_DSSS_POOL_CORE_H
#define DP_ASYNC_DSSS_POOL_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/dp_parallel.h"
#include "doppler/dp_state.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/ber/ber_core.h"
#include "doppler/dll/dll_core.h"
#include "doppler/costas/costas_core.h"
#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/mpsk_receiver/mpsk_receiver_core.h"
#include "doppler/cic/cic_core.h"
#include "doppler/resample/resample_core.h"
#include "doppler/psd/psd_core.h"
#include "doppler/detector/detector_core.h"
#include "doppler/detection/detection_core.h"
#include "doppler/spectral/spectral_core.h"
#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/corr/corr_core.h"
#include "doppler/fft/fft_core.h"
#include "doppler/acc_trace/acc_trace_core.h"
#include "doppler/acq/acq_core.h"
#include "doppler/carrier_acq/carrier_acq_core.h"
#include "doppler/resamp/resamp_core.h"
#include "doppler/hbdecim/hbdecim_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /** Most receivers a pool runs: one rank per peak a dwell reports
   *  (@ref ASYNC_DSSS_RX_MAX_PEAKS), so every searcher has a rank. */
#define ASYNC_DSSS_POOL_MAX_RECEIVERS ASYNC_DSSS_RX_MAX_PEAKS

  /** `status().state` for a receiver index past the pool: no such
   *  receiver. */
#define ASYNC_DSSS_POOL_NO_RECEIVER (-1)

#define ASYNC_DSSS_POOL_STATE_MAGIC   DP_FOURCC ('A', 'D', 'P', 'L')
/* v4, not 3: 'ADPL' v3 was the cell-receiver pool this one replaced
 * (v0.48.0 - v0.73.0), and v0.74.0 shipped this pool as v2. Each version
 * of a magic names one layout, so this pool steps past every number the
 * magic has carried (#2569; scripts/check_state_versions.py). */
#define ASYNC_DSSS_POOL_STATE_VERSION 4u

  /** The ghost rule's window, s: the trailing span both its tests read
   *  (doppler#2529). */
#define ASYNC_DSSS_POOL_GHOST_WINDOW_S 0.2
  /** How near the lattice the mean carrier delta must sit, Hz. */
#define ASYNC_DSSS_POOL_GHOST_EPS_HZ 10.0
  /** The stillness limit: the carrier delta's standard deviation over the
   *  window must be below this, Hz. */
#define ASYNC_DSSS_POOL_GHOST_STILL_HZ 10.0
  /** The code-lock fraction a ghost holds below: of the window's steps,
   *  the share with status().code_locked set. */
#define ASYNC_DSSS_POOL_GHOST_LOCK_FRAC 0.2

  /**
   * @brief AsyncDsssPool state.
   *
   * Allocate with dp_async_dsss_pool_create().
   */
  typedef struct
  {
    dp_async_dsss_receiver_state_t **rx; /**< The receivers, n_receivers. */
    dp_pool_t *workers; /**< NULL when one thread: push() runs serially. */
    int        threads; /**< Workers asked for; 0 = the online cores. */
    float _Complex *sym_buf; /**< n_receivers rows of sym_cap symbols. */
    size_t          sym_cap; /**< Symbols one row holds.                */
    size_t sym_extra; /**< Symbols one code period carries, rounded up,
                           plus one: what a call can emit past its
                           input's own (dp_async_dsss_receiver_steps()). */
    size_t         *n_sym;   /**< Symbols each receiver emitted, last push. */
    const float _Complex *feed_x; /**< The block push() is stepping.    */
    size_t                feed_n;
    size_t   period;             /**< Samples a code period: the controller
                                      runs every period of `samples`.   */
    float _Complex *carry;       /**< The pushed samples past the last whole
                                      period, not yet fed: period long. */
    size_t          carry_len;   /**< How many; < period.               */
    size_t   n_receivers;        /**< Receivers, fixed at create.         */
    uint64_t samples;            /**< Input samples pushed since create or
                                      reset: the pool's shared clock.   */
    uint64_t released_lost;      /**< Lost receivers the controller reset. */
    uint64_t released_duplicate; /**< Duplicate holders it reset.          */
    uint64_t released_ghost;     /**< Sidelobe ghosts it reset.            */
    double   lattice_hz;   /**< The ghost lattice spacing: chip_rate /
                                (2 code_len), half the code-epoch rate. */
    size_t   ghost_window; /**< Controller steps (code periods) in the
                                ghost rule's window.                    */
    double  *car_hist;     /**< n_receivers rings of ghost_window carrier
                                estimates (status().doppler_hz), Hz.    */
    uint8_t *cl_hist;      /**< The same rings of status().code_locked. */
    size_t  *hist_len;     /**< Entries each ring holds: the controller
                                steps the receiver has been tracking in
                                a row, at most ghost_window. Every other
                                entry is 0.                             */
    size_t   hist_pos;     /**< The ring slot the next step writes; one
                                clock for every ring.                   */
    dp_tlm_t *tlm;               /**< Attached context; NULL = detached.  */
    char      tlm_prefix[DP_TLM_NAME_MAX]; /**< Its prefix.               */
    uint32_t  tlm_decim;                   /**< Its decimation.           */
  } dp_async_dsss_pool_state_t;

  /**
   * @brief Create a pool of @p n_receivers receivers built alike.
   *
   * Every parameter after @p n_receivers and before @p threads is
   * dp_async_dsss_receiver_create()'s, passed through to each receiver
   * unchanged; see it for each one's meaning, range and default. Each
   * receiver's search engine is then set to one thread.
   *
   * @param code         Spreading code, 0/1 chips.
   * @param code_len     Chips in @p code.
   * @param n_receivers  Receivers, 1 to @ref ASYNC_DSSS_POOL_MAX_RECEIVERS;
   *                     the most signals the pool holds at once (default 3).
   * @param chip_rate    Chip rate, Hz.
   * @param symbol_rate  Symbol rate, Hz.
   * @param spc          Samples per chip.
   * @param m            PSK order.
   * @param cn0_dbhz     Design C/N0, dB-Hz.
   * @param pfa          Search false-alarm probability.
   * @param pd           Search detection probability.
   * @param doppler_uncertainty  Doppler search half-range, Hz.
   * @param segments     Dll segments per symbol.
   * @param sps          Demodulator samples per symbol.
   * @param differential Differential detection (nonzero) or coherent.
   * @param refine_max_error_db  Refine stage's allowed loss, dB.
   * @param refine_samples_per_symbol  Refine stage's samples per symbol.
   * @param refine_design_margin_db  Refine stage's design margin, dB.
   * @param refine_n_fft  Refine stage's FFT length.
   * @param refine_zero_pad  Refine stage's zero-pad factor.
   * @param refine_sequential  Refine stage's sequential mode.
   * @param refine_max_n_blocks  Refine stage's block cap.
   * @param carrier_freq_hz  RF carrier for code aiding, Hz; 0 = none.
   * @param lost_confirm_s  The release rule's confirm time, s (default 2.0
   *                        here: a pool whose receivers never release
   *                        never frees one). 0 = never lost.
   * @param reacquire    The receiver's own release-to-search.
   * @param code_only_s  Code-only window, s: sets the slow-time depth D.
   * @param doppler_rate Doppler rate bound, Hz/s.
   * @param threads      Workers push() steps the receivers on, the calling
   *                     thread included: 1 = serial, 0 = the online cores
   *                     (default). Never more than @p n_receivers run.
   * @return A new pool, every receiver searching; NULL on an invalid
   *         argument, or one any receiver refuses.
   * @note Caller must call dp_async_dsss_pool_destroy() when done.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssPool
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> pool = AsyncDsssPool(code, n_receivers=3, chip_rate=3.069e6,
   * ...                      symbol_rate=2700.0, spc=2,
   * ...                      doppler_uncertainty=500.0, threads=1)
   * >>> (pool.n_receivers, pool.threads, pool.coherent_bins)
   * (3, 1, 1)
   * >>> AsyncDsssPool(code, n_receivers=17)    # one rank per peak: 16 at most
   * Traceback (most recent call last):
   *     ...
   * ValueError: AsyncDsssPool: invalid parameter ...
   *
   * @endcode
   */
  dp_async_dsss_pool_state_t *dp_async_dsss_pool_create (
      const uint8_t *code, size_t code_len, size_t n_receivers,
      double chip_rate, double symbol_rate, size_t spc, int m,
      double cn0_dbhz, double pfa, double pd, double doppler_uncertainty,
      size_t segments, size_t sps, int differential,
      double refine_max_error_db, size_t refine_samples_per_symbol,
      double refine_design_margin_db, size_t refine_n_fft,
      size_t refine_zero_pad, bool refine_sequential,
      size_t refine_max_n_blocks, double carrier_freq_hz,
      double lost_confirm_s, bool reacquire, double code_only_s,
      double doppler_rate, int threads);

  /**
   * @brief Destroy a pool and every receiver.
   * @param state  May be NULL.
   */
  void dp_async_dsss_pool_destroy (dp_async_dsss_pool_state_t *state);

  /**
   * @brief Every receiver back to searching, with no exclusions and rank
   *        0; the clock, the release counts and the ghost history back to
   *        0.
   * @param state  Must be non-NULL.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssPool
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> pool = AsyncDsssPool(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                      spc=2, doppler_uncertainty=500.0, threads=1)
   * >>> _ = pool.push(np.zeros(2046, dtype=np.complex64))
   * >>> pool.samples
   * 2046
   * >>> pool.reset()
   * >>> pool.samples
   * 0
   *
   * @endcode
   */
  void dp_async_dsss_pool_reset (dp_async_dsss_pool_state_t *state);

  /** @brief Bytes in the blob dp_async_dsss_pool_get_state() writes: the
   *  envelope, the pool's own fields, and every receiver's own blob. */
  size_t
  dp_async_dsss_pool_state_bytes (const dp_async_dsss_pool_state_t *state);

  /** @brief Write the state into @p blob: the clock, the release counts,
   *  the thread count (a mutator's value, #2022), the samples carried to
   *  the next whole period, the ghost rule's history (each receiver's
   *  carriers and code-lock flags over its window), and each receiver's
   *  state, which carries the
   *  exclusions and rank the controller gave it. The last push's symbols
   *  do not travel. */
  void dp_async_dsss_pool_get_state (const dp_async_dsss_pool_state_t *state,
                                     void                             *blob);

  /** @brief Restore the state, whole or not at all. `DP_ERR_INVALID` on
   *  a blob of another object or version, another receiver count, a carry
   *  of a period or more, a ghost history control() cannot leave (a length
   *  past the window, a flag other than 0 or 1, a nonzero entry outside a
   *  ring's span, or a history on a receiver not tracking), or a receiver
   *  blob its receiver refuses; a
   *  refusal leaves the pool and every receiver as they were. */
  int dp_async_dsss_pool_set_state (dp_async_dsss_pool_state_t *state,
                                    const void                 *blob);

  /** @brief Workers push() steps the receivers on, the calling thread
   *  included: what set_threads() asked for, at most n_receivers, and
   *  the online cores for 0. */
  int dp_async_dsss_pool_get_threads (const dp_async_dsss_pool_state_t *state);

  /** @brief The receivers' slow-time depth D, epochs per coherent block
   *  (dp_async_dsss_receiver_get_coherent_bins()); alike for all. */
  size_t
  dp_async_dsss_pool_get_coherent_bins (const dp_async_dsss_pool_state_t *state);

  /**
   * @brief Step every receiver on one block, running the controller at
   *        every code period it crosses.
   *
   * Hands every whole code period it completes to every receiver (header
   * step 1), keeps each one's symbols for symbols(), and after each period
   * releases the lost, resolves duplicates, releases ghosts and hands out
   * exclusions and ranks (step 2). The samples past the last whole period are kept for
   * the next push. Accepts any block size, and state carries across calls:
   * the result is a function of the stream, not of its blocking.
   *
   * @param state  Must be non-NULL.
   * @param x      Input cf32 samples.
   * @param x_len  Number of input samples.
   * @return The receivers holding a signal (refining or tracking) at the
   *         end of the block.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssPool
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
   * ...                      symbol_rate=2700.0, spc=2,
   * ...                      doppler_uncertainty=500.0, threads=1)
   * >>> pool.push(np.zeros(2046, dtype=np.complex64))
   * 0
   *
   * @endcode
   */
  size_t dp_async_dsss_pool_push (dp_async_dsss_pool_state_t *state,
                                  const float _Complex *x, size_t x_len);

  /**
   * @brief Receiver @p receiver's status, now.
   *
   * dp_async_dsss_receiver_status() of that receiver, as of the last whole
   * code period fed, after the controller's step on it: a receiver the
   * controller released reads searching. A
   * @p receiver past the pool returns a zeroed record with
   * `state` @ref ASYNC_DSSS_POOL_NO_RECEIVER.
   *
   * @param state     Must be non-NULL.
   * @param receiver  0 to n_receivers - 1.
   * @return The status record.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssPool
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
   * ...                      symbol_rate=2700.0, spc=2,
   * ...                      doppler_uncertainty=500.0, threads=1)
   * >>> (pool.status(0).state, pool.status(2).state)  # 2 is past the pool
   * (0, -1)
   *
   * @endcode
   */
  async_dsss_receiver_status_t
  dp_async_dsss_pool_status (const dp_async_dsss_pool_state_t *state,
                             size_t                            receiver);

  /** @brief The most symbols symbols() can return: the largest count any
   *  receiver emitted during the last push(). */
  size_t
  dp_async_dsss_pool_symbols_max_out (const dp_async_dsss_pool_state_t *state);

  /**
   * @brief The symbols receiver @p receiver emitted during the last push().
   *
   * Empty for a receiver that was searching, refining or lost throughout
   * the push, and for a @p receiver past the pool. Tracking emits symbols
   * whether or not they are a signal's (dp_async_dsss_receiver_steps()):
   * read status().locked for that. A push replaces them, and set_state()
   * and reset() clear them.
   *
   * @param state     Must be non-NULL.
   * @param receiver  0 to n_receivers - 1.
   * @param out       Output symbols.
   * @param max_out   Capacity of @p out; the symbols past it are not
   *                  written.
   * @return Symbols written.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssPool
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
   * ...                      symbol_rate=2700.0, spc=2,
   * ...                      doppler_uncertainty=500.0, threads=1)
   * >>> _ = pool.push(np.zeros(2046, dtype=np.complex64))
   * >>> pool.symbols(0).size                    # searching: nothing yet
   * 0
   *
   * @endcode
   */
  size_t dp_async_dsss_pool_symbols (const dp_async_dsss_pool_state_t *state,
                                     size_t receiver, float _Complex *out,
                                     size_t max_out);

  /**
   * @brief Set the workers push() steps the receivers on.
   *
   * The receivers' results do not change with it (header step 1); only
   * the time a push takes does. A mutator's value, so it travels in the
   * state blob (#2022).
   *
   * @param state  Must be non-NULL.
   * @param n      1 = serial, 0 = the online cores; at most n_receivers
   *               run.
   * @return `DP_OK`; `DP_ERR_INVALID` (unchanged) on a negative @p n, or
   *         on one that would run more than one worker while a telemetry
   *         context is attached (dp_async_dsss_pool_set_telemetry()).
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssPool
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
   * ...                      symbol_rate=2700.0, spc=2,
   * ...                      doppler_uncertainty=500.0)
   * >>> pool.set_threads(1)
   * >>> pool.threads
   * 1
   *
   * @endcode
   */
  int dp_async_dsss_pool_set_threads (dp_async_dsss_pool_state_t *state,
                                      int                         n);

  /**
   * @brief Attach (or detach) a telemetry context across every receiver.
   *
   * Receiver i reports under "<prefix>.r<i>" exactly as
   * dp_async_dsss_receiver_set_telemetry() describes, on the one context.
   * A context is single-producer, so this needs one producer: it is
   * refused unless push() runs on one worker (get_threads() 1), and while
   * a context is attached set_threads() refuses more. Parallel telemetry,
   * one context per receiver, is doppler#2547.
   *
   * Whole or nothing: every receiver's names must fit before any
   * registers on @p tlm (they are first attached to a scratch context),
   * and a refusal leaves the pool on its previous context, or detached.
   * The attachment is a request kept on the pool, so a receiver the
   * controller resets keeps reporting under the same names.
   *
   * @param state   Must be non-NULL.
   * @param tlm     Context to attach, or NULL to detach every receiver.
   * @param prefix  Probe-name prefix; NULL means "pool".
   * @param decim   Emit every decim-th event on each probe; >= 1.
   * @return `DP_OK`; `DP_ERR_INVALID` on more than one worker, decim 0, a
   *         prefix too long for a receiver's names, or no room for them.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssPool
   * >>> from doppler.telemetry import Telemetry
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> pool = AsyncDsssPool(code, n_receivers=2, chip_rate=3.069e6,
   * ...                      symbol_rate=2700.0, spc=2,
   * ...                      doppler_uncertainty=500.0, threads=1)
   * >>> tlm = Telemetry(1 << 16)
   * >>> pool.set_telemetry(tlm, "pool")
   * >>> sorted(p for p in tlm.probe_names if p.endswith(".state"))
   * ['pool.r0.state', 'pool.r1.state']
   *
   * @endcode
   */
  int dp_async_dsss_pool_set_telemetry (dp_async_dsss_pool_state_t *state,
                                        dp_tlm_t *tlm, const char *prefix,
                                        uint32_t decim);

  /**
   * @brief Whether receiver @p j tracks a Doppler-offset sidelobe of
   *        receiver @p i's signal, by the controller's ghost rule (header
   *        step 2) over the history it holds now.
   *
   * Both tests over the last @ref ASYNC_DSSS_POOL_GHOST_WINDOW_S of
   * controller steps: @p j's carrier minus @p i's has a mean within
   * @ref ASYNC_DSSS_POOL_GHOST_EPS_HZ of a nonzero multiple of
   * `lattice_hz` and a standard deviation below
   * @ref ASYNC_DSSS_POOL_GHOST_STILL_HZ, and @p j held code lock in fewer
   * than @ref ASYNC_DSSS_POOL_GHOST_LOCK_FRAC of those steps. Both must
   * have tracked the whole window. Which of the two is stronger is the
   * controller's question, not this one's.
   *
   * @param state  Must be non-NULL.
   * @param i      The stronger holder.
   * @param j      The candidate ghost.
   * @return 1 if @p j is @p i's ghost; 0 otherwise, and for an index past
   *         the pool or @p i equal to @p j.
   */
  int dp_async_dsss_pool_ghost_of (const dp_async_dsss_pool_state_t *state,
                                   size_t i, size_t j);

  /**
   * @brief Borrow receiver @p receiver, for C-only configuration: its lock
   *        and chain settings (dp_async_dsss_receiver_configure_lock_raw(),
   *        dp_async_dsss_receiver_configure_chain_raw()).
   *
   * Not for telemetry: dp_async_dsss_pool_set_telemetry() is the one
   * route that keeps a context to one producer. The controller sets the exclusions and the rank at every push, so a
   * value set through the borrow for either is overwritten.
   *
   * @param state     Must be non-NULL.
   * @param receiver  0 to n_receivers - 1.
   * @return The receiver, owned by the pool; NULL past the pool.
   */
  dp_async_dsss_receiver_state_t *
  dp_async_dsss_pool_receiver (const dp_async_dsss_pool_state_t *state,
                               size_t                            receiver);

#ifdef __cplusplus
}
#endif

#endif /* DP_ASYNC_DSSS_POOL_CORE_H */
