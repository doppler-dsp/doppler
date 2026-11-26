

# Struct dp\_timing\_est\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_timing\_est\_state\_t**](structdp__timing__est__state__t.md)



_SymbolTimingEstimator state._ [More...](#detailed-description)

* `#include <timing_est_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**blk\_first**](#variable-blk_first)  <br> |
|  uint64\_t | [**blk\_n**](#variable-blk_n)  <br> |
|  double | [**c\_im**](#variable-c_im)  <br> |
|  double | [**c\_re**](#variable-c_re)  <br> |
|  double | [**clock\_sps**](#variable-clock_sps)  <br> |
|  uint64\_t | [**count**](#variable-count)  <br> |
|  double | [**delay**](#variable-delay)  <br> |
|  int | [**have\_prev**](#variable-have_prev)  <br> |
|  uint32\_t | [**inc**](#variable-inc)  <br> |
|  uint64\_t | [**lead**](#variable-lead)  <br> |
|  uint64\_t | [**lead\_left**](#variable-lead_left)  <br> |
|  [**dp\_RateConverter\_state\_t**](structdp__RateConverter__state__t.md) \* | [**mf**](#variable-mf)  <br> |
|  uint32\_t | [**phase**](#variable-phase)  <br> |
|  double | [**prev\_mid**](#variable-prev_mid)  <br> |
|  double | [**prev\_tau**](#variable-prev_tau)  <br> |
|  double | [**pwr**](#variable-pwr)  <br> |
|  double | [**sps**](#variable-sps)  <br> |












































## Detailed Description


Allocate with [**dp\_timing\_est\_create()**](timing__est__core_8h.md#function-dp_timing_est_create) or [**dp\_timing\_est\_create\_matched()**](timing__est__core_8h.md#function-dp_timing_est_create_matched). `sps`, `clock_sps`, `inc`, `lead`, `delay` and the converter's plan are config; the rest is running state and is in the blob. 


    
## Public Attributes Documentation




### variable blk\_first 

```C++
uint64_t dp_timing_est_state_t::blk_first;
```



`count` at the block's first sample. 
 


        

<hr>



### variable blk\_n 

```C++
uint64_t dp_timing_est_state_t::blk_n;
```



Samples in the block so far. 
 


        

<hr>



### variable c\_im 

```C++
double dp_timing_est_state_t::c_im;
```



Imaginary part of the running coefficient. 
 


        

<hr>



### variable c\_re 

```C++
double dp_timing_est_state_t::c_re;
```



Real part of the running coefficient. 
 


        

<hr>



### variable clock\_sps 

```C++
double dp_timing_est_state_t::clock_sps;
```



Samples per symbol the clock runs at: `sps`, or TIMING\_EST\_MATCHED\_SPS on a matched instance, whose clock runs on its converter's output. 
 


        

<hr>



### variable count 

```C++
uint64_t dp_timing_est_state_t::count;
```



Samples taken since create or reset. 
 


        

<hr>



### variable delay 

```C++
double dp_timing_est_state_t::delay;
```



The converter's group delay, in input samples. 0 on a plain instance. 
 


        

<hr>



### variable have\_prev 

```C++
int dp_timing_est_state_t::have_prev;
```



1 once a block with power has finished. 
 


        

<hr>



### variable inc 

```C++
uint32_t dp_timing_est_state_t::inc;
```



The clock's step a sample: the NCO's increment for `1 / clock_sps` cycles a sample. 
 


        

<hr>



### variable lead 

```C++
uint64_t dp_timing_est_state_t::lead;
```



Converter outputs skipped after create or reset: `ceil (memory * rate) + 1`, the converter's own start. 0 on a plain instance. 


        

<hr>



### variable lead\_left 

```C++
uint64_t dp_timing_est_state_t::lead_left;
```



How many of those are still to skip. 
 


        

<hr>



### variable mf 

```C++
dp_RateConverter_state_t* dp_timing_est_state_t::mf;
```



The matched converter, owned; NULL on a plain instance. 
 


        

<hr>



### variable phase 

```C++
uint32_t dp_timing_est_state_t::phase;
```



The symbol clock: a 32-bit fraction of one symbol, at the next sample. 
 


        

<hr>



### variable prev\_mid 

```C++
double dp_timing_est_state_t::prev_mid;
```



That block's middle, in symbols of the clock since create or reset. 
 


        

<hr>



### variable prev\_tau 

```C++
double dp_timing_est_state_t::prev_tau;
```



That block's `tau`. 
 


        

<hr>



### variable pwr 

```C++
double dp_timing_est_state_t::pwr;
```



Running sum of `|x|^2`. 
 


        

<hr>



### variable sps 

```C++
double dp_timing_est_state_t::sps;
```



Samples per symbol of the stream handed to [**dp\_timing\_est\_estimate()**](timing__est__core_8h.md#function-dp_timing_est_estimate). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/timing_est/timing_est_core.h`

