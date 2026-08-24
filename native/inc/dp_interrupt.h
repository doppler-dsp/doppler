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
 * ONE flag per PROCESS, and in Python that takes a rendezvous. Each
 * extension module links this file statically and CPython imports
 * extensions `RTLD_LOCAL`, so every `.so` would otherwise hold its own
 * copy -- doppler#976, where a stop requested through `doppler.interrupt`
 * left a ring wait in `doppler.buffer` spinning on a different variable.
 * The state and the two accessors that share it live in `dp_interrupt.c`;
 * their names carry the `dp_interrupt_guard` prefix because the declared
 * COMPONENT is what just-makeit binds, and it generates the capsule
 * hand-off into every module's `PyInit_` from that declaration. Nothing
 * here is a C caller's concern: one archive means one copy.
 *
 * See `docs/design/io-termination.md` for the contract this is one third
 * of; the other two are end-of-stream and durable completion.
 */

#ifndef DP_INTERRUPT_H
#define DP_INTERRUPT_H

#include "clib_common.h"

#include <signal.h>
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
   * @brief Most signals this facility will handle at once.
   *
   * Not a budget anyone reasoned about -- it is "more signals than a
   * program sensibly interrupts on". Public because a guard records what
   * it armed in a fixed array of this size, and the two must agree.
   */
#define DP_INTERRUPT_MAX_SIGNALS 8u

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

#ifdef __cplusplus
}
#endif

#endif /* DP_INTERRUPT_H */
