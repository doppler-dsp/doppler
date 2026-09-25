

# File dp\_thread.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_thread.h**](dp__thread_8h.md)

[Go to the source code of this file](dp__thread_8h_source.md)

_The threading primitives doppler uses, with one platform split._ [More...](#detailed-description)

* `#include <pthread.h>`
* `#include <sched.h>`
* `#include <time.h>`
* `#include <unistd.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef pthread\_cond\_t | [**dp\_cond\_t**](#typedef-dp_cond_t)  <br> |
| typedef pthread\_mutex\_t | [**dp\_mutex\_t**](#typedef-dp_mutex_t)  <br> |
| typedef pthread\_t | [**dp\_thread\_t**](#typedef-dp_thread_t)  <br> |






















## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_cond\_broadcast**](#function-dp_cond_broadcast) ([**dp\_cond\_t**](dp__thread_8h.md#typedef-dp_cond_t) \* c) <br> |
|  void | [**dp\_cond\_destroy**](#function-dp_cond_destroy) ([**dp\_cond\_t**](dp__thread_8h.md#typedef-dp_cond_t) \* c) <br> |
|  void | [**dp\_cond\_init**](#function-dp_cond_init) ([**dp\_cond\_t**](dp__thread_8h.md#typedef-dp_cond_t) \* c) <br> |
|  void | [**dp\_cond\_signal**](#function-dp_cond_signal) ([**dp\_cond\_t**](dp__thread_8h.md#typedef-dp_cond_t) \* c) <br> |
|  void | [**dp\_cond\_wait**](#function-dp_cond_wait) ([**dp\_cond\_t**](dp__thread_8h.md#typedef-dp_cond_t) \* c, [**dp\_mutex\_t**](dp__thread_8h.md#typedef-dp_mutex_t) \* m) <br> |
|  int | [**dp\_cpu\_count**](#function-dp_cpu_count) (void) <br>_Online processor count, or 1 if it cannot be determined._  |
|  void | [**dp\_mutex\_destroy**](#function-dp_mutex_destroy) ([**dp\_mutex\_t**](dp__thread_8h.md#typedef-dp_mutex_t) \* m) <br> |
|  void | [**dp\_mutex\_init**](#function-dp_mutex_init) ([**dp\_mutex\_t**](dp__thread_8h.md#typedef-dp_mutex_t) \* m) <br> |
|  void | [**dp\_mutex\_lock**](#function-dp_mutex_lock) ([**dp\_mutex\_t**](dp__thread_8h.md#typedef-dp_mutex_t) \* m) <br> |
|  void | [**dp\_mutex\_unlock**](#function-dp_mutex_unlock) ([**dp\_mutex\_t**](dp__thread_8h.md#typedef-dp_mutex_t) \* m) <br> |
|  int | [**dp\_thread\_create**](#function-dp_thread_create) ([**dp\_thread\_t**](dp__thread_8h.md#typedef-dp_thread_t) \* t, void \*(\*)(void \*) fn, void \* arg) <br>_Starts_ `fn` _on a new thread. Returns 0 on success._ |
|  int | [**dp\_thread\_join**](#function-dp_thread_join) ([**dp\_thread\_t**](dp__thread_8h.md#typedef-dp_thread_t) t) <br>_Waits for_ `t` _to finish and releases it. Returns 0 on success, as pthread\_join does, so a caller that asserted the join can still._ |
|  void | [**dp\_thread\_sleep\_us**](#function-dp_thread_sleep_us) (unsigned us) <br>_Sleeps for at least_ `us` _microseconds. For "let the other thread
get there", never for timing._ |
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
|`sysconf(_SC_NPROCESSORS_ONLN)`   |`GetActiveProcessorCount`   |






`_beginthreadex` rather than `CreateThread`: a thread that touches the CRT (these do  malloc, and the capture writer does file I/O) needs the CRT's per-thread state initialised, and `CreateThread` does not do it.


Neither `SRWLOCK` nor `CONDITION_VARIABLE` has a destructor, so the destroy calls are no-ops on Windows rather than absent  the call sites keep their symmetry and a reader is not left wondering which platform leaked. 


    
## Public Types Documentation




### typedef dp\_cond\_t 

```C++
typedef pthread_cond_t dp_cond_t;
```




<hr>



### typedef dp\_mutex\_t 

```C++
typedef pthread_mutex_t dp_mutex_t;
```




<hr>



### typedef dp\_thread\_t 

```C++
typedef pthread_t dp_thread_t;
```




<hr>
## Public Static Functions Documentation




### function dp\_cond\_broadcast 

```C++
static inline void dp_cond_broadcast (
    dp_cond_t * c
) 
```




<hr>



### function dp\_cond\_destroy 

```C++
static inline void dp_cond_destroy (
    dp_cond_t * c
) 
```




<hr>



### function dp\_cond\_init 

```C++
static inline void dp_cond_init (
    dp_cond_t * c
) 
```




<hr>



### function dp\_cond\_signal 

```C++
static inline void dp_cond_signal (
    dp_cond_t * c
) 
```




<hr>



### function dp\_cond\_wait 

```C++
static inline void dp_cond_wait (
    dp_cond_t * c,
    dp_mutex_t * m
) 
```




<hr>



### function dp\_cpu\_count 

_Online processor count, or 1 if it cannot be determined._ 
```C++
static inline int dp_cpu_count (
    void
) 
```




<hr>



### function dp\_mutex\_destroy 

```C++
static inline void dp_mutex_destroy (
    dp_mutex_t * m
) 
```




<hr>



### function dp\_mutex\_init 

```C++
static inline void dp_mutex_init (
    dp_mutex_t * m
) 
```




<hr>



### function dp\_mutex\_lock 

```C++
static inline void dp_mutex_lock (
    dp_mutex_t * m
) 
```




<hr>



### function dp\_mutex\_unlock 

```C++
static inline void dp_mutex_unlock (
    dp_mutex_t * m
) 
```




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




<hr>



### function dp\_thread\_join 

_Waits for_ `t` _to finish and releases it. Returns 0 on success, as pthread\_join does, so a caller that asserted the join can still._
```C++
static inline int dp_thread_join (
    dp_thread_t t
) 
```




<hr>



### function dp\_thread\_sleep\_us 

_Sleeps for at least_ `us` _microseconds. For "let the other thread
get there", never for timing._
```C++
static inline void dp_thread_sleep_us (
    unsigned us
) 
```




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

