

# File Resampler\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**Resampler**](dir_6dca75203c5d2d5de468e6acc97392e7.md) **>** [**Resampler\_core.h**](Resampler__core_8h.md)

[Go to the source code of this file](Resampler__core_8h_source.md)

_Continuously-variable polyphase resampler, CF32 IQ._ [More...](#detailed-description)

* `#include "resamp/resamp_core.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef [**resamp\_state\_t**](structresamp__state__t.md) | [**Resampler\_state\_t**](#typedef-resampler_state_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* | [**Resampler\_create**](#function-resampler_create) (double rate) <br>_Create a Resampler with the built-in 4096×19 Kaiser bank. The bank provides ~60 dB alias rejection with 0.4/0.6 pass/stop normalised cutoffs. Pass rate &gt;= 1.0 to interpolate (upsample); pass rate &lt; 1.0 to decimate (downsample). For a custom bank use_ [_**Resampler\_create\_custom()**_](Resampler__core_8h.md#function-resampler_create_custom) _instead._ |
|  [**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* | [**Resampler\_create\_custom**](#function-resampler_create_custom) (size\_t num\_phases, size\_t num\_taps, const float \* bank, double rate) <br>_Create a Resampler with a user-supplied polyphase bank._  |
|  void | [**Resampler\_destroy**](#function-resampler_destroy) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br> |
|  size\_t | [**Resampler\_execute**](#function-resampler_execute) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state, const float complex \* x, size\_t x\_len, float complex \* out) <br>_Resample a block of CF32 samples at the fixed base rate. Uses the dual-mode polyphase engine: output-driven for rate &gt;= 1 (interpolation), input-driven transposed-form for rate &lt; 1 (decimation). State carries over between calls, so contiguous blocks produce the same result as one large block._  |
|  size\_t | [**Resampler\_execute\_ctrl**](#function-resampler_execute_ctrl) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state, const float complex \* x, size\_t x\_len, const float complex \* ctrl, size\_t ctrl\_len, float complex \* out) <br>_Resample with per-sample additive rate deviations. Effective rate for sample i is base\_rate + real(_ `ctrl[i]` _). Uses a unified double-precision accumulator that handles both interpolation and decimation in a single code path — suitable for Doppler-shift simulation and fractional-sample timing correction. ctrl and x must have the same length._ |
|  size\_t | [**Resampler\_execute\_ctrl\_max\_out**](#function-resampler_execute_ctrl_max_out) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br> |
|  size\_t | [**Resampler\_execute\_max\_out**](#function-resampler_execute_max_out) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br> |
|  size\_t | [**Resampler\_get\_num\_phases**](#function-resampler_get_num_phases) (const [**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br>_Number of polyphase branches in the filter bank. Always a power of two. The built-in bank has 4096 phases giving sub-sample timing resolution of 1/4096 of an input sample period._  |
|  size\_t | [**Resampler\_get\_num\_taps**](#function-resampler_get_num_taps) (const [**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br>_Taps per polyphase branch. Total prototype filter length is num\_phases \* num\_taps - 1. The built-in bank uses 19 taps per branch._  |
|  double | [**Resampler\_get\_rate**](#function-resampler_get_rate) (const [**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br>_Get / set the output-to-input sample rate ratio. The setter recomputes the phase increment immediately; the delay line and phase accumulator are preserved so in-stream rate changes are glitch-free. Switching sign of (rate - 1) (i.e. crossing the boundary between interp and decim modes) requires a fresh create()._  |
|  void | [**Resampler\_get\_state**](#function-resampler_get_state) (const [**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state, void \* blob) <br>_Serialize the resampler's phase + delay-line state into_ `blob` _._ |
|  void | [**Resampler\_reset**](#function-resampler_reset) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br>_Zero the delay line and phase accumulator. Rate and polyphase bank are preserved so the resampler can be resumed at the same ratio. Zeroing state eliminates transient artefacts when starting a new signal burst._  |
|  void | [**Resampler\_set\_rate**](#function-resampler_set_rate) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state, double rate) <br> |
|  int | [**Resampler\_set\_state**](#function-resampler_set_state) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state, const void \* blob) <br>_Restore state from_ `blob` _; DP\_OK, or DP\_ERR\_INVALID if rejected._ |
|  size\_t | [**Resampler\_state\_bytes**](#function-resampler_state_bytes) (const [**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br>_Serialized-state byte size (forwarded to the resamp leaf)._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**RESAMPLER\_MAX\_OUT**](Resampler__core_8h.md#define-resampler_max_out)  `65536`<br> |

## Detailed Description


Thin adapter over resamp\_core ([**resamp\_state\_t**](structresamp__state__t.md)). Exposes a Resampler-prefixed API so the generated ext.c compiles without changes.


Lifecycle: 
```C++
Resampler_state_t *r = Resampler_create(0.5);
float complex out[4096];
size_t n = Resampler_execute(r, in, 1024, out);
Resampler_destroy(r);
```



Output buffer sizing: execute: allocate [**Resampler\_execute\_max\_out()**](Resampler__core_8h.md#function-resampler_execute_max_out) samples. execute\_ctrl: same. 


    
## Public Types Documentation




### typedef Resampler\_state\_t 

```C++
typedef resamp_state_t Resampler_state_t;
```




<hr>
## Public Functions Documentation




### function Resampler\_create 

_Create a Resampler with the built-in 4096×19 Kaiser bank. The bank provides ~60 dB alias rejection with 0.4/0.6 pass/stop normalised cutoffs. Pass rate &gt;= 1.0 to interpolate (upsample); pass rate &lt; 1.0 to decimate (downsample). For a custom bank use_ [_**Resampler\_create\_custom()**_](Resampler__core_8h.md#function-resampler_create_custom) _instead._
```C++
Resampler_state_t * Resampler_create (
    double rate
) 
```





**Parameters:**


* `rate` Output-to-input sample rate ratio (any positive float). Values &gt;= 1.0 interpolate; values &lt; 1.0 decimate. 



**Returns:**

Non-NULL on success, NULL on OOM.



```C++
>>> from doppler.resample import Resampler
>>> import numpy as np
>>> r = Resampler(rate=2.0)
>>> r.num_phases, r.num_taps
(4096, 19)
>>> r.rate
2.0
```
 


        

<hr>



### function Resampler\_create\_custom 

_Create a Resampler with a user-supplied polyphase bank._ 
```C++
Resampler_state_t * Resampler_create_custom (
    size_t num_phases,
    size_t num_taps,
    const float * bank,
    double rate
) 
```





**Parameters:**


* `num_phases` Number of polyphase branches (must be power of two). 
* `num_taps` Taps per branch. 
* `bank` Row-major float32 array, shape num\_phases × num\_taps. 
* `rate` Initial resample ratio. 



**Returns:**

Non-NULL on success, NULL on invalid args or OOM. 





        

<hr>



### function Resampler\_destroy 

```C++
void Resampler_destroy (
    Resampler_state_t * state
) 
```



Free all resources. NULL is a no-op. 


        

<hr>



### function Resampler\_execute 

_Resample a block of CF32 samples at the fixed base rate. Uses the dual-mode polyphase engine: output-driven for rate &gt;= 1 (interpolation), input-driven transposed-form for rate &lt; 1 (decimation). State carries over between calls, so contiguous blocks produce the same result as one large block._ 
```C++
size_t Resampler_execute (
    Resampler_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out
) 
```





**Parameters:**


* `state` Pointer to a valid [**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t). 
* `x` CF32 input samples. 
* `x_len` Number of input samples. 
* `out` Output buffer; must hold at least RESAMPLER\_MAX\_OUT samples. 



**Returns:**

CF32 output array; length is approximately x\_len \* rate.



```C++
>>> from doppler.resample import Resampler
>>> import numpy as np
>>> r = Resampler(rate=2.0)
>>> y = r.execute(np.zeros(128, dtype=np.complex64))
>>> y.shape, y.dtype
((256,), dtype('complex64'))
```
 


        

<hr>



### function Resampler\_execute\_ctrl 

_Resample with per-sample additive rate deviations. Effective rate for sample i is base\_rate + real(_ `ctrl[i]` _). Uses a unified double-precision accumulator that handles both interpolation and decimation in a single code path — suitable for Doppler-shift simulation and fractional-sample timing correction. ctrl and x must have the same length._
```C++
size_t Resampler_execute_ctrl (
    Resampler_state_t * state,
    const float complex * x,
    size_t x_len,
    const float complex * ctrl,
    size_t ctrl_len,
    float complex * out
) 
```





**Parameters:**


* `state` Pointer to a valid [**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t). 
* `x` CF32 input samples. 
* `x_len` Number of input samples. 
* `ctrl` CF32 array, same length as x; only the real part is used as a per-sample rate addend. 
* `ctrl_len` Number of control samples; must equal x\_len. 
* `out` Output buffer; must hold at least RESAMPLER\_MAX\_OUT samples. 



**Returns:**

CF32 output array; length depends on accumulated rate deviations.



```C++
>>> from doppler.resample import Resampler
>>> import numpy as np
>>> r = Resampler(rate=1.0)
>>> x = np.zeros(64, dtype=np.complex64)
>>> ctrl = np.zeros(64, dtype=np.complex64)
>>> y = r.execute_ctrl(x, ctrl)
>>> y.shape, y.dtype
((64,), dtype('complex64'))
```
 


        

<hr>



### function Resampler\_execute\_ctrl\_max\_out 

```C++
size_t Resampler_execute_ctrl_max_out (
    Resampler_state_t * state
) 
```



Always returns RESAMPLER\_MAX\_OUT. 


        

<hr>



### function Resampler\_execute\_max\_out 

```C++
size_t Resampler_execute_max_out (
    Resampler_state_t * state
) 
```



Always returns RESAMPLER\_MAX\_OUT. 


        

<hr>



### function Resampler\_get\_num\_phases 

_Number of polyphase branches in the filter bank. Always a power of two. The built-in bank has 4096 phases giving sub-sample timing resolution of 1/4096 of an input sample period._ 
```C++
size_t Resampler_get_num_phases (
    const Resampler_state_t * state
) 
```




```C++
>>> from doppler.resample import Resampler
>>> Resampler(rate=1.0).num_phases
4096
```
 


        

<hr>



### function Resampler\_get\_num\_taps 

_Taps per polyphase branch. Total prototype filter length is num\_phases \* num\_taps - 1. The built-in bank uses 19 taps per branch._ 
```C++
size_t Resampler_get_num_taps (
    const Resampler_state_t * state
) 
```




```C++
>>> from doppler.resample import Resampler
>>> Resampler(rate=1.0).num_taps
19
```
 


        

<hr>



### function Resampler\_get\_rate 

_Get / set the output-to-input sample rate ratio. The setter recomputes the phase increment immediately; the delay line and phase accumulator are preserved so in-stream rate changes are glitch-free. Switching sign of (rate - 1) (i.e. crossing the boundary between interp and decim modes) requires a fresh create()._ 
```C++
double Resampler_get_rate (
    const Resampler_state_t * state
) 
```




```C++
>>> from doppler.resample import Resampler
>>> r = Resampler(rate=0.5)
>>> r.rate
0.5
>>> r.rate = 1.5
>>> r.rate
1.5
```
 


        

<hr>



### function Resampler\_get\_state 

_Serialize the resampler's phase + delay-line state into_ `blob` _._
```C++
void Resampler_get_state (
    const Resampler_state_t * state,
    void * blob
) 
```




<hr>



### function Resampler\_reset 

_Zero the delay line and phase accumulator. Rate and polyphase bank are preserved so the resampler can be resumed at the same ratio. Zeroing state eliminates transient artefacts when starting a new signal burst._ 
```C++
void Resampler_reset (
    Resampler_state_t * state
) 
```




```C++
>>> from doppler.resample import Resampler
>>> import numpy as np
>>> r = Resampler(rate=2.0)
>>> _ = r.execute(np.ones(64, dtype=np.complex64))
>>> r.reset()
>>> r.rate
2.0
```
 


        

<hr>



### function Resampler\_set\_rate 

```C++
void Resampler_set_rate (
    Resampler_state_t * state,
    double rate
) 
```




<hr>



### function Resampler\_set\_state 

_Restore state from_ `blob` _; DP\_OK, or DP\_ERR\_INVALID if rejected._
```C++
int Resampler_set_state (
    Resampler_state_t * state,
    const void * blob
) 
```




<hr>



### function Resampler\_state\_bytes 

_Serialized-state byte size (forwarded to the resamp leaf)._ 
```C++
size_t Resampler_state_bytes (
    const Resampler_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define RESAMPLER\_MAX\_OUT 

```C++
#define RESAMPLER_MAX_OUT `65536`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/Resampler/Resampler_core.h`

