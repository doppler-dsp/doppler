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

#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#include <process.h>
#include <windows.h>

typedef SRWLOCK            dp_mutex_t;
typedef CONDITION_VARIABLE dp_cond_t;
typedef HANDLE             dp_thread_t;

/** @brief Declares a thread entry point portably.
    `arg` is a declarator name, so it cannot be parenthesised.
    NOLINTNEXTLINE(bugprone-macro-parentheses) */
#define DP_THREAD_FN(name, arg) static unsigned __stdcall name (void *arg)
/** @brief Returns from a #DP_THREAD_FN body. */
#define DP_THREAD_RETURN return 0

static inline void
dp_mutex_init (dp_mutex_t *m)
{
  InitializeSRWLock (m);
}
static inline void
dp_mutex_destroy (dp_mutex_t *m)
{
  (void)m; /* an SRWLOCK owns nothing to release */
}
static inline void
dp_mutex_lock (dp_mutex_t *m)
{
  AcquireSRWLockExclusive (m);
}
static inline void
dp_mutex_unlock (dp_mutex_t *m)
{
  ReleaseSRWLockExclusive (m);
}

static inline void
dp_cond_init (dp_cond_t *c)
{
  InitializeConditionVariable (c);
}
static inline void
dp_cond_destroy (dp_cond_t *c)
{
  (void)c; /* likewise */
}
static inline void
dp_cond_wait (dp_cond_t *c, dp_mutex_t *m)
{
  SleepConditionVariableSRW (c, m, INFINITE, 0);
}
static inline void
dp_cond_signal (dp_cond_t *c)
{
  WakeConditionVariable (c);
}
static inline void
dp_cond_broadcast (dp_cond_t *c)
{
  WakeAllConditionVariable (c);
}

/** @brief Starts @p fn on a new thread. Returns 0 on success. */
static inline int
dp_thread_create (dp_thread_t *t, unsigned (__stdcall *fn) (void *), void *arg)
{
  uintptr_t h = _beginthreadex (NULL, 0, fn, arg, 0, NULL);
  if (!h)
    return -1;
  *t = (HANDLE)h;
  return 0;
}

/** @brief Waits for @p t to finish and releases it. */
static inline void
dp_thread_join (dp_thread_t t)
{
  WaitForSingleObject (t, INFINITE);
  CloseHandle (t);
}

/** @brief Online processor count, or 1 if it cannot be determined. */
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
 *  precise sleep. */
static inline void
dp_thread_sleep_us (unsigned us)
{
  Sleep ((DWORD)((us + 999u) / 1000u));
}

#else /* POSIX */

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

typedef pthread_mutex_t dp_mutex_t;
typedef pthread_cond_t  dp_cond_t;
typedef pthread_t       dp_thread_t;

/** @brief Declares a thread entry point portably.
    `arg` is a declarator name, so it cannot be parenthesised.
    NOLINTNEXTLINE(bugprone-macro-parentheses) */
#define DP_THREAD_FN(name, arg) static void *name (void *arg)
/** @brief Returns from a #DP_THREAD_FN body. */
#define DP_THREAD_RETURN return NULL

static inline void
dp_mutex_init (dp_mutex_t *m)
{
  pthread_mutex_init (m, NULL);
}
static inline void
dp_mutex_destroy (dp_mutex_t *m)
{
  pthread_mutex_destroy (m);
}
static inline void
dp_mutex_lock (dp_mutex_t *m)
{
  pthread_mutex_lock (m);
}
static inline void
dp_mutex_unlock (dp_mutex_t *m)
{
  pthread_mutex_unlock (m);
}

static inline void
dp_cond_init (dp_cond_t *c)
{
  pthread_cond_init (c, NULL);
}
static inline void
dp_cond_destroy (dp_cond_t *c)
{
  pthread_cond_destroy (c);
}
static inline void
dp_cond_wait (dp_cond_t *c, dp_mutex_t *m)
{
  pthread_cond_wait (c, m);
}
static inline void
dp_cond_signal (dp_cond_t *c)
{
  pthread_cond_signal (c);
}
static inline void
dp_cond_broadcast (dp_cond_t *c)
{
  pthread_cond_broadcast (c);
}

/** @brief Starts @p fn on a new thread. Returns 0 on success. */
static inline int
dp_thread_create (dp_thread_t *t, void *(*fn) (void *), void *arg)
{
  return pthread_create (t, NULL, fn, arg);
}

/** @brief Waits for @p t to finish and releases it. */
static inline void
dp_thread_join (dp_thread_t t)
{
  pthread_join (t, NULL);
}

/** @brief Online processor count, or 1 if it cannot be determined. */
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
 *  get there", never for timing. */
static inline void
dp_thread_sleep_us (unsigned us)
{
  struct timespec ts;
  ts.tv_sec  = (time_t)(us / 1000000u);
  ts.tv_nsec = (long)(us % 1000000u) * 1000L;
  nanosleep (&ts, NULL);
}

#endif /* _WIN32 */

#endif /* DP_THREAD_H */
