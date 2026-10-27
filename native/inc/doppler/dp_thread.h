/**
 * @file dp_thread.h
 * @brief The threading primitives doppler uses, with one platform split.
 *
 * A mutex, a condition variable, a joinable thread and a core count. That is
 * the entire set doppler needs -- `dp_tlm_capture` (a writer thread behind a
 * mutex and two condvars) and `dp_parallel` (a bounded parallel-for) both use
 * exactly these and nothing more.
 *
 * Why a shim rather than `#ifdef` at each call site: those two files carry
 * **thirty-three** and **twenty-odd** pthread calls respectively, and
 * scattering a platform conditional through a working producer/consumer is
 * how a deadlock gets introduced by a port that "only changed includes". One
 * home, both platforms, and the call sites stay readable.
 *
 * The Windows side is Vista-era and deliberately not `pthreads-win32`:
 *
 * | pthread                  | Win32                                  |
 * |--------------------------|----------------------------------------|
 * | `pthread_mutex_t`        | `SRWLOCK` (smaller and faster than a   |
 * |                          | `CRITICAL_SECTION`, and doppler never  |
 * |                          | needs recursion)                       |
 * | `pthread_cond_t`         | `CONDITION_VARIABLE`                   |
 * | `pthread_cond_wait`      | `SleepConditionVariableSRW`            |
 * | `pthread_create`/`join`  | `_beginthreadex` / `WaitForSingleObject` |
 * | `sysconf(_SC_NPROCESSORS_ONLN)` | `GetActiveProcessorCount`       |
 * | `pthread_once`           | `InitOnceExecuteOnce`, both behind     |
 * |                          | dp_once() (dp_once.h, included here)   |
 *
 * A one-time build has its own header and its own dp_once.c because the
 * tables it guards live in components that start no thread (AWGN, LO,
 * CCSDS): they include dp_once.h alone, and `<windows.h>` stays inside
 * dp_once.c (doppler#2328).
 *
 * `_beginthreadex` rather than `CreateThread`: a thread that touches the CRT
 * (these do -- malloc, and the capture writer does file I/O) needs the CRT's
 * per-thread state initialised, and `CreateThread` does not do it.
 *
 * Neither `SRWLOCK` nor `CONDITION_VARIABLE` has a destructor, so the destroy
 * calls are no-ops on Windows rather than absent -- the call sites keep their
 * symmetry and a reader is not left wondering which platform leaked.
 */
#ifndef DP_THREAD_H
#define DP_THREAD_H

#include "doppler/dp_once.h"

#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#include <process.h>
#include <windows.h>

/** @brief A non-recursive mutex (an SRWLOCK on Windows). */
typedef SRWLOCK            dp_mutex_t;
/** @brief A condition variable, waited on with a dp_mutex_t held. */
typedef CONDITION_VARIABLE dp_cond_t;
/** @brief A joinable thread handle (from _beginthreadex on Windows). */
typedef HANDLE             dp_thread_t;

/** @brief Declares a thread entry point portably.
    `arg` is a declarator name, so it cannot be parenthesised.
    NOLINTNEXTLINE(bugprone-macro-parentheses) */
#define DP_THREAD_FN(name, arg) static unsigned __stdcall name (void *arg)
/** @brief Returns from a #DP_THREAD_FN body. */
#define DP_THREAD_RETURN return 0

/** @brief Initializes @p m, unlocked.
 *  @param m  Storage for the mutex. */
static inline void
dp_mutex_init (dp_mutex_t *m)
{
  InitializeSRWLock (m);
}
/** @brief Releases @p m (a no-op on Windows). @p m must be unlocked.
 *  @param m  Mutex from dp_mutex_init(). */
static inline void
dp_mutex_destroy (dp_mutex_t *m)
{
  (void)m; /* an SRWLOCK owns nothing to release */
}
/** @brief Locks @p m, blocking until it is free. Not recursive: locking
 *  a mutex this thread already holds deadlocks or is undefined.
 *  @param m  Mutex from dp_mutex_init(). */
static inline void
dp_mutex_lock (dp_mutex_t *m)
{
  AcquireSRWLockExclusive (m);
}
/** @brief Unlocks @p m, which this thread must hold.
 *  @param m  Mutex from dp_mutex_init(). */
static inline void
dp_mutex_unlock (dp_mutex_t *m)
{
  ReleaseSRWLockExclusive (m);
}

/** @brief Initializes @p c with no waiters.
 *  @param c  Storage for the condition variable. */
