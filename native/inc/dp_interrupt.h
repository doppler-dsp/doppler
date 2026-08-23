/**
 * @file dp_interrupt.h
 * @brief Asking a blocking wait to stop, whatever it is waiting on.
 *
 * doppler moves samples over three transports — a NATS subject, a
 * double-mapped ring, and a capture file — and every one of them has a
 * consumer-side wait that a caller may need to abandon. This is the one
 * flag all three consult, so that Ctrl+C means the same thing wherever
 * the samples are coming from.
 *
 * It lives in the core library rather than in the optional stream
 * component because two of its three callers are core: a file writer and
 * a ring buffer are available in a build with no NATS at all. It was in
 * `native/src/stream/stream_core.c` until it acquired that second caller,
 * which is also when it turned out to have no NATS dependency to begin
 * with — a `volatile sig_atomic_t` and four accessors over libc.
 *
 * See `docs/design/io-termination.md` for the contract this is one third
 * of; the other two are end-of-stream and durable completion.
 */

#ifndef DP_INTERRUPT_H
#define DP_INTERRUPT_H

#include "clib_common.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Default interrupt latency, in milliseconds.
   *
   * Ten wakeups a second on an idle waiter, and a delay no human
   * perceives when they press Ctrl+C. It is a default rather than a
   * constant of the design: see dp_set_interrupt_latency_ms().
   */
