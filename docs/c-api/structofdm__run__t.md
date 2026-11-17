

# Struct ofdm\_run\_t



[**ClassList**](annotated.md) **>** [**ofdm\_run\_t**](structofdm__run__t.md)



_A run of OFDM symbols of one class._ 

* `#include <ofdm_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**class\_index**](#variable-class_index)  <br> |
|  uint32\_t | [**count**](#variable-count)  <br> |
|  uint32\_t | [**cyclic\_prefix**](#variable-cyclic_prefix)  <br> |
|  uint32\_t | [**first\_phase**](#variable-first_phase)  <br> |
|  uint32\_t | [**polarity\_offset**](#variable-polarity_offset)  <br> |












































## Public Attributes Documentation




### variable class\_index 

```C++
uint32_t ofdm_run_t::class_index;
```



Index into the layout's classes. 
 


        

<hr>



### variable count 

```C++
uint32_t ofdm_run_t::count;
```



OFDM symbols in the run. 
 


        

<hr>



### variable cyclic\_prefix 

```C++
uint32_t ofdm_run_t::cyclic_prefix;
```



Override, or OFDM\_CP\_DEFAULT. 
 


        

<hr>



### variable first\_phase 

```C++
uint32_t ofdm_run_t::first_phase;
```



Pattern phase of the first symbol. 
 


        

<hr>



### variable polarity\_offset 

```C++
uint32_t ofdm_run_t::polarity_offset;
```



Polarity element of the first symbol. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ofdm/ofdm_core.h`