static inline void
dp_cond_init (dp_cond_t *c)
{
  InitializeConditionVariable (c);
}
/** @brief Releases @p c (a no-op on Windows); no thread may be waiting.
 *  @param c  Condition variable from dp_cond_init(). */
static inline void
dp_cond_destroy (dp_cond_t *c)
{
  (void)c; /* likewise */
}
/** @brief Atomically unlocks @p m and waits on @p c, relocking @p m
 *  before returning. No timeout. Wakeups may be spurious, so wait in a
 *  loop that re-checks the predicate.
 *  @param c  Condition variable to wait on.
 *  @param m  Mutex the caller holds. */
static inline void
dp_cond_wait (dp_cond_t *c, dp_mutex_t *m)
{
  SleepConditionVariableSRW (c, m, INFINITE, 0);
}
/** @brief Wakes at most one thread waiting on @p c.
 *  @param c  Condition variable from dp_cond_init(). */
static inline void
dp_cond_signal (dp_cond_t *c)
{
  WakeConditionVariable (c);
}
/** @brief Wakes every thread waiting on @p c.
 *  @param c  Condition variable from dp_cond_init(). */
static inline void
dp_cond_broadcast (dp_cond_t *c)
{
  WakeAllConditionVariable (c);
}

/** @brief Starts @p fn on a new thread. Returns 0 on success.
 *  @param[out] t    Handle of the new thread; join it with
 *              dp_thread_join().
 *  @param fn   Entry point, declared with #DP_THREAD_FN.
 *  @param arg  Passed to @p fn unchanged.
 *  @return 0 on success, -1 if the thread could not be started. */
static inline int
dp_thread_create (dp_thread_t *t, unsigned (__stdcall *fn) (void *), void *arg)
{
  uintptr_t h = _beginthreadex (NULL, 0, fn, arg, 0, NULL);
  if (!h)
    return -1;
  *t = (HANDLE)h;
  return 0;
}

/** @brief Waits for @p t to finish and releases it. Returns 0 on success,
 *  as pthread_join does, so a caller that asserted the join can still.
 *  @param t  Thread from dp_thread_create(); not usable afterward.
 *  @return 0 on success, -1 if the wait or the handle close failed. */
static inline int
dp_thread_join (dp_thread_t t)
{
  DWORD waited = WaitForSingleObject (t, INFINITE);
  BOOL  closed = CloseHandle (t);
  return (waited == WAIT_OBJECT_0 && closed) ? 0 : -1;
}

/** @brief Online processor count, or 1 if it cannot be determined.
 *  @return Number of online logical processors, always >= 1. */
static inline int
dp_cpu_count (void)
{
  DWORD n = GetActiveProcessorCount (ALL_PROCESSOR_GROUPS);
  return n ? (int)n : 1;
}

/** @brief Gives up the rest of this thread's time slice. */
static inline void
dp_thread_yield (void)
{
  SwitchToThread ();
}

/** @brief Sleeps for at least @p us microseconds.
 *
 *  AT LEAST, and on Windows usually longer: Sleep() takes milliseconds and
 *  honours the system timer, 15.6 ms by default. So this rounds up to one
 *  millisecond and may well take a tick. Use it for "let the other thread
 *  get there", never for timing -- timing_core's waitable timer is the
 *  precise sleep.
 *  @param us  Minimum sleep, microseconds. */
static inline void
dp_thread_sleep_us (unsigned us)
{
  Sleep ((DWORD)((us + 999u) / 1000u));
}

#else /* POSIX */

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

/** @brief A non-recursive mutex (a default pthread_mutex_t on POSIX). */
typedef pthread_mutex_t dp_mutex_t;
/** @brief A condition variable, waited on with a dp_mutex_t held. */
typedef pthread_cond_t  dp_cond_t;
/** @brief A joinable thread handle (a pthread_t on POSIX). */
typedef pthread_t       dp_thread_t;

/** @brief Declares a thread entry point portably.
    `arg` is a declarator name, so it cannot be parenthesised.
    NOLINTNEXTLINE(bugprone-macro-parentheses) */
#define DP_THREAD_FN(name, arg) static void *name (void *arg)
/** @brief Returns from a #DP_THREAD_FN body. */
#define DP_THREAD_RETURN return NULL

/** @brief Initializes @p m, unlocked.
 *  @param m  Storage for the mutex. */
