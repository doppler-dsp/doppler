

# File RateConverter\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**RateConverter**](dir_ab9e07a54a3e9554c466f24859c37292.md) **>** [**RateConverter\_core.h**](RateConverter__core_8h.md)

[Go to the source code of this file](RateConverter__core_8h_source.md)

_Optimal-speed rate conversion cascade._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include <complex.h>`
* `#include <stddef.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**RateConverter\_state\_t**](structRateConverter__state__t.md) <br>_Cascade state_  _owns all sub-stage C objects._ |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**rc\_stage\_t**](#enum-rc_stage_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**RateConverter\_convert**](#function-rateconverter_convert) (double rate, int compensate, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_One-shot rate conversion — no persistent state required._  |
|  [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* | [**RateConverter\_create**](#function-rateconverter_create) (double rate, int compensate) <br>_Create a rate converter for the given output/input rate ratio. Selects the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase Resampler stages at construction time (see file header for the selection table). Setting compensate=1 appends a closed-form Molnar-Vucic CIC droop-compensating FIR after any CIC stage, which improves passband flatness at the cost of one extra FIR stage._  |
|  void | [**RateConverter\_destroy**](#function-rateconverter_destroy) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Free all resources. NULL is a no-op._  |
|  size\_t | [**RateConverter\_execute**](#function-rateconverter_execute) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Convert a block of CF32 samples through the cascade. Passes input through each stage in order, ping-ponging between two intermediate buffers. State persists between calls, so contiguous calls on sequential blocks give the same result as one large call. Output length is approximately n\_in \* rate._  |
|  size\_t | [**RateConverter\_execute\_max\_out**](#function-rateconverter_execute_max_out) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Upper bound on execute output for a standard 65536-sample block._  |
|  double | [**RateConverter\_get\_rate**](#function-rateconverter_get_rate) (const [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Get / set the output-to-input sample rate ratio. The setter rebuilds the entire cascade (new stage selection, new sub-objects) and resets all filter memories — equivalent to destroying and recreating with the new rate. Setting rate &lt;= 0 is silently ignored._  |
|  void | [**RateConverter\_reset**](#function-rateconverter_reset) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s) <br>_Zero all sub-stage filter memories. Rate, stage count, and stage types are preserved. Processing from a reset state produces the same output as a freshly created converter fed the same input. Use between signal bursts to suppress transient artefacts from prior filter memory._  |
|  void | [**RateConverter\_set\_rate**](#function-rateconverter_set_rate) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s, double rate) <br>_Change the rate; rebuilds the cascade and resets all filter state. Silently ignores rate &lt;= 0._  |
|  int | [**RateConverter\_stage\_label**](#function-rateconverter_stage_label) ([**RateConverter\_state\_t**](structRateConverter__state__t.md) \* s, int i, char \* buf, size\_t len) <br>_Write a human-readable label for stage i into buf._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**RC\_MAX\_STAGES**](RateConverter__core_8h.md#define-rc_max_stages)  `3`<br> |

## Detailed Description


Selects the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase Resampler stages for a given output/input sample rate ratio at creation time. All sub-stage C objects are owned by the state struct.


Stage selection (D = 1/rate):


rate &gt;= 1.0 or D &lt; 2 [Resampler(rate)] D ~= 2^1 [HalfbandDecimator] D ~= 2^2 [HalfbandDecimator, HalfbandDecimator] D ~= 2^n, n&gt;=3, D&lt;=4096 [CIC(D)] D &gt;= 8, non-power-of-2 [CIC(R\*), Resampler correction] R\* = nearest power-of-2 to D otherwise (2 &lt;= D &lt; 8, non-int) [Resampler(rate)]


Lifecycle: 
```C++
RateConverter_state_t *rc = RateConverter_create(0.1, 0);
// rc->n_stages == 2: CIC(8) then Resampler(0.8)
float _Complex out[512];
size_t n = RateConverter_execute(rc, in, 4096, out, 512);
RateConverter_destroy(rc);
```
 


    
## Public Types Documentation




### enum rc\_stage\_t 

```C++
enum rc_stage_t {
    RC_STAGE_HB = 0,
    RC_STAGE_CIC = 1,
    RC_STAGE_RESAMP = 2
};
```



Stage type tags. 


        

<hr>
## Public Functions Documentation




### function RateConverter\_convert 

_One-shot rate conversion — no persistent state required._ 
```C++
size_t RateConverter_convert (
    double rate,
    int compensate,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```



Creates a temporary converter, converts n\_in samples, destroys it. Equivalent to: 
```C++
RateConverter_state_t *rc = RateConverter_create(rate, compensate);
size_t n = RateConverter_execute(rc, in, n_in, out, max_out);
RateConverter_destroy(rc);
```



Use [**RateConverter\_create()**](RateConverter__core_8h.md#function-rateconverter_create) directly when processing multiple blocks at the same rate — the one-shot form resets filter memory on every call.




**Parameters:**


* `rate` Output-to-input sample rate ratio. 
* `compensate` Non-zero to enable CIC droop compensation. 
* `in` CF32 input samples. 
* `n_in` Number of input samples. 
* `out` Output buffer. 
* `max_out` Output buffer capacity in samples. 



**Returns:**

Number of output samples written; 0 only if OOM or n\_in == 0. 





        

<hr>



### function RateConverter\_create 

_Create a rate converter for the given output/input rate ratio. Selects the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase Resampler stages at construction time (see file header for the selection table). Setting compensate=1 appends a closed-form Molnar-Vucic CIC droop-compensating FIR after any CIC stage, which improves passband flatness at the cost of one extra FIR stage._ 
```C++
RateConverter_state_t * RateConverter_create (
    double rate,
    int compensate
) 
```





**Parameters:**


* `rate` Output-to-input sample rate ratio. Any positive float. 
* `compensate` Non-zero to append a CIC passband-droop compensating FIR after any CIC stage. 



**Returns:**

Non-NULL on success; NULL if rate &lt;= 0 or OOM.



```C++
>>> from doppler.resample import RateConverter
>>> rc = RateConverter(rate=0.5, compensate=0)
>>> rc.rate
0.5
```
 


        

<hr>



### function RateConverter\_destroy 

_Free all resources. NULL is a no-op._ 
```C++
void RateConverter_destroy (
    RateConverter_state_t * s
) 
```




<hr>



### function RateConverter\_execute 

_Convert a block of CF32 samples through the cascade. Passes input through each stage in order, ping-ponging between two intermediate buffers. State persists between calls, so contiguous calls on sequential blocks give the same result as one large call. Output length is approximately n\_in \* rate._ 
```C++
size_t RateConverter_execute (
    RateConverter_state_t * s,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `s` Pointer to a valid [**RateConverter\_state\_t**](structRateConverter__state__t.md). 
* `in` CF32 input block. 
* `n_in` Number of input samples. 
* `out` Output buffer; must hold at least max\_out samples. 
* `max_out` Capacity of out in samples. 



**Returns:**

CF32 output array; length is approximately n\_in \* rate.



```C++
>>> from doppler.resample import RateConverter
>>> import numpy as np
>>> rc = RateConverter(rate=0.5, compensate=0)
>>> y = rc.execute(np.zeros(1024, dtype=np.complex64))
>>> y.shape, y.dtype
((512,), dtype('complex64'))
```
 


        

<hr>



### function RateConverter\_execute\_max\_out 

_Upper bound on execute output for a standard 65536-sample block._ 
```C++
size_t RateConverter_execute_max_out (
    RateConverter_state_t * s
) 
```



Returns (size\_t)(65536 \* max(rate, 1.0)) + 2. The Python extension uses this to pre-allocate the output buffer on the first execute call. 


        

<hr>



### function RateConverter\_get\_rate 

_Get / set the output-to-input sample rate ratio. The setter rebuilds the entire cascade (new stage selection, new sub-objects) and resets all filter memories — equivalent to destroying and recreating with the new rate. Setting rate &lt;= 0 is silently ignored._ 
```C++
double RateConverter_get_rate (
    const RateConverter_state_t * s
) 
```




```C++
>>> from doppler.resample import RateConverter
>>> rc = RateConverter(rate=0.5, compensate=0)
>>> rc.rate
0.5
>>> rc.rate = 2.0
>>> rc.rate
2.0
```
 


        

<hr>



### function RateConverter\_reset 

_Zero all sub-stage filter memories. Rate, stage count, and stage types are preserved. Processing from a reset state produces the same output as a freshly created converter fed the same input. Use between signal bursts to suppress transient artefacts from prior filter memory._ 
```C++
void RateConverter_reset (
    RateConverter_state_t * s
) 
```




```C++
>>> from doppler.resample import RateConverter
>>> rc = RateConverter(rate=0.5, compensate=0)
>>> rc.reset()
>>> rc.rate
0.5
```
 


        

<hr>



### function RateConverter\_set\_rate 

_Change the rate; rebuilds the cascade and resets all filter state. Silently ignores rate &lt;= 0._ 
```C++
void RateConverter_set_rate (
    RateConverter_state_t * s,
    double rate
) 
```





**Parameters:**


* `s` Pointer to a valid [**RateConverter\_state\_t**](structRateConverter__state__t.md). 
* `rate` New output/input rate ratio. 




        

<hr>



### function RateConverter\_stage\_label 

_Write a human-readable label for stage i into buf._ 
```C++
int RateConverter_stage_label (
    RateConverter_state_t * s,
    int i,
    char * buf,
    size_t len
) 
```



Examples: "HalfbandDecimator", "CIC(8)", "CIC(8)+FIR", "Resampler(0.8)".




**Parameters:**


* `s` Must be non-NULL. 
* `i` Stage index in [0, s-&gt;n\_stages). 
* `buf` Output buffer. 
* `len` Capacity of buf in bytes. 



**Returns:**

1 on success, 0 if i is out of range. 





        

<hr>
## Macro Definition Documentation





### define RC\_MAX\_STAGES 

```C++
#define RC_MAX_STAGES `3`
```



Maximum number of visible cascade stages. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/RateConverter/RateConverter_core.h`

