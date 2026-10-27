

# Struct dp\_fir\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_fir\_state\_t**](structdp__fir__state__t.md)



_Direct-form FIR filter state (opaque to callers; use the API)._ [More...](#detailed-description)

* `#include <fir_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float \_Complex \* | [**delay**](#variable-delay)  <br> |
|  size\_t | [**num\_taps**](#variable-num_taps)  <br> |
|  float \* | [**rtaps**](#variable-rtaps)  <br> |
|  float \_Complex \* | [**scratch**](#variable-scratch)  <br> |
|  size\_t | [**scratch\_cap**](#variable-scratch_cap)  <br> |
|  float \_Complex \* | [**taps**](#variable-taps)  <br> |












































## Detailed Description


Exactly one of `taps` / `rtaps` is non-NULL, chosen by the constructor. Config (taps, num\_taps) is fixed at create; the delay line is the only running state; the scratch buffer is a reusable workspace. 


    
## Public Attributes Documentation




### variable delay 

```C++
float _Complex* dp_fir_state_t::delay;
```



Delay line: the last num\_taps - 1 inputs, oldest first. Running state, written by every execute/fir\_step call and zeroed by [**dp\_fir\_reset()**](fir__core_8h.md#function-dp_fir_reset); NULL when num\_taps == 1. 


        

<hr>



### variable num\_taps 

```C++
size_t dp_fir_state_t::num_taps;
```



Filter length in taps (&gt;= 1). Config, fixed at create. 


        

<hr>



### variable rtaps 

```C++
float* dp_fir_state_t::rtaps;
```



Real tap copy, num\_taps long; NULL for a complex-tap filter. Config, set by [**dp\_fir\_create\_real()**](fir__core_8h.md#function-dp_fir_create_real). 


        

<hr>



### variable scratch 

```C++
float _Complex* dp_fir_state_t::scratch;
```



Workspace laid out `[delay | input | zero pad]`; allocated on the first [**dp\_fir\_execute()**](fir__core_8h.md#function-dp_fir_execute) and grown (never shrunk) as blocks grow. 


        

<hr>



### variable scratch\_cap 

```C++
size_t dp_fir_state_t::scratch_cap;
```



Allocated length of `scratch` in complex samples (0 until the first [**dp\_fir\_execute()**](fir__core_8h.md#function-dp_fir_execute)). 


        

<hr>



### variable taps 

```C++
float _Complex* dp_fir_state_t::taps;
```



Complex tap copy, num\_taps long; NULL for a real-tap filter. Config, set by [**dp\_fir\_create()**](fir__core_8h.md#function-dp_fir_create). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/fir/fir_core.h`