static inline void
dp_mutex_init (dp_mutex_t *m)
{
  pthread_mutex_init (m, NULL);
}
/** @brief Releases @p m (a no-op on Windows). @p m must be unlocked.
 *  @param m  Mutex from dp_mutex_init(). */
static inline void
dp_mutex_destroy (dp_mutex_t *m)
{
  pthread_mutex_destroy (m);
}
/** @brief Locks @p m, blocking until it is free. Not recursive: locking
 *  a mutex this thread already holds deadlocks or is undefined.
 *  @param m  Mutex from dp_mutex_init(). */
static inline void
dp_mutex_lock (dp_mutex_t *m)
{
  pthread_mutex_lock (m);
}
/** @brief Unlocks @p m, which this thread must hold.
 *  @param m  Mutex from dp_mutex_init(). */
static inline void
dp_mutex_unlock (dp_mutex_t *m)
{
  pthread_mutex_unlock (m);
}

/** @brief Initializes @p c with no waiters.
 *  @param c  Storage for the condition variable. */
static inline void
dp_cond_init (dp_cond_t *c)
{
  pthread_cond_init (c, NULL);
}
/** @brief Releases @p c (a no-op on Windows); no thread may be waiting.
 *  @param c  Condition variable from dp_cond_init(). */
static inline void
dp_cond_destroy (dp_cond_t *c)
{
  pthread_cond_destroy (c);
}
/** @brief Atomically unlocks @p m and waits on @p c, relocking @p m
 *  before returning. No timeout. Wakeups may be spurious, so wait in a
 *  loop that re-checks the predicate.
 *  @param c  Condition variable to wait on.
 *  @param m  Mutex the caller holds. */
static inline void
dp_cond_wait (dp_cond_t *c, dp_mutex_t *m)
{
  pthread_cond_wait (c, m);
}
/** @brief Wakes at most one thread waiting on @p c.
 *  @param c  Condition variable from dp_cond_init(). */
static inline void
dp_cond_signal (dp_cond_t *c)
{
  pthread_cond_signal (c);
}
/** @brief Wakes every thread waiting on @p c.
 *  @param c  Condition variable from dp_cond_init(). */
static inline void
dp_cond_broadcast (dp_cond_t *c)
{
  pthread_cond_broadcast (c);
}

/** @brief Starts @p fn on a new thread. Returns 0 on success.
 *  @param[out] t    Handle of the new thread; join it with
 *              dp_thread_join().
 *  @param fn   Entry point, declared with #DP_THREAD_FN.
 *  @param arg  Passed to @p fn unchanged.
 *  @return 0 on success, else pthread_create()'s error number. */
static inline int
dp_thread_create (dp_thread_t *t, void *(*fn) (void *), void *arg)
{
  return pthread_create (t, NULL, fn, arg);
}

/** @brief Waits for @p t to finish and releases it. Returns 0 on success,
 *  as pthread_join does, so a caller that asserted the join can still.
 *  @param t  Thread from dp_thread_create(); not usable afterward.
 *  @return 0 on success, else pthread_join()'s error number. */
static inline int
dp_thread_join (dp_thread_t t)
{
  return pthread_join (t, NULL);
}

/** @brief Online processor count, or 1 if it cannot be determined.
 *  @return Number of online logical processors, always >= 1. */
static inline int
dp_cpu_count (void)
{
  long n = sysconf (_SC_NPROCESSORS_ONLN);
  return n > 0 ? (int)n : 1;
}

/** @brief Gives up the rest of this thread's time slice. */
static inline void
dp_thread_yield (void)
{
  sched_yield ();
}

/** @brief Sleeps for at least @p us microseconds. For "let the other thread
 *  get there", never for timing. A signal does not cut the sleep short:
 *  nanosleep() reports the time it had left, and the sleep resumes with
 *  that remainder until it is spent (#2647).
 *  @param us  Requested sleep, microseconds. */
static inline void
dp_thread_sleep_us (unsigned us)
{
  struct timespec ts;
  ts.tv_sec  = (time_t)(us / 1000000u);
  ts.tv_nsec = (long)(us % 1000000u) * 1000L;
  /* nanosleep writes the unslept remainder into its second argument when a
     signal interrupts it, so passing ts for both resumes where it stopped.
     Any failure other than EINTR (EINVAL cannot arise from these fields)
     ends the loop rather than spinning. */
  while (nanosleep (&ts, &ts) != 0 && errno == EINTR)
    ;
}

#endif /* _WIN32 */

#endif /* DP_THREAD_H */
