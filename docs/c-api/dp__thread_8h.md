

# File dp\_thread.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_thread.h**](dp__thread_8h.md)

[Go to the source code of this file](dp__thread_8h_source.md)

_The threading primitives doppler uses, with one platform split._ [More...](#detailed-description)

* `#include "doppler/dp_once.h"`
* `#include <errno.h>`
* `#include <pthread.h>`
* `#include <sched.h>`
* `#include <time.h>`
* `#include <unistd.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef pthread\_cond\_t | [**dp\_cond\_t**](#typedef-dp_cond_t)  <br>_A condition variable, waited on with a dp\_mutex\_t held._  |
| typedef pthread\_mutex\_t | [**dp\_mutex\_t**](#typedef-dp_mutex_t)  <br>_A non-recursive mutex (a default pthread\_mutex\_t on POSIX)._  |
| typedef pthread\_t | [**dp\_thread\_t**](#typedef-dp_thread_t)  <br>_A joinable thread handle (a pthread\_t on POSIX)._  |






















## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_cond\_broadcast**](#function-dp_cond_broadcast) ([**dp\_cond\_t**](dp__thread_8h.md#typedef-dp_cond_t) \* c) <br>_Wakes every thread waiting on_ `c` _._ |
|  void | [**dp\_cond\_destroy**](#function-dp_cond_destroy) ([**dp\_cond\_t**](dp__thread_8h.md#typedef-dp_cond_t) \* c) <br>_Releases_ `c` _(a no-op on Windows); no thread may be waiting._ |
|  void | [**dp\_cond\_init**](#function-dp_cond_init) ([**dp\_cond\_t**](dp__thread_8h.md#typedef-dp_cond_t) \* c) <br>_Initializes_ `c` _with no waiters._ |
|  void | [**dp\_cond\_signal**](#function-dp_cond_signal) ([**dp\_cond\_t**](dp__thread_8h.md#typedef-dp_cond_t) \* c) <br>_Wakes at most one thread waiting on_ `c` _._ |
|  void | [**dp\_cond\_wait**](#function-dp_cond_wait) ([**dp\_cond\_t**](dp__thread_8h.md#typedef-dp_cond_t) \* c, [**dp\_mutex\_t**](dp__thread_8h.md#typedef-dp_mutex_t) \* m) <br>_Atomically unlocks_ `m` _and waits on_`c` _, relocking_`m` _before returning. No timeout. Wakeups may be spurious, so wait in a loop that re-checks the predicate._ |
|  int | [**dp\_cpu\_count**](#function-dp_cpu_count) (void) <br>_Online processor count, or 1 if it cannot be determined._  |
|  void | [**dp\_mutex\_destroy**](#function-dp_mutex_destroy) ([**dp\_mutex\_t**](dp__thread_8h.md#typedef-dp_mutex_t) \* m) <br>_Releases_ `m` _(a no-op on Windows)._`m` _must be unlocked._ |
|  void | [**dp\_mutex\_init**](#function-dp_mutex_init) ([**dp\_mutex\_t**](dp__thread_8h.md#typedef-dp_mutex_t) \* m) <br>_Initializes_ `m` _, unlocked._ |
|  void | [**dp\_mutex\_lock**](#function-dp_mutex_lock) ([**dp\_mutex\_t**](dp__thread_8h.md#typedef-dp_mutex_t) \* m) <br>_Locks_ `m` _, blocking until it is free. Not recursive: locking a mutex this thread already holds deadlocks or is undefined._ |
|  void | [**dp\_mutex\_unlock**](#function-dp_mutex_unlock) ([**dp\_mutex\_t**](dp__thread_8h.md#typedef-dp_mutex_t) \* m) <br>_Unlocks_ `m` _, which this thread must hold._ |
|  int | [**dp\_thread\_create**](#function-dp_thread_create) ([**dp\_thread\_t**](dp__thread_8h.md#typedef-dp_thread_t) \* t, void \*(\*)(void \*) fn, void \* arg) <br>_Starts_ `fn` _on a new thread. Returns 0 on success._ |
|  int | [**dp\_thread\_join**](#function-dp_thread_join) ([**dp\_thread\_t**](dp__thread_8h.md#typedef-dp_thread_t) t) <br>_Waits for_ `t` _to finish and releases it. Returns 0 on success, as pthread\_join does, so a caller that asserted the join can still._ |
|  void | [**dp\_thread\_sleep\_us**](#function-dp_thread_sleep_us) (unsigned us) <br>_Sleeps for at least_ `us` _microseconds. For "let the other thread
get there", never for timing. A signal does not cut the sleep short: nanosleep() reports the time it had left, and the sleep resumes with that remainder until it is spent (#2647)._ |
|  void | [**dp\_thread\_yield**](#function-dp_thread_yield) (void) <br>_Gives up the rest of this thread's time slice._  |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_THREAD\_FN**](dp__thread_8h.md#define-dp_thread_fn) (name, arg) `static void \*name (void \*arg)`<br>_Declares a thread entry point portably._ `arg` _is a declarator name, so it cannot be parenthesised. NOLINTNEXTLINE(bugprone-macro-parentheses)_ |
| define  | [**DP\_THREAD\_RETURN**](dp__thread_8h.md#define-dp_thread_return)  `return NULL`<br>_Returns from a_ [_**DP\_THREAD\_FN**_](dp__thread_8h.md#define-dp_thread_fn) _body._ |

## Detailed Description


A mutex, a condition variable, a joinable thread and a core count. That is the entire set doppler needs  `dp_tlm_capture` (a writer thread behind a mutex and two condvars) and `dp_parallel` (a bounded parallel-for) both use exactly these and nothing more.


Why a shim rather than `#ifdef` at each call site: those two files carry **thirty-three** and **twenty-odd** pthread calls respectively, and scattering a platform conditional through a working producer/consumer is how a deadlock gets introduced by a port that "only changed includes". One home, both platforms, and the call sites stay readable.


The Windows side is Vista-era and deliberately not `pthreads-win32`:



|pthread   |Win32    |
|-----|-----|
|`pthread_mutex_t`   |`SRWLOCK` (smaller and faster than a    |
||`CRITICAL_SECTION`, and doppler never    |
||needs recursion)    |
|`pthread_cond_t`   |`CONDITION_VARIABLE`    |
|`pthread_cond_wait`   |`SleepConditionVariableSRW`    |
|`pthread_create`/`join`   |`_beginthreadex` / `WaitForSingleObject`    |
|`sysconf(_SC_NPROCESSORS_ONLN)`   |`GetActiveProcessorCount`    |
|`pthread_once`   |`InitOnceExecuteOnce`, both behind    |
||[**dp\_once()**](dp__once_8h.md#function-dp_once) ([**dp\_once.h**](dp__once_8h.md), included here)   |






A one-time build has its own header and its own dp\_once.c because the tables it guards live in components that start no thread (AWGN, LO, CCSDS): they include [**dp\_once.h**](dp__once_8h.md) alone, and `<windows.h>` stays inside dp\_once.c (doppler#2328).


`_beginthreadex` rather than `CreateThread`: a thread that touches the CRT (these do  malloc, and the capture writer does file I/O) needs the CRT's per-thread state initialised, and `CreateThread` does not do it.


Neither `SRWLOCK` nor `CONDITION_VARIABLE` has a destructor, so the destroy calls are no-ops on Windows rather than absent  the call sites keep their symmetry and a reader is not left wondering which platform leaked. 


    
## Public Types Documentation




### typedef dp\_cond\_t 

_A condition variable, waited on with a dp\_mutex\_t held._ 
```C++
typedef pthread_cond_t dp_cond_t;
```




<hr>



### typedef dp\_mutex\_t 

_A non-recursive mutex (a default pthread\_mutex\_t on POSIX)._ 
```C++
typedef pthread_mutex_t dp_mutex_t;
```




<hr>



### typedef dp\_thread\_t 

_A joinable thread handle (a pthread\_t on POSIX)._ 
```C++
typedef pthread_t dp_thread_t;
```




<hr>
## Public Static Functions Documentation




### function dp\_cond\_broadcast 

_Wakes every thread waiting on_ `c` _._
```C++
static inline void dp_cond_broadcast (
    dp_cond_t * c
) 
```





**Parameters:**


* `c` Condition variable from dp\_cond\_init(). 




        

<hr>



### function dp\_cond\_destroy 

_Releases_ `c` _(a no-op on Windows); no thread may be waiting._
```C++
static inline void dp_cond_destroy (
    dp_cond_t * c
) 
```





**Parameters:**


* `c` Condition variable from dp\_cond\_init(). 




        

<hr>



### function dp\_cond\_init 

_Initializes_ `c` _with no waiters._
```C++
static inline void dp_cond_init (
    dp_cond_t * c
) 
```





**Parameters:**


* `c` Storage for the condition variable. 




        

<hr>



### function dp\_cond\_signal 

_Wakes at most one thread waiting on_ `c` _._
```C++
static inline void dp_cond_signal (
    dp_cond_t * c
) 
```





**Parameters:**


* `c` Condition variable from dp\_cond\_init(). 




        

<hr>



### function dp\_cond\_wait 

_Atomically unlocks_ `m` _and waits on_`c` _, relocking_`m` _before returning. No timeout. Wakeups may be spurious, so wait in a loop that re-checks the predicate._
```C++
static inline void dp_cond_wait (
    dp_cond_t * c,
    dp_mutex_t * m
) 
```





**Parameters:**


* `c` Condition variable to wait on. 
* `m` Mutex the caller holds. 




        

<hr>



### function dp\_cpu\_count 

_Online processor count, or 1 if it cannot be determined._ 
```C++
static inline int dp_cpu_count (
    void
) 
```





**Returns:**

Number of online logical processors, always &gt;= 1. 





        

<hr>



### function dp\_mutex\_destroy 

_Releases_ `m` _(a no-op on Windows)._`m` _must be unlocked._
```C++
static inline void dp_mutex_destroy (
    dp_mutex_t * m
) 
```





**Parameters:**


* `m` Mutex from dp\_mutex\_init(). 




        

<hr>



### function dp\_mutex\_init 

_Initializes_ `m` _, unlocked._
```C++
static inline void dp_mutex_init (
    dp_mutex_t * m
) 
```





**Parameters:**


* `m` Storage for the mutex. 




        

<hr>



### function dp\_mutex\_lock 

_Locks_ `m` _, blocking until it is free. Not recursive: locking a mutex this thread already holds deadlocks or is undefined._
```C++
static inline void dp_mutex_lock (
    dp_mutex_t * m
) 
```





**Parameters:**


* `m` Mutex from dp\_mutex\_init(). 




        

<hr>



### function dp\_mutex\_unlock 

_Unlocks_ `m` _, which this thread must hold._
```C++
static inline void dp_mutex_unlock (
    dp_mutex_t * m
) 
```





**Parameters:**


* `m` Mutex from dp\_mutex\_init(). 




        

<hr>



### function dp\_thread\_create 

_Starts_ `fn` _on a new thread. Returns 0 on success._
```C++
static inline int dp_thread_create (
    dp_thread_t * t,
    void *(*)(void *) fn,
    void * arg
) 
```





**Parameters:**


* `t` Handle of the new thread; join it with dp\_thread\_join(). 
* `fn` Entry point, declared with [**DP\_THREAD\_FN**](dp__thread_8h.md#define-dp_thread_fn). 
* `arg` Passed to `fn` unchanged. 



**Returns:**

0 on success, else pthread\_create()'s error number. 





        

<hr>



### function dp\_thread\_join 

_Waits for_ `t` _to finish and releases it. Returns 0 on success, as pthread\_join does, so a caller that asserted the join can still._
```C++
static inline int dp_thread_join (
    dp_thread_t t
) 
```





**Parameters:**


* `t` Thread from dp\_thread\_create(); not usable afterward. 



**Returns:**

0 on success, else pthread\_join()'s error number. 





        

<hr>



### function dp\_thread\_sleep\_us 

_Sleeps for at least_ `us` _microseconds. For "let the other thread
get there", never for timing. A signal does not cut the sleep short: nanosleep() reports the time it had left, and the sleep resumes with that remainder until it is spent (#2647)._
```C++
static inline void dp_thread_sleep_us (
    unsigned us
) 
```





**Parameters:**


* `us` Requested sleep, microseconds. 




        

<hr>



### function dp\_thread\_yield 

_Gives up the rest of this thread's time slice._ 
```C++
static inline void dp_thread_yield (
    void
) 
```




<hr>
## Macro Definition Documentation





### define DP\_THREAD\_FN 

_Declares a thread entry point portably._ `arg` _is a declarator name, so it cannot be parenthesised. NOLINTNEXTLINE(bugprone-macro-parentheses)_
```C++
#define DP_THREAD_FN (
    name,
    arg
) `static void *name (void *arg)`
```




<hr>



### define DP\_THREAD\_RETURN 

_Returns from a_ [_**DP\_THREAD\_FN**_](dp__thread_8h.md#define-dp_thread_fn) _body._
```C++
#define DP_THREAD_RETURN `return NULL`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dp_thread.h`

