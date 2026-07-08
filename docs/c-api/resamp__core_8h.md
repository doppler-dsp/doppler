

# File resamp\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**resamp**](dir_289a9297ce406b952fab973539197d1c.md) **>** [**resamp\_core.h**](resamp__core_8h.md)

[Go to the source code of this file](resamp__core_8h_source.md)

_Continuously-variable polyphase resampler for CF32 IQ._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`















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
|  size\_t | [**resamp\_get\_num\_phases**](#function-resamp_get_num_phases) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br> |
|  size\_t | [**resamp\_get\_num\_taps**](#function-resamp_get_num_taps) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br> |
|  double | [**resamp\_get\_rate**](#function-resamp_get_rate) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br> |
|  void | [**resamp\_get\_state**](#function-resamp_get_state) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state, void \* blob) <br>_Serialize_ `state's` _mutable state into_`blob` _._ |
|  void | [**resamp\_reset**](#function-resamp_reset) ([**resamp\_state\_t**](structresamp__state__t.md) \* state) <br> |
|  void | [**resamp\_set\_rate**](#function-resamp_set_rate) ([**resamp\_state\_t**](structresamp__state__t.md) \* state, double rate) <br> |
|  int | [**resamp\_set\_state**](#function-resamp_set_state) ([**resamp\_state\_t**](structresamp__state__t.md) \* state, const void \* blob) <br>_Restore mutable state from_ `blob` _(same rate)._ |
|  size\_t | [**resamp\_state\_bytes**](#function-resamp_state_bytes) (const [**resamp\_state\_t**](structresamp__state__t.md) \* state) <br>_Bytes_ [_**resamp\_get\_state()**_](resamp__core_8h.md#function-resamp_get_state) _writes for_`state` _(envelope + payload)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**RESAMP\_STATE\_MAGIC**](resamp__core_8h.md#define-resamp_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('R', 'S', 'M', 'P')`<br> |
| define  | [**RESAMP\_STATE\_VERSION**](resamp__core_8h.md#define-resamp_state_version)  `1u`<br> |

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
#define RESAMP_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/resamp/resamp_core.h`

