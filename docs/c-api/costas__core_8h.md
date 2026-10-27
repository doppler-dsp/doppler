

# File costas\_core.h



[**FileList**](files.md) **>** [**costas**](dir_8ebd78c7800b34d5dee6ef27ff63e7b3.md) **>** [**costas\_core.h**](costas__core_8h.md)

[Go to the source code of this file](costas__core_8h_source.md)

_Costas carrier-tracking loop (integer-NCO de-rotation + PI loop)._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/lo/lo_core.h"`
* `#include "doppler/lockdet/lockdet_core.h"`
* `#include "doppler/loop_filter/loop_filter_core.h"`
* `#include "doppler/dp_tlm/dp_tlm_core.h"`
* `#include <math.h>`
* `#include "doppler/telemetry/telemetry_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**costas\_tlm\_t**](structcostas__tlm__t.md) <br>_Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then a single predicted-not-taken branch per symbol. Zeroed in state blobs and preserved across set\_state (DP\_DEFINE\_POD\_STATE\_TLM)._  |
| struct | [**dp\_costas\_state\_t**](structdp__costas__state__t.md) <br>_Costas loop state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**costas\_update**](#function-costas_update) ([**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* s, float \_Complex P) <br>_Per-symbol carrier update: discriminator -&gt; loop filter -&gt; steer NCO._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float \_Complex | [**costas\_wipeoff**](#function-costas_wipeoff) ([**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* s, float \_Complex x) <br>_Per-sample carrier wipe-off: de-rotate_ `x` _by the NCO, advance it._ |
|  int | [**dp\_costas\_configure**](#function-dp_costas_configure) ([**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state, double bn, double zeta) <br>_Recompute the loop-filter gains for a new (_ `bn` _,_`zeta` _) without disturbing the frequency/phase estimate._ |
|  void | [**dp\_costas\_configure\_lock**](#function-dp_costas_configure_lock) ([**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune the carrier lock detector's thresholds and verify counts._  |
|  [**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* | [**dp\_costas\_create**](#function-dp_costas_create) (double bn, double zeta, double init\_norm\_freq, size\_t tsamps, double bn\_fll) <br>_Create a Costas instance._  |
|  void | [**dp\_costas\_destroy**](#function-dp_costas_destroy) ([**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state) <br>_Destroy a Costas instance and release all memory._  |
|  double | [**dp\_costas\_get\_bn**](#function-dp_costas_get_bn) (const [**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state) <br>_PLL loop noise bandwidth currently in use._  |
|  double | [**dp\_costas\_get\_bn\_fll**](#function-dp_costas_get_bn_fll) (const [**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state) <br>_FLL-assist bandwidth._  |
|  double | [**dp\_costas\_get\_last\_error**](#function-dp_costas_get_last_error) (const [**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state) <br>_Last PLL discriminator output, the loop stress._  |
|  double | [**dp\_costas\_get\_lock\_metric**](#function-dp_costas_get_lock_metric) (const [**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state) <br>_Lock-metric EMA: \|Re P\|/\|P\| smoothed by COSTAS\_LOCK\_ALPHA._  |
|  int | [**dp\_costas\_get\_locked**](#function-dp_costas_get_locked) (const [**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state) <br>_Current carrier lock decision (1 = locked, 0 = not), from the verify-counted detector on the lock-metric EMA (see dp\_costas\_configure\_lock)._  |
|  double | [**dp\_costas\_get\_nco\_freq**](#function-dp_costas_get_nco_freq) (const [**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state) <br>_Effective NCO frequency command (loop-filter output = integrator + proportional), cycles/sample. Mean rides a ramp with no lag, unlike the integrator-only get\_norm\_freq._  |
|  double | [**dp\_costas\_get\_norm\_freq**](#function-dp_costas_get_norm_freq) (const [**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state) <br>_Tracked carrier frequency: the NCO frequency register._  |
|  void | [**dp\_costas\_get\_state**](#function-dp_costas_get_state) (const [**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state, void \* blob) <br>_Serialize the full loop state into_ `blob` _._ |
|  void | [**dp\_costas\_init**](#function-dp_costas_init) ([**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* s, double bn, double zeta, double init\_norm\_freq, size\_t tsamps, double bn\_fll) <br>_Initialise a Costas loop in place (no allocation)._  |
|  void | [**dp\_costas\_reset**](#function-dp_costas_reset) ([**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state) <br>_Re-seed the loop to its create-time frequency/phase; keep config._  |
|  int | [**dp\_costas\_set\_bn**](#function-dp_costas_set_bn) ([**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state, double val) <br>_Retune bn, keeping zeta:_ [_**dp\_costas\_configure()**_](costas__core_8h.md#function-dp_costas_configure) _'s refusal and status. As a writable property a refusal raises ValueError and the value stays put (doppler#1987)._ |
|  void | [**dp\_costas\_set\_bn\_fll**](#function-dp_costas_set_bn_fll) ([**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state, double val) <br>_Set the FLL-assist bandwidth live; the FLL gain becomes 4 \*_ `val` _._ |
|  void | [**dp\_costas\_set\_norm\_freq**](#function-dp_costas_set_norm_freq) ([**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state, double val) <br>_Re-seed the loop at a new carrier frequency._  |
|  int | [**dp\_costas\_set\_state**](#function-dp_costas_set_state) ([**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state, const void \* blob) <br>_Restore a state written by_ [_**dp\_costas\_get\_state()**_](costas__core_8h.md#function-dp_costas_get_state) _._ |
|  int | [**dp\_costas\_set\_telemetry**](#function-dp_costas_set_telemetry) ([**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context and register the carrier loop's probes on it. Registers five probes, emitted once per dumped symbol and further thinned by decim: "&lt;prefix&gt;.lock" (the \|Re P\|/\|P\| lock-metric EMA, 1 = phase-locked), "&lt;prefix&gt;.e" (the PLL discriminator output — the loop stress), "&lt;prefix&gt;.freq" (the NCO's frequency register, which holds the loop filter's integrator: the loop's frequency memory, cycles/sample), "&lt;prefix&gt;.locked" (the verify-counted lock decision, 0/1 — see dp\_costas\_configure\_lock) and "&lt;prefix&gt;.nco" (the complete loop-filter output, the integrator plus the proportional term, cycles/sample —_ [_**dp\_costas\_get\_nco\_freq()**_](costas__core_8h.md#function-dp_costas_get_nco_freq) _; its mean rides a frequency ramp with no lag, where ".freq" lags by the loop's ramp error). Passing NULL detaches. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in_[_**dp\_tlm/dp\_tlm\_core.h**_](dp__tlm__core_8h.md) _)._ |
|  size\_t | [**dp\_costas\_state\_bytes**](#function-dp_costas_state_bytes) (const [**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  size\_t | [**dp\_costas\_steps**](#function-dp_costas_steps) ([**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, float \_Complex \* out, size\_t max\_out) <br>_De-rotate a cf32 block with the carrier NCO, integrate-and-dump each symbol, and emit one decision-directed Costas prompt per symbol._  |
|  size\_t | [**dp\_costas\_steps\_max\_out**](#function-dp_costas_steps_max_out) ([**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* state) <br>_Output capacity hint for_ [_**dp\_costas\_steps()**_](costas__core_8h.md#function-dp_costas_steps) _: always 0._ |
|  void | [**dp\_costas\_tlm\_flush**](#function-dp_costas_tlm_flush) (const [**dp\_costas\_state\_t**](structdp__costas__state__t.md) \* s) <br>_Emit the carrier loop's telemetry records for the symbol just dumped._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**COSTAS\_EPS**](costas__core_8h.md#define-costas_eps)  `1e-12f`<br>_Numerical guard added to \|P\| in the discriminator and lock metric so a zero prompt cannot divide by zero (not tunable)._  |
| define  | [**COSTAS\_LOCK\_ALPHA**](costas__core_8h.md#define-costas_lock_alpha)  `0.1`<br>_EMA smoothing factor (per symbol) for the \|Re P\|/\|P\| lock metric, the input to the lock detector._  |
| define  | [**COSTAS\_STATE\_MAGIC**](costas__core_8h.md#define-costas_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc)('C', 'S', 'T', 'S')`<br>_State-blob magic for the Costas loop (FourCC "CSTS")._  |
| define  | [**COSTAS\_STATE\_VERSION**](costas__core_8h.md#define-costas_state_version)  `3u /\* v3: lockdet decision rule \*/`<br>_State-blob layout version (v3 added the lockdet decision rule)._  |

## Detailed Description


A continuous BPSK carrier-recovery loop: per sample it de-rotates the input with the integer-phase `lo` NCO (carrier wipe-off); every `tsamps` samples it dumps the coherent integrate-and-dump accumulator, runs a decision-directed Costas phase discriminator, filters the error through an embedded 2nd-order [**dp\_loop\_filter\_state\_t**](structdp__loop__filter__state__t.md), and steers the NCO frequency + phase. It tracks a small _residual_ carrier offset (the bulk Doppler is removed upstream by FFT acquisition); the steering NCO is `lo`, so the phase is bounded and exactly reproducible (no double-accumulator drift).


The block API (dp\_costas\_steps) is the Python face; the JM\_FORCEINLINE [**costas\_wipeoff()**](costas__core_8h.md#function-costas_wipeoff)/costas\_update() are the C composition API a despreader / tracking channel inlines into its own sample loop.


Lifecycle: `dp_costas_create -> (steps / configure / reset)* -> dp_costas_destroy`, or embed by value with [**dp\_costas\_init()**](costas__core_8h.md#function-dp_costas_init).


Set `bn_fll > 0` to enable FLL assist (a wide-pull-in frequency-lock loop aiding the PLL) for large or fast-moving residuals; `bn_fll = 0` is a pure Costas PLL.



```C++
dp_costas_state_t *c = dp_costas_create(0.05, 0.707, 0.01, 64, 0.0);
float _Complex sym[16];
size_t k = dp_costas_steps(c, rx, rx_len, sym, 16);  // one prompt per symbol
double f = c->nco.norm_freq;                       // tracked residual
dp_costas_destroy(c);
```
 


    
## Public Functions Documentation




### function costas\_update 

_Per-symbol carrier update: discriminator -&gt; loop filter -&gt; steer NCO._ 
```C++
JM_FORCEINLINE  JM_HOT void costas_update (
    dp_costas_state_t * s,
    float _Complex P
) 
```



Runs the decision-directed BPSK Costas discriminator on the prompt `P`, filters it, and writes the new frequency (dp\_lo\_set\_norm\_freq) plus a proportional phase nudge into the NCO. Updates the lock metric and last\_error (the instantaneous loop stress). Inline for composition.




**Parameters:**


* `s` Costas state. Must be non-NULL. 
* `P` The dumped integrate-and-dump prompt for this symbol. 




        

<hr>



### function costas\_wipeoff 

_Per-sample carrier wipe-off: de-rotate_ `x` _by the NCO, advance it._
```C++
JM_FORCEINLINE  JM_HOT float _Complex costas_wipeoff (
    dp_costas_state_t * s,
    float _Complex x
) 
```



`x * conj(lo_step(nco))` — strips the (tracked) carrier ahead of the matched-filter integrate-and-dump. Inline, zero call overhead.




**Parameters:**


* `s` Costas state. Must be non-NULL. 
* `x` One input sample. 



**Returns:**

The de-rotated sample to feed the integrator. 





        

<hr>



### function dp\_costas\_configure 

_Recompute the loop-filter gains for a new (_ `bn` _,_`zeta` _) without disturbing the frequency/phase estimate._
```C++
int dp_costas_configure (
    dp_costas_state_t * state,
    double bn,
    double zeta
) 
```



Re-derives the PI coefficients from the loop bandwidth and damping and installs them live. The NCO frequency, phase and loop integrator are left untouched, so a converged loop keeps tracking straight through the re-tune — narrow the bandwidth once pulled in for lower phase jitter, or widen it to chase a faster-moving residual.




**Parameters:**


* `state` Costas state. Must be non-NULL. 
* `bn` Loop noise bandwidth, normalised to the symbol rate. 
* `zeta` Damping factor (0.707 = critically damped). 
```C++
>>> from doppler.track import Costas
>>> c = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.01, tsamps=16)
>>> c.configure(0.02, 1.0)              # narrow the loop, over-damp
>>> (round(c.bn, 3), round(c.norm_freq, 3))  # new gains, est kept
(0.02, 0.01)
```
 



**Returns:**

`DP_OK`, or `DP_ERR_INVALID` for a (bn, zeta) outside [**dp\_loop\_filter\_params\_ok()**](loop__filter__core_8h.md#function-dp_loop_filter_params_ok); a refusal changes nothing, so the bandwidth the getter reports is always the one the loop runs (doppler#2112). 





        

<hr>



### function dp\_costas\_configure\_lock 

_Re-tune the carrier lock detector's thresholds and verify counts._ 
```C++
void dp_costas_configure_lock (
    dp_costas_state_t * state,
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



### function dp\_costas\_create 

_Create a Costas instance._ 
```C++
dp_costas_state_t * dp_costas_create (
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

Caller must call [**dp\_costas\_destroy()**](costas__core_8h.md#function-dp_costas_destroy) when done. 





        

<hr>



### function dp\_costas\_destroy 

_Destroy a Costas instance and release all memory._ 
```C++
void dp_costas_destroy (
    dp_costas_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_costas\_get\_bn 

_PLL loop noise bandwidth currently in use._ 
```C++
double dp_costas_get_bn (
    const dp_costas_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

bn, normalized to the symbol rate: the create() value or the last one [**dp\_costas\_configure()**](costas__core_8h.md#function-dp_costas_configure) / [**dp\_costas\_set\_bn()**](costas__core_8h.md#function-dp_costas_set_bn) accepted. 





        

<hr>



### function dp\_costas\_get\_bn\_fll 

_FLL-assist bandwidth._ 
```C++
double dp_costas_get_bn_fll (
    const dp_costas_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

bn\_fll, normalized to the symbol rate; 0 means pure PLL. 





        

<hr>



### function dp\_costas\_get\_last\_error 

_Last PLL discriminator output, the loop stress._ 
```C++
double dp_costas_get_last_error (
    const dp_costas_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

sign(Re P) \* Im P / \|P\|, about the phase error in radians when small, between -1 and 1; 0 before the first symbol. 





        

<hr>



### function dp\_costas\_get\_lock\_metric 

_Lock-metric EMA: \|Re P\|/\|P\| smoothed by COSTAS\_LOCK\_ALPHA._ 
```C++
double dp_costas_get_lock_metric (
    const dp_costas_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Value from 0 to 1; near 1 when phase-locked, ~2/pi with no carrier; 0 after create/reset until the first symbol. 





        

<hr>



### function dp\_costas\_get\_locked 

_Current carrier lock decision (1 = locked, 0 = not), from the verify-counted detector on the lock-metric EMA (see dp\_costas\_configure\_lock)._ 
```C++
int dp_costas_get_locked (
    const dp_costas_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

1 when locked, 0 otherwise (0 after create/reset). 





        

<hr>



### function dp\_costas\_get\_nco\_freq 

_Effective NCO frequency command (loop-filter output = integrator + proportional), cycles/sample. Mean rides a ramp with no lag, unlike the integrator-only get\_norm\_freq._ 
```C++
double dp_costas_get_nco_freq (
    const dp_costas_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

norm\_freq + kp \* last\_error / (2 pi tsamps), in cycles/sample; equals the seed frequency before the first symbol. 





        

<hr>



### function dp\_costas\_get\_norm\_freq 

_Tracked carrier frequency: the NCO frequency register._ 
```C++
double dp_costas_get_norm_freq (
    const dp_costas_state_t * state
) 
```



Holds the loop filter's integrator (the frequency memory); it excludes the proportional term, so on a frequency ramp it lags by the Type-II ramp error (see [**dp\_costas\_get\_nco\_freq()**](costas__core_8h.md#function-dp_costas_get_nco_freq)).




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Frequency in cycles/sample; the seed frequency before the first symbol is dumped. 





        

<hr>



### function dp\_costas\_get\_state 

_Serialize the full loop state into_ `blob` _._
```C++
void dp_costas_get_state (
    const dp_costas_state_t * state,
    void * blob
) 
```



A whole-struct snapshot (config, NCO, loop filter, I&D accumulator, lock detector) with the telemetry attachment zeroed.




**Parameters:**


* `state` Must be non-NULL. 
* `blob` Caller-owned buffer of at least [**dp\_costas\_state\_bytes()**](costas__core_8h.md#function-dp_costas_state_bytes) bytes. 




        

<hr>



### function dp\_costas\_init 

_Initialise a Costas loop in place (no allocation)._ 
```C++
void dp_costas_init (
    dp_costas_state_t * s,
    double bn,
    double zeta,
    double init_norm_freq,
    size_t tsamps,
    double bn_fll
) 
```



The by-value counterpart to [**dp\_costas\_create()**](costas__core_8h.md#function-dp_costas_create): a tracking channel that embeds a [**dp\_costas\_state\_t**](structdp__costas__state__t.md) initialises it here. Seeds the NCO at `init_norm_freq` and the loop integrator to the matching per-symbol frequency so de-rotation is correct from the first sample.




**Parameters:**


* `s` State to initialise. Must be non-NULL. 
* `bn` Loop noise bandwidth, normalised to the symbol rate. 
* `zeta` Damping factor (0.707 = critically damped). 
* `init_norm_freq` Seed carrier frequency, cycles/sample. 
* `tsamps` Samples per symbol (the integrate-and-dump period). 
* `bn_fll` FLL-assist bandwidth (0 = pure PLL).

Unguarded, as the by-value path: the caller checks (bn, zeta) with [**dp\_loop\_filter\_params\_ok()**](loop__filter__core_8h.md#function-dp_loop_filter_params_ok) first. Outside it the loop's gains go NaN (doppler#2103). 


        

<hr>



### function dp\_costas\_reset 

_Re-seed the loop to its create-time frequency/phase; keep config._ 
```C++
void dp_costas_reset (
    dp_costas_state_t * state
) 
```



Drops the lock and rewinds the NCO, loop integrator and integrate-and-dump accumulators to the create-time seed frequency, while retaining the configured loop bandwidth, damping and lock-detector thresholds. Reprocess the same input after a reset and the output is bit-identical.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.track import Costas
>>> tsamps = 16
>>> rng = np.random.default_rng(3)
>>> bits = rng.integers(0, 2, 1500) * 2 - 1
>>> sig = np.repeat(bits.astype(np.complex64), tsamps)
>>> k = np.arange(len(sig))
>>> rx = (sig * np.exp(2j * np.pi * 0.002 * k)).astype(np.complex64)
>>> c = Costas(bn=0.05, zeta=0.707, tsamps=tsamps)
>>> _ = c.steps(rx)
>>> round(c.norm_freq, 4) != 0.0     # loop pulled onto the residual
True
>>> c.reset()
>>> c.norm_freq                       # back to the create-time seed
0.0
>>> c.lock_metric
0.0
```
 




        

<hr>



### function dp\_costas\_set\_bn 

_Retune bn, keeping zeta:_ [_**dp\_costas\_configure()**_](costas__core_8h.md#function-dp_costas_configure) _'s refusal and status. As a writable property a refusal raises ValueError and the value stays put (doppler#1987)._
```C++
int dp_costas_set_bn (
    dp_costas_state_t * state,
    double val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New loop noise bandwidth, normalized to the symbol rate. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if (val, zeta) fails [**dp\_loop\_filter\_params\_ok()**](loop__filter__core_8h.md#function-dp_loop_filter_params_ok) (nothing changes). 





        

<hr>



### function dp\_costas\_set\_bn\_fll 

_Set the FLL-assist bandwidth live; the FLL gain becomes 4 \*_ `val` _._
```C++
void dp_costas_set_bn_fll (
    dp_costas_state_t * state,
    double val
) 
```



Not validated: any value &lt;= 0 disables the assist.




**Parameters:**


* `state` Must be non-NULL. 
* `val` FLL bandwidth, normalized to the symbol rate (0 = off). 




        

<hr>



### function dp\_costas\_set\_norm\_freq 

_Re-seed the loop at a new carrier frequency._ 
```C++
void dp_costas_set_norm_freq (
    dp_costas_state_t * state,
    double val
) 
```



Not a gentle nudge: it resets the loop filter, re-initializes the NCO (phase included) at `val`, clears the integrate-and-dump accumulator, the lock metric and the lock decision, and makes `val` the frequency a later [**dp\_costas\_reset()**](costas__core_8h.md#function-dp_costas_reset) returns to.




**Parameters:**


* `state` Must be non-NULL. 
* `val` New carrier frequency, cycles/sample. 




        

<hr>



### function dp\_costas\_set\_state 

_Restore a state written by_ [_**dp\_costas\_get\_state()**_](costas__core_8h.md#function-dp_costas_get_state) _._
```C++
int dp_costas_set_state (
    dp_costas_state_t * state,
    const void * blob
) 
```



Restores every field, config included, but keeps the receiver's own telemetry attachment. On rejection `state` is untouched.




**Parameters:**


* `state` Must be non-NULL. 
* `blob` Blob from [**dp\_costas\_get\_state()**](costas__core_8h.md#function-dp_costas_get_state). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID on a magic, version, endian or size mismatch. 





        

<hr>



### function dp\_costas\_set\_telemetry 

_Attach (or detach) a telemetry context and register the carrier loop's probes on it. Registers five probes, emitted once per dumped symbol and further thinned by decim: "&lt;prefix&gt;.lock" (the \|Re P\|/\|P\| lock-metric EMA, 1 = phase-locked), "&lt;prefix&gt;.e" (the PLL discriminator output — the loop stress), "&lt;prefix&gt;.freq" (the NCO's frequency register, which holds the loop filter's integrator: the loop's frequency memory, cycles/sample), "&lt;prefix&gt;.locked" (the verify-counted lock decision, 0/1 — see dp\_costas\_configure\_lock) and "&lt;prefix&gt;.nco" (the complete loop-filter output, the integrator plus the proportional term, cycles/sample —_ [_**dp\_costas\_get\_nco\_freq()**_](costas__core_8h.md#function-dp_costas_get_nco_freq) _; its mean rides a frequency ramp with no lag, where ".freq" lags by the loop's ramp error). Passing NULL detaches. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in_[_**dp\_tlm/dp\_tlm\_core.h**_](dp__tlm__core_8h.md) _)._
```C++
int dp_costas_set_telemetry (
    dp_costas_state_t * state,
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

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take all five probes (the attach fails whole; the object stays detached). 
```C++
>>> import numpy as np
>>> from doppler.track import Costas
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> c = Costas(bn=0.05, zeta=0.707, tsamps=64)
>>> c.set_telemetry(tlm, "car")
>>> sorted(tlm.probe_names)
['car.e', 'car.freq', 'car.lock', 'car.locked', 'car.nco']
>>> x = np.ones(64 * 100, dtype=np.complex64)
>>> _ = c.steps(x)
>>> recs = tlm.read()   # five records per dumped symbol
>>> len(recs) == 5 * 100
True
```
 





        

<hr>



### function dp\_costas\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t dp_costas_state_bytes (
    const dp_costas_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL (its contents do not affect the size). 



**Returns:**

Bytes: the [**dp\_state\_hdr\_t**](structdp__state__hdr__t.md) envelope plus the whole struct. 





        

<hr>



### function dp\_costas\_steps 

_De-rotate a cf32 block with the carrier NCO, integrate-and-dump each symbol, and emit one decision-directed Costas prompt per symbol._ 
```C++
size_t dp_costas_steps (
    dp_costas_state_t * state,
    const float _Complex * x,
    size_t x_len,
    float _Complex * out,
    size_t max_out
) 
```



The streaming Python face of the loop. For every input sample it wipes the (tracked) carrier off `x` with the integer-phase NCO, sums the result into the coherent integrate-and-dump accumulator, and on each symbol boundary (one every tsamps samples) dumps the accumulator as the prompt, runs the BPSK Costas discriminator to steer the NCO frequency and phase, and appends the mean-scaled prompt to the output. Loop state carries across calls, so a long capture can be fed block by block; exactly one prompt symbol comes out per tsamps input samples.




**Parameters:**


* `state` Costas state. Must be non-NULL. 
* `x` Input samples, one complex baseband sample each. 
* `x_len` Number of input samples in `x`. 
* `out` Prompt-symbol output buffer. 
* `max_out` Capacity of `out`, in symbols. 



**Returns:**

Number of prompt symbols written to `out` (one per `tsamps` input samples). On the Python face this is the recovered-symbol array. 
```C++
>>> import numpy as np
>>> from doppler.track import Costas
>>> tsamps = 16
>>> rng = np.random.default_rng(1)
>>> bits = rng.integers(0, 2, 4000) * 2 - 1
>>> sig = np.repeat(bits.astype(np.complex64), tsamps)
>>> k = np.arange(len(sig))
>>> rx = (sig * np.exp(2j * np.pi * 0.003 * k)).astype(np.complex64)
>>> c = Costas(bn=0.05, zeta=0.707, tsamps=tsamps)
>>> sym = c.steps(rx)             # one prompt per tsamps samples
>>> sym.shape
(4000,)
>>> round(c.norm_freq, 4)         # pulled onto the 0.003 residual
0.003
>>> c.lock_metric > 0.9
True
```
 





        

<hr>



### function dp\_costas\_steps\_max\_out 

_Output capacity hint for_ [_**dp\_costas\_steps()**_](costas__core_8h.md#function-dp_costas_steps) _: always 0._
```C++
size_t dp_costas_steps_max_out (
    dp_costas_state_t * state
) 
```



The output is one prompt per `tsamps` inputs, so it never exceeds the input length; size `out` to the input length (or input / tsamps).




**Parameters:**


* `state` Unused. 



**Returns:**

0 (no state-derived bound). 





        

<hr>



### function dp\_costas\_tlm\_flush 

_Emit the carrier loop's telemetry records for the symbol just dumped._ 
```C++
void dp_costas_tlm_flush (
    const dp_costas_state_t * s
) 
```



Out-of-line on purpose: the emit machinery must not inline into a per-sample hot loop (inlined ring-write expansions bloat the loop body and an extern call site forces per-iteration state reloads — both measured ~20% slower detached on other loops). Callers gate on `s->tlm.ctx` and call this once per dumped symbol. Records "&lt;prefix&gt;.lock" (the \|Re P\|/\|P\| lock-metric EMA), "&lt;prefix&gt;.e" (the last PLL discriminator — the loop stress) and "&lt;prefix&gt;.freq" (the tracked NCO frequency, cycles/sample). A composing tracking channel (the DSSS despreader) calls this from its own per-epoch update.




**Parameters:**


* `s` State with a non-NULL tlm.ctx (caller-checked). 




        

<hr>
## Macro Definition Documentation





### define COSTAS\_EPS 

_Numerical guard added to \|P\| in the discriminator and lock metric so a zero prompt cannot divide by zero (not tunable)._ 
```C++
#define COSTAS_EPS `1e-12f`
```




<hr>



### define COSTAS\_LOCK\_ALPHA 

_EMA smoothing factor (per symbol) for the \|Re P\|/\|P\| lock metric, the input to the lock detector._ 
```C++
#define COSTAS_LOCK_ALPHA `0.1`
```




<hr>



### define COSTAS\_STATE\_MAGIC 

_State-blob magic for the Costas loop (FourCC "CSTS")._ 
```C++
#define COSTAS_STATE_MAGIC `DP_FOURCC ('C', 'S', 'T', 'S')`
```




<hr>



### define COSTAS\_STATE\_VERSION 

_State-blob layout version (v3 added the lockdet decision rule)._ 
```C++
#define COSTAS_STATE_VERSION `3u /* v3: lockdet decision rule */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/costas/costas_core.h`

