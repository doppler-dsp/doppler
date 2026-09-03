

# Struct carrier\_acq\_state\_t



[**ClassList**](annotated.md) **>** [**carrier\_acq\_state\_t**](structcarrier__acq__state__t.md)



_CarrierAcquisition state._ [More...](#detailed-description)

* `#include <carrier_acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float \_Complex \* | [**carry\_buf**](#variable-carry_buf)  <br> |
|  size\_t | [**carry\_len**](#variable-carry_len)  <br> |
|  [**detector\_state\_t**](structdetector__state__t.md) \* | [**det**](#variable-det)  <br> |
|  size\_t | [**dwell\_target**](#variable-dwell_target)  <br> |
|  size\_t | [**max\_n\_blocks**](#variable-max_n_blocks)  <br> |
|  size\_t | [**n\_blocks**](#variable-n_blocks)  <br> |
|  size\_t | [**nfft**](#variable-nfft)  <br> |
|  double | [**pfa**](#variable-pfa)  <br> |
|  float \_Complex \* | [**power\_buf**](#variable-power_buf)  <br> |
|  [**psd\_state\_t**](structpsd__state__t.md) \* | [**psd**](#variable-psd)  <br> |
|  float \* | [**pwr\_buf**](#variable-pwr_buf)  <br> |
|  bool | [**ready**](#variable-ready)  <br> |
|  double | [**residual\_hz**](#variable-residual_hz)  <br> |
|  double | [**s\_t**](#variable-s_t)  <br> |
|  double | [**s\_t2**](#variable-s_t2)  <br> |
|  double | [**sample\_rate\_hz**](#variable-sample_rate_hz)  <br> |
|  bool | [**sequential**](#variable-sequential)  <br> |












































## Detailed Description


Allocate with [**carrier\_acq\_create()**](carrier__acq__core_8h.md#function-carrier_acq_create). 


    
## Public Attributes Documentation




### variable carry\_buf 

```C++
float _Complex* carrier_acq_state_t::carry_buf;
```



Raw-input carry, capacity psd-&gt;n. 
 


        

<hr>



### variable carry\_len 

```C++
size_t carrier_acq_state_t::carry_len;
```



Valid samples in carry\_buf, 0..n-1. 


        

<hr>



### variable det 

```C++
detector_state_t* carrier_acq_state_t::det;
```



Correlate averaged power vs. template. 
 


        

<hr>



### variable dwell\_target 

```C++
size_t carrier_acq_state_t::dwell_target;
```



Non-sequential mode's fixed wait count. 
 


        

<hr>



### variable max\_n\_blocks 

```C++
size_t carrier_acq_state_t::max_n_blocks;
```



Sequential mode's OWN give-up cap  \* deliberately independent of dwell\_target \* (see [**carrier\_acq\_create()**](carrier__acq__core_8h.md#function-carrier_acq_create)'s own doc). 
 


        

<hr>



### variable n\_blocks 

```C++
size_t carrier_acq_state_t::n_blocks;
```




<hr>



### variable nfft 

```C++
size_t carrier_acq_state_t::nfft;
```




<hr>



### variable pfa 

```C++
double carrier_acq_state_t::pfa;
```




<hr>



### variable power\_buf 

```C++
float _Complex* carrier_acq_state_t::power_buf;
```



pwr\_buf packed complex (imag=0). 
 


        

<hr>



### variable psd 

```C++
psd_state_t* carrier_acq_state_t::psd;
```



FFT + window + non-coherent power avg. 
 


        

<hr>



### variable pwr\_buf 

```C++
float* carrier_acq_state_t::pwr_buf;
```



[**psd\_power\_twosided()**](psd__core_8h.md#function-psd_power_twosided) output, nfft. 
 


        

<hr>



### variable ready 

```C++
bool carrier_acq_state_t::ready;
```




<hr>



### variable residual\_hz 

```C++
double carrier_acq_state_t::residual_hz;
```




<hr>



### variable s\_t 

```C++
double carrier_acq_state_t::s_t;
```



sum(template)  ratio-threshold calibration. 
 


        

<hr>



### variable s\_t2 

```C++
double carrier_acq_state_t::s_t2;
```



sum(template^2)  ratio-threshold calibration. 


        

<hr>



### variable sample\_rate\_hz 

```C++
double carrier_acq_state_t::sample_rate_hz;
```




<hr>



### variable sequential 

```C++
bool carrier_acq_state_t::sequential;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/carrier_acq/carrier_acq_core.h`

