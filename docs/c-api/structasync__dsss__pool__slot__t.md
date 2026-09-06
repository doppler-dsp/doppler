

# Struct async\_dsss\_pool\_slot\_t



[**ClassList**](annotated.md) **>** [**async\_dsss\_pool\_slot\_t**](structasync__dsss__pool__slot__t.md)



_One slot's picture, by value_  _what_`status()` _returns._[More...](#detailed-description)

* `#include <async_dsss_pool_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**assigned**](#variable-assigned)  <br> |
|  uint64\_t | [**assigned\_samples**](#variable-assigned_samples)  <br> |
|  uint64\_t | [**both\_down\_samples**](#variable-both_down_samples)  <br> |
|  double | [**chip\_phase**](#variable-chip_phase)  <br> |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  int | [**code\_locked**](#variable-code_locked)  <br> |
|  double | [**code\_rate**](#variable-code_rate)  <br> |
|  double | [**doppler\_hz**](#variable-doppler_hz)  <br> |
|  double | [**lock\_metric**](#variable-lock_metric)  <br> |
|  int | [**locked**](#variable-locked)  <br> |
|  double | [**seed\_chip\_phase**](#variable-seed_chip_phase)  <br> |
|  double | [**seed\_cn0\_dbhz**](#variable-seed_cn0_dbhz)  <br> |
|  double | [**seed\_doppler\_hz**](#variable-seed_doppler_hz)  <br> |
|  uint64\_t | [**seed\_sample**](#variable-seed_sample)  <br> |
|  size\_t | [**slot**](#variable-slot)  <br> |
|  int | [**state**](#variable-state)  <br> |
|  uint64\_t | [**state\_samples**](#variable-state_samples)  <br> |












































## Detailed Description


The seed fields are the searcher's hand-off record, verbatim, at the sample the row was assigned; the live fields are the receiver's own status record ([**async\_dsss\_receiver\_status\_t**](structasync__dsss__receiver__status__t.md)) at the last push. For an unassigned slot the live fields are the idle receiver's (state ASYNC\_DSSS\_RX\_IDLE); for a slot outside `[0, n_slots)` the record is zero with `state` -1. 


    
## Public Attributes Documentation




### variable assigned 

```C++
int async_dsss_pool_slot_t::assigned;
```



1 while a receiver holds an emitter. 
 


        

<hr>



### variable assigned\_samples 

```C++
uint64_t async_dsss_pool_slot_t::assigned_samples;
```



Samples since the row was assigned. 
 


        

<hr>



### variable both\_down\_samples 

```C++
uint64_t async_dsss_pool_slot_t::both_down_samples;
```



The release clock, samples. 
 


        

<hr>



### variable chip\_phase 

```C++
double async_dsss_pool_slot_t::chip_phase;
```



Live Dll code phase, chips. 
 


        

<hr>



### variable cn0\_dbhz\_est 

```C++
double async_dsss_pool_slot_t::cn0_dbhz_est;
```



C/N0 estimate, dB-Hz. 
 


        

<hr>



### variable code\_locked 

```C++
int async_dsss_pool_slot_t::code_locked;
```



Presence flag. 
 


        

<hr>



### variable code\_rate 

```C++
double async_dsss_pool_slot_t::code_rate;
```



Live Dll code rate, chips per sample. 
 


        

<hr>



### variable doppler\_hz 

```C++
double async_dsss_pool_slot_t::doppler_hz;
```



Where the emitter is now, Hz. 
 


        

<hr>



### variable lock\_metric 

```C++
double async_dsss_pool_slot_t::lock_metric;
```



The symbol-lock statistic. 
 


        

<hr>



### variable locked 

```C++
int async_dsss_pool_slot_t::locked;
```



Health flag (symbol lock). 
 


        

<hr>



### variable seed\_chip\_phase 

```C++
double async_dsss_pool_slot_t::seed_chip_phase;
```



The seed's code phase, chips. 
 


        

<hr>



### variable seed\_cn0\_dbhz 

```C++
double async_dsss_pool_slot_t::seed_cn0_dbhz;
```



The seed's C/N0 estimate, dB-Hz. 
 


        

<hr>



### variable seed\_doppler\_hz 

```C++
double async_dsss_pool_slot_t::seed_doppler_hz;
```



The seed's Doppler, Hz. 
 


        

<hr>



### variable seed\_sample 

```C++
uint64_t async_dsss_pool_slot_t::seed_sample;
```



Stream position the row was assigned at. 
 


        

<hr>



### variable slot 

```C++
size_t async_dsss_pool_slot_t::slot;
```



The slot asked for. 
 


        

<hr>



### variable state 

```C++
int async_dsss_pool_slot_t::state;
```



The receiver's ASYNC\_DSSS\_RX\_\* state; -1 for a slot that does not exist. 
 


        

<hr>



### variable state\_samples 

```C++
uint64_t async_dsss_pool_slot_t::state_samples;
```



Samples since the receiver's state was entered. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/async_dsss_pool/async_dsss_pool_core.h`

