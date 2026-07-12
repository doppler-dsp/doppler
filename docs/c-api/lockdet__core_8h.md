

# File lockdet\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**lockdet**](dir_87531a87e500e672b7d093c5682794b4.md) **>** [**lockdet\_core.h**](lockdet__core_8h.md)

[Go to the source code of this file](lockdet__core_8h_source.md)

_Portable lock detector — level + time hysteresis over any scalar lock metric, embeddable in every loop that makes a lock decision._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**lockdet\_state\_t**](structlockdet__state__t.md) <br>_Lock-detector state (embeddable by value; pointer-free POD)._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**lockdet\_configure**](#function-lockdet_configure) ([**lockdet\_state\_t**](structlockdet__state__t.md) \* state, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune thresholds and verify counts; preserve the decision._  |
|  [**lockdet\_state\_t**](structlockdet__state__t.md) \* | [**lockdet\_create**](#function-lockdet_create) (double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Create a lockdet instance._  |
|  void | [**lockdet\_destroy**](#function-lockdet_destroy) ([**lockdet\_state\_t**](structlockdet__state__t.md) \* state) <br>_Destroy a lockdet instance and release all memory._  |
|  void | [**lockdet\_get\_state**](#function-lockdet_get_state) (const [**lockdet\_state\_t**](structlockdet__state__t.md) \* state, void \* blob) <br>_Serialize the detector state into_ `blob` _._ |
|  void | [**lockdet\_init**](#function-lockdet_init) ([**lockdet\_state\_t**](structlockdet__state__t.md) \* state, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Initialise a lock detector in place (no allocation)._  |
|  void | [**lockdet\_reset**](#function-lockdet_reset) ([**lockdet\_state\_t**](structlockdet__state__t.md) \* state) <br>_Drop the lock and clear the verify counter; keep the config._  |
|  int | [**lockdet\_set\_state**](#function-lockdet_set_state) ([**lockdet\_state\_t**](structlockdet__state__t.md) \* state, const void \* blob) <br>_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._  |
|  size\_t | [**lockdet\_state\_bytes**](#function-lockdet_state_bytes) (const [**lockdet\_state\_t**](structlockdet__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**lockdet\_step**](#function-lockdet_step) ([**lockdet\_state\_t**](structlockdet__state__t.md) \* state, double x) <br>_Feed one look of the lock metric; return the current decision._  |
|  void | [**lockdet\_steps**](#function-lockdet_steps) ([**lockdet\_state\_t**](structlockdet__state__t.md) \* state, const double \* input, int \* output, size\_t n) <br>_Run a block of lock-metric looks through the detector._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**LOCKDET\_STATE\_MAGIC**](lockdet__core_8h.md#define-lockdet_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc)('L', 'K', 'D', 'T')`<br> |
| define  | [**LOCKDET\_STATE\_VERSION**](lockdet__core_8h.md#define-lockdet_state_version)  `1u`<br> |

## Detailed Description


A tracking loop that computes a lock statistic (a CFAR ratio, a coherence metric, an error variance) still needs a _decision rule_: when is the statistic "high enough, long enough" to declare lock, and "low enough,
long enough" to drop it? This component is that rule, factored out once:



* **Level hysteresis**: separate declare (`up_thresh`) and drop (`down_thresh`) thresholds. With `up_thresh >= down_thresh` the band between them is sticky in both directions — a metric wobbling around a single threshold cannot chatter the flag.
* **Time hysteresis**: `n_up` consecutive looks above `up_thresh` to declare, `n_down` consecutive looks below `down_thresh` to drop. A single contrary look resets the run (consecutive, not cumulative), so the verify counts compose probabilistically: at per-look false-alarm rate p the false-declare rate is p^n\_up. Size the counts with [**det\_verify\_count()**](detection__core_8h.md#function-det_verify_count) and predict the declare latency with [**det\_verify\_delay()**](detection__core_8h.md#function-det_verify_delay) (detection module).




The state struct is **public** so a tracker embeds it by value (no heap) and drives it with [**lockdet\_init()**](lockdet__core_8h.md#function-lockdet_init)/lockdet\_step() — e.g. the DLL steps one on its CFAR statistic each N-look decision, the MPSK receiver steps one on the carrier lock metric each recovered symbol. [**lockdet\_create()**](lockdet__core_8h.md#function-lockdet_create) is the heap path used by the Python wrapper. Pointer-free POD: it rides an embedding composer's whole-struct state snapshot with no extra packing.


Lifecycle: `create -> (step / steps / configure / reset)* -> destroy`



```C++
lockdet_state_t d;
lockdet_init (&d, 1.5, 1.2, 2, 3);       // declare: 2 looks > 1.5
lockdet_reset (&d);                      // cnt = 0, locked = 0
int locked = lockdet_step (&d, metric);  // one look -> current flag
```
 


    
## Public Functions Documentation




### function lockdet\_configure 

_Re-tune thresholds and verify counts; preserve the decision._ 
```C++
void lockdet_configure (
    lockdet_state_t * state,
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




        

<hr>



### function lockdet\_create 

_Create a lockdet instance._ 
```C++
lockdet_state_t * lockdet_create (
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

Caller must call [**lockdet\_destroy()**](lockdet__core_8h.md#function-lockdet_destroy) when done. 





        

<hr>



### function lockdet\_destroy 

_Destroy a lockdet instance and release all memory._ 
```C++
void lockdet_destroy (
    lockdet_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function lockdet\_get\_state 

_Serialize the detector state into_ `blob` _._
```C++
void lockdet_get_state (
    const lockdet_state_t * state,
    void * blob
) 
```




<hr>



### function lockdet\_init 

_Initialise a lock detector in place (no allocation)._ 
```C++
void lockdet_init (
    lockdet_state_t * state,
    double up_thresh,
    double down_thresh,
    uint32_t n_up,
    uint32_t n_down
) 
```



Stores the thresholds and verify counts (each count clamped to &gt;= 1; a count of 1 means no time hysteresis on that side). Does **not** touch `cnt` / `locked`, so it doubles as a reconfigure that preserves the current decision. Use this for a `lockdet_state_t` embedded by value; [**lockdet\_create()**](lockdet__core_8h.md#function-lockdet_create) is calloc + [**lockdet\_init()**](lockdet__core_8h.md#function-lockdet_init).




**Parameters:**


* `state` Must be non-NULL. 
* `up_thresh` Declare threshold (hit when metric &gt; up\_thresh). 
* `down_thresh` Drop threshold (miss when metric &lt; down\_thresh); choose &lt;= up\_thresh for level hysteresis. 
* `n_up` Consecutive hits to declare; clamped to &gt;= 1. 
* `n_down` Consecutive misses to drop; clamped to &gt;= 1. 




        

<hr>



### function lockdet\_reset 

_Drop the lock and clear the verify counter; keep the config._ 
```C++
void lockdet_reset (
    lockdet_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function lockdet\_set\_state 

_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._ 
```C++
int lockdet_set_state (
    lockdet_state_t * state,
    const void * blob
) 
```




<hr>



### function lockdet\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t lockdet_state_bytes (
    const lockdet_state_t * state
) 
```




<hr>



### function lockdet\_step 

_Feed one look of the lock metric; return the current decision._ 
```C++
JM_FORCEINLINE  JM_HOT int lockdet_step (
    lockdet_state_t * state,
    double x
) 
```



Unlocked: a hit (`x > up_thresh`) advances the verify run and the n\_up-th consecutive hit declares lock; any miss resets the run. Locked: a miss (`x < down_thresh`) advances the run and the n\_down-th consecutive miss drops the lock; any hit (`x >= down_thresh`) resets it. A metric inside the `[down_thresh, up_thresh]` band is sticky — it neither advances a declare nor a drop.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Lock metric for this look. 



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



### function lockdet\_steps 

_Run a block of lock-metric looks through the detector._ 
```C++
void lockdet_steps (
    lockdet_state_t * state,
    const double * input,
    int * output,
    size_t n
) 
```





**Parameters:**


* `state` Component state (mutated). 
* `input` Metric array (length &gt;= n). 
* `output` Decision array, 0/1 per look (length &gt;= n). 
* `n` Number of looks. 




        

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
The documentation for this class was generated from the following file `native/inc/lockdet/lockdet_core.h`

