

# File dp\_parallel.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_parallel.h**](dp__parallel_8h.md)

[Go to the source code of this file](dp__parallel_8h_source.md)

_A minimal bounded parallel-for over an index range, and a persistent pool with the same contract._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_thread.h"`
* `#include <stdatomic.h>`
* `#include <stddef.h>`
* `#include <stdlib.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_pf\_shared\_t**](structdp__pf__shared__t.md) <br>_One parallel-for run: the range, the work, and the shared cursor._  |
| struct | [**dp\_pool\_t**](structdp__pool__t.md) <br>_A persistent worker pool for repeated parallel-for runs._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|   | [**DP\_THREAD\_FN**](#function-dp_thread_fn) (dp\_pf\_worker, arg) <br> |
|   | [**DP\_THREAD\_FN**](#function-dp_thread_fn) (dp\_pool\_worker, arg) <br> |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_parallel\_for**](#function-dp_parallel_for) (size\_t n, void(\*)(size\_t, void \*) body, void \* ctx, int max\_threads) <br>_Run body(i, ctx) for every i in [0, n) across up to max\_threads workers._  |
|  [**dp\_pool\_t**](structdp__pool__t.md) \* | [**dp\_pool\_create**](#function-dp_pool_create) (int max\_threads) <br>_Create a pool of max\_threads workers (the caller counts as one)._  |
|  void | [**dp\_pool\_destroy**](#function-dp_pool_destroy) ([**dp\_pool\_t**](structdp__pool__t.md) \* p) <br>_Stop and join the helpers, free the pool._  |
|  void | [**dp\_pool\_run**](#function-dp_pool_run) ([**dp\_pool\_t**](structdp__pool__t.md) \* p, size\_t n, void(\*)(size\_t, void \*) body, void \* ctx) <br>_Run body(i, ctx) for every i in [0, n) across the pool's threads._  |
|  int | [**dp\_pool\_threads**](#function-dp_pool_threads) (const [**dp\_pool\_t**](structdp__pool__t.md) \* p) <br>_Workers the pool runs work on, the caller included._  |


























## Detailed Description


doppler is single-threaded C by default; this is the one place that fans a genuinely independent, CPU-bound workload across cores (the per-source signal build in wfm\_plan's prepare(); the searcher's roll per thread). The contract is deliberately narrow: run body(i, ctx) for every i in [0, n), possibly concurrently, and return only once all n have completed. body MUST be free of cross-i data races — it may read shared read-only state through ctx and must write only to per-i outputs (a distinct slot per i). Given that, the observable result is bit-identical to the plain serial loop no matter how the work is scheduled.


Portability: POSIX threads on the two supported platforms (linux, macos). Every path to _not_ running parallel — a single online core, n &lt;= 1, a forced-serial caller, or a malloc/thread-create failure — degrades to running the whole range on the calling thread, so the function always does all the work. Header-only (static) so the sole caller pulls it in with no separate translation unit; link Threads::Threads on that target. 


    
## Public Functions Documentation




### function DP\_THREAD\_FN 

```C++
DP_THREAD_FN (
    dp_pf_worker,
    arg
) 
```



A worker drains indices off the shared cursor until the range is exhausted. fetch-add is the whole synchronization story: each index is handed to exactly one worker, so there is no contention on the outputs.


Declared through DP\_THREAD\_FN, so it is also a thread entry point; the caller of dp\_parallel\_for() runs it directly as the last worker. 


        

<hr>



### function DP\_THREAD\_FN 

```C++
DP_THREAD_FN (
    dp_pool_worker,
    arg
) 
```



A pool helper's thread body: park on `wake` until dp\_pool\_run() bumps `gen` or dp\_pool\_destroy() sets `stop`, drain the run's cursor with dp\_pf\_worker(), decrement `busy` (signaling `done` at zero), repeat. Returns only on `stop`. 


        

<hr>
## Public Static Functions Documentation




### function dp\_parallel\_for 

_Run body(i, ctx) for every i in [0, n) across up to max\_threads workers._ 
```C++
static inline void dp_parallel_for (
    size_t n,
    void(*)(size_t, void *) body,
    void * ctx,
    int max_threads
) 
```



max\_threads &lt;= 0 auto-selects the online core count; 1 forces serial. The calling thread is itself a worker, so total concurrency is max\_threads (max\_threads - 1 helpers spawned + the caller draining alongside them), capped at `n`. Whenever helpers cannot be spawned the caller alone drains the full range, so the range is always completed. Helpers are created and joined within this call.




**Parameters:**


* `n` Number of indices; 0 returns at once. 
* `body` Per-index work. Each index is passed to exactly one call, in no guaranteed order, possibly concurrently. 
* `ctx` Opaque context passed to every body() call. 
* `max_threads` Total worker count including the caller; &lt;= 0 means the online core count, 1 means serial. 




        

<hr>



### function dp\_pool\_create 

_Create a pool of max\_threads workers (the caller counts as one)._ 
```C++
static inline dp_pool_t * dp_pool_create (
    int max_threads
) 
```



Spawns max\_threads - 1 helper threads that park on a condition variable until dp\_pool\_run(). A helper that fails to start leaves a smaller pool; dp\_pool\_threads() reports the size actually achieved.




**Parameters:**


* `max_threads` Total worker count including the caller; &lt;= 0 auto-selects the online core count; 1 creates no helper. 



**Returns:**

The pool, owned by the caller (free with dp\_pool\_destroy()). Never NULL: the pool's own allocations are fixed-size and abort on OOM. 





        

<hr>



### function dp\_pool\_destroy 

_Stop and join the helpers, free the pool._ 
```C++
static inline void dp_pool_destroy (
    dp_pool_t * p
) 
```





**Parameters:**


* `p` The pool, or NULL (a no-op). Must not be mid-run. 




        

<hr>



### function dp\_pool\_run 

_Run body(i, ctx) for every i in [0, n) across the pool's threads._ 
```C++
static inline void dp_pool_run (
    dp_pool_t * p,
    size_t n,
    void(*)(size_t, void *) body,
    void * ctx
) 
```



Returns once all n have completed; dp\_parallel\_for()'s contract exactly. A NULL pool, a pool with no helpers, or n == 1 runs serially on the caller. Not reentrant: one run at a time, from one thread.




**Parameters:**


* `p` The pool, or NULL for serial execution. 
* `n` Number of indices; 0 returns at once. 
* `body` Per-index work; each index goes to exactly one call. 
* `ctx` Opaque context passed to every body() call. 




        

<hr>



### function dp\_pool\_threads 

_Workers the pool runs work on, the caller included._ 
```C++
static inline int dp_pool_threads (
    const dp_pool_t * p
) 
```





**Parameters:**


* `p` The pool, or NULL. 



**Returns:**

helpers + 1; 1 for a NULL pool. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dp_parallel.h`

