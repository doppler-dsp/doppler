

# Struct detector\_state\_t



[**ClassList**](annotated.md) **>** [**detector\_state\_t**](structdetector__state__t.md)



_1-D signal detector state._ [More...](#detailed-description)

* `#include <detector_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**\_last\_corr\_valid**](#variable-_last_corr_valid)  <br> |
|  [**corr\_state\_t**](structcorr__state__t.md) \* | [**corr**](#variable-corr)  <br> |
|  float \* | [**mag\_buf**](#variable-mag_buf)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  float | [**noise\_est**](#variable-noise_est)  <br> |
|  size\_t | [**noise\_hi**](#variable-noise_hi)  <br> |
|  size\_t | [**noise\_lo**](#variable-noise_lo)  <br> |
|  [**det\_noise\_mode\_t**](detector__core_8h.md#enum-det_noise_mode_t) | [**noise\_mode**](#variable-noise_mode)  <br> |
|  float \* | [**noise\_scratch**](#variable-noise_scratch)  <br> |
|  float complex \* | [**out\_buf**](#variable-out_buf)  <br> |
|  size\_t | [**peak\_lag**](#variable-peak_lag)  <br> |
|  float | [**peak\_mag**](#variable-peak_mag)  <br> |
|  dp\_f32\_t \* | [**ring**](#variable-ring)  <br> |
|  size\_t | [**ring\_cap**](#variable-ring_cap)  <br> |
|  float | [**test\_stat**](#variable-test_stat)  <br> |
|  float | [**threshold**](#variable-threshold)  <br> |












































## Detailed Description


Allocate with [**detector\_create()**](detector__core_8h.md#function-detector_create); never stack-allocate. 


    
## Public Attributes Documentation




### variable \_last\_corr\_valid 

```C++
int detector_state_t::_last_corr_valid;
```



1 after the first dump, else 0. 


        

<hr>



### variable corr 

```C++
corr_state_t* detector_state_t::corr;
```



FFT correlator + int-dump engine. 


        

<hr>



### variable mag\_buf 

```C++
float* detector_state_t::mag_buf;
```



\|out\_buf&#91;k&#93;\|, n floats. 


        

<hr>



### variable n 

```C++
size_t detector_state_t::n;
```



Frame / FFT length in complex samples. 


        

<hr>



### variable noise\_est 

```C++
float detector_state_t::noise_est;
```




<hr>



### variable noise\_hi 

```C++
size_t detector_state_t::noise_hi;
```



Noise bin range upper bound (inclusive). 


        

<hr>



### variable noise\_lo 

```C++
size_t detector_state_t::noise_lo;
```



Noise bin range lower bound (inclusive). 


        

<hr>



### variable noise\_mode 

```C++
det_noise_mode_t detector_state_t::noise_mode;
```




<hr>



### variable noise\_scratch 

```C++
float* detector_state_t::noise_scratch;
```



Scratch for median sort. 


        

<hr>



### variable out\_buf 

```C++
float complex* detector_state_t::out_buf;
```



Corr output buffer (n complex samples). 


        

<hr>



### variable peak\_lag 

```C++
size_t detector_state_t::peak_lag;
```




<hr>



### variable peak\_mag 

```C++
float detector_state_t::peak_mag;
```




<hr>



### variable ring 

```C++
dp_f32_t* detector_state_t::ring;
```



Double-mapped ring buffer (auto-sized). 


        

<hr>



### variable ring\_cap 

```C++
size_t detector_state_t::ring_cap;
```



Ring buffer capacity in complex samples. 


        

<hr>



### variable test\_stat 

```C++
float detector_state_t::test_stat;
```




<hr>



### variable threshold 

```C++
float detector_state_t::threshold;
```



0 = always fire; &gt;0 = gate on test\_stat. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/detector/detector_core.h`

