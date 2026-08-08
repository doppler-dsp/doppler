

# File resamp\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**resamp**](dir_289a9297ce406b952fab973539197d1c.md) **>** [**resamp\_core.h**](resamp__core_8h.md)

[Go to the source code of this file](resamp__core_8h_source.md)

_Continuously-variable polyphase resampler for CF32 IQ._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "nco/nco_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**resamp\_state\_t**](structresamp__state__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**resamp\_state\_t**](structresamp__state__t.md) \* | [**resamp\_create**](#function-resamp_create) (double rate) <br> |
|  [**resamp\_state\_t**](structresamp__state__t.md) \* | [**resamp\_create\_custom**](#function-resamp_create_custom) (size\_t num\_phases, size\_t num\_taps, const float \* bank, double rate) <br> |
|  void | [**resamp\_destroy**](#function-resamp_destroy) ([**resamp\_state\_t**](structresamp__state__t.md) \* state) <br> |
|  size\_t | [**resamp\_execute**](#function-resamp_execute) ([**resamp\_state\_t**](structresamp__state__t.md) \* state, const float \_Complex \* in, size\_t num\_in, float \_Complex \* out, size\_t max\_out) <br>_Resample a block of CF32 samples (fixed rate)._  |
|  size\_t | [**resamp\_execute\_ctrl**](#function-resamp_execute_ctrl) ([**resamp\_state\_t**](structresamp__state__t.md) \* state, const float \_Complex \* in, const float \_Complex \* ctrl, size\_t num\_in, float \_Complex \* out, size\_t max\_out) <br>_Resample with per-sample additive rate deviation._  |
|  size\_t | [**resamp\_execute\_ctrl\_push**](#function-resamp_execute_ctrl_push) ([**resamp\_state\_t**](structresamp__state__t.md) \* state, float \_Complex x, double ctrl, float \_Complex \* out, size\_t max\_out) <br>_Push one input at an instantaneous rate deviation; emit any outputs._  |
|  double | [**resamp\_get\_ctrl\_acc**](#function-resamp_get_ctrl_acc) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br>_The control accumulator's fractional phase, in [0, 1)._  |
|  size\_t | [**resamp\_get\_num\_phases**](#function-resamp_get_num_phases) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br> |
|  size\_t | [**resamp\_get\_num\_taps**](#function-resamp_get_num_taps) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br> |
|  double | [**resamp\_get\_rate**](#function-resamp_get_rate) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br> |
|  void | [**resamp\_get\_state**](#function-resamp_get_state) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state, void \* blob) <br>_Serialize_ `state's` _mutable state into_`blob` _._ |
|  size\_t | [**resamp\_interp\_fill**](#function-resamp_interp_fill) ([**resamp\_state\_t**](structresamp__state__t.md) \* state, const float \_Complex \* in, float \_Complex \* out, size\_t max\_out) <br>_Emit exactly_ `max_out` _interpolated outputs, pulling inputs on overflow._ |
|  size\_t | [**resamp\_interp\_inputs\_needed**](#function-resamp_interp_inputs_needed) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state, size\_t max\_out) <br>_Input samples an interpolating fill of_ `max_out` _outputs consumes._ |
|  void | [**resamp\_reset**](#function-resamp_reset) ([**resamp\_state\_t**](structresamp__state__t.md) \* state) <br> |
|  void | [**resamp\_set\_rate**](#function-resamp_set_rate) ([**resamp\_state\_t**](structresamp__state__t.md) \* state, double rate) <br> |
|  int | [**resamp\_set\_state**](#function-resamp_set_state) ([**resamp\_state\_t**](structresamp__state__t.md) \* state, const void \* blob) <br>_Restore mutable state from_ `blob` _(same rate)._ |
|  size\_t | [**resamp\_state\_bytes**](#function-resamp_state_bytes) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br>_Bytes_ [_**resamp\_get\_state()**_](resamp__core_8h.md#function-resamp_get_state) _writes for_`state` _(envelope + payload)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**RESAMP\_STATE\_MAGIC**](resamp__core_8h.md#define-resamp_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('R', 'S', 'M', 'P')`<br> |
| define  | [**RESAMP\_STATE\_VERSION**](resamp__core_8h.md#define-resamp_state_version)  `2u`<br> |

## Detailed Description


Two execute paths:


resamp\_execute — dual-mode:
* Interpolation (rate &gt;= 1): output-driven, one NCO tick per output sample, overflow pushes the next input into the delay line.
* Decimation (rate &lt; 1): input-driven transposed-form polyphase. Each input is multiplied by the current polyphase arm and accumulated into N integrate-and-dump registers; on NCO overflow the I&D dump through a transposed tapped delay line to produce one output. Bank coefficients are pre-scaled by rate so the passband gain is unity.




resamp\_execute\_ctrl — unified input-driven with a double-precision accumulator that handles all rates and per-sample deviations. Each input advances the accumulator by (rate + ctrl(i)); every time the accumulator crosses 1.0 an output is emitted.


Phase accumulator (execute): upper log2(num\_phases) bits of the 32-bit NCO word index the polyphase bank — nearest-neighbor, no interpolation between branches.


Default constructor builds a 4096-phase × 19-tap Kaiser bank (60 dB rejection, 0.4/0.6 pass/stop) at first call. Use [**resamp\_create\_custom()**](resamp__core_8h.md#function-resamp_create_custom) to supply your own bank.


Lifecycle: 
```C++
resamp_state_t *r = resamp_create(0.5);
float _Complex out[64];
size_t n = resamp_execute(r, in, 128, out, 64);
resamp_destroy(r);
```
 


    
## Public Functions Documentation




### function resamp\_create 

```C++
resamp_state_t * resamp_create (
    double rate
) 
```



Built-in 4096×19 Kaiser bank (60 dB, 0.4/0.6 pass/stop). 


        

<hr>



### function resamp\_create\_custom 

```C++
resamp_state_t * resamp_create_custom (
    size_t num_phases,
    size_t num_taps,
    const float * bank,
    double rate
) 
```



User-supplied bank, shape num\_phases × num\_taps, row-major. num\_phases must be a power of two. 


        

<hr>



### function resamp\_destroy 

```C++
void resamp_destroy (
    resamp_state_t * state
) 
```



Free all resources. NULL is a no-op. 


        

<hr>



### function resamp\_execute 

_Resample a block of CF32 samples (fixed rate)._ 
```C++
size_t resamp_execute (
    resamp_state_t * state,
    const float _Complex * in,
    size_t num_in,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `in` Input samples. 
* `num_in` Number of input samples. 
* `out` Output buffer. 
* `max_out` Capacity of out in samples. 



**Returns:**

Number of output samples written. 





        

<hr>



### function resamp\_execute\_ctrl 

_Resample with per-sample additive rate deviation._ 
```C++
size_t resamp_execute_ctrl (
    resamp_state_t * state,
    const float _Complex * in,
    const float _Complex * ctrl,
    size_t num_in,
    float _Complex * out,
    size_t max_out
) 
```



rate\_i = base\_rate + crealf(ctrl(i)). ctrl is treated as real-valued; only the real part of each element is used.


Output buffer: allocate ceil(num\_in × (rate + max\_ctrl)) samples.




**Parameters:**


* `state` Must be non-NULL. 
* `in` Input CF32 samples (length num\_in). 
* `ctrl` Rate deviations, parallel to in (float \_Complex, real part only, length num\_in). 
* `num_in` Number of input samples (= length of ctrl). 
* `out` Output buffer. 
* `max_out` Capacity of out in samples. 



**Returns:**

Number of output samples written. 





        

<hr>



### function resamp\_execute\_ctrl\_push 

_Push one input at an instantaneous rate deviation; emit any outputs._ 
```C++
size_t resamp_execute_ctrl_push (
    resamp_state_t * state,
    float _Complex x,
    double ctrl,
    float _Complex * out,
    size_t max_out
) 
```



The single-input streaming form of [**resamp\_execute\_ctrl()**](resamp__core_8h.md#function-resamp_execute_ctrl): pushes `x` into the delay line, advances the double-precision accumulator by `rate + ctrl`, and emits every output whose accumulator period completes (0 for a decimator between strobes, 1 typically, or several for an interpolator) at the polyphase arm the fractional remainder selects. Feeding a stream of `(x, ctrl)` through this one input at a time reproduces [**resamp\_execute\_ctrl()**](resamp__core_8h.md#function-resamp_execute_ctrl) on the same `(in, ctrl[])` bit-for-bit — but, unlike the block form's precomputed `ctrl[]`, `ctrl` here can depend on the outputs already emitted. That closes the loop: a timing-recovery or rate-tracking loop reads each emitted output, computes its correction, and feeds it back as the next call's `ctrl` to steer the strobe. This is the per-output feedback a matched-filter timing loop (track.RrcSync) needs and the block `execute_ctrl` cannot provide.




**Parameters:**


* `state` Must be non-NULL. 
* `x` One input sample. 
* `ctrl` Rate deviation added to the base rate for this input (real-valued; the effective rate is `rate + ctrl`). 
* `out` Output buffer for any emitted samples. 
* `max_out` Capacity of `out` (emission stops at this bound). 



**Returns:**

Number of outputs emitted into `out` (0, 1, or more). 





        

<hr>



### function resamp\_get\_ctrl\_acc 

_The control accumulator's fractional phase, in [0, 1)._ 
```C++
double resamp_get_ctrl_acc (
    const resamp_state_t * state
) 
```



This is the timing NCO's state, and observing it is the only way to see what a closed timing loop is actually doing to the sampling instant: the arm the last output read is `floor(mu * num_phases)`, so `mu` IS the fractional delay applied to the stream, in output periods. A steady `mu` means the loop has settled on a sampling phase; a `mu` that slews and wraps means a residual RATE error the loop has not absorbed, and one cycle of wrap is one output period of slip.


Reported after the last output this stage emitted, which for a decimating terminal stage (`rate <= 1`) is the phase the next output will read from. 


        

<hr>



### function resamp\_get\_num\_phases 

```C++
size_t resamp_get_num_phases (
    const resamp_state_t * state
) 
```




<hr>



### function resamp\_get\_num\_taps 

```C++
size_t resamp_get_num_taps (
    const resamp_state_t * state
) 
```




<hr>



### function resamp\_get\_rate 

```C++
double resamp_get_rate (
    const resamp_state_t * state
) 
```




<hr>



### function resamp\_get\_state 

_Serialize_ `state's` _mutable state into_`blob` _._
```C++
void resamp_get_state (
    const resamp_state_t * state,
    void * blob
) 
```




<hr>



### function resamp\_interp\_fill 

_Emit exactly_ `max_out` _interpolated outputs, pulling inputs on overflow._
```C++
size_t resamp_interp_fill (
    resamp_state_t * state,
    const float _Complex * in,
    float _Complex * out,
    size_t max_out
) 
```



The output-count-driven twin of the interpolation branch of [**resamp\_execute()**](resamp__core_8h.md#function-resamp_execute): it emits one output per phase tick and pushes the next input on each NCO overflow, but — unlike [**resamp\_execute()**](resamp__core_8h.md#function-resamp_execute), whose loop halts as soon as the input is exhausted even with output capacity left — it always writes `max_out` outputs. The caller must therefore supply at least resamp\_interp\_inputs\_needed(state, max\_out) inputs in `in`; supplying exactly that many (the common case) consumes them all. This is what lets a streaming producer (e.g. a pulse-shaping synth) feed symbols on demand and get a bit-exact match between a single call for `max_out` outputs and `max_out` single-output calls (the resampler is block-boundary invariant).




**Parameters:**


* `state` Must be non-NULL, upsampling. 
* `in` Inputs to push on overflow (&gt;= inputs\_needed available). 
* `out` Output buffer, capacity &gt;= `max_out`. 
* `max_out` Number of outputs to emit. 



**Returns:**

Inputs consumed (== resamp\_interp\_inputs\_needed(state, max\_out)). 





        

<hr>



### function resamp\_interp\_inputs\_needed 

_Input samples an interpolating fill of_ `max_out` _outputs consumes._
```C++
size_t resamp_interp_inputs_needed (
    const resamp_state_t * state,
    size_t max_out
) 
```



The exact number of delay-line pushes producing `max_out` outputs from the current phase: `((uint64_t)phase + max_out * phase_inc) >> 32`. For an integer interpolation factor (phase\_inc = 2^32 / rate divides evenly, i.e. a power-of-two `num_phases` bank at rate == num\_phases) this is exact, so a caller can generate precisely this many inputs — no over- or under-production. Meaningful only for an upsampling resampler (rate &gt;= 1).




**Parameters:**


* `state` Must be non-NULL, upsampling. 
* `max_out` Number of outputs the following [**resamp\_interp\_fill()**](resamp__core_8h.md#function-resamp_interp_fill) call will request. 



**Returns:**

Inputs that call will consume. 





        

<hr>



### function resamp\_reset 

```C++
void resamp_reset (
    resamp_state_t * state
) 
```



Zero phase accumulator, ctrl accumulator, and delay line. Rate and bank are preserved. 


        

<hr>



### function resamp\_set\_rate 

```C++
void resamp_set_rate (
    resamp_state_t * state,
    double rate
) 
```



Update rate and recompute phase\_inc. Accumulator phase and delay line are preserved. Switching between interp and decim modes requires a new create() + destroy() pair. 


        

<hr>



### function resamp\_set\_state 

_Restore mutable state from_ `blob` _(same rate)._
```C++
int resamp_set_state (
    resamp_state_t * state,
    const void * blob
) 
```





**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the blob's envelope rejects. 





        

<hr>



### function resamp\_state\_bytes 

_Bytes_ [_**resamp\_get\_state()**_](resamp__core_8h.md#function-resamp_get_state) _writes for_`state` _(envelope + payload)._
```C++
size_t resamp_state_bytes (
    const resamp_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define RESAMP\_STATE\_MAGIC 

```C++
#define RESAMP_STATE_MAGIC `DP_FOURCC ('R', 'S', 'M', 'P')`
```




<hr>



### define RESAMP\_STATE\_VERSION 

```C++
#define RESAMP_STATE_VERSION `2u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/resamp/resamp_core.h`

