

# File hbdecim\_r2c\_core.h



[**FileList**](files.md) **>** [**hbdecim**](dir_3828151286b0ff520a0d701b39db5af1.md) **>** [**hbdecim\_r2c\_core.h**](hbdecim__r2c__core_8h.md)

[Go to the source code of this file](hbdecim__r2c__core_8h_source.md)

_Real-to-complex halfband 2:1 decimator (Architecture D2)._ [More...](#detailed-description)

* `#include "clib_common.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct hbdecim\_r2c\_state | [**hbdecim\_r2c\_state\_t**](#typedef-hbdecim_r2c_state_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* | [**hbdecim\_r2c\_create**](#function-hbdecim_r2c_create) (size\_t num\_taps, const float \* h) <br>_Allocate a real-to-complex halfband decimator._  |
|  void | [**hbdecim\_r2c\_destroy**](#function-hbdecim_r2c_destroy) ([**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* r) <br> |
|  size\_t | [**hbdecim\_r2c\_execute**](#function-hbdecim_r2c_execute) ([**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* r, const float \* in, size\_t num\_in, float \_Complex \* out, size\_t max\_out) <br>_Decimate real float32 input by 2, producing CF32._  |
|  size\_t | [**hbdecim\_r2c\_get\_num\_taps**](#function-hbdecim_r2c_get_num_taps) (const [**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* r) <br> |
|  double | [**hbdecim\_r2c\_get\_rate**](#function-hbdecim_r2c_get_rate) (const [**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* r) <br> |
|  void | [**hbdecim\_r2c\_reset**](#function-hbdecim_r2c_reset) ([**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* r) <br> |




























## Detailed Description


Lifted from dp\_hbdecim\_r2cf32\_t (c/src/hbdecim.c). Accepts real float32 input and produces CF32 IQ at half the input rate, with an embedded fs/4 frequency shift (same effect as mixing with e^{jπn/2} before decimating, at zero extra multiplications).


The output at sample m is: y(m) = (FIR(even) + j·delay(odd)) · (-1)^m


where the sign pattern (-1)^m provides the fs/4 shift correction.


Lifecycle: 
```C++
hbdecim_r2c_state_t *r = hbdecim_r2c_create(num_taps, h);
size_t n = hbdecim_r2c_execute(r, in, num_in, out, max_out);
hbdecim_r2c_destroy(r);
```
 


    
## Public Types Documentation




### typedef hbdecim\_r2c\_state\_t 

```C++
typedef struct hbdecim_r2c_state hbdecim_r2c_state_t;
```




<hr>
## Public Functions Documentation




### function hbdecim\_r2c\_create 

_Allocate a real-to-complex halfband decimator._ 
```C++
hbdecim_r2c_state_t * hbdecim_r2c_create (
    size_t num_taps,
    const float * h
) 
```





**Parameters:**


* `num_taps` FIR branch length. 
* `h` FIR coefficients, float32, length num\_taps. 



**Returns:**

Non-NULL on success, NULL on invalid args or OOM. 





        

<hr>



### function hbdecim\_r2c\_destroy 

```C++
void hbdecim_r2c_destroy (
    hbdecim_r2c_state_t * r
) 
```



Free all resources. NULL is a no-op. 


        

<hr>



### function hbdecim\_r2c\_execute 

_Decimate real float32 input by 2, producing CF32._ 
```C++
size_t hbdecim_r2c_execute (
    hbdecim_r2c_state_t * r,
    const float * in,
    size_t num_in,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `r` Must be non-NULL. 
* `in` Real input samples. 
* `num_in` Number of input samples. 
* `out` CF32 output buffer. 
* `max_out` Capacity in samples. 



**Returns:**

Number of output samples written. 





        

<hr>



### function hbdecim\_r2c\_get\_num\_taps 

```C++
size_t hbdecim_r2c_get_num_taps (
    const hbdecim_r2c_state_t * r
) 
```



Returns the FIR branch length passed to hbdecim\_r2c\_create. 


        

<hr>



### function hbdecim\_r2c\_get\_rate 

```C++
double hbdecim_r2c_get_rate (
    const hbdecim_r2c_state_t * r
) 
```



Always returns 0.5. 


        

<hr>



### function hbdecim\_r2c\_reset 

```C++
void hbdecim_r2c_reset (
    hbdecim_r2c_state_t * r
) 
```



Zero history and output parity without freeing. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/hbdecim/hbdecim_r2c_core.h`

