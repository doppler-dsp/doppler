

# File dp\_thread.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_thread.h**](dp__thread_8h.md)

[Go to the documentation of this file](dp__thread_8h.md)


```C++

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

#define DP_THREAD_FN(name, arg) static unsigned __stdcall name (void *arg)
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

static inline int
dp_thread_create (dp_thread_t *t, unsigned (__stdcall *fn) (void *), void *arg)
{
  uintptr_t h = _beginthreadex (NULL, 0, fn, arg, 0, NULL);
  if (!h)
    return -1;
  *t = (HANDLE)h;
  return 0;
}

static inline void
dp_thread_join (dp_thread_t t)
{
  WaitForSingleObject (t, INFINITE);
  CloseHandle (t);
}

static inline int
dp_cpu_count (void)
{
  DWORD n = GetActiveProcessorCount (ALL_PROCESSOR_GROUPS);
  return n ? (int)n : 1;
}

#else /* POSIX */

#include <pthread.h>
#include <unistd.h>

typedef pthread_mutex_t dp_mutex_t;
typedef pthread_cond_t  dp_cond_t;
typedef pthread_t       dp_thread_t;

#define DP_THREAD_FN(name, arg) static void *name (void *arg)
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

static inline int
dp_thread_create (dp_thread_t *t, void *(*fn) (void *), void *arg)
{
  return pthread_create (t, NULL, fn, arg);
}

static inline void
dp_thread_join (dp_thread_t t)
{
  pthread_join (t, NULL);
}

static inline int
dp_cpu_count (void)
{
  long n = sysconf (_SC_NPROCESSORS_ONLN);
  return n > 0 ? (int)n : 1;
}

#endif /* _WIN32 */

#endif /* DP_THREAD_H */
```


