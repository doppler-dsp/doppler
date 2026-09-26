

# File lockdet\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**lockdet**](dir_0a3dcc380b80b2c01366f0cb5ed7ee07.md) **>** [**lockdet\_core.h**](lockdet__core_8h.md)

[Go to the source code of this file](lockdet__core_8h_source.md)

_Portable lock detector — level + time hysteresis over any scalar lock metric, embeddable in every loop that makes a lock decision._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/util/util_core.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) <br>_Lock-detector state (embeddable by value; pointer-free POD)._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_lockdet\_configure**](#function-dp_lockdet_configure) ([**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) \* state, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune thresholds and verify counts; preserve the decision._  |
|  [**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) \* | [**dp\_lockdet\_create**](#function-dp_lockdet_create) (double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Create a lockdet instance._  |
|  void | [**dp\_lockdet\_destroy**](#function-dp_lockdet_destroy) ([**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) \* state) <br>_Destroy a lockdet instance and release all memory._  |
|  void | [**dp\_lockdet\_get\_state**](#function-dp_lockdet_get_state) (const [**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) \* state, void \* blob) <br>_Serialize the detector state into_ `blob` _._ |
|  void | [**dp\_lockdet\_reset**](#function-dp_lockdet_reset) ([**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) \* state) <br>_Drop the lock and clear the verify counter; keep the config. Returns the detector to the unlocked state with an empty verify run, as if freshly constructed with the same thresholds. Call it at a segment boundary so a decision made on one capture does not leak into an unrelated next one._  |
|  int | [**dp\_lockdet\_set\_state**](#function-dp_lockdet_set_state) ([**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) \* state, const void \* blob) <br>_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._  |
|  size\_t | [**dp\_lockdet\_state\_bytes**](#function-dp_lockdet_state_bytes) (const [**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**dp\_lockdet\_step**](#function-dp_lockdet_step) ([**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) \* state, double x) <br>_Feed one look of the lock metric; return the current decision._  |
|  void | [**dp\_lockdet\_steps**](#function-dp_lockdet_steps) ([**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) \* state, const double \* x, int \* out, size\_t n) <br>_Run a block of lock-metric looks through the detector. Applies_ [_**dp\_lockdet\_step()**_](lockdet__core_8h.md#function-dp_lockdet_step) _to each look in turn, so the decision flag and the in-flight verify run carry across the block exactly as they would look by look — a signal can be processed in frames of any size with no seam._ |
|  void | [**lockdet\_init**](#function-lockdet_init) ([**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) \* state, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Initialise a lock detector in place (no allocation)._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**LOCKDET\_STATE\_MAGIC**](lockdet__core_8h.md#define-lockdet_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc)('L', 'K', 'D', 'T')`<br> |
| define  | [**LOCKDET\_STATE\_VERSION**](lockdet__core_8h.md#define-lockdet_state_version)  `1u`<br> |

## Detailed Description


A tracking loop that computes a lock statistic (a CFAR ratio, a coherence metric, an error variance) still needs a _decision rule_: when is the statistic "high enough, long enough" to declare lock, and "low enough,
long enough" to drop it? This component is that rule, factored out once:



* **Level hysteresis**: separate declare (`up_thresh`) and drop (`down_thresh`) thresholds. With `up_thresh >= down_thresh` the band between them is sticky in both directions — a metric wobbling around a single threshold cannot chatter the flag.
* **Time hysteresis**: `n_up` consecutive looks above `up_thresh` to declare, `n_down` consecutive looks below `down_thresh` to drop. A single contrary look resets the run (consecutive, not cumulative), so the verify counts compose probabilistically. At per-look false-alarm rate p the false-declare rate per look is `p^n_up * (1 - p) / (1 - p^n_up)`, whose reciprocal is exactly det\_verify\_delay(p, n\_up), the mean looks to a declare. `p^n_up` alone is the **p -&gt; 0 limit** of that, and is what [**dp\_det\_verify\_count()**](detection__core_8h.md#function-dp_det_verify_count) sizes against  correct to 0.001% at p = 1e-5, 10% at p = 0.1, and **+87% at p = 0.5 with n\_up = 4**. Use it as the budget (it errs high, so it over-provisions n\_up) and [**dp\_det\_verify\_delay()**](detection__core_8h.md#function-dp_det_verify_delay) for the number a caller actually observes. Measured across p from 0.1 to 0.5 and n\_up from 1 to 4: native/validation/lockdet\_verify.c.(Both the formula and those ranges are written without an indented block or square brackets on purpose: mkdoxy renders this comment into markdown, where an indented line is swallowed into the paragraph before it and a bare `p in [0.1, 0.5]` parses as a link reference and fails the strict docs build.)
* **Non-finite looks**: a NaN look is a **miss in both states** — it never advances a declare, and while locked it advances the drop run like any other miss, so a metric that goes NaN drops the lock after `n_down` rather than holding it lit. An unknown lock is not a lock. The policy is not implemented here: the look is passed through [**util\_core.h**](util__core_8h.md)'s [**dp\_saturate()**](util__core_8h.md#function-dp_saturate), whose `nan_to` parameter documents a lock statistic as the caller that wants the floor. Only NaN is unordered — the infinities are ordinary looks (+inf a hit, -inf a miss), and the exclusive edges are unchanged.




The state struct is **public** so a tracker embeds it by value (no heap) and drives it with [**lockdet\_init()**](lockdet__core_8h.md#function-lockdet_init)/dp\_lockdet\_step() — e.g. the DLL steps one on its CFAR statistic each N-look decision, the MPSK receiver steps one on the carrier lock metric each recovered symbol. [**dp\_lockdet\_create()**](lockdet__core_8h.md#function-dp_lockdet_create) is the heap path used by the Python wrapper. Pointer-free POD: it rides an embedding composer's whole-struct state snapshot with no extra packing.


Lifecycle: `create -> (step / steps / configure / reset)* -> destroy`



```C++
dp_lockdet_state_t d;
lockdet_init (&d, 1.5, 1.2, 2, 3);       // declare: 2 looks > 1.5
dp_lockdet_reset (&d);                      // cnt = 0, locked = 0
int locked = dp_lockdet_step (&d, metric);  // one look -> current flag
```
 


    
## Public Functions Documentation




### function dp\_lockdet\_configure 

_Re-tune thresholds and verify counts; preserve the decision._ 
```C++
void dp_lockdet_configure (
    dp_lockdet_state_t * state,
    double up_thresh,
    double down_thresh,
    uint32_t n_up,
    uint32_t n_down
) 
```



The current `locked` flag survives (a live lock is not dropped by a re-tune); the in-flight verify counter is cleared so the next run is counted entirely under the new config.




**Parameters:**


* `state` Must be non-NULL. 
* `up_thresh` Declare threshold (hit when metric &gt; up\_thresh). 
* `down_thresh` Drop threshold (miss when metric &lt; down\_thresh). 
* `n_up` Consecutive hits to declare; clamped to &gt;= 1. 
* `n_down` Consecutive misses to drop; clamped to &gt;= 1. 
```C++
>>> from doppler.detection import LockDet
>>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=2)
>>> d.configure(up_thresh=3.0, down_thresh=2.5, n_up=1, n_down=1)
>>> d.up_thresh          # thresholds re-tuned in place
3.0
>>> d.step(4.0)          # a single hit now declares (n_up=1)
1
```
 




        

<hr>



### function dp\_lockdet\_create 

_Create a lockdet instance._ 
```C++
dp_lockdet_state_t * dp_lockdet_create (
    double up_thresh,
    double down_thresh,
    uint32_t n_up,
    uint32_t n_down
) 
```





**Parameters:**


* `up_thresh` Declare threshold (hit when metric &gt; up\_thresh). 
* `down_thresh` Drop threshold (miss when metric &lt; down\_thresh). 
* `n_up` Consecutive hits to declare; clamped &gt;= 1 (default 1). 
* `n_down` Consecutive misses to drop; clamped &gt;= 1 (default 1). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**dp\_lockdet\_destroy()**](lockdet__core_8h.md#function-dp_lockdet_destroy) when done. 





        

<hr>



### function dp\_lockdet\_destroy 

_Destroy a lockdet instance and release all memory._ 
```C++
void dp_lockdet_destroy (
    dp_lockdet_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_lockdet\_get\_state 

_Serialize the detector state into_ `blob` _._
```C++
void dp_lockdet_get_state (
    const dp_lockdet_state_t * state,
    void * blob
) 
```




<hr>



### function dp\_lockdet\_reset 

_Drop the lock and clear the verify counter; keep the config. Returns the detector to the unlocked state with an empty verify run, as if freshly constructed with the same thresholds. Call it at a segment boundary so a decision made on one capture does not leak into an unrelated next one._ 
```C++
void dp_lockdet_reset (
    dp_lockdet_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> from doppler.detection import LockDet
>>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=1, n_down=1)
>>> d.step(2.0)          # one hit declares lock (n_up=1)
1
>>> d.reset()            # drop it and clear the verify run
>>> d.locked
False
```
 




        

<hr>



### function dp\_lockdet\_set\_state 

_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._ 
```C++
int dp_lockdet_set_state (
    dp_lockdet_state_t * state,
    const void * blob
) 
```




<hr>



### function dp\_lockdet\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t dp_lockdet_state_bytes (
    const dp_lockdet_state_t * state
) 
```




<hr>



### function dp\_lockdet\_step 

_Feed one look of the lock metric; return the current decision._ 
```C++
JM_FORCEINLINE  JM_HOT int dp_lockdet_step (
    dp_lockdet_state_t * state,
    double x
) 
```



Unlocked: a hit (`x > up_thresh`) advances the verify run and the n\_up-th consecutive hit declares lock; any miss resets the run. Locked: a miss (`x < down_thresh`) advances the run and the n\_down-th consecutive miss drops the lock; any hit (`x >= down_thresh`) resets it. A metric inside the `[down_thresh, up_thresh]` band is sticky — it neither advances a declare nor a drop.


A **non-finite look is a miss in both states**: it never advances a declare, and while locked it advances the drop run like any other miss. An unknown lock is not a lock, which is the rule [**util\_core.h**](util__core_8h.md) states for lock statistics generally. So a metric that goes NaN drops the lock after `n_down` looks rather than holding it lit indefinitely.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Lock metric for this look. Non-finite counts as a miss. 



**Returns:**

Decision after this look (1 = locked, 0 = not).



```C++
>>> from doppler.detection import LockDet
>>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=3)
>>> [d.step(2.0), d.step(2.0)]     # declared on the 2nd straight hit
[0, 1]
>>> d.step(1.3)                    # in the hysteresis band: stays up
1
>>> [d.step(1.0), d.step(1.0), d.step(1.0)]  # 3rd straight miss drops
[1, 1, 0]
```
 


        

<hr>



### function dp\_lockdet\_steps 

_Run a block of lock-metric looks through the detector. Applies_ [_**dp\_lockdet\_step()**_](lockdet__core_8h.md#function-dp_lockdet_step) _to each look in turn, so the decision flag and the in-flight verify run carry across the block exactly as they would look by look — a signal can be processed in frames of any size with no seam._
```C++
void dp_lockdet_steps (
    dp_lockdet_state_t * state,
    const double * x,
    int * out,
    size_t n
) 
```





**Parameters:**


* `state` Component state (mutated). Must be non-NULL. 
* `x` Lock-metric looks, one scalar per look (length &gt;= n). 
* `out` Per-look decision output, 0 or 1 (length &gt;= n). 
* `n` Number of looks to process. 
```C++
>>> import numpy as np
>>> from doppler.detection import LockDet
>>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=2)
>>> x = np.array([2.0, 2.0, 1.0, 2.0])   # declares on the 2nd hit
>>> d.steps(x).tolist()
[0, 1, 1, 1]
```
 




        

<hr>



### function lockdet\_init 

_Initialise a lock detector in place (no allocation)._ 
```C++
void lockdet_init (
    dp_lockdet_state_t * state,
    double up_thresh,
    double down_thresh,
    uint32_t n_up,
    uint32_t n_down
) 
```



Stores the thresholds and verify counts (each count clamped to &gt;= 1; a count of 1 means no time hysteresis on that side). Does **not** touch `cnt` / `locked`, so it doubles as a reconfigure that preserves the current decision. Use this for a `dp_lockdet_state_t` embedded by value; [**dp\_lockdet\_create()**](lockdet__core_8h.md#function-dp_lockdet_create) is calloc + [**lockdet\_init()**](lockdet__core_8h.md#function-lockdet_init).




**Parameters:**


* `state` Must be non-NULL. 
* `up_thresh` Declare threshold (hit when metric &gt; up\_thresh). 
* `down_thresh` Drop threshold (miss when metric &lt; down\_thresh); choose &lt;= up\_thresh for level hysteresis. 
* `n_up` Consecutive hits to declare; clamped to &gt;= 1. 
* `n_down` Consecutive misses to drop; clamped to &gt;= 1. 




        

<hr>
## Macro Definition Documentation





### define LOCKDET\_STATE\_MAGIC 

```C++
#define LOCKDET_STATE_MAGIC `DP_FOURCC ('L', 'K', 'D', 'T')`
```




<hr>



### define LOCKDET\_STATE\_VERSION 

```C++
#define LOCKDET_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/lockdet/lockdet_core.h`

