

# File doppler\_channel\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**doppler\_channel**](dir_8380ecb58e2e244790f54835382515ec.md) **>** [**doppler\_channel\_core.h**](doppler__channel__core_8h.md)

[Go to the source code of this file](doppler__channel__core_8h_source.md)

_Clock Doppler as a propagation impairment: dilate the time base and shift the carrier, coherently, from one physical parameter._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/resamp/resamp_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) <br>_DopplerChannel state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* | [**dp\_doppler\_channel\_create**](#function-dp_doppler_channel_create) (double fs, double carrier\_hz, double doppler\_ppm, double doppler\_rate\_ppm\_s) <br>_Create a doppler\_channel instance._  |
|  void | [**dp\_doppler\_channel\_destroy**](#function-dp_doppler_channel_destroy) ([**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* state) <br>_Destroy a doppler\_channel instance and release all memory._  |
|  size\_t | [**dp\_doppler\_channel\_execute**](#function-dp_doppler_channel_execute) ([**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, float \_Complex \* out, size\_t max\_out) <br>_Apply clock Doppler to a block of complex baseband._  |
|  size\_t | [**dp\_doppler\_channel\_execute\_max\_out**](#function-dp_doppler_channel_execute_max_out) ([**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* state) <br>_Upper bound on the output of one execute() call._  |
|  size\_t | [**dp\_doppler\_channel\_execute\_profile**](#function-dp_doppler_channel_execute_profile) ([**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, const double \* ppm, size\_t ppm\_len, float \_Complex \* out, size\_t max\_out) <br>_Apply a per-sample Doppler PROFILE to a block of complex baseband._  |
|  size\_t | [**dp\_doppler\_channel\_execute\_profile\_max\_out**](#function-dp_doppler_channel_execute_profile_max_out) ([**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* state, size\_t n) <br>_The BINDING's output bound for execute\_profile() (jm pass\_capacity)._  |
|  double | [**dp\_doppler\_channel\_get\_delay\_samples**](#function-dp_doppler_channel_get_delay_samples) (const [**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* state) <br>_The resampler's group delay, in samples (10.5 for the built-in bank)._  |
|  double | [**dp\_doppler\_channel\_get\_elapsed\_s**](#function-dp_doppler_channel_get_elapsed_s) (const [**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* state) <br>_Receive time in seconds produced so far (_ `n_out/fs` _)._ |
|  double | [**dp\_doppler\_channel\_get\_offset\_hz**](#function-dp_doppler_channel_get_offset_hz) (const [**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* state) <br>_Instantaneous carrier offset_ `fc*d(t)` _in Hz at_`elapsed_s` _._ |
|  void | [**dp\_doppler\_channel\_get\_state**](#function-dp_doppler_channel_get_state) (const [**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* state, void \* blob) <br>_Serialize the running state (both clocks + the resampler's)._  |
|  void | [**dp\_doppler\_channel\_reset**](#function-dp_doppler_channel_reset) ([**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* state) <br>_Reset DopplerChannel to its post-create state._  |
|  int | [**dp\_doppler\_channel\_set\_state**](#function-dp_doppler_channel_set_state) ([**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* state, const void \* blob) <br>_Restore a blob written by_ [_**dp\_doppler\_channel\_get\_state()**_](doppler__channel__core_8h.md#function-dp_doppler_channel_get_state) _._ |
|  size\_t | [**dp\_doppler\_channel\_state\_bytes**](#function-dp_doppler_channel_state_bytes) (const [**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* state) <br>_Bytes_ [_**dp\_doppler\_channel\_get\_state()**_](doppler__channel__core_8h.md#function-dp_doppler_channel_get_state) _writes (envelope + payload)._ |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  double | [**doppler\_channel\_excess**](#function-doppler_channel_excess) (const [**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* s, double t) <br>_Excess delay (seconds) accumulated by receive time_ `t:` __`tau(t)-t` _._ |
|  double | [**doppler\_channel\_phase**](#function-doppler_channel_phase) (const [**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* s, double t) <br>_Carrier phase in CYCLES at receive time_ `t:` __`fc * excess(t)` _._ |
|  double | [**doppler\_channel\_scale**](#function-doppler_channel_scale) (const [**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* s, double t) <br>_Instantaneous time-base scale_ `1 + d(t)` _at receive time_`t` _._ |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DOPPLER\_CHANNEL\_MAX\_BLOCK**](doppler__channel__core_8h.md#define-doppler_channel_max_block)  `65536u`<br>_Largest input block one_ `dp_doppler_channel_execute()` _call accepts._ |
| define  | [**DOPPLER\_CHANNEL\_STATE\_MAGIC**](doppler__channel__core_8h.md#define-doppler_channel_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc)('D', 'P', 'C', 'H')`<br>_State-blob magic ('DPCH') and layout version._  |
| define  | [**DOPPLER\_CHANNEL\_STATE\_VERSION**](doppler__channel__core_8h.md#define-doppler_channel_state_version)  `2u`<br> |

## Detailed Description


A real Doppler shift is not a frequency offset. Relative motion rescales the whole received time base, so _every_ clock in the signal changes together — carrier, chip rate, symbol rate, frame rate. Modelling only the carrier is the classic unphysical shortcut, and it silently hides the code-rate error that a receiver's delay-lock loop exists to track.


This object takes any complex baseband stream and applies both halves of the effect from a single parameter, so they cannot disagree:



* **Time-base dilation** — the input is resampled at output/input ratio `1/(1+d)`, which makes a stream carrying `Rc` chips/s and `Rs` symbols/s come out at `Rc(1+d)` and `Rs(1+d)`. One resampling on the whole stream, rather than a per-clock adjustment, is what keeps the clocks consistent.
* **Carrier offset** — multiplication by `exp(j2*pi*fc*excess(t))`, whose instantaneous frequency is `fc*d(t)`.




Doppler is specified in **ppm of the nominal time base**, which makes it carrier-frequency agnostic: 20 ppm is +50 kHz at 2.5 GHz and +61.4 chip/s at 3.069 Mcps at the same time, and no caller converts between the two by hand. `doppler_rate_ppm_s` ramps it linearly for a pass-like geometry (0.2 ppm/s is 500 Hz/s at 2.5 GHz).


\*\*`carrier_hz` is load-bearing, not metadata.\*\* It is the only thing that converts a dimensionless ppm into a carrier offset in Hz. Leave it 0 and the clocks still dilate correctly but the carrier never moves — a physically inconsistent capture whose code rate runs fast while its carrier sits exactly on frequency. That combination is occasionally useful for isolating a code loop under test, so it is permitted rather than rejected, but it is not what a real channel does.


The dilation is `dp_resamp_execute_ctrl` (see `resamp_core.h`), whose per-sample rate deviation tracks the ramp exactly instead of approximating it with a piecewise-constant ratio re-set once per block. No resampling math is implemented here.


**The output is delayed by the resampler's group delay**  [**dp\_doppler\_channel\_get\_delay\_samples()**](doppler__channel__core_8h.md#function-dp_doppler_channel_get_delay_samples), 10.5 samples for the built-in bank  on top of the dilation. Output sample `k`, at receive time `t = k/fs`, carries the input at time `t + excess(t) - delay/fs`. A receiver started at the INPUT's phase is that far from the peak: at two samples per chip, five chips, outside a DLL's pull-in and onto a Gold code's sidelobe (doppler-dsp/doppler#1189). Subtract it, or start the loop there.


Lifecycle: create -&gt; `[execute / reset]*` -&gt; destroy


Example — a 2.5 GHz carrier seen at +20 ppm, ramping at 0.2 ppm/s: 
```C++
dp_doppler_channel_state_t *ch =
    dp_doppler_channel_create (6.138e6, 2.5e9, 20.0, 0.2);
size_t         cap = dp_doppler_channel_execute_max_out (ch);
float _Complex *out = malloc (cap * sizeof *out);
size_t         n   = dp_doppler_channel_execute (ch, in, 65536, out, cap);
// n ~= 65536/(1+20e-6); dp_doppler_channel_get_offset_hz (ch) ~= 50000.0
free (out);
dp_doppler_channel_destroy (ch);
```
 


    
## Public Functions Documentation




### function dp\_doppler\_channel\_create 

_Create a doppler\_channel instance._ 
```C++
dp_doppler_channel_state_t * dp_doppler_channel_create (
    double fs,
    double carrier_hz,
    double doppler_ppm,
    double doppler_rate_ppm_s
) 
```





**Parameters:**


* `fs` Receive sample rate in Hz (&gt; 0). 
* `carrier_hz` RF carrier in Hz (&gt;= 0). Load-bearing: converts ppm into a carrier offset. 0 dilates the clocks but never moves the carrier (default: 0.0). 
* `doppler_ppm` Doppler d0 in ppm of the nominal time base; positive is a closing range — clocks run fast, carrier shifts up (default: 0.0). 
* `doppler_rate_ppm_s` Linear ramp of d in ppm per second (default: 0.0). 



**Returns:**

Heap-allocated state, or NULL on allocation failure or `fs <= 0`. 




**Note:**

Caller must call [**dp\_doppler\_channel\_destroy()**](doppler__channel__core_8h.md#function-dp_doppler_channel_destroy) when done. 





        

<hr>



### function dp\_doppler\_channel\_destroy 

_Destroy a doppler\_channel instance and release all memory._ 
```C++
void dp_doppler_channel_destroy (
    dp_doppler_channel_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_doppler\_channel\_execute 

_Apply clock Doppler to a block of complex baseband._ 
```C++
size_t dp_doppler_channel_execute (
    dp_doppler_channel_state_t * state,
    const float _Complex * x,
    size_t x_len,
    float _Complex * out,
    size_t max_out
) 
```



Resamples `x` by `1/(1+d(t))` and multiplies the result by the coherent carrier `exp(j*2*pi*fc*excess(t))`. State persists across calls, so feeding a stream in blocks gives the same samples as one large call (subject to `DOPPLER_CHANNEL_MAX_BLOCK`).


Output length is approximately `x_len/(1+d)` and varies by a sample from call to call as the fractional resampling accumulator crosses — that variation is the dilation itself, not a defect.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input block. 
* `x_len` Input length in samples. 
* `out` Output buffer. 
* `max_out` Capacity of `out`; production stops there. 



**Returns:**

Samples written to `out`. 
```C++
>>> import numpy as np
>>> from doppler.impairment import DopplerChannel
>>> ch = DopplerChannel(fs=1e6, carrier_hz=2.5e9, doppler_ppm=20.0)
>>> y = ch.execute(np.ones(1000, dtype=np.complex64))
>>> y.shape                   # 20 ppm is 0.02 samples over this block
(1000,)
>>> round(ch.offset_hz, 1)    # fc * d = 2.5e9 * 20e-6, in Hz
50000.0
```
 





        

<hr>



### function dp\_doppler\_channel\_execute\_max\_out 

_Upper bound on the output of one execute() call._ 
```C++
size_t dp_doppler_channel_execute_max_out (
    dp_doppler_channel_state_t * state
) 
```



Assumes an input of at most `DOPPLER_CHANNEL_MAX_BLOCK` samples — see that macro for why the bound cannot depend on the actual input length. 


        

<hr>



### function dp\_doppler\_channel\_execute\_profile 

_Apply a per-sample Doppler PROFILE to a block of complex baseband._ 
```C++
size_t dp_doppler_channel_execute_profile (
    dp_doppler_channel_state_t * state,
    const float _Complex * x,
    size_t x_len,
    const double * ppm,
    size_t ppm_len,
    float _Complex * out,
    size_t max_out
) 
```



The array form of [**dp\_doppler\_channel\_execute()**](doppler__channel__core_8h.md#function-dp_doppler_channel_execute): instead of the create-time `(doppler_ppm, doppler_rate_ppm_s)` closed form  a straight line, which a real pass is not  the Doppler is supplied as one value per INPUT sample. The length contract is the one `dp_resamp_execute_ctrl()` underneath already has (`ctrl` parallel to `in`), so a profile is handed to the resampler rather than reduced to fit it.


**The profile is ABSOLUTE.** `ppm[i]` is the total instantaneous Doppler at input sample `i`; the create-time scalars do not add to it. They cancel exactly rather than by convention: the resampler's rate is `base + ctrl`, and this fills `ctrl = ratio(ppm[i]) - base` with the same `base` it was built with. Creating with zeros and supplying a profile is the ordinary use.


**The carrier is read off the resampler, not integrated beside it.** The excess delay at output `k` is `(p_k - k + 1)/fs`, where `p_k` is the input position the resampler's own accumulator reports for that output ([**dp\_resamp\_execute\_ctrl\_pos()**](resamp__core_8h.md#function-dp_resamp_execute_ctrl_pos)). So the carrier `exp(j*2*pi*fc*excess)` is derived from the dilation the resampler actually performed:



* It cannot disagree with the dilation, and there is no second accumulator to drift from the first.
* It cannot depend on how the stream was chunked: nothing here maps a profile index to an output index, which is what made the first attempt at this (reverted) chunk-dependent. Output `k` has the carrier of the position it was interpolated at, whatever call it fell in.
* It has no cancellation: the integer part of `p_k - k` is exact, and only a fraction below one input sample is floating point.




Mixing the two calls on one stream is permitted and coherent  both advance the same clocks  but a stream a profile has driven reports [**dp\_doppler\_channel\_get\_offset\_hz()**](doppler__channel__core_8h.md#function-dp_doppler_channel_get_offset_hz) from the profile, since the closed form no longer describes it. The profile is in the serialized state (layout version 2) only as that last value; the carrier needs none.



```C++
>>> import numpy as np
>>> from doppler.impairment import DopplerChannel
>>> ch = DopplerChannel(fs=1e6, carrier_hz=2.5e9)
>>> n = 1000
>>> ppm = np.where(np.arange(n) < n // 2, 20.0, -20.0)
>>> y = ch.execute_profile(np.ones(n, dtype=np.complex64), ppm)
>>> y.shape          # closing then opening: the record STRETCHES overall
(1001,)
>>> round(ch.offset_hz, 1)   # fc * d at the last profile sample
-50000.0
```



A sign change mid-record is the point: no `(doppler_ppm, doppler_rate_ppm_s)` pair produces it.




**Parameters:**


* `state` channel state. 
* `x` Input CF32 samples, `x_len` of them. 
* `x_len` Input sample count. 
* `ppm` Doppler in ppm, parallel to `x`. 
* `ppm_len` Profile length; must EQUAL `x_len`. 
* `out` Output buffer. 
* `max_out` Capacity of `out` in samples. 



**Returns:**

Samples written. 0 if any pointer is NULL, if `ppm_len` differs from `x_len`, or if any profile sample is non-finite or below -5e5 ppm (a scale under 1/2: past the 2x expansion the output is sized for, and at -1e6 ppm time stops, which create() already refuses for the scalar). All checked over the whole profile BEFORE any output is produced, so a bad call writes nothing rather than a valid prefix. 





        

<hr>



### function dp\_doppler\_channel\_execute\_profile\_max\_out 

_The BINDING's output bound for execute\_profile() (jm pass\_capacity)._ 
```C++
size_t dp_doppler_channel_execute_profile_max_out (
    dp_doppler_channel_state_t * state,
    size_t n
) 
```



The generated binding knows the INPUT LENGTH `n`  it passes it  but has not looked at the profile array, so it knows how many samples go in and not how far they dilate. With the profile unseen there is no exact answer: the bound scales `n` by a floor on the scale and the kernel clamps to the caller's real capacity, as `Resampler_execute_ctrl_max_out` does for its equally arbitrary `ctrl`. The floor allows a 2x expansion, i.e. a Doppler of -500000 ppm  half the speed of light closing, six orders of magnitude past any geometry this object models.




**Parameters:**


* `state` channel state (unused; the signature is jm's). 
* `n` Input sample count the binding is about to pass. 



**Returns:**

The capacity the binding allocates. 





        

<hr>



### function dp\_doppler\_channel\_get\_delay\_samples 

_The resampler's group delay, in samples (10.5 for the built-in bank)._ 
```C++
double dp_doppler_channel_get_delay_samples (
    const dp_doppler_channel_state_t * state
) 
```



Constant, and in addition to the dilation: output `k` at receive time `t = k/fs` carries the input at `t + excess(t) - delay/fs` (doppler\_channel\_excess()). Input and receive samples differ by the ppm of Doppler, so the unit is either to that precision. [**dp\_resamp\_get\_delay()**](resamp__core_8h.md#function-dp_resamp_get_delay) for the derivation.



```C++
>>> from doppler.impairment import DopplerChannel
>>> DopplerChannel(fs=1e6).delay_samples
10.5
```
 


        

<hr>



### function dp\_doppler\_channel\_get\_elapsed\_s 

_Receive time in seconds produced so far (_ `n_out/fs` _)._
```C++
double dp_doppler_channel_get_elapsed_s (
    const dp_doppler_channel_state_t * state
) 
```




<hr>



### function dp\_doppler\_channel\_get\_offset\_hz 

_Instantaneous carrier offset_ `fc*d(t)` _in Hz at_`elapsed_s` _._
```C++
double dp_doppler_channel_get_offset_hz (
    const dp_doppler_channel_state_t * state
) 
```




<hr>



### function dp\_doppler\_channel\_get\_state 

_Serialize the running state (both clocks + the resampler's)._ 
```C++
void dp_doppler_channel_get_state (
    const dp_doppler_channel_state_t * state,
    void * blob
) 
```




<hr>



### function dp\_doppler\_channel\_reset 

_Reset DopplerChannel to its post-create state._ 
```C++
void dp_doppler_channel_reset (
    dp_doppler_channel_state_t * state
) 
```



Zeroes both sample clocks (so `elapsed_s` and the carrier phase restart at zero) and clears the resampler's delay line and fractional accumulator. The configured `fs`/`carrier_hz`/`doppler_ppm`/`doppler_rate_ppm_s` are kept.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.impairment import DopplerChannel
>>> ch = DopplerChannel(fs=1e6, carrier_hz=2.5e9, doppler_ppm=20.0)
>>> _ = ch.execute(np.ones(1000, dtype=np.complex64))
>>> round(ch.elapsed_s, 6)    # receive time consumed: 1000 / 1e6
0.001
>>> ch.reset()                # both sample clocks back to zero
>>> ch.elapsed_s
0.0
```
 




        

<hr>



### function dp\_doppler\_channel\_set\_state 

_Restore a blob written by_ [_**dp\_doppler\_channel\_get\_state()**_](doppler__channel__core_8h.md#function-dp_doppler_channel_get_state) _._
```C++
int dp_doppler_channel_set_state (
    dp_doppler_channel_state_t * state,
    const void * blob
) 
```





**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the envelope or a child blob is rejected. 





        

<hr>



### function dp\_doppler\_channel\_state\_bytes 

_Bytes_ [_**dp\_doppler\_channel\_get\_state()**_](doppler__channel__core_8h.md#function-dp_doppler_channel_get_state) _writes (envelope + payload)._
```C++
size_t dp_doppler_channel_state_bytes (
    const dp_doppler_channel_state_t * state
) 
```




<hr>
## Public Static Functions Documentation




### function doppler\_channel\_excess 

_Excess delay (seconds) accumulated by receive time_ `t:` __`tau(t)-t` _._
```C++
static inline double doppler_channel_excess (
    const dp_doppler_channel_state_t * s,
    double t
) 
```



The one place the dilation integral is evaluated; the scale and the carrier phase are both derived from it, so the clocks cannot drift apart — there is only ever one number to drift.


`tau(t) = integral_0^t (1 + d(u)) du`, `d(u) = (d0 + d_dot*u) * 1e-6` `tau(t) - t = d0*t + 0.5*d_dot*t^2`


Note this is the _integral_, not `t*d(t)`: the latter double-counts the ramp and puts the instantaneous offset at `fc*(d0 + 2*d_dot*t)`, exactly twice the intended Doppler rate.




**Parameters:**


* `s` channel state. 
* `t` receive time in seconds (&gt;= 0). 



**Returns:**

Excess delay in seconds (negative for an opening-range geometry). 





        

<hr>



### function doppler\_channel\_phase 

_Carrier phase in CYCLES at receive time_ `t:` __`fc * excess(t)` _._
```C++
static inline double doppler_channel_phase (
    const dp_doppler_channel_state_t * s,
    double t
) 
```



Its derivative is `fc * d(t)`, the instantaneous offset — so the carrier is driven by the same dilation the clocks are, not by a separately-specified frequency that could be set inconsistently with them.


Evaluated closed-form from `t` rather than accumulated per sample: an incremental accumulator would drift, and the closed form keeps a long capture phase-exact (a 1000 s run at 20 ppm on a 2.5 GHz carrier is ~5e7 cycles, ~1e-8 cycles of representation error in double).




**Parameters:**


* `s` channel state. 
* `t` receive time in seconds (&gt;= 0). 



**Returns:**

Phase in cycles; multiply by 2\*pi for radians. 





        

<hr>



### function doppler\_channel\_scale 

_Instantaneous time-base scale_ `1 + d(t)` _at receive time_`t` _._
```C++
static inline double doppler_channel_scale (
    const dp_doppler_channel_state_t * s,
    double t
) 
```



The reciprocal of the resampler's output/input ratio: a stream resampled at `1/(1+d)` comes out with every one of its clocks running `(1+d)` times faster.




**Parameters:**


* `s` channel state. 
* `t` receive time in seconds (&gt;= 0). 



**Returns:**

`1 + d(t)`, a number very close to 1. 





        

<hr>
## Macro Definition Documentation





### define DOPPLER\_CHANNEL\_MAX\_BLOCK 

_Largest input block one_ `dp_doppler_channel_execute()` _call accepts._
```C++
#define DOPPLER_CHANNEL_MAX_BLOCK `65536u`
```



`dp_doppler_channel_execute_max_out()` reports a bound for the output buffer, and the generated Python binding sizes its buffer from that alone — it never sees the input length. So the bound has to assume a worst-case input, and this is that assumption (the same convention, and the same value, as `dp_RateConverter_execute_max_out`). Longer inputs are processed up to the caller's `max_out` and the remainder is _not_ consumed; feed large streams in blocks of at most this many samples. 


        

<hr>



### define DOPPLER\_CHANNEL\_STATE\_MAGIC 

_State-blob magic ('DPCH') and layout version._ 
```C++
#define DOPPLER_CHANNEL_STATE_MAGIC `DP_FOURCC ('D', 'P', 'C', 'H')`
```




<hr>



### define DOPPLER\_CHANNEL\_STATE\_VERSION 

```C++
#define DOPPLER_CHANNEL_STATE_VERSION `2u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/doppler_channel/doppler_channel_core.h`

