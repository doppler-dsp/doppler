

# Struct dp\_doppler\_channel\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md)



_DopplerChannel state._ [More...](#detailed-description)

* `#include <doppler_channel_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**carrier\_hz**](#variable-carrier_hz)  <br> |
|  size\_t | [**consumed**](#variable-consumed)  <br> |
|  double \* | [**ctrl**](#variable-ctrl)  <br> |
|  size\_t | [**ctrl\_cap**](#variable-ctrl_cap)  <br> |
|  double | [**doppler\_ppm**](#variable-doppler_ppm)  <br> |
|  double | [**doppler\_rate\_ppm\_s**](#variable-doppler_rate_ppm_s)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  uint64\_t | [**n\_in**](#variable-n_in)  <br> |
|  uint64\_t | [**n\_out**](#variable-n_out)  <br> |
|  double \* | [**pos**](#variable-pos)  <br> |
|  size\_t | [**pos\_cap**](#variable-pos_cap)  <br> |
|  double | [**prof\_d**](#variable-prof_d)  <br> |
|  uint8\_t | [**profiled**](#variable-profiled)  <br> |
|  [**resamp\_state\_t**](structresamp__state__t.md) \* | [**rs**](#variable-rs)  <br> |












































## Detailed Description


Allocate with [**dp\_doppler\_channel\_create()**](doppler__channel__core_8h.md#function-dp_doppler_channel_create). 


    
## Public Attributes Documentation




### variable carrier\_hz 

```C++
double dp_doppler_channel_state_t::carrier_hz;
```



RF carrier fc, Hz — drives the offset (config; 0 = clocks only) 
 


        

<hr>



### variable consumed 

```C++
size_t dp_doppler_channel_state_t::consumed;
```



input samples the last execute() or execute\_profile() took (per-call, not state) 
 


        

<hr>



### variable ctrl 

```C++
double* dp_doppler_channel_state_t::ctrl;
```



owned per-sample rate deviation scratch (not state) 
 


        

<hr>



### variable ctrl\_cap 

```C++
size_t dp_doppler_channel_state_t::ctrl_cap;
```



length of `ctrl`, samples (DOPPLER\_CHANNEL\_MAX\_BLOCK) 
 


        

<hr>



### variable doppler\_ppm 

```C++
double dp_doppler_channel_state_t::doppler_ppm;
```



d0, ppm of nominal (config) 
 


        

<hr>



### variable doppler\_rate\_ppm\_s 

```C++
double dp_doppler_channel_state_t::doppler_rate_ppm_s;
```



d-dot, ppm/s (config) 
 


        

<hr>



### variable fs 

```C++
double dp_doppler_channel_state_t::fs;
```



receive sample rate, Hz (config) 
 


        

<hr>



### variable n\_in 

```C++
uint64_t dp_doppler_channel_state_t::n_in;
```



running: input samples consumed 
 


        

<hr>



### variable n\_out 

```C++
uint64_t dp_doppler_channel_state_t::n_out;
```



running: output samples produced 
 


        

<hr>



### variable pos 

```C++
double* dp_doppler_channel_state_t::pos;
```



owned per-output input position scratch (not state); grown on demand 
 


        

<hr>



### variable pos\_cap 

```C++
size_t dp_doppler_channel_state_t::pos_cap;
```



length of `pos`, samples 
 


        

<hr>



### variable prof\_d 

```C++
double dp_doppler_channel_state_t::prof_d;
```



running: most recent instantaneous d from a profile (dimensionless, not ppm) 
 


        

<hr>



### variable profiled 

```C++
uint8_t dp_doppler_channel_state_t::profiled;
```



running: set by an execute\_profile() call that consumes input; cleared by a non-empty execute() or reset 


        

<hr>



### variable rs 

```C++
resamp_state_t* dp_doppler_channel_state_t::rs;
```



owned resampler doing the dilation — never hand-rolled here 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/doppler_channel/doppler_channel_core.h`

