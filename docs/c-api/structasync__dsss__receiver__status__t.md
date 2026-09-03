

# Struct async\_dsss\_receiver\_status\_t



[**ClassList**](annotated.md) **>** [**async\_dsss\_receiver\_status\_t**](structasync__dsss__receiver__status__t.md)



_One consistent picture of what the receiver is doing, by value._ [More...](#detailed-description)

* `#include <async_dsss_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**both\_down\_samples**](#variable-both_down_samples)  <br> |
|  double | [**car\_last\_error**](#variable-car_last_error)  <br> |
|  double | [**chip\_phase**](#variable-chip_phase)  <br> |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  int | [**code\_locked**](#variable-code_locked)  <br> |
|  double | [**code\_rate**](#variable-code_rate)  <br> |
|  double | [**doppler\_hz**](#variable-doppler_hz)  <br> |
|  double | [**lock\_metric**](#variable-lock_metric)  <br> |
|  double | [**lock\_threshold**](#variable-lock_threshold)  <br> |
|  int | [**locked**](#variable-locked)  <br> |
|  double | [**mpsk\_last\_error**](#variable-mpsk_last_error)  <br> |
|  int | [**state**](#variable-state)  <br> |
|  uint64\_t | [**state\_samples**](#variable-state_samples)  <br> |












































## Detailed Description


The status record of docs/design/async-dsss-receiver.md section 11.3: the holder of a pool reads it on demand  once per data-free window at least, because the searcher's exclusion zone is keyed on the receiver's CURRENT estimate, not on its seed  and gets every field from one call, so a reader on another thread never assembles a picture across a `steps()` from the one-at-a-time getters (which remain the same fields' other face). It is a read of live state, not `get_state()`: the bytes triplet resumes the receiver elsewhere, this describes it here. No timestamp: the counters are in input samples, and the holder, which owns the sample clock, stamps them (section 2.2's rule). 


    
## Public Attributes Documentation




### variable both\_down\_samples 

```C++
uint64_t async_dsss_receiver_status_t::both_down_samples;
```



Input samples both flags have been down without a break (the release clock); in lost it keeps counting  samples since the flags dropped. 
 


        

<hr>



### variable car\_last\_error 

```C++
double async_dsss_receiver_status_t::car_last_error;
```



Pre-despread Costas residual, rad. 
 


        

<hr>



### variable chip\_phase 

```C++
double async_dsss_receiver_status_t::chip_phase;
```



Live Dll code phase, chips. 
 


        

<hr>



### variable cn0\_dbhz\_est 

```C++
double async_dsss_receiver_status_t::cn0_dbhz_est;
```



C/N0 estimate, dB-Hz (the hit's). 
 


        

<hr>



### variable code\_locked 

```C++
int async_dsss_receiver_status_t::code_locked;
```



Presence flag: the Dll's lock detector. 
 


        

<hr>



### variable code\_rate 

```C++
double async_dsss_receiver_status_t::code_rate;
```



Live Dll code rate, chips/sample. 
 


        

<hr>



### variable doppler\_hz 

```C++
double async_dsss_receiver_status_t::doppler_hz;
```



Where the emitter is NOW: the live carrier loop's estimate, Hz (the seed while refining, 0 when idle, frozen where it was when lost). 


        

<hr>



### variable lock\_metric 

```C++
double async_dsss_receiver_status_t::lock_metric;
```



cos(2\*phi) over the symbols, drives `locked`. 
 


        

<hr>



### variable lock\_threshold 

```C++
double async_dsss_receiver_status_t::lock_threshold;
```



`locked` latches above this. 
 


        

<hr>



### variable locked 

```C++
int async_dsss_receiver_status_t::locked;
```



Health flag: the symbol-lock detector. 
 


        

<hr>



### variable mpsk\_last\_error 

```C++
double async_dsss_receiver_status_t::mpsk_last_error;
```



Post-despread carrier residual, rad. 
 


        

<hr>



### variable state 

```C++
int async_dsss_receiver_status_t::state;
```



ASYNC\_DSSS\_RX\_SEARCHING .. \_LOST  where it is. 
 


        

<hr>



### variable state\_samples 

```C++
uint64_t async_dsss_receiver_status_t::state_samples;
```



Input samples since `state` was entered. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/async_dsss_receiver/async_dsss_receiver_core.h`

