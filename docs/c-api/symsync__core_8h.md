

# File symsync\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**symsync**](dir_bee143323fe2e99a30a6d3a881f82f29.md) **>** [**symsync\_core.h**](symsync__core_8h.md)

[Go to the source code of this file](symsync__core_8h_source.md)

_SymbolSync component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "farrow/farrow_core.h"`
* `#include "jm_perf.h"`
* `#include "lockdet/lockdet_core.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include "nco/nco_core.h"`
* `#include "telemetry/telemetry.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**symsync\_state\_t**](structsymsync__state__t.md) <br>_SymbolSync state._  |
| struct | [**symsync\_tlm\_t**](structsymsync__tlm__t.md) <br>_Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then a single predicted-not-taken branch per recovered symbol. Zeroed in state blobs and preserved across set\_state (DP\_DEFINE\_POD\_STATE\_TLM)._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**symsync\_\_core\_8h\_1aff70bde6f02f7ccdd0eb1c8f28cbed91**](#enum-symsync__core_8h_1aff70bde6f02f7ccdd0eb1c8f28cbed91)  <br>_Timing-error-detector selection for_ [_**symsync\_state\_t::ted**_](structsymsync__state__t.md#variable-ted) _._ |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**dttl\_ted**](#function-dttl_ted) (float complex mid, float complex y, float complex prev) <br>_Sign-sign DTTL: gate the transition sample by the hard-decision transition on each rail._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**gardner\_ted**](#function-gardner_ted) (float complex mid, float complex diff) <br>_Gardner timing-error detector: Re{ conj(mid) \* (y - prev) }._  |
|  void | [**symsync\_configure**](#function-symsync_configure) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state, double bn, double zeta) <br> |
|  int | [**symsync\_configure\_lock**](#function-symsync_configure_lock) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state, double rolloff, double esno\_min\_db, double pfa, double pd) <br>_Tune the always-on timing-lock detector to a target (pfa, pd) at a given link operating point._  |
|  void | [**symsync\_configure\_lock\_raw**](#function-symsync_configure_lock_raw) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state, size\_t avgs, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Set the lock detector's raw geometry directly._  |
|  [**symsync\_state\_t**](structsymsync__state__t.md) \* | [**symsync\_create**](#function-symsync_create) (size\_t sps, double bn, double zeta, int order, int ted) <br>_Create a symsync instance._  |
|  void | [**symsync\_destroy**](#function-symsync_destroy) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br>_Destroy a symsync instance and release all memory._  |
|  double | [**symsync\_get\_bn**](#function-symsync_get_bn) (const [**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br> |
|  double | [**symsync\_get\_lock\_stat**](#function-symsync_get_lock_stat) (const [**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br>_Last block-averaged lock statistic: mean(2\*(\|on-time\|^2-\|mid\|^2)/(\|on-time\|^2+\|mid\|^2)) over the configured avgs looks; compare against the configured threshold (see symsync\_configure\_lock)._  |
|  int | [**symsync\_get\_locked**](#function-symsync_get_locked) (const [**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br>_Current lock decision (1 = locked, 0 = not), with the configured verify-count / hysteresis rule applied._  |
|  double | [**symsync\_get\_rate**](#function-symsync_get_rate) (const [**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br> |
|  void | [**symsync\_get\_state**](#function-symsync_get_state) (const [**symsync\_state\_t**](structsymsync__state__t.md) \* state, void \* blob) <br> |
|  double | [**symsync\_get\_timing\_error**](#function-symsync_get_timing_error) (const [**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br> |
|  void | [**symsync\_init**](#function-symsync_init) ([**symsync\_state\_t**](structsymsync__state__t.md) \* s, size\_t sps, double bn, double zeta, int order, int ted) <br>_Initialise a SymbolSync in place (no allocation)._  |
|  void | [**symsync\_reset**](#function-symsync_reset) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br>_Reset SymbolSync to its post-create state._  |
|  void | [**symsync\_set\_bn**](#function-symsync_set_bn) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state, double val) <br> |
|  int | [**symsync\_set\_state**](#function-symsync_set_state) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state, const void \* blob) <br> |
|  int | [**symsync\_set\_telemetry**](#function-symsync_set_telemetry) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state, [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context and register the timing loop's probes on it. Registers five probes, emitted once per recovered symbol and further thinned by decim: "&lt;prefix&gt;.e" (the normalised TED error — the loop stress), "&lt;prefix&gt;.freq" (the loop-filter control steering the timing NCO, fractional rate offset), "&lt;prefix&gt;.rate" (the smoothed tracked samples/symbol), "&lt;prefix&gt;.lock" (the last block-averaged lock\_signal, held between avgs-look updates) and "&lt;prefix&gt;.locked" (the verify-counted lockdet decision, 0/1). Passing NULL detaches. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in_ [_**telemetry/telemetry.h**_](telemetry_8h.md) _)._ |
|  size\_t | [**symsync\_state\_bytes**](#function-symsync_state_bytes) (const [**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**symsync\_step**](#function-symsync_step) ([**symsync\_state\_t**](structsymsync__state__t.md) \* s, float complex x, float complex \* y\_out) <br>_Per-sample symbol-timing step (the inline composition API)._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**symsync\_step\_ted**](#function-symsync_step_ted) ([**symsync\_state\_t**](structsymsync__state__t.md) \* s, float complex x, float complex \* y\_out, int ted) <br>_Per-sample symbol-timing step with the TED selection as a parameter._  |
|  size\_t | [**symsync\_steps**](#function-symsync_steps) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br> |
|  size\_t | [**symsync\_steps\_max\_out**](#function-symsync_steps_max_out) ([**symsync\_state\_t**](structsymsync__state__t.md) \* state) <br> |
|  void | [**symsync\_tlm\_flush**](#function-symsync_tlm_flush) (const [**symsync\_state\_t**](structsymsync__state__t.md) \* s) <br>_Emit the timing loop's telemetry records for the symbol just recovered._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**SYMSYNC\_LOCK\_EPS**](symsync__core_8h.md#define-symsync_lock_eps)  `1e-12`<br> |
| define  | [**SYMSYNC\_STATE\_MAGIC**](symsync__core_8h.md#define-symsync_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('S', 'Y', 'N', 'C')`<br> |
| define  | [**SYMSYNC\_STATE\_VERSION**](symsync__core_8h.md#define-symsync_state_version)  `/* multi line expression */`<br> |

## Detailed Description


Lifecycle: `create -> (step / steps / reset)* -> destroy`


Example: 
```C++
symsync_state_t *obj = symsync_create(4, 0.01, 0.707, 0, 0);
float complex y = symsync_step(obj, 0.0f + 0.0f * I);
symsync_destroy(obj);
```
 


    
## Public Types Documentation




### enum symsync\_\_core\_8h\_1aff70bde6f02f7ccdd0eb1c8f28cbed91 

_Timing-error-detector selection for_ [_**symsync\_state\_t::ted**_](structsymsync__state__t.md#variable-ted) _._
```C++
enum symsync__core_8h_1aff70bde6f02f7ccdd0eb1c8f28cbed91 {
    SYMSYNC_TED_GARDNER = 0,
    SYMSYNC_TED_DTTL = 1
};
```




<hr>
## Public Functions Documentation




### function dttl\_ted 

_Sign-sign DTTL: gate the transition sample by the hard-decision transition on each rail._ 
```C++
JM_FORCEINLINE double dttl_ted (
    float complex mid,
    float complex y,
    float complex prev
) 
```



Decision-directed (M.K. Simon's Data Transition Tracking Loop, digital point-sample reduction): zero unless a rail's hard decision actually flips between `on_time[k-1]` and `on_time[k]`, in which case the error is the transition-gate sample's value on that rail. Valid only for constellations with independent, rectangular I/Q decision boundaries (BPSK, QPSK/OQPSK)  not 8PSK/QAM. Diff order (current minus previous) matches gardner\_ted's convention so both TEDs share one loop-filter polarity.




**Parameters:**


* `mid` Mid-symbol (transition-gate) interpolant. 
* `y` `on_time[k]`. 
* `prev` `on_time[k-1]`. 



**Returns:**

Raw (pre-AGC-normalized) timing error. 





        

<hr>



### function gardner\_ted 

_Gardner timing-error detector: Re{ conj(mid) \* (y - prev) }._ 
```C++
JM_FORCEINLINE double gardner_ted (
    float complex mid,
    float complex diff
) 
```



Blind (non-data-aided): correlates the transition-gate sample against the on-time step, so it locks for any constellation but pays a non-transition-symbol self-noise cost. 

**See also:** [**dttl\_ted**](symsync__core_8h.md#function-dttl_ted) for the decision-directed alternative.


**Parameters:**


* `mid` Mid-symbol (transition-gate) interpolant. 
* `diff` `on_time[k] - on_time[k-1]`. 



**Returns:**

Raw (pre-AGC-normalized) timing error. 





        

<hr>



### function symsync\_configure 

```C++
void symsync_configure (
    symsync_state_t * state,
    double bn,
    double zeta
) 
```




<hr>



### function symsync\_configure\_lock 

_Tune the always-on timing-lock detector to a target (pfa, pd) at a given link operating point._ 
```C++
int symsync_configure_lock (
    symsync_state_t * state,
    double rolloff,
    double esno_min_db,
    double pfa,
    double pd
) 
```



Sizes the non-coherent block size (avgs) and declare threshold from a Gaussian sizing of the eye-opening statistic lock\_signal = 2\*(\|on-time\|^2-\|mid\|^2)/(\|on-time\|^2+\|mid\|^2): a per-look mean (mean\_lock\_detect, from rolloff and the minimum operating Es/N0) drives the classic N = variance\*((Q^-1(pfa)-Q^-1(pd))/mean)^2 / threshold = Q^-1(pfa)\*mean/(Q^-1(pfa)-Q^-1(pd)) derivation, implemented directly from a formula supplied by a doppler user (not re-derived against a primary source), with "variance" set from a direct measurement of lock\_signal's real per-look variance under noise (~1.343, 5,000,000-sample Monte Carlo) rather than the placeholder "8" this API originally shipped with  see symsync\_core.c's SYMSYNC\_LOCK\_STAT\_VARIANCE comment for the full derivation (a factor-of-2 correction for the erfcinv-vs-Q^-1 convention applies on top of the measured variance; the two hypotheses were empirically compared before picking one). Empirically validated at the default operating point (avgs=133, threshold=0.311): 429 false declares over 500,000 independent noise-only blocks against a nominal pfa=1e-3 (8.58e-4, correctly sized with safe margin, not accidentally oversized); 2000/2000 true declares at the esno\_min design SNR against a nominal pd=0.9  see native/validation/symsync\_lock.c for the harness. No level hysteresis by default (up = down = threshold, matching dll\_configure\_lock's shape); the raw escape hatch (symsync\_configure\_lock\_raw) exposes split thresholds, an explicit avgs, and independent n\_up/n\_down.




**Parameters:**


* `state` Must be non-NULL. 
* `rolloff` Matched-filter excess bandwidth (e.g. 0.35 for a typical RRC system). 
* `esno_min_db` Minimum operating Es/N0, dB  the worst-case link point the detector must still declare lock at. 
* `pfa` Target false-alarm probability per decision, in (0, 1). 
* `pd` Target detection probability per decision, in (0, 1); must exceed pfa. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if pfa/pd are out of range or pd &lt;= pfa. 
```C++
>>> from doppler.track import SymbolSync
>>> ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)
>>> ss.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=1e-3, pd=0.9)
>>> ss.locked
False
>>> ss.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=0.9, pd=0.9)
Traceback (most recent call last):
    ...
ValueError: configure_lock failed (rc=-4)
```
 





        

<hr>



### function symsync\_configure\_lock\_raw 

_Set the lock detector's raw geometry directly._ 
```C++
void symsync_configure_lock_raw (
    symsync_state_t * state,
    size_t avgs,
    double up_thresh,
    double down_thresh,
    uint32_t n_up,
    uint32_t n_down
) 
```



The escape hatch under [**symsync\_configure\_lock()**](symsync__core_8h.md#function-symsync_configure_lock) for a caller that derives its own averaging/threshold geometry: the block size (avgs), a split declare/drop threshold pair on lock\_stat (level hysteresis), and both verify counts (time hysteresis). Re-tuning clears the in-flight block sum and drops the lock so the next decision uses only looks gathered under the new config.




**Parameters:**


* `state` Must be non-NULL. 
* `avgs` Non-coherent block size (looks/decision); clamped &gt;= 1. 
* `up_thresh` Declare threshold on lock\_stat. 
* `down_thresh` Drop threshold; choose &lt;= up\_thresh for level hysteresis. 
* `n_up` Consecutive above-threshold decisions to declare; clamped &gt;= 1. 
* `n_down` Consecutive below-threshold decisions to drop; clamped &gt;= 1. 




        

<hr>



### function symsync\_create 

_Create a symsync instance._ 
```C++
symsync_state_t * symsync_create (
    size_t sps,
    double bn,
    double zeta,
    int order,
    int ted
) 
```





**Parameters:**


* `sps` sps (default: 4). 
* `bn` bn (default: 0.01). 
* `zeta` zeta (default: 0.707). 
* `order` Enum index; 0=linear…2=cubic. 
* `ted` Enum index; 0=gardner, 1=dttl (BPSK/QPSK only). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**symsync\_destroy()**](symsync__core_8h.md#function-symsync_destroy) when done. 





        

<hr>



### function symsync\_destroy 

_Destroy a symsync instance and release all memory._ 
```C++
void symsync_destroy (
    symsync_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function symsync\_get\_bn 

```C++
double symsync_get_bn (
    const symsync_state_t * state
) 
```




<hr>



### function symsync\_get\_lock\_stat 

_Last block-averaged lock statistic: mean(2\*(\|on-time\|^2-\|mid\|^2)/(\|on-time\|^2+\|mid\|^2)) over the configured avgs looks; compare against the configured threshold (see symsync\_configure\_lock)._ 
```C++
double symsync_get_lock_stat (
    const symsync_state_t * state
) 
```




<hr>



### function symsync\_get\_locked 

_Current lock decision (1 = locked, 0 = not), with the configured verify-count / hysteresis rule applied._ 
```C++
int symsync_get_locked (
    const symsync_state_t * state
) 
```




<hr>



### function symsync\_get\_rate 

```C++
double symsync_get_rate (
    const symsync_state_t * state
) 
```




<hr>



### function symsync\_get\_state 

```C++
void symsync_get_state (
    const symsync_state_t * state,
    void * blob
) 
```




<hr>



### function symsync\_get\_timing\_error 

```C++
double symsync_get_timing_error (
    const symsync_state_t * state
) 
```




<hr>



### function symsync\_init 

_Initialise a SymbolSync in place (no allocation)._ 
```C++
void symsync_init (
    symsync_state_t * s,
    size_t sps,
    double bn,
    double zeta,
    int order,
    int ted
) 
```



The by-value counterpart to [**symsync\_create()**](symsync__core_8h.md#function-symsync_create): lets a composing object embed a [**symsync\_state\_t**](structsymsync__state__t.md) by value and initialise it without a heap allocation ([**symsync\_state\_t**](structsymsync__state__t.md) holds no heap members — the NCO, Farrow and loop filter are all by value). Mirrors [**loop\_filter\_init()**](loop__filter__core_8h.md#function-loop_filter_init)/costas\_init().




**Parameters:**


* `s` State to initialise. Must be non-NULL. 
* `sps` Nominal samples per symbol. 
* `bn` Loop noise bandwidth (normalised to the symbol rate). 
* `zeta` Damping factor (0.707 = critically damped). 
* `order` Farrow interpolator order (0=linear, 1=parabolic, 2=cubic). 
* `ted` Timing-error detector: SYMSYNC\_TED\_GARDNER (0, blind) or SYMSYNC\_TED\_DTTL (1, decision-directed; BPSK/QPSK only). 




        

<hr>



### function symsync\_reset 

_Reset SymbolSync to its post-create state._ 
```C++
void symsync_reset (
    symsync_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function symsync\_set\_bn 

```C++
void symsync_set_bn (
    symsync_state_t * state,
    double val
) 
```




<hr>



### function symsync\_set\_state 

```C++
int symsync_set_state (
    symsync_state_t * state,
    const void * blob
) 
```




<hr>



### function symsync\_set\_telemetry 

_Attach (or detach) a telemetry context and register the timing loop's probes on it. Registers five probes, emitted once per recovered symbol and further thinned by decim: "&lt;prefix&gt;.e" (the normalised TED error — the loop stress), "&lt;prefix&gt;.freq" (the loop-filter control steering the timing NCO, fractional rate offset), "&lt;prefix&gt;.rate" (the smoothed tracked samples/symbol), "&lt;prefix&gt;.lock" (the last block-averaged lock\_signal, held between avgs-look updates) and "&lt;prefix&gt;.locked" (the verify-counted lockdet decision, 0/1). Passing NULL detaches. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in_ [_**telemetry/telemetry.h**_](telemetry_8h.md) _)._
```C++
int symsync_set_telemetry (
    symsync_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "sync" or "rx.sync". 
* `decim` Emit every decim-th symbol; &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take all five probes (the attach fails whole; the object stays detached). 
```C++
>>> import numpy as np
>>> from doppler.track import SymbolSync
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)
>>> ss.set_telemetry(tlm, "sync")
>>> sorted(tlm.probe_names())
['sync.e', 'sync.freq', 'sync.lock', 'sync.locked', 'sync.rate']
>>> x = np.repeat([1 + 1j, -1 - 1j], 4 * 64).astype(np.complex64)
>>> _ = ss.steps(x)
>>> recs = tlm.read()   # five records per recovered symbol
>>> len(recs) > 0 and len(recs) % 5 == 0
True
```
 





        

<hr>



### function symsync\_state\_bytes 

```C++
size_t symsync_state_bytes (
    const symsync_state_t * state
) 
```




<hr>



### function symsync\_step 

_Per-sample symbol-timing step (the inline composition API)._ 
```C++
JM_FORCEINLINE  JM_HOT int symsync_step (
    symsync_state_t * s,
    float complex x,
    float complex * y_out
) 
```



The public form of [**symsync\_step\_ted()**](symsync__core_8h.md#function-symsync_step_ted): dispatches on the state's configured detector (`s->ted`) and flushes telemetry when attached. [**symsync\_steps()**](symsync__core_8h.md#function-symsync_steps) is this in a loop (with the TED specialised per detector); a tracking channel inlines it to drive a downstream carrier loop on the recovered symbols.




**Parameters:**


* `s` State. Must be non-NULL. 
* `x` One input sample. 
* `y_out` Receives the symbol when the return is 1. 



**Returns:**

1 if a symbol was emitted (into `y_out`), 0 otherwise. 





        

<hr>



### function symsync\_step\_ted 

_Per-sample symbol-timing step with the TED selection as a parameter._ 
```C++
JM_FORCEINLINE  JM_HOT int symsync_step_ted (
    symsync_state_t * s,
    float complex x,
    float complex * y_out,
    int ted
) 
```



The workhorse behind [**symsync\_step()**](symsync__core_8h.md#function-symsync_step)/symsync\_steps(). Pushes one input sample into the Farrow history and advances the integer timing NCO. When the NCO crosses its half-scale (mid-symbol) it stores the transition-gate interpolant; when it wraps (on-time) it forms the on-time interpolant, runs the selected TED (see gardner\_ted / dttl\_ted), steers the NCO frequency, and emits the timing-corrected symbol.


Passing a literal `ted` (SYMSYNC\_TED\_GARDNER / SYMSYNC\_TED\_DTTL) lets the force-inlined body constant-fold the detector branch away, so a specialised block loop carries exactly one TED — the runtime `s->ted` branch inside the 64k-block loop measured ~30% slower (both TED bodies kept live across the per-sample path). Compositions that hardcode a detector (the MPSK receiver is Gardner-only) call this directly with the literal; runtime-configured callers use [**symsync\_step()**](symsync__core_8h.md#function-symsync_step).




**Parameters:**


* `s` State. Must be non-NULL. 
* `x` One input sample. 
* `y_out` Receives the symbol when the return is 1. 
* `ted` SYMSYNC\_TED\_GARDNER or SYMSYNC\_TED\_DTTL — pass a literal for a specialised (branch-free) instantiation. 



**Returns:**

1 if a symbol was emitted (into `y_out`), 0 otherwise. 





        

<hr>



### function symsync\_steps 

```C++
size_t symsync_steps (
    symsync_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```




<hr>



### function symsync\_steps\_max\_out 

```C++
size_t symsync_steps_max_out (
    symsync_state_t * state
) 
```




<hr>



### function symsync\_tlm\_flush 

_Emit the timing loop's telemetry records for the symbol just recovered._ 
```C++
void symsync_tlm_flush (
    const symsync_state_t * s
) 
```



Out-of-line on purpose: the emit machinery must not inline into the per-sample hot loops (three inlined ring-write expansions measured ~20% slower detached, from sheer body growth). Callers gate on `s->tlm.ctx` and call this once per emitted symbol — the detached cost stays one predicted-not-taken branch per symbol, outside the force-inlined step. Records "&lt;prefix&gt;.e" (last TED error), "&lt;prefix&gt;.freq" (the NCO rate control, reconstructed as phase\_inc/base\_inc - 1), "&lt;prefix&gt;.rate" (tracked samples/symbol), "&lt;prefix&gt;.lock" (the last block-averaged lock\_signal, refreshed every avgs looks) and "&lt;prefix&gt;.locked" (the verify-counted lockdet decision, 0/1).




**Parameters:**


* `s` State with a non-NULL tlm.ctx (caller-checked). 




        

<hr>
## Macro Definition Documentation





### define SYMSYNC\_LOCK\_EPS 

```C++
#define SYMSYNC_LOCK_EPS `1e-12`
```




<hr>



### define SYMSYNC\_STATE\_MAGIC 

```C++
#define SYMSYNC_STATE_MAGIC `DP_FOURCC ('S', 'Y', 'N', 'C')`
```




<hr>



### define SYMSYNC\_STATE\_VERSION 

```C++
#define SYMSYNC_STATE_VERSION `/* multi line expression */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/symsync/symsync_core.h`

