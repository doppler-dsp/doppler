

# File HalfbandDecimator\_core.h



[**FileList**](files.md) **>** [**HalfbandDecimator**](dir_6ac3f68ee82e011454c15c865a37e192.md) **>** [**HalfbandDecimator\_core.h**](HalfbandDecimator__core_8h.md)

[Go to the source code of this file](HalfbandDecimator__core_8h_source.md)

_Halfband 2:1 decimator for CF32 IQ (adapter over hbdecim\_core)._ [More...](#detailed-description)

* `#include "hbdecim/hbdecim_core.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef [**hbdecim\_state\_t**](structhbdecim__state__t.md) | [**HalfbandDecimator\_state\_t**](#typedef-halfbanddecimator_state_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-halfbanddecimator_state_t) \* | [**HalfbandDecimator\_create**](#function-halfbanddecimator_create) (size\_t num\_taps, const float \* h) <br>_Create a HalfbandDecimator._  |
|  void | [**HalfbandDecimator\_destroy**](#function-halfbanddecimator_destroy) ([**HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-halfbanddecimator_state_t) \* state) <br> |
|  size\_t | [**HalfbandDecimator\_execute**](#function-halfbanddecimator_execute) ([**HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-halfbanddecimator_state_t) \* state, const float complex \* x, size\_t x\_len, float complex \* out) <br>_Decimate x(0..x\_len-1) by 2 into out(0..n\_out-1)._  |
|  size\_t | [**HalfbandDecimator\_execute\_max\_out**](#function-halfbanddecimator_execute_max_out) ([**HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-halfbanddecimator_state_t) \* state) <br> |
|  size\_t | [**HalfbandDecimator\_get\_num\_taps**](#function-halfbanddecimator_get_num_taps) (const [**HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-halfbanddecimator_state_t) \* state) <br> |
|  double | [**HalfbandDecimator\_get\_rate**](#function-halfbanddecimator_get_rate) (const [**HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-halfbanddecimator_state_t) \* state) <br> |
|  void | [**HalfbandDecimator\_reset**](#function-halfbanddecimator_reset) ([**HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-halfbanddecimator_state_t) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**HBDECIM\_MAX\_OUT**](HalfbandDecimator__core_8h.md#define-hbdecim_max_out)  `32768`<br> |

## Detailed Description


Thin adapter over [**hbdecim\_state\_t**](structhbdecim__state__t.md). The caller supplies a FIR coefficient array at construction; use resample.build\_bank(2, ...) from Python to generate a suitable halfband prototype.


Lifecycle: 
```C++
float h[] = { ... };  // num_taps FIR branch coefficients
HalfbandDecimator_state_t *r =
    HalfbandDecimator_create(num_taps, h);
float complex out[512];
size_t n = HalfbandDecimator_execute(r, in, 1024, out);
HalfbandDecimator_destroy(r);
```
 


    
## Public Types Documentation




### typedef HalfbandDecimator\_state\_t 

```C++
typedef hbdecim_state_t HalfbandDecimator_state_t;
```




<hr>
## Public Functions Documentation




### function HalfbandDecimator\_create 

_Create a HalfbandDecimator._ 
```C++
HalfbandDecimator_state_t * HalfbandDecimator_create (
    size_t num_taps,
    const float * h
) 
```





**Parameters:**


* `num_taps` FIR branch length. 
* `h` FIR branch coefficients, length num\_taps. 



**Returns:**

Non-NULL on success, NULL on invalid args or OOM. 





        

<hr>



### function HalfbandDecimator\_destroy 

```C++
void HalfbandDecimator_destroy (
    HalfbandDecimator_state_t * state
) 
```



Free all resources. NULL is a no-op. 


        

<hr>



### function HalfbandDecimator\_execute 

_Decimate x(0..x\_len-1) by 2 into out(0..n\_out-1)._ 
```C++
size_t HalfbandDecimator_execute (
    HalfbandDecimator_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out
) 
```



out must be at least [**HalfbandDecimator\_execute\_max\_out()**](HalfbandDecimator__core_8h.md#function-halfbanddecimator_execute_max_out) samples. Returns actual output count (roughly x\_len / 2). 


        

<hr>



### function HalfbandDecimator\_execute\_max\_out 

```C++
size_t HalfbandDecimator_execute_max_out (
    HalfbandDecimator_state_t * state
) 
```



Always returns HBDECIM\_MAX\_OUT. 


        

<hr>



### function HalfbandDecimator\_get\_num\_taps 

```C++
size_t HalfbandDecimator_get_num_taps (
    const HalfbandDecimator_state_t * state
) 
```



Returns the FIR branch length passed to create. 


        

<hr>



### function HalfbandDecimator\_get\_rate 

```C++
double HalfbandDecimator_get_rate (
    const HalfbandDecimator_state_t * state
) 
```



Always returns 0.5. 


        

<hr>



### function HalfbandDecimator\_reset 

```C++
void HalfbandDecimator_reset (
    HalfbandDecimator_state_t * state
) 
```



Zero delay lines. Coefficients preserved. 


        

<hr>
## Macro Definition Documentation





### define HBDECIM\_MAX\_OUT 

```C++
#define HBDECIM_MAX_OUT `32768`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/HalfbandDecimator/HalfbandDecimator_core.h`

