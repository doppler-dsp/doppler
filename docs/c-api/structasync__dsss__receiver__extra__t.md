

# Struct async\_dsss\_receiver\_extra\_t



[**ClassList**](annotated.md) **>** [**async\_dsss\_receiver\_extra\_t**](structasync__dsss__receiver__extra__t.md)





* `#include <async_dsss_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**\_pad**](#variable-_pad)  <br> |
|  uint64\_t | [**both\_down\_samples**](#variable-both_down_samples)  <br> |
|  uint64\_t | [**car\_carry\_len**](#variable-car_carry_len)  <br> |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  double | [**doppler\_hz\_est**](#variable-doppler_hz_est)  <br> |
|  uint8\_t | [**handoff**](#variable-handoff)  <br> |
|  double | [**lock\_den**](#variable-lock_den)  <br> |
|  double | [**lock\_metric**](#variable-lock_metric)  <br> |
|  double | [**lock\_num**](#variable-lock_num)  <br> |
|  uint64\_t | [**n**](#variable-n)  <br> |
|  uint64\_t | [**refine\_samples\_fed**](#variable-refine_samples_fed)  <br> |
|  uint64\_t | [**refine\_segments**](#variable-refine_segments)  <br> |
|  double | [**seed\_chip\_phase**](#variable-seed_chip_phase)  <br> |
|  double | [**seed\_doppler\_hz\_est**](#variable-seed_doppler_hz_est)  <br> |
|  uint64\_t | [**segments**](#variable-segments)  <br> |
|  uint64\_t | [**sps**](#variable-sps)  <br> |
|  uint8\_t | [**state**](#variable-state)  <br> |
|  uint64\_t | [**state\_samples**](#variable-state_samples)  <br> |
|  [**lockdet\_state\_t**](structlockdet__state__t.md) | [**sym\_lockdet**](#variable-sym_lockdet)  <br> |












































## Public Attributes Documentation




### variable \_pad 

```C++
uint8_t async_dsss_receiver_extra_t::_pad[6];
```




<hr>



### variable both\_down\_samples 

```C++
uint64_t async_dsss_receiver_extra_t::both_down_samples;
```




<hr>



### variable car\_carry\_len 

```C++
uint64_t async_dsss_receiver_extra_t::car_carry_len;
```




<hr>



### variable cn0\_dbhz\_est 

```C++
double async_dsss_receiver_extra_t::cn0_dbhz_est;
```




<hr>



### variable doppler\_hz\_est 

```C++
double async_dsss_receiver_extra_t::doppler_hz_est;
```




<hr>



### variable handoff 

```C++
uint8_t async_dsss_receiver_extra_t::handoff;
```



1 = no acq child in the blob (hand-off mode); a blob does not travel between the flavors. 
 


        

<hr>



### variable lock\_den 

```C++
double async_dsss_receiver_extra_t::lock_den;
```



running state that survives a checkpoint 
 


        

<hr>



### variable lock\_metric 

```C++
double async_dsss_receiver_extra_t::lock_metric;
```



(config  alpha, thresholds  is 


        

<hr>



### variable lock\_num 

```C++
double async_dsss_receiver_extra_t::lock_num;
```



symbol-lock EMAs + hysteretic detector: the 


        

<hr>



### variable n 

```C++
uint64_t async_dsss_receiver_extra_t::n;
```




<hr>



### variable refine\_samples\_fed 

```C++
uint64_t async_dsss_receiver_extra_t::refine_samples_fed;
```




<hr>



### variable refine\_segments 

```C++
uint64_t async_dsss_receiver_extra_t::refine_segments;
```




<hr>



### variable seed\_chip\_phase 

```C++
double async_dsss_receiver_extra_t::seed_chip_phase;
```




<hr>



### variable seed\_doppler\_hz\_est 

```C++
double async_dsss_receiver_extra_t::seed_doppler_hz_est;
```




<hr>



### variable segments 

```C++
uint64_t async_dsss_receiver_extra_t::segments;
```




<hr>



### variable sps 

```C++
uint64_t async_dsss_receiver_extra_t::sps;
```




<hr>



### variable state 

```C++
uint8_t async_dsss_receiver_extra_t::state;
```




<hr>



### variable state\_samples 

```C++
uint64_t async_dsss_receiver_extra_t::state_samples;
```




<hr>



### variable sym\_lockdet 

```C++
lockdet_state_t async_dsss_receiver_extra_t::sym_lockdet;
```



restored by create()). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/async_dsss_receiver/async_dsss_receiver_core.h`

