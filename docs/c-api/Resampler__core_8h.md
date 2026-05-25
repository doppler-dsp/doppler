

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
|  [**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* | [**Resampler\_create**](#function-resampler_create) (double rate) <br>_Create a Resampler with the built-in 4096×19 Kaiser bank._  |
|  [**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* | [**Resampler\_create\_custom**](#function-resampler_create_custom) (size\_t num\_phases, size\_t num\_taps, const float \* bank, double rate) <br>_Create a Resampler with a user-supplied polyphase bank._  |
|  void | [**Resampler\_destroy**](#function-resampler_destroy) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br> |
|  size\_t | [**Resampler\_execute**](#function-resampler_execute) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state, const float complex \* x, size\_t x\_len, float complex \* out) <br>_Resample x(0..x\_len-1) into out(0..n\_out-1)._  |
|  size\_t | [**Resampler\_execute\_ctrl**](#function-resampler_execute_ctrl) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state, const float complex \* x, size\_t x\_len, const float complex \* ctrl, size\_t ctrl\_len, float complex \* out) <br>_Resample with per-sample rate deviations._  |
|  size\_t | [**Resampler\_execute\_ctrl\_max\_out**](#function-resampler_execute_ctrl_max_out) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br> |
|  size\_t | [**Resampler\_execute\_max\_out**](#function-resampler_execute_max_out) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br> |
|  size\_t | [**Resampler\_get\_num\_phases**](#function-resampler_get_num_phases) (const [**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br> |
|  size\_t | [**Resampler\_get\_num\_taps**](#function-resampler_get_num_taps) (const [**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br> |
|  double | [**Resampler\_get\_rate**](#function-resampler_get_rate) (const [**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br> |
|  void | [**Resampler\_reset**](#function-resampler_reset) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state) <br> |
|  void | [**Resampler\_set\_rate**](#function-resampler_set_rate) ([**Resampler\_state\_t**](Resampler__core_8h.md#typedef-resampler_state_t) \* state, double rate) <br> |



























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

_Create a Resampler with the built-in 4096×19 Kaiser bank._ 
```C++
Resampler_state_t * Resampler_create (
    double rate
) 
```





**Parameters:**


* `rate` Resample ratio (out/in). Values &gt;= 1.0 interpolate; values &lt; 1.0 decimate. 



**Returns:**

Non-NULL on success, NULL on OOM. 





        

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

_Resample x(0..x\_len-1) into out(0..n\_out-1)._ 
```C++
size_t Resampler_execute (
    Resampler_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out
) 
```



out must be at least [**Resampler\_execute\_max\_out()**](Resampler__core_8h.md#function-resampler_execute_max_out) samples wide. Returns the number of output samples written. 


        

<hr>



### function Resampler\_execute\_ctrl 

_Resample with per-sample rate deviations._ 
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



rate\_i = base\_rate + crealf(ctrl(i)). ctrl and x must be the same length. Returns number of output samples written. 


        

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

```C++
size_t Resampler_get_num_phases (
    const Resampler_state_t * state
) 
```




<hr>



### function Resampler\_get\_num\_taps 

```C++
size_t Resampler_get_num_taps (
    const Resampler_state_t * state
) 
```




<hr>



### function Resampler\_get\_rate 

```C++
double Resampler_get_rate (
    const Resampler_state_t * state
) 
```




<hr>



### function Resampler\_reset 

```C++
void Resampler_reset (
    Resampler_state_t * state
) 
```



Zero delay line and phase accumulator. Rate and bank preserved. 


        

<hr>



### function Resampler\_set\_rate 

```C++
void Resampler_set_rate (
    Resampler_state_t * state,
    double rate
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

