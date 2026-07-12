

# File costas\_core.h



[**FileList**](files.md) **>** [**costas**](dir_9b517cb2745356d7938c9e100210a101.md) **>** [**costas\_core.h**](costas__core_8h.md)

[Go to the source code of this file](costas__core_8h_source.md)

_Costas carrier-tracking loop (integer-NCO de-rotation + PI loop)._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "lo/lo_core.h"`
* `#include "lockdet/lockdet_core.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include "telemetry/telemetry.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**costas\_state\_t**](structcostas__state__t.md) <br>_Costas loop state._  |
| struct | [**costas\_tlm\_t**](structcostas__tlm__t.md) <br>_Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then a single predicted-not-taken branch per symbol. Zeroed in state blobs and preserved across set\_state (DP\_DEFINE\_POD\_STATE\_TLM)._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**costas\_configure**](#function-costas_configure) ([**costas\_state\_t**](structcostas__state__t.md) \* state, double bn, double zeta) <br> |
|  void | [**costas\_configure\_lock**](#function-costas_configure_lock) ([**costas\_state\_t**](structcostas__state__t.md) \* state, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune the carrier lock detector's thresholds and verify counts._  |
|  [**costas\_state\_t**](structcostas__state__t.md) \* | [**costas\_create**](#function-costas_create) (double bn, double zeta, double init\_norm\_freq, size\_t tsamps, double bn\_fll) <br>_Create a Costas instance._  |
|  void | [**costas\_destroy**](#function-costas_destroy) ([**costas\_state\_t**](structcostas__state__t.md) \* state) <br>_Destroy a Costas instance and release all memory._  |
|  double | [**costas\_get\_bn**](#function-costas_get_bn) (const [**costas\_state\_t**](structcostas__state__t.md) \* state) <br> |
|  double | [**costas\_get\_bn\_fll**](#function-costas_get_bn_fll) (const [**costas\_state\_t**](structcostas__state__t.md) \* state) <br> |
|  double | [**costas\_get\_last\_error**](#function-costas_get_last_error) (const [**costas\_state\_t**](structcostas__state__t.md) \* state) <br> |
|  double | [**costas\_get\_lock\_metric**](#function-costas_get_lock_metric) (const [**costas\_state\_t**](structcostas__state__t.md) \* state) <br> |
|  int | [**costas\_get\_locked**](#function-costas_get_locked) (const [**costas\_state\_t**](structcostas__state__t.md) \* state) <br>_Current carrier lock decision (1 = locked, 0 = not), from the verify-counted detector on the lock-metric EMA (see costas\_configure\_lock)._  |
|  double | [**costas\_get\_norm\_freq**](#function-costas_get_norm_freq) (const [**costas\_state\_t**](structcostas__state__t.md) \* state) <br> |
|  void | [**costas\_get\_state**](#function-costas_get_state) (const [**costas\_state\_t**](structcostas__state__t.md) \* state, void \* blob) <br>_Serialize the full loop state into_ `blob` _._ |
|  void | [**costas\_init**](#function-costas_init) ([**costas\_state\_t**](structcostas__state__t.md) \* s, double bn, double zeta, double init\_norm\_freq, size\_t tsamps, double bn\_fll) <br>_Initialise a Costas loop in place (no allocation)._  |
|  void | [**costas\_reset**](#function-costas_reset) ([**costas\_state\_t**](structcostas__state__t.md) \* state) <br>_Re-seed the loop to its create-time frequency/phase; keep config._  |
|  void | [**costas\_set\_bn**](#function-costas_set_bn) ([**costas\_state\_t**](structcostas__state__t.md) \* state, double val) <br> |
|  void | [**costas\_set\_bn\_fll**](#function-costas_set_bn_fll) ([**costas\_state\_t**](structcostas__state__t.md) \* state, double val) <br> |
|  void | [**costas\_set\_norm\_freq**](#function-costas_set_norm_freq) ([**costas\_state\_t**](structcostas__state__t.md) \* state, double val) <br> |
|  int | [**costas\_set\_state**](#function-costas_set_state) ([**costas\_state\_t**](structcostas__state__t.md) \* state, const void \* blob) <br>_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._  |
|  int | [**costas\_set\_telemetry**](#function-costas_set_telemetry) ([**costas\_state\_t**](structcostas__state__t.md) \* state, [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context and register the carrier loop's probes on it. Registers four probes, emitted once per dumped symbol and further thinned by decim: "&lt;prefix&gt;.lock" (the \|Re P\|/\|P\| lock-metric EMA, 1 = phase-locked), "&lt;prefix&gt;.e" (the PLL discriminator output — the loop stress), "&lt;prefix&gt;.freq" (the tracked NCO frequency, cycles/sample) and "&lt;prefix&gt;.locked" (the verify-counted lock decision, 0/1 — see costas\_configure\_lock). Passing NULL detaches. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in_ [_**telemetry/telemetry.h**_](telemetry_8h.md) _)._ |
|  size\_t | [**costas\_state\_bytes**](#function-costas_state_bytes) (const [**costas\_state\_t**](structcostas__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  size\_t | [**costas\_steps**](#function-costas_steps) ([**costas\_state\_t**](structcostas__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br> |
|  size\_t | [**costas\_steps\_max\_out**](#function-costas_steps_max_out) ([**costas\_state\_t**](structcostas__state__t.md) \* state) <br> |
|  void | [**costas\_tlm\_flush**](#function-costas_tlm_flush) (const [**costas\_state\_t**](structcostas__state__t.md) \* s) <br>_Emit the carrier loop's telemetry records for the symbol just dumped._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**costas\_update**](#function-costas_update) ([**costas\_state\_t**](structcostas__state__t.md) \* s, float complex P) <br>_Per-symbol carrier update: discriminator -&gt; loop filter -&gt; steer NCO._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float complex | [**costas\_wipeoff**](#function-costas_wipeoff) ([**costas\_state\_t**](structcostas__state__t.md) \* s, float complex x) <br>_Per-sample carrier wipe-off: de-rotate_ `x` _by the NCO, advance it._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**COSTAS\_EPS**](costas__core_8h.md#define-costas_eps)  `1e-12f`<br> |
| define  | [**COSTAS\_LOCK\_ALPHA**](costas__core_8h.md#define-costas_lock_alpha)  `0.1`<br> |
| define  | [**COSTAS\_STATE\_MAGIC**](costas__core_8h.md#define-costas_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc)('C', 'S', 'T', 'S')`<br> |
| define  | [**COSTAS\_STATE\_VERSION**](costas__core_8h.md#define-costas_state_version)  `3u /\* v3: lockdet decision rule \*/`<br> |

## Detailed Description


A continuous BPSK carrier-recovery loop: per sample it de-rotates the input with the integer-phase `lo` NCO (carrier wipe-off); every `tsamps` samples it dumps the coherent integrate-and-dump accumulator, runs a decision-directed Costas phase discriminator, filters the error through an embedded 2nd-order [**loop\_filter\_state\_t**](structloop__filter__state__t.md), and steers the NCO frequency + phase. It tracks a small _residual_ carrier offset (the bulk Doppler is removed upstream by FFT acquisition); the steering NCO is `lo`, so the phase is bounded and exactly reproducible (no double-accumulator drift).


The block API (costas\_steps) is the Python face; the JM\_FORCEINLINE [**costas\_wipeoff()**](costas__core_8h.md#function-costas_wipeoff)/costas\_update() are the C composition API a despreader / tracking channel inlines into its own sample loop.


Lifecycle: `costas_create -> (steps / configure / reset)* -> costas_destroy`, or embed by value with [**costas\_init()**](costas__core_8h.md#function-costas_init).


Set `bn_fll > 0` to enable FLL assist (a wide-pull-in frequency-lock loop aiding the PLL) for large or fast-moving residuals; `bn_fll = 0` is a pure Costas PLL.



```C++
costas_state_t *c = costas_create(0.05, 0.707, 0.01, 64, 0.0);
float complex sym[16];
size_t k = costas_steps(c, rx, rx_len, sym, 16);  // one prompt per symbol
double f = c->nco.norm_freq;                       // tracked residual
costas_destroy(c);
```
 


    
## Public Functions Documentation




### function costas\_configure 

```C++
void costas_configure (
    costas_state_t * state,
    double bn,
    double zeta
) 
```




<hr>



### function costas\_configure\_lock 

_Re-tune the carrier lock detector's thresholds and verify counts._ 
```C++
void costas_configure_lock (
    costas_state_t * state,
    double up_thresh,
    double down_thresh,
    uint32_t n_up,
    uint32_t n_down
) 
```



The always-on lock decision steps a verify-counted detector ([**lockdet\_core.h**](lockdet__core_8h.md)) on the \|Re P\|/\|P\| lock-metric EMA once per dumped symbol: `locked` flips up after `n_up` consecutive symbols with the metric above `up_thresh` and drops after `n_down` consecutive symbols below `down_thresh`. The defaults derive from the metric's own H0 statistics — with no carrier, \|Re P\|/\|P\| = \|cos(theta)\| for a uniform theta, whose mean is 2/pi (~0.637) and per-symbol std ~0.31; the COSTAS\_LOCK\_ALPHA = 0.1 EMA reduces that to ~0.071, so the default declare threshold 0.85 sits ~3 sigma above the no-carrier mean, with the drop threshold at 0.78 for level hysteresis and 8-up/32-down verify counts for time hysteresis (declare fast, drop reluctantly — the EMA already correlates adjacent looks, so the counts guard against band-edge dwell rather than compounding i.i.d. probabilities). A live lock survives the re-tune; the in-flight verify run restarts.




**Parameters:**


* `state` Costas state. Must be non-NULL. 
* `up_thresh` Declare threshold on the lock-metric EMA. 
* `down_thresh` Drop threshold (&lt;= up\_thresh for level hysteresis). 
* `n_up` Consecutive above-threshold symbols to declare; clamped to &gt;= 1. 
* `n_down` Consecutive below-threshold symbols to drop; clamped to &gt;= 1. 
```C++
>>> from doppler.track import Costas
>>> c = Costas(bn=0.05, zeta=0.707, tsamps=64)
>>> c.locked
False
>>> c.configure_lock(0.9, 0.8, 4, 16)   # tighter declare, faster drop
```
 




        

<hr>



### function costas\_create 

_Create a Costas instance._ 
```C++
costas_state_t * costas_create (
    double bn,
    double zeta,
    double init_norm_freq,
    size_t tsamps,
    double bn_fll
) 
```





**Parameters:**


* `bn` Loop noise bandwidth (default 0.05). 
* `zeta` Damping factor (default 0.707). 
* `init_norm_freq` Seed carrier frequency, cycles/sample (default 0.0). 
* `tsamps` Samples per symbol (default 64). 
* `bn_fll` FLL-assist bandwidth (default 0.0 = pure PLL). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**costas\_destroy()**](costas__core_8h.md#function-costas_destroy) when done. 





        

<hr>



### function costas\_destroy 

_Destroy a Costas instance and release all memory._ 
```C++
void costas_destroy (
    costas_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function costas\_get\_bn 

```C++
double costas_get_bn (
    const costas_state_t * state
) 
```




<hr>



### function costas\_get\_bn\_fll 

```C++
double costas_get_bn_fll (
    const costas_state_t * state
) 
```




<hr>



### function costas\_get\_last\_error 

```C++
double costas_get_last_error (
    const costas_state_t * state
) 
```




<hr>



### function costas\_get\_lock\_metric 

```C++
double costas_get_lock_metric (
    const costas_state_t * state
) 
```




<hr>



### function costas\_get\_locked 

_Current carrier lock decision (1 = locked, 0 = not), from the verify-counted detector on the lock-metric EMA (see costas\_configure\_lock)._ 
```C++
int costas_get_locked (
    const costas_state_t * state
) 
```




<hr>



### function costas\_get\_norm\_freq 

```C++
double costas_get_norm_freq (
    const costas_state_t * state
) 
```




<hr>



### function costas\_get\_state 

_Serialize the full loop state into_ `blob` _._
```C++
void costas_get_state (
    const costas_state_t * state,
    void * blob
) 
```




<hr>



### function costas\_init 

_Initialise a Costas loop in place (no allocation)._ 
```C++
void costas_init (
    costas_state_t * s,
    double bn,
    double zeta,
    double init_norm_freq,
    size_t tsamps,
    double bn_fll
) 
```



The by-value counterpart to [**costas\_create()**](costas__core_8h.md#function-costas_create): a tracking channel that embeds a [**costas\_state\_t**](structcostas__state__t.md) initialises it here. Seeds the NCO at `init_norm_freq` and the loop integrator to the matching per-symbol frequency so de-rotation is correct from the first sample.




**Parameters:**


* `s` State to initialise. Must be non-NULL. 
* `bn` Loop noise bandwidth, normalised to the symbol rate. 
* `zeta` Damping factor (0.707 = critically damped). 
* `init_norm_freq` Seed carrier frequency, cycles/sample. 
* `tsamps` Samples per symbol (the integrate-and-dump period). 
* `bn_fll` FLL-assist bandwidth (0 = pure PLL). 




        

<hr>



### function costas\_reset 

_Re-seed the loop to its create-time frequency/phase; keep config._ 
```C++
void costas_reset (
    costas_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function costas\_set\_bn 

```C++
void costas_set_bn (
    costas_state_t * state,
    double val
) 
```




<hr>



### function costas\_set\_bn\_fll 

```C++
void costas_set_bn_fll (
    costas_state_t * state,
    double val
) 
```




<hr>



### function costas\_set\_norm\_freq 

```C++
void costas_set_norm_freq (
    costas_state_t * state,
    double val
) 
```




<hr>



### function costas\_set\_state 

_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._ 
```C++
int costas_set_state (
    costas_state_t * state,
    const void * blob
) 
```




<hr>



### function costas\_set\_telemetry 

_Attach (or detach) a telemetry context and register the carrier loop's probes on it. Registers four probes, emitted once per dumped symbol and further thinned by decim: "&lt;prefix&gt;.lock" (the \|Re P\|/\|P\| lock-metric EMA, 1 = phase-locked), "&lt;prefix&gt;.e" (the PLL discriminator output — the loop stress), "&lt;prefix&gt;.freq" (the tracked NCO frequency, cycles/sample) and "&lt;prefix&gt;.locked" (the verify-counted lock decision, 0/1 — see costas\_configure\_lock). Passing NULL detaches. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in_ [_**telemetry/telemetry.h**_](telemetry_8h.md) _)._
```C++
int costas_set_telemetry (
    costas_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "car" or "ch0.car". 
* `decim` Emit every decim-th symbol; &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take all four probes (the attach fails whole; the object stays detached). 
```C++
>>> import numpy as np
>>> from doppler.track import Costas
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> c = Costas(bn=0.05, zeta=0.707, tsamps=64)
>>> c.set_telemetry(tlm, "car")
>>> sorted(tlm.probe_names())
['car.e', 'car.freq', 'car.lock', 'car.locked']
>>> x = np.ones(64 * 100, dtype=np.complex64)
>>> _ = c.steps(x)
>>> recs = tlm.read()   # four records per dumped symbol
>>> len(recs) == 4 * 100
True
```
 





        

<hr>



### function costas\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t costas_state_bytes (
    const costas_state_t * state
) 
```




<hr>



### function costas\_steps 

```C++
size_t costas_steps (
    costas_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```




<hr>



### function costas\_steps\_max\_out 

```C++
size_t costas_steps_max_out (
    costas_state_t * state
) 
```




<hr>



### function costas\_tlm\_flush 

_Emit the carrier loop's telemetry records for the symbol just dumped._ 
```C++
void costas_tlm_flush (
    const costas_state_t * s
) 
```



Out-of-line on purpose: the emit machinery must not inline into a per-sample hot loop (inlined ring-write expansions bloat the loop body and an extern call site forces per-iteration state reloads — both measured ~20% slower detached on other loops). Callers gate on `s->tlm.ctx` and call this once per dumped symbol. Records "&lt;prefix&gt;.lock" (the \|Re P\|/\|P\| lock-metric EMA), "&lt;prefix&gt;.e" (the last PLL discriminator — the loop stress) and "&lt;prefix&gt;.freq" (the tracked NCO frequency, cycles/sample). A composing tracking channel (the DSSS despreader) calls this from its own per-epoch update.




**Parameters:**


* `s` State with a non-NULL tlm.ctx (caller-checked). 




        

<hr>



### function costas\_update 

_Per-symbol carrier update: discriminator -&gt; loop filter -&gt; steer NCO._ 
```C++
JM_FORCEINLINE  JM_HOT void costas_update (
    costas_state_t * s,
    float complex P
) 
```



Runs the decision-directed BPSK Costas discriminator on the prompt `P`, filters it, and writes the new frequency (lo\_set\_norm\_freq) plus a proportional phase nudge into the NCO. Updates the lock metric and last\_error (the instantaneous loop stress). Inline for composition.




**Parameters:**


* `s` Costas state. Must be non-NULL. 
* `P` The dumped integrate-and-dump prompt for this symbol. 




        

<hr>



### function costas\_wipeoff 

_Per-sample carrier wipe-off: de-rotate_ `x` _by the NCO, advance it._
```C++
JM_FORCEINLINE  JM_HOT float complex costas_wipeoff (
    costas_state_t * s,
    float complex x
) 
```



`x * conj(lo_step(nco))` — strips the (tracked) carrier ahead of the matched-filter integrate-and-dump. Inline, zero call overhead.




**Parameters:**


* `s` Costas state. Must be non-NULL. 
* `x` One input sample. 



**Returns:**

The de-rotated sample to feed the integrator. 





        

<hr>
## Macro Definition Documentation





### define COSTAS\_EPS 

```C++
#define COSTAS_EPS `1e-12f`
```




<hr>



### define COSTAS\_LOCK\_ALPHA 

```C++
#define COSTAS_LOCK_ALPHA `0.1`
```




<hr>



### define COSTAS\_STATE\_MAGIC 

```C++
#define COSTAS_STATE_MAGIC `DP_FOURCC ('C', 'S', 'T', 'S')`
```




<hr>



### define COSTAS\_STATE\_VERSION 

```C++
#define COSTAS_STATE_VERSION `3u /* v3: lockdet decision rule */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/costas/costas_core.h`

