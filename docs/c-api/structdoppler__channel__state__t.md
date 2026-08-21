

# Struct doppler\_channel\_state\_t



[**ClassList**](annotated.md) **>** [**doppler\_channel\_state\_t**](structdoppler__channel__state__t.md)



_DopplerChannel state._ [More...](#detailed-description)

* `#include <doppler_channel_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**carrier\_hz**](#variable-carrier_hz)  <br> |
|  double \* | [**ctrl**](#variable-ctrl)  <br> |
|  size\_t | [**ctrl\_cap**](#variable-ctrl_cap)  <br> |
|  double | [**doppler\_ppm**](#variable-doppler_ppm)  <br> |
|  double | [**doppler\_rate\_ppm\_s**](#variable-doppler_rate_ppm_s)  <br> |
|  double | [**excess\_s**](#variable-excess_s)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  uint64\_t | [**n\_in**](#variable-n_in)  <br> |
|  uint64\_t | [**n\_out**](#variable-n_out)  <br> |
|  double | [**prof\_d**](#variable-prof_d)  <br> |
|  uint8\_t | [**profiled**](#variable-profiled)  <br> |
|  [**resamp\_state\_t**](structresamp__state__t.md) \* | [**rs**](#variable-rs)  <br> |












































## Detailed Description


Allocate with [**doppler\_channel\_create()**](doppler__channel__core_8h.md#function-doppler_channel_create). 


    
## Public Attributes Documentation




### variable carrier\_hz 

```C++
double doppler_channel_state_t::carrier_hz;
```




<hr>



### variable ctrl 

```C++
double* doppler_channel_state_t::ctrl;
```




<hr>



### variable ctrl\_cap 

```C++
size_t doppler_channel_state_t::ctrl_cap;
```




<hr>



### variable doppler\_ppm 

```C++
double doppler_channel_state_t::doppler_ppm;
```




<hr>



### variable doppler\_rate\_ppm\_s 

```C++
double doppler_channel_state_t::doppler_rate_ppm_s;
```




<hr>



### variable excess\_s 

```C++
double doppler_channel_state_t::excess_s;
```




<hr>



### variable fs 

```C++
double doppler_channel_state_t::fs;
```




<hr>



### variable n\_in 

```C++
uint64_t doppler_channel_state_t::n_in;
```




<hr>



### variable n\_out 

```C++
uint64_t doppler_channel_state_t::n_out;
```




<hr>



### variable prof\_d 

```C++
double doppler_channel_state_t::prof_d;
```




<hr>



### variable profiled 

```C++
uint8_t doppler_channel_state_t::profiled;
```




<hr>



### variable rs 

```C++
resamp_state_t* doppler_channel_state_t::rs;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler_channel/doppler_channel_core.h`

