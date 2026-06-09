

# File ddc\_core.h



[**FileList**](files.md) **>** [**ddc**](dir_b33dc116452ac5c7d7799725e78b6bdc.md) **>** [**ddc\_core.h**](ddc__core_8h.md)

[Go to the source code of this file](ddc__core_8h_source.md)

_Digital Down-Converter — composes LO + RateConverter cascade._ [More...](#detailed-description)

* `#include <complex.h>`
* `#include <stddef.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct ddc\_state | [**ddc\_state\_t**](#typedef-ddc_state_t)  <br> |
| typedef struct ddcr\_state | [**ddcr\_state\_t**](#typedef-ddcr_state_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* | [**ddc\_create**](#function-ddc_create) (double norm\_freq, double rate) <br>_Create a complex-input Digital Down-Converter. Allocates internal state for the LO and RateConverter cascade. The RateConverter selects the cheapest multi-stage decimation chain (CIC + optional halfband + polyphase resampler) for the given rate._  |
|  void | [**ddc\_destroy**](#function-ddc_destroy) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state) <br>_Free all resources held by a DDC instance. Releases the RateConverter and LO substructures, then the struct itself. Passing NULL is a no-op._  |
|  size\_t | [**ddc\_execute**](#function-ddc_execute) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br>_Mix and resample a block of CF32 samples. Multiplies each input sample by the current LO phasor (advancing the NCO phase per sample), then feeds the mixed block into the RateConverter. The resampler maintains history across calls, so arbitrary block sizes produce contiguous output with no edge artefacts. Output length ≈ x\_len \* rate (varies by ±1 due to polyphase indexing)._  |
|  size\_t | [**ddc\_execute\_max\_out**](#function-ddc_execute_max_out) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state) <br>_Return the maximum output samples for one execute call._  |
|  double | [**ddc\_get\_norm\_freq**](#function-ddc_get_norm_freq) (const [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state) <br>_Return the current LO normalised frequency (cycles/sample)._  |
|  double | [**ddc\_get\_rate**](#function-ddc_get_rate) (const [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state) <br>_Return the configured output/input rate ratio (read-only). The rate is fixed at create time; change it by destroying and recreating the DDC with the new value._  |
|  void | [**ddc\_reset**](#function-ddc_reset) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state) <br>_Zero LO phase and resampler history. After reset, the next execute call produces the same output as the first execute after create — useful for reproducible block-by-block processing or looped test fixtures._  |
|  void | [**ddc\_set\_norm\_freq**](#function-ddc_set_norm_freq) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* state, double val) <br>_Retune the LO without resetting phase or resampler history. Updates the NCO phase increment atomically so the carrier shift changes seamlessly across block boundaries. The resampler history and LO phase accumulator are left intact, avoiding the transient that a full reset would cause._  |
|  [**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* | [**ddcr\_create**](#function-ddcr_create) (double norm\_freq, double rate) <br>_Create a real-input Digital Down-Converter (Architecture D2). The signal chain is: halfband R2C (2:1, bakes in +fs/4 shift) → fine LO mix at the intermediate rate (fs\_in/2) → RateConverter → CF32 output. The halfband stage uses ±1/0 coefficients (no multiplications), making DDCR roughly 2× cheaper than DDC at the same total decimation ratio._  |
|  void | [**ddcr\_destroy**](#function-ddcr_destroy) ([**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* s) <br>_Free all resources held by a DDCR instance. Releases the halfband, RateConverter, and LO substructures, then the struct itself. Passing NULL is a no-op._  |
|  size\_t | [**ddcr\_execute**](#function-ddcr_execute) ([**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* s, const float \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Process a block of real float32 samples through the full DDCR signal chain: halfband R2C → LO mix → RateConverter → CF32. The halfband decimates by 2 and applies a built-in +fs/4 frequency shift; the fine NCO then completes the tuning. State is maintained across calls for contiguous streaming. Output length ≈ n\_in \* rate (±1 from polyphase indexing). A real tone at input normalised frequency f\_c has amplitude 0.5 in the baseband output (one-sided spectrum), consistent with analytic signal theory._  |
|  double | [**ddcr\_get\_norm\_freq**](#function-ddcr_get_norm_freq) (const [**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* s) <br>_Return the current fine NCO normalised frequency at the intermediate rate (fs\_in/2, cycles/sample)._  |
|  double | [**ddcr\_get\_rate**](#function-ddcr_get_rate) (const [**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* s) <br>_Return the total configured rate (fs\_out / fs\_in, read-only). This is the end-to-end ratio from ADC input to CF32 output. Change it by destroying and recreating the DDCR._  |
|  void | [**ddcr\_reset**](#function-ddcr_reset) ([**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* s) <br>_Zero halfband filter history, LO phase, and resampler history. After reset, the next execute call reproduces the output of the first call after create, enabling repeatable block-by-block tests._  |
|  void | [**ddcr\_set\_norm\_freq**](#function-ddcr_set_norm_freq) ([**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* s, double norm\_freq) <br>_Retune the fine NCO without resetting halfband or resampler history. Updates the LO phase increment only; state is preserved for seamless tuning across block boundaries._  |




























## Detailed Description


## Public Types Documentation




### typedef ddc\_state\_t 

```C++
typedef struct ddc_state ddc_state_t;
```




<hr>



### typedef ddcr\_state\_t 

```C++
typedef struct ddcr_state ddcr_state_t;
```




<hr>
## Public Functions Documentation




### function ddc\_create 

_Create a complex-input Digital Down-Converter. Allocates internal state for the LO and RateConverter cascade. The RateConverter selects the cheapest multi-stage decimation chain (CIC + optional halfband + polyphase resampler) for the given rate._ 
```C++
ddc_state_t * ddc_create (
    double norm_freq,
    double rate
) 
```





**Parameters:**


* `norm_freq` LO frequency in cycles/sample at the input rate. Set to -f\_carrier to shift a carrier at f\_carrier to DC. Any real value is accepted. 
* `rate` Output rate / input rate. Must be &gt; 0. Values ≥ 1 are up-sampling; typical use is decimation (0 &lt; rate &lt; 1). 



**Returns:**

Non-NULL on success, NULL on OOM or invalid args.



```C++
>>> from doppler.ddc import DDC
>>> ddc = DDC(norm_freq=-0.1, rate=0.25)
>>> ddc.norm_freq
-0.1
>>> ddc.rate
0.25
```
 


        

<hr>



### function ddc\_destroy 

_Free all resources held by a DDC instance. Releases the RateConverter and LO substructures, then the struct itself. Passing NULL is a no-op._ 
```C++
void ddc_destroy (
    ddc_state_t * state
) 
```




```C++
>>> from doppler.ddc import DDC
>>> ddc = DDC(norm_freq=0.0, rate=0.25)
>>> ddc.destroy()   # releases C memory immediately
```
 


        

<hr>



### function ddc\_execute 

_Mix and resample a block of CF32 samples. Multiplies each input sample by the current LO phasor (advancing the NCO phase per sample), then feeds the mixed block into the RateConverter. The resampler maintains history across calls, so arbitrary block sizes produce contiguous output with no edge artefacts. Output length ≈ x\_len \* rate (varies by ±1 due to polyphase indexing)._ 
```C++
size_t ddc_execute (
    ddc_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` CF32 input block; accepted as float32 (auto-cast). 
* `x_len` Number of input samples (C-only, hidden from Python). 
* `out` CF32 output buffer (C-only, hidden from Python). 
* `max_out` Output buffer capacity (C-only, hidden from Python). 



**Returns:**

Number of output samples written (C-only).



```C++
>>> from doppler.ddc import DDC
>>> import numpy as np
>>> ddc = DDC(norm_freq=-0.1, rate=0.25)
>>> t = np.arange(4096)
>>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)
>>> y = ddc.execute(x)
>>> y.shape
(1024,)
>>> y.dtype
dtype('complex64')
>>> round(float(abs(y[500])), 2)   # shifted to DC; amplitude ≈ 1
1.0
```
 


        

<hr>



### function ddc\_execute\_max\_out 

_Return the maximum output samples for one execute call._ 
```C++
size_t ddc_execute_max_out (
    ddc_state_t * state
) 
```



Returns 0, signalling the Python extension to fall back to allocating n\_in samples — always sufficient for a decimating DDC. 


        

<hr>



### function ddc\_get\_norm\_freq 

_Return the current LO normalised frequency (cycles/sample)._ 
```C++
double ddc_get_norm_freq (
    const ddc_state_t * state
) 
```




```C++
>>> from doppler.ddc import DDC
>>> ddc = DDC(norm_freq=-0.1, rate=0.25)
>>> ddc.norm_freq
-0.1
```
 


        

<hr>



### function ddc\_get\_rate 

_Return the configured output/input rate ratio (read-only). The rate is fixed at create time; change it by destroying and recreating the DDC with the new value._ 
```C++
double ddc_get_rate (
    const ddc_state_t * state
) 
```




```C++
>>> from doppler.ddc import DDC
>>> ddc = DDC(norm_freq=0.0, rate=0.25)
>>> ddc.rate
0.25
```
 


        

<hr>



### function ddc\_reset 

_Zero LO phase and resampler history. After reset, the next execute call produces the same output as the first execute after create — useful for reproducible block-by-block processing or looped test fixtures._ 
```C++
void ddc_reset (
    ddc_state_t * state
) 
```




```C++
>>> from doppler.ddc import DDC
>>> import numpy as np
>>> ddc = DDC(norm_freq=0.0, rate=0.25)
>>> x = np.ones(64, dtype=np.complex64)
>>> y1 = ddc.execute(x)
>>> ddc.reset()
>>> y2 = ddc.execute(x)
>>> bool(np.array_equal(y1, y2))
True
```
 


        

<hr>



### function ddc\_set\_norm\_freq 

_Retune the LO without resetting phase or resampler history. Updates the NCO phase increment atomically so the carrier shift changes seamlessly across block boundaries. The resampler history and LO phase accumulator are left intact, avoiding the transient that a full reset would cause._ 
```C++
void ddc_set_norm_freq (
    ddc_state_t * state,
    double val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New normalised frequency (cycles/sample at input rate).


```C++
>>> from doppler.ddc import DDC
>>> ddc = DDC(norm_freq=-0.1, rate=0.25)
>>> ddc.norm_freq = -0.2
>>> ddc.norm_freq
-0.2
```
 


        

<hr>



### function ddcr\_create 

_Create a real-input Digital Down-Converter (Architecture D2). The signal chain is: halfband R2C (2:1, bakes in +fs/4 shift) → fine LO mix at the intermediate rate (fs\_in/2) → RateConverter → CF32 output. The halfband stage uses ±1/0 coefficients (no multiplications), making DDCR roughly 2× cheaper than DDC at the same total decimation ratio._ 
```C++
ddcr_state_t * ddcr_create (
    double norm_freq,
    double rate
) 
```





**Parameters:**


* `norm_freq` Fine NCO frequency at the intermediate rate (fs\_in/2, cycles/sample). To tune a real tone at normalised input frequency f\_c to DC, set norm\_freq = -(2\*f\_c + 0.5). 
* `rate` Total output/input rate. Must be in (0, 0.5) because the halfband pre-decimates by 2. 



**Returns:**

Non-NULL on success, NULL on OOM or invalid args.



```C++
>>> from doppler.ddc import DDCR
>>> ddcr = DDCR(norm_freq=-0.7, rate=0.25)
>>> ddcr.norm_freq
-0.7
>>> ddcr.rate
0.25
```
 


        

<hr>



### function ddcr\_destroy 

_Free all resources held by a DDCR instance. Releases the halfband, RateConverter, and LO substructures, then the struct itself. Passing NULL is a no-op._ 
```C++
void ddcr_destroy (
    ddcr_state_t * s
) 
```




```C++
>>> from doppler.ddc import DDCR
>>> ddcr = DDCR(norm_freq=0.0, rate=0.25)
>>> ddcr.destroy()   # releases C memory immediately
```
 


        

<hr>



### function ddcr\_execute 

_Process a block of real float32 samples through the full DDCR signal chain: halfband R2C → LO mix → RateConverter → CF32. The halfband decimates by 2 and applies a built-in +fs/4 frequency shift; the fine NCO then completes the tuning. State is maintained across calls for contiguous streaming. Output length ≈ n\_in \* rate (±1 from polyphase indexing). A real tone at input normalised frequency f\_c has amplitude 0.5 in the baseband output (one-sided spectrum), consistent with analytic signal theory._ 
```C++
size_t ddcr_execute (
    ddcr_state_t * s,
    const float * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `s` Must be non-NULL. 
* `in` Real float32 input block. 
* `n_in` Number of input samples (C-only, hidden from Python). 
* `out` CF32 output buffer (C-only, hidden from Python). 
* `max_out` Output buffer capacity (C-only, hidden from Python). 



**Returns:**

Number of output samples written (C-only).



```C++
>>> from doppler.ddc import DDCR
>>> import numpy as np
>>> ddcr = DDCR(norm_freq=-0.7, rate=0.25)
>>> t = np.arange(4096)
>>> x = np.cos(2 * np.pi * 0.1 * t).astype(np.float32)
>>> y = ddcr.execute(x)
>>> y.shape
(1024,)
>>> y.dtype
dtype('complex64')
>>> round(float(abs(y[500])), 2)   # one-sided cosine amplitude ≈ 0.5
0.5
```
 


        

<hr>



### function ddcr\_get\_norm\_freq 

_Return the current fine NCO normalised frequency at the intermediate rate (fs\_in/2, cycles/sample)._ 
```C++
double ddcr_get_norm_freq (
    const ddcr_state_t * s
) 
```




```C++
>>> from doppler.ddc import DDCR
>>> ddcr = DDCR(norm_freq=-0.7, rate=0.25)
>>> ddcr.norm_freq
-0.7
```
 


        

<hr>



### function ddcr\_get\_rate 

_Return the total configured rate (fs\_out / fs\_in, read-only). This is the end-to-end ratio from ADC input to CF32 output. Change it by destroying and recreating the DDCR._ 
```C++
double ddcr_get_rate (
    const ddcr_state_t * s
) 
```




```C++
>>> from doppler.ddc import DDCR
>>> ddcr = DDCR(norm_freq=0.0, rate=0.25)
>>> ddcr.rate
0.25
```
 


        

<hr>



### function ddcr\_reset 

_Zero halfband filter history, LO phase, and resampler history. After reset, the next execute call reproduces the output of the first call after create, enabling repeatable block-by-block tests._ 
```C++
void ddcr_reset (
    ddcr_state_t * s
) 
```




```C++
>>> from doppler.ddc import DDCR
>>> import numpy as np
>>> ddcr = DDCR(norm_freq=0.0, rate=0.25)
>>> x = np.ones(64, dtype=np.float32)
>>> y1 = ddcr.execute(x)
>>> ddcr.reset()
>>> y2 = ddcr.execute(x)
>>> bool(np.array_equal(y1, y2))
True
```
 


        

<hr>



### function ddcr\_set\_norm\_freq 

_Retune the fine NCO without resetting halfband or resampler history. Updates the LO phase increment only; state is preserved for seamless tuning across block boundaries._ 
```C++
void ddcr_set_norm_freq (
    ddcr_state_t * s,
    double norm_freq
) 
```





**Parameters:**


* `s` Must be non-NULL. 
* `norm_freq` New frequency at the intermediate rate (fs\_in/2).


```C++
>>> from doppler.ddc import DDCR
>>> ddcr = DDCR(norm_freq=-0.7, rate=0.25)
>>> ddcr.norm_freq = -0.5
>>> ddcr.norm_freq
-0.5
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ddc/ddc_core.h`

