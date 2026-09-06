

# Struct async\_dsss\_pool\_row\_t



[**ClassList**](annotated.md) **>** [**async\_dsss\_pool\_row\_t**](structasync__dsss__pool__row__t.md)



[More...](#detailed-description)

* `#include <async_dsss_pool_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**assigned**](#variable-assigned)  <br> |
|  double | [**chip\_phase**](#variable-chip_phase)  <br> |
|  double | [**doppler\_hz**](#variable-doppler_hz)  <br> |
|  int | [**prev\_code**](#variable-prev_code)  <br> |
|  int | [**prev\_state**](#variable-prev_state)  <br> |
|  int | [**prev\_sym**](#variable-prev_sym)  <br> |
|  double | [**seed\_chip\_phase**](#variable-seed_chip_phase)  <br> |
|  double | [**seed\_cn0\_dbhz**](#variable-seed_cn0_dbhz)  <br> |
|  double | [**seed\_doppler\_hz**](#variable-seed_doppler_hz)  <br> |
|  uint64\_t | [**seed\_sample**](#variable-seed_sample)  <br> |












































## Detailed Description


One row of the assigned table. Plain data: it is the blob's payload. 


    
## Public Attributes Documentation




### variable assigned 

```C++
int async_dsss_pool_row_t::assigned;
```



1 while the slot holds an emitter. 
 


        

<hr>



### variable chip\_phase 

```C++
double async_dsss_pool_row_t::chip_phase;
```




<hr>



### variable doppler\_hz 

```C++
double async_dsss_pool_row_t::doppler_hz;
```



The row's current coordinates: the exclusion zone is keyed on these, refreshed every push. 


        

<hr>



### variable prev\_code 

```C++
int async_dsss_pool_row_t::prev_code;
```



Its lock flags at the last push. 
 


        

<hr>



### variable prev\_state 

```C++
int async_dsss_pool_row_t::prev_state;
```



The receiver's state at the last push, for the transitions' edges. 
 


        

<hr>



### variable prev\_sym 

```C++
int async_dsss_pool_row_t::prev_sym;
```




<hr>



### variable seed\_chip\_phase 

```C++
double async_dsss_pool_row_t::seed_chip_phase;
```



The hand-off record, verbatim. 
 


        

<hr>



### variable seed\_cn0\_dbhz 

```C++
double async_dsss_pool_row_t::seed_cn0_dbhz;
```




<hr>



### variable seed\_doppler\_hz 

```C++
double async_dsss_pool_row_t::seed_doppler_hz;
```




<hr>



### variable seed\_sample 

```C++
uint64_t async_dsss_pool_row_t::seed_sample;
```



Stream position of the assignment. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/async_dsss_pool/async_dsss_pool_core.h`

