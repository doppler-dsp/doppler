

# File ddc\_core.h



[**FileList**](files.md) **>** [**ddc**](dir_b33dc116452ac5c7d7799725e78b6bdc.md) **>** [**ddc\_core.h**](ddc__core_8h.md)

[Go to the source code of this file](ddc__core_8h_source.md)

_Digital Down-Converter — composes LO + polyphase resampler._ [More...](#detailed-description)

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
|  [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* | [**ddc\_create**](#function-ddc_create) (double norm\_freq, double rate) <br>_Create a complex-input DDC._  |
|  void | [**ddc\_destroy**](#function-ddc_destroy) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* s) <br> |
|  size\_t | [**ddc\_execute**](#function-ddc_execute) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* s, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Mix and resample a block of CF32 samples._  |
|  double | [**ddc\_get\_norm\_freq**](#function-ddc_get_norm_freq) (const [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* s) <br> |
|  double | [**ddc\_get\_rate**](#function-ddc_get_rate) (const [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* s) <br> |
|  void | [**ddc\_reset**](#function-ddc_reset) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* s) <br> |
|  void | [**ddc\_set\_norm\_freq**](#function-ddc_set_norm_freq) ([**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* s, double norm\_freq) <br>_Retune the LO without resetting phase or resampler history._  |
|  [**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* | [**ddcr\_create**](#function-ddcr_create) (double norm\_freq, double rate) <br>_Create a real-input DDC._  |
|  void | [**ddcr\_destroy**](#function-ddcr_destroy) ([**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* s) <br> |
|  size\_t | [**ddcr\_execute**](#function-ddcr_execute) ([**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* s, const float \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_Process a block of real float32 samples._  |
|  double | [**ddcr\_get\_norm\_freq**](#function-ddcr_get_norm_freq) (const [**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* s) <br> |
|  double | [**ddcr\_get\_rate**](#function-ddcr_get_rate) (const [**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* s) <br> |
|  void | [**ddcr\_reset**](#function-ddcr_reset) ([**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* s) <br> |
|  void | [**ddcr\_set\_norm\_freq**](#function-ddcr_set_norm_freq) ([**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* s, double norm\_freq) <br> |




























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

_Create a complex-input DDC._ 
```C++
ddc_state_t * ddc_create (
    double norm_freq,
    double rate
) 
```





**Parameters:**


* `norm_freq` LO frequency in cycles/sample at the input rate. 
* `rate` Output rate / input rate. Must be &gt; 0. 



**Returns:**

Non-NULL on success, NULL on OOM or invalid args. 





        

<hr>



### function ddc\_destroy 

```C++
void ddc_destroy (
    ddc_state_t * s
) 
```



Free all resources. NULL is a no-op. 


        

<hr>



### function ddc\_execute 

_Mix and resample a block of CF32 samples._ 
```C++
size_t ddc_execute (
    ddc_state_t * s,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `s` Must be non-NULL. 
* `in` Input samples, complex64, length n\_in. 
* `n_in` Number of input samples. 
* `out` Output buffer, complex64, capacity max\_out. 
* `max_out` Maximum output samples to write. 



**Returns:**

Number of output samples written. 





        

<hr>



### function ddc\_get\_norm\_freq 

```C++
double ddc_get_norm_freq (
    const ddc_state_t * s
) 
```



Return the current LO normalised frequency. 


        

<hr>



### function ddc\_get\_rate 

```C++
double ddc_get_rate (
    const ddc_state_t * s
) 
```



Return the configured output/input rate ratio. 


        

<hr>



### function ddc\_reset 

```C++
void ddc_reset (
    ddc_state_t * s
) 
```



Zero LO phase and resampler history. 


        

<hr>



### function ddc\_set\_norm\_freq 

_Retune the LO without resetting phase or resampler history._ 
```C++
void ddc_set_norm_freq (
    ddc_state_t * s,
    double norm_freq
) 
```





**Parameters:**


* `s` Must be non-NULL. 
* `norm_freq` New normalised frequency. 




        

<hr>



### function ddcr\_create 

_Create a real-input DDC._ 
```C++
ddcr_state_t * ddcr_create (
    double norm_freq,
    double rate
) 
```





**Parameters:**


* `norm_freq` Fine NCO frequency at the intermediate rate (fs\_in/2). 
* `rate` Total output/input rate. Must be in (0, 0.5). 



**Returns:**

Non-NULL on success, NULL on OOM or invalid args. 





        

<hr>



### function ddcr\_destroy 

```C++
void ddcr_destroy (
    ddcr_state_t * s
) 
```



Free all resources. NULL is a no-op. 


        

<hr>



### function ddcr\_execute 

_Process a block of real float32 samples._ 
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
* `in` Real input samples, float32, length n\_in. 
* `n_in` Number of input samples. 
* `out` CF32 output buffer, capacity max\_out. 
* `max_out` Maximum output samples to write. 



**Returns:**

Number of output samples written. 





        

<hr>



### function ddcr\_get\_norm\_freq 

```C++
double ddcr_get_norm_freq (
    const ddcr_state_t * s
) 
```



Return the current fine NCO normalised frequency (at intermediate rate). 


        

<hr>



### function ddcr\_get\_rate 

```C++
double ddcr_get_rate (
    const ddcr_state_t * s
) 
```



Return the total configured rate (fs\_out / fs\_in). 


        

<hr>



### function ddcr\_reset 

```C++
void ddcr_reset (
    ddcr_state_t * s
) 
```



Zero halfband, LO phase, and resampler history. 


        

<hr>



### function ddcr\_set\_norm\_freq 

```C++
void ddcr_set_norm_freq (
    ddcr_state_t * s,
    double norm_freq
) 
```



Retune the fine NCO without resetting state. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ddc/ddc_core.h`

