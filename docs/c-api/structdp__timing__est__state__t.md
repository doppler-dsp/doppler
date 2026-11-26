

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
|  uint64\_t | [**count**](#variable-count)  <br> |
|  int | [**have\_prev**](#variable-have_prev)  <br> |
|  uint32\_t | [**inc**](#variable-inc)  <br> |
|  uint32\_t | [**phase**](#variable-phase)  <br> |
|  double | [**prev\_mid**](#variable-prev_mid)  <br> |
|  double | [**prev\_tau**](#variable-prev_tau)  <br> |
|  double | [**pwr**](#variable-pwr)  <br> |
|  double | [**sps**](#variable-sps)  <br> |












































## Detailed Description


Allocate with [**dp\_timing\_est\_create()**](timing__est__core_8h.md#function-dp_timing_est_create). `sps` and `inc` are config; the rest is running state and is in the blob. 


    
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



### variable count 

```C++
uint64_t dp_timing_est_state_t::count;
```



Samples taken since create or reset. 
 


        

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



The clock's step a sample: the NCO's increment for `1 / sps` cycles a sample. 
 


        

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



Samples per symbol the clock runs at. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/timing_est/timing_est_core.h`

