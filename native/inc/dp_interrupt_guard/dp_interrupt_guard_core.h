/* dp_interrupt_guard_core.h — the object face of the interrupt facility.
 *
 * The flag itself lives in dp_interrupt.h, a root header three modules
 * include. This is the component that binds it: a scoped handle whose
 * create arms and whose destroy restores exactly what it armed.
 *
 * See docs/design/io-termination.md.
 */
#ifndef DP_INTERRUPT_GUARD_CORE_H
#define DP_INTERRUPT_GUARD_CORE_H

#include "dp_interrupt.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

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

  /* jm derives a component's state type as <comp>_state_t, and doppler's
     public spelling is dp_interrupt_guard_t. One alias bridges them until
     just-makeit#797 lands `state_type`; dp_tlm_core.h carries the same
     line for the same reason. */
  typedef dp_interrupt_guard_t dp_interrupt_guard_state_t;

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
   *                   alone, and only a non-zero value is restored. Fixed
   *                   width rather than `unsigned`, because a public ABI
   *                   should not carry a platform-dependent one.
   * @return A guard, or NULL if a handler could not be installed -- in
   *         which case any already installed by this call are restored
   *         first, so a failed create arms nothing.
   *
   * @code
   * >>> from doppler.interrupt import Interrupt
   * >>> it = Interrupt([])
   * >>> it.interrupted()
   * 0
   * @endcode
   */
  dp_interrupt_guard_t *dp_interrupt_guard_create (const int32_t *signals,
                                                   size_t     n_signals,
                                                   uint32_t   latency_ms);

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
   * >>> it = Interrupt([])
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
   *
   * @code
   * >>> from doppler.interrupt import Interrupt
   * >>> import numpy as np
   * >>> it = Interrupt(np.array([], dtype=np.int32))
   * >>> it.interrupted()
   * 0
   * >>> it.interrupt()
   * >>> it.interrupted()
   * 1
   * @endcode
   */
  int dp_interrupt_guard_interrupted (const dp_interrupt_guard_t *guard);

  /**
   * @brief Clear the flag so waits proceed again.
   *
   * @param guard Guard; NULL is still honoured, for the reason above.
   *
   * @code
   * >>> from doppler.interrupt import Interrupt
   * >>> it = Interrupt([])
   * >>> it.interrupt()
   * >>> it.resume()
   * >>> it.interrupted()
   * 0
   * @endcode
   */
  void dp_interrupt_guard_resume (dp_interrupt_guard_t *guard);

  /**
   * @brief The wait slice every blocking wait in this process uses.
   *
   * The readback for the constructor's `latency_ms`, and it reads the
   * PROCESS setting rather than what this guard asked for -- those differ
   * when the guard passed 0, which means "leave it alone". A value a
   * caller can set and not read back is a value they cannot reason about.
   *
   * @param guard Guard; NULL reads the process setting anyway.
   * @return Milliseconds.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.interrupt import Interrupt
   * >>> it = Interrupt(np.array([], dtype=np.int32), latency_ms=25)
   * >>> it.latency_ms()
   * 25
   * @endcode
   */
  uint32_t dp_interrupt_guard_latency_ms (const dp_interrupt_guard_t *guard);

#ifdef __cplusplus
}
#endif

#endif /* DP_INTERRUPT_GUARD_CORE_H */