#define DP_INTERRUPT_LATENCY_DEFAULT_MS 100u

  /**
   * @brief Ask every blocking wait in this process to stop.
   *
   * Assigns to a `volatile sig_atomic_t` and does nothing else, which is
   * the only thing the C standard promises can be done from a signal
   * handler without tearing — and being callable from a handler is the
   * entire point of this API.
   *
   * The flag is **sticky**: one handler firing may have to release
   * several parked loops, so it stays set until dp_resume() clears it.
   *
   * @code
   * static void on_sigint (int sig) { (void)sig; dp_interrupt (); }
   * signal (SIGINT, on_sigint);
   * @endcode
   */
  void dp_interrupt (void);

  /**
   * @brief Clear the interrupt, so blocking waits block again.
   */
  void dp_resume (void);

  /**
   * @brief Non-zero when an interrupt is pending.
   *
   * The check a hand-written loop makes between blocks. A wait that
   * cannot be sliced — a busy-spin over a ring, a read of a file still
   * being appended to — polls this and gives up when it is set.
   *
   * @return Non-zero when interrupted, 0 otherwise.
   */
  int dp_interrupted (void);

  /**
   * @brief How soon a blocking wait must notice an interrupt.
   *
   * A wait that cannot be woken is taken in slices, with the flag checked
   * between them. This is the size of that slice, expressed as the thing
   * a caller actually cares about — the worst-case delay between
   * dp_interrupt() and the wait returning — rather than as an
   * implementation detail.
   *
   * It is a knob because the right answer is not the library's to know.
   * A human pressing Ctrl+C cannot perceive 100 ms; a control loop that
   * must hand back within one symbol period can, and a battery-powered
   * sensor would rather wake once a second than ten times. The cost is
   * one wakeup per slice on an otherwise idle waiter.
   *
   * Process-wide, like the flag it serves. Takes effect on the next
   * slice, so a wait already blocked adopts it within one old slice.
   *
   * @param ms Milliseconds; 0 selects @ref DP_INTERRUPT_LATENCY_DEFAULT_MS.
   */
  void dp_set_interrupt_latency_ms (unsigned ms);

  /**
   * @brief The interrupt latency in force.
   *
   * @return Milliseconds.
   */
  unsigned dp_interrupt_latency_ms (void);

  /**
   * @brief Install a handler for @p sig that calls dp_interrupt().
   *
   * Uses `sigaction` and **chains** to whatever handler was already
   * installed, so adding this to a program does not silently disable the
   * one it had.
   *
   * Install it EARLY — before opening transports, not after. A signal
   * arriving before this call is not ignored, it terminates the process,
   * and that window is real: measured at ~5 ms for a dynamically linked
   * binary, which is long enough for a supervisor's stop signal to land
   * inside it.
   *
   * @param sig Signal number, e.g. `SIGINT`.
   * @return DP_OK, or @ref DP_ERR_INVALID if the handler could not be
   *         installed or all handler slots are in use.
   */
  int dp_interrupt_on_signal (int sig);

  /**
   * @brief Put back whatever handler dp_interrupt_on_signal() displaced.
   *
   * @param sig Signal number previously passed to
   *            dp_interrupt_on_signal().
   * @return DP_OK, or @ref DP_ERR_INVALID if @p sig was never installed.
   */
  int dp_restore_signal (int sig);

  /**
   * @brief A scoped handle to the process-wide interrupt facility.
   *
   * The flag above is process-wide and stays so, so this is a handle to a
   * facility rather than an instance of one: two guards observe the same
   * flag. What a guard scopes is the *arming* -- which signals it
   * installed, and the latency it overrode -- so that both can be undone
   * exactly, by the code that did them, without a caller tracking it.
   *
   * It exists because that bookkeeping had been living in the Python
   * binding, which is the one place doppler does not put logic. See
   * docs/design/io-termination.md.
   */
  typedef struct dp_interrupt_guard dp_interrupt_guard_t;

  /**
   * @brief Clear the flag, optionally install handlers, and remember what
   *        to undo.
   *
   * Construction is what ARMS: on return the handlers are installed and
   * the flag is clear. A stale flag would otherwise refuse the first wait
   * inside the very block that just armed it.
   *
   * @param signals    Signals to install on; may be NULL for none, in
   *                   which case the guard is only a handle to the flag.
   * @param n_signals  How many @p signals holds.
   * @param latency_ms Wait-slice override; 0 leaves the process setting
   *                   alone, and only a non-zero value is restored.
   * @return A guard, or NULL if a handler could not be installed -- in
   *         which case any already installed by this call are restored
   *         first, so a failed create arms nothing.
   *
   * @code
   * >>> from doppler.interrupt import Interrupt
   * >>> it = Interrupt()
   * >>> it.interrupted()
   * 0
   * @endcode
   */
  dp_interrupt_guard_t *dp_interrupt_guard_create (const int *signals,
                                                   size_t     n_signals,
                                                   unsigned   latency_ms);

  /**
   * @brief Restore every handler and latency this guard changed.
   *
   * Does NOT clear the flag: a caller that was interrupted still needs to
   * see that it was, after the block that noticed has exited.
   *
   * @param guard Guard; NULL is a no-op.
   */
  void dp_interrupt_guard_destroy (dp_interrupt_guard_t *guard);

  /**
   * @brief Ask every blocking wait in this process to stop.
   *
   * The object's face onto dp_interrupt(). It takes a guard because that
   * is how a method is called, not because the request is scoped to one --
   * the flag is process-wide, and a request through any guard is seen by
   * every waiter.
   *
   * @param guard Guard; NULL is a no-op.
   *
   * @code
   * >>> from doppler.interrupt import Interrupt
   * >>> it = Interrupt()
   * >>> it.interrupt()
   * >>> it.interrupted()
   * 1
   * @endcode
   */
  void dp_interrupt_guard_interrupt (dp_interrupt_guard_t *guard);

  /**
   * @brief Non-zero once a stop has been requested.
   *
   * @param guard Guard; NULL reads the flag anyway, since it is
   *              process-wide and a guard is not what holds it.
   * @return Non-zero if interrupted.
   */
  int dp_interrupt_guard_interrupted (const dp_interrupt_guard_t *guard);

  /**
   * @brief Clear the flag so waits proceed again.
   *
   * @param guard Guard; NULL is still honoured, for the reason above.
   *
   * @code
   * >>> from doppler.interrupt import Interrupt
   * >>> it = Interrupt()
   * >>> it.interrupt()
   * >>> it.resume()
   * >>> it.interrupted()
   * 0
   * @endcode
   */
  void dp_interrupt_guard_resume (dp_interrupt_guard_t *guard);

#ifdef __cplusplus
}
#endif

#endif /* DP_INTERRUPT_H */
