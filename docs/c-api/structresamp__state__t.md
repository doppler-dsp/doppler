

# Struct resamp\_state\_t



[**ClassList**](annotated.md) **>** [**resamp\_state\_t**](structresamp__state__t.md)



_Polyphase resampler state._ [More...](#detailed-description)

* `#include <resamp_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float \* | [**bank**](#variable-bank)  <br> |
|  size\_t | [**consumed**](#variable-consumed)  <br> |
|  uint32\_t | [**ctrl\_ahead**](#variable-ctrl_ahead)  <br> |
|  uint32\_t | [**ctrl\_debt**](#variable-ctrl_debt)  <br> |
|  uint32\_t | [**ctrl\_phase**](#variable-ctrl_phase)  <br> |
|  float \_Complex \* | [**decim\_iad**](#variable-decim_iad)  <br> |
|  float \_Complex \* | [**decim\_tfd**](#variable-decim_tfd)  <br> |
|  float \_Complex \* | [**delay\_buf**](#variable-delay_buf)  <br> |
|  size\_t | [**delay\_cap**](#variable-delay_cap)  <br> |
|  size\_t | [**delay\_head**](#variable-delay_head)  <br> |
|  size\_t | [**delay\_mask**](#variable-delay_mask)  <br> |
|  unsigned | [**log2\_phases**](#variable-log2_phases)  <br> |
|  size\_t | [**num\_phases**](#variable-num_phases)  <br> |
|  size\_t | [**num\_taps**](#variable-num_taps)  <br> |
|  uint32\_t | [**phase**](#variable-phase)  <br> |
|  uint32\_t | [**phase\_inc**](#variable-phase_inc)  <br> |
|  double | [**rate**](#variable-rate)  <br> |
|  int | [**upsample**](#variable-upsample)  <br> |












































## Detailed Description


Allocate with [**dp\_resamp\_create()**](resamp__core_8h.md#function-dp_resamp_create) or [**dp\_resamp\_create\_custom()**](resamp__core_8h.md#function-dp_resamp_create_custom); free with [**dp\_resamp\_destroy()**](resamp__core_8h.md#function-dp_resamp_destroy), which owns every pointer below. Config fields are set at create (rate and phase\_inc also by [**dp\_resamp\_set\_rate()**](resamp__core_8h.md#function-dp_resamp_set_rate)); the rest is running state. 


    
## Public Attributes Documentation




### variable bank 

```C++
float* resamp_state_t::bank;
```



Owned copy of the bank: num\_phases x num\_taps, row-major (one row per arm). 


        

<hr>



### variable consumed 

```C++
size_t resamp_state_t::consumed;
```



Inputs the last block call took: execute(), execute\_ctrl() or execute\_ctrl\_pos(); per-call, not in the blob. 


        

<hr>



### variable ctrl\_ahead 

```C++
uint32_t resamp_state_t::ctrl_ahead;
```



execute\_ctrl: inputs loaded ahead of any request (see above). 


        

<hr>



### variable ctrl\_debt 

```C++
uint32_t resamp_state_t::ctrl_debt;
```



execute\_ctrl: inputs requested and not yet given (whole intervals owed). 


        

<hr>



### variable ctrl\_phase 

```C++
uint32_t resamp_state_t::ctrl_phase;
```



execute\_ctrl phase: uint32 fraction of the current input interval; selects the arm. 


        

<hr>



### variable decim\_iad 

```C++
float _Complex* resamp_state_t::decim_iad;
```



Decimator integrate-and-dump registers, num\_taps elements. 


        

<hr>



### variable decim\_tfd 

```C++
float _Complex* resamp_state_t::decim_tfd;
```



Decimator transposed delay line, num\_taps - 1 elements (1 allocated when num\_taps == 1). 


        

<hr>



### variable delay\_buf 

```C++
float _Complex* resamp_state_t::delay_buf;
```



Mirrored delay line, 2 x delay\_cap elements, so num\_taps reads are contiguous. 


        

<hr>



### variable delay\_cap 

```C++
size_t resamp_state_t::delay_cap;
```



Delay-line length: num\_taps rounded up to a power of two. 


        

<hr>



### variable delay\_head 

```C++
size_t resamp_state_t::delay_head;
```



Delay-line write position, &lt; delay\_cap. 


        

<hr>



### variable delay\_mask 

```C++
size_t resamp_state_t::delay_mask;
```



delay\_cap - 1, the index wrap mask. 


        

<hr>



### variable log2\_phases 

```C++
unsigned resamp_state_t::log2_phases;
```



log2(num\_phases), rounded up: the bits of the phase word that select an arm. 


        

<hr>



### variable num\_phases 

```C++
size_t resamp_state_t::num_phases;
```



Polyphase arms in the bank (power of two). 


        

<hr>



### variable num\_taps 

```C++
size_t resamp_state_t::num_taps;
```



Taps per arm. 


        

<hr>



### variable phase 

```C++
uint32_t resamp_state_t::phase;
```



[**dp\_resamp\_execute()**](resamp__core_8h.md#function-dp_resamp_execute) phase accumulator (32-bit fraction of one interval). 


        

<hr>



### variable phase\_inc 

```C++
uint32_t resamp_state_t::phase_inc;
```



[**dp\_resamp\_execute()**](resamp__core_8h.md#function-dp_resamp_execute) step per tick: 2^32/rate interpolating, 2^32\*rate decimating, modulo 2^32 (0 at rate 1). 


        

<hr>



### variable rate 

```C++
double resamp_state_t::rate;
```



Output/input rate ratio (&gt; 1 interpolates). 


        

<hr>



### variable upsample 

```C++
int resamp_state_t::upsample;
```



1 = rate &gt;= 1.0 (interpolator path), 0 = rate &lt; 1.0 (decimator path) for [**dp\_resamp\_execute()**](resamp__core_8h.md#function-dp_resamp_execute). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/resamp/resamp_core.h`

