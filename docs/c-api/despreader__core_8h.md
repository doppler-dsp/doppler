

# File despreader\_core.h



[**FileList**](files.md) **>** [**despreader**](dir_9949992fff5aebed427f83f9eaa478ca.md) **>** [**despreader\_core.h**](despreader__core_8h.md)

[Go to the source code of this file](despreader__core_8h_source.md)

_Despreader component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "loop_filter/loop_filter_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**despreader\_state\_t**](structdespreader__state__t.md) <br>_Despreader state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**despreader\_bits**](#function-despreader_bits) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, const float complex \* x, size\_t x\_len, uint8\_t \* out, size\_t max\_out) <br>_Despread a CF32 block; emit one hard BPSK bit (0/1) per code period._  |
|  size\_t | [**despreader\_bits\_max\_out**](#function-despreader_bits_max_out) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br>_Upper bound on bits_ `despreader_bits` _can emit (0; see despreader\_steps\_max\_out)._ |
|  [**despreader\_state\_t**](structdespreader__state__t.md) \* | [**despreader\_create**](#function-despreader_create) (const uint8\_t \* code, size\_t code\_len, size\_t sf, size\_t sps, double init\_norm\_freq, double init\_chip\_phase, double bn\_carrier, double bn\_code) <br>_Create a despreader instance._  |
|  void | [**despreader\_destroy**](#function-despreader_destroy) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br>_Destroy a despreader instance and release all memory._  |
|  double | [**despreader\_get\_bn\_carrier**](#function-despreader_get_bn_carrier) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br>_Carrier (Costas) loop noise bandwidth, normalized to the symbol rate._  |
|  double | [**despreader\_get\_bn\_code**](#function-despreader_get_bn_code) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br>_Code (DLL) loop noise bandwidth, normalized to the symbol rate._  |
|  double | [**despreader\_get\_code\_phase**](#function-despreader_get_code_phase) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br>_Current tracked code phase within the symbol, chips._  |
|  double | [**despreader\_get\_lock\_metric**](#function-despreader_get_lock_metric) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br>_Lock indicator in [0,1] (EMA of \|Re prompt\|/\|prompt\|; ~1 = locked)._  |
|  double | [**despreader\_get\_norm\_freq**](#function-despreader_get_norm_freq) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br>_Current carrier frequency estimate, cycles/sample._  |
|  double | [**despreader\_get\_snr\_est**](#function-despreader_get_snr_est) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br>_Post-despread SNR estimate (EMA of (Re prompt)^2 / (Im prompt)^2)._  |
|  void | [**despreader\_get\_state**](#function-despreader_get_state) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state, void \* blob) <br> |
|  void | [**despreader\_reset**](#function-despreader_reset) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br>_Reset Despreader to its post-create state._  |
|  void | [**despreader\_set\_acq**](#function-despreader_set_acq) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, const uint8\_t \* acq\_code, size\_t acq\_code\_len, size\_t acq\_reps) <br>_Enable preamble-aided pull-in with a distinct acquisition code._  |
|  void | [**despreader\_set\_bn\_carrier**](#function-despreader_set_bn_carrier) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, double val) <br>_Set the carrier loop bandwidth (recomputes the loop gains)._  |
|  void | [**despreader\_set\_bn\_code**](#function-despreader_set_bn_code) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, double val) <br>_Set the code loop bandwidth (recomputes the loop gains)._  |
|  void | [**despreader\_set\_norm\_freq**](#function-despreader_set_norm_freq) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, double val) <br>_Override the carrier frequency estimate, cycles/sample (re-seed)._  |
|  int | [**despreader\_set\_state**](#function-despreader_set_state) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**despreader\_state\_bytes**](#function-despreader_state_bytes) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br> |
|  size\_t | [**despreader\_steps**](#function-despreader_steps) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br>_Despread a CF32 block; emit one complex prompt symbol per code period._  |
|  size\_t | [**despreader\_steps\_max\_out**](#function-despreader_steps_max_out) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br>_Upper bound on symbols_ `despreader_steps` _can emit (0; the caller sizes the output buffer to the input length, which always suffices)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DESPREADER\_STATE\_MAGIC**](despreader__core_8h.md#define-despreader_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('D','S','P','R')`<br> |
| define  | [**DESPREADER\_STATE\_VERSION**](despreader__core_8h.md#define-despreader_state_version)  `1u`<br> |

## Detailed Description


Lifecycle: create -&gt; [step / steps / reset]\* -&gt; destroy


Example: 
```C++
despreader_state_t *obj = despreader_create(NULL, 0, 1, 2, 0.0, 0.0, 0.01, 0.002);
float complex y = despreader_step(obj, 0.0f + 0.0f * I);
despreader_destroy(obj);
```
 


    
## Public Functions Documentation




### function despreader\_bits 

_Despread a CF32 block; emit one hard BPSK bit (0/1) per code period._ 
```C++
size_t despreader_bits (
    despreader_state_t * state,
    const float complex * x,
    size_t x_len,
    uint8_t * out,
    size_t max_out
) 
```



Same streaming kernel as [**despreader\_steps()**](despreader__core_8h.md#function-despreader_steps), but emits the hard decision `crealf(prompt) >= 0` instead of the complex symbol.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input CF32 samples, length `x_len`. 
* `x_len` Number of input samples. 
* `out` Output buffer for bits (&gt;= max\_out). 
* `max_out` Capacity of `out` in bits. 



**Returns:**

Number of bits written. 





        

<hr>



### function despreader\_bits\_max\_out 

_Upper bound on bits_ `despreader_bits` _can emit (0; see despreader\_steps\_max\_out)._
```C++
size_t despreader_bits_max_out (
    despreader_state_t * state
) 
```




<hr>



### function despreader\_create 

_Create a despreader instance._ 
```C++
despreader_state_t * despreader_create (
    const uint8_t * code,
    size_t code_len,
    size_t sf,
    size_t sps,
    double init_norm_freq,
    double init_chip_phase,
    double bn_carrier,
    double bn_code
) 
```





**Parameters:**


* `code` Data spreading code (0/1 chips), length `code_len`; copied. 
* `code_len` Length of `code` in chips (&gt;= sf). 
* `sf` sf (default: 1). 
* `sps` sps (default: 2). 
* `init_norm_freq` init\_norm\_freq (default: 0.0). 
* `init_chip_phase` init\_chip\_phase (default: 0.0). 
* `bn_carrier` bn\_carrier (default: 0.01). 
* `bn_code` bn\_code (default: 0.002). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**despreader\_destroy()**](despreader__core_8h.md#function-despreader_destroy) when done. 





        

<hr>



### function despreader\_destroy 

_Destroy a despreader instance and release all memory._ 
```C++
void despreader_destroy (
    despreader_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function despreader\_get\_bn\_carrier 

_Carrier (Costas) loop noise bandwidth, normalized to the symbol rate._ 
```C++
double despreader_get_bn_carrier (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_bn\_code 

_Code (DLL) loop noise bandwidth, normalized to the symbol rate._ 
```C++
double despreader_get_bn_code (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_code\_phase 

_Current tracked code phase within the symbol, chips._ 
```C++
double despreader_get_code_phase (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_lock\_metric 

_Lock indicator in [0,1] (EMA of \|Re prompt\|/\|prompt\|; ~1 = locked)._ 
```C++
double despreader_get_lock_metric (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_norm\_freq 

_Current carrier frequency estimate, cycles/sample._ 
```C++
double despreader_get_norm_freq (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_snr\_est 

_Post-despread SNR estimate (EMA of (Re prompt)^2 / (Im prompt)^2)._ 
```C++
double despreader_get_snr_est (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_state 

```C++
void despreader_get_state (
    const despreader_state_t * state,
    void * blob
) 
```




<hr>



### function despreader\_reset 

_Reset Despreader to its post-create state._ 
```C++
void despreader_reset (
    despreader_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function despreader\_set\_acq 

_Enable preamble-aided pull-in with a distinct acquisition code._ 
```C++
void despreader_set_acq (
    despreader_state_t * state,
    const uint8_t * acq_code,
    size_t acq_code_len,
    size_t acq_reps
) 
```



Track `acq_reps` periods of `acq_code` coherently (the unmodulated, repeated acquisition preamble — a full ±pi phase discriminator, so the loops pull in even a wide residual) before switching to the data code for the payload. Call before feeding the burst; the acq mode clears automatically once the preamble is consumed, and re-arms on [**despreader\_reset()**](despreader__core_8h.md#function-despreader_reset).




**Parameters:**


* `state` Must be non-NULL. 
* `acq_code` Acquisition code (0/1), length acq\_code\_len; copied. 
* `acq_code_len` Acquisition code length in chips. 
* `acq_reps` Number of acq-code periods in the preamble. 




        

<hr>



### function despreader\_set\_bn\_carrier 

_Set the carrier loop bandwidth (recomputes the loop gains)._ 
```C++
void despreader_set_bn_carrier (
    despreader_state_t * state,
    double val
) 
```




<hr>



### function despreader\_set\_bn\_code 

_Set the code loop bandwidth (recomputes the loop gains)._ 
```C++
void despreader_set_bn_code (
    despreader_state_t * state,
    double val
) 
```




<hr>



### function despreader\_set\_norm\_freq 

_Override the carrier frequency estimate, cycles/sample (re-seed)._ 
```C++
void despreader_set_norm_freq (
    despreader_state_t * state,
    double val
) 
```




<hr>



### function despreader\_set\_state 

```C++
int despreader_set_state (
    despreader_state_t * state,
    const void * blob
) 
```




<hr>



### function despreader\_state\_bytes 

```C++
size_t despreader_state_bytes (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_steps 

_Despread a CF32 block; emit one complex prompt symbol per code period._ 
```C++
size_t despreader_steps (
    despreader_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```



Streams: a partial symbol is carried in state across calls. Each emitted symbol is the complex prompt integrate-and-dump (carrier-wiped, code-stripped) — its sign is the BPSK decision, its phase/magnitude the soft information. During a `despreader_set_acq` preamble no symbols are emitted (the loops are pulling in); payload symbols follow.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input CF32 samples, length `x_len`. 
* `x_len` Number of input samples. 
* `out` Output buffer for prompt symbols (&gt;= max\_out). 
* `max_out` Capacity of `out` in symbols. 



**Returns:**

Number of symbols written.



```C++
// seed from acquisition (norm_freq cyc/sample, chip phase in chips):
despreader_state_t *d = despreader_create(code, n, 32, 2, f0, chip, .05, .01);
float complex sym[256];
size_t k = despreader_steps(d, rx, rx_len, sym, 256);
// hard bit of sym[i] = crealf(sym[i]) >= 0
despreader_destroy(d);
```
 


        

<hr>



### function despreader\_steps\_max\_out 

_Upper bound on symbols_ `despreader_steps` _can emit (0; the caller sizes the output buffer to the input length, which always suffices)._
```C++
size_t despreader_steps_max_out (
    despreader_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define DESPREADER\_STATE\_MAGIC 

```C++
#define DESPREADER_STATE_MAGIC `DP_FOURCC ('D','S','P','R')`
```




<hr>



### define DESPREADER\_STATE\_VERSION 

```C++
#define DESPREADER_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/despreader/despreader_core.h`

