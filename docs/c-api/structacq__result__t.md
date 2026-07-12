

# Struct acq\_result\_t



[**ClassList**](annotated.md) **>** [**acq\_result\_t**](structacq__result__t.md)



_One acquisition detection event._ 

* `#include <acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**code\_phase**](#variable-code_phase)  <br> |
|  size\_t | [**doppler\_bin**](#variable-doppler_bin)  <br> |
|  float | [**noise\_est**](#variable-noise_est)  <br> |
|  float | [**peak\_mag**](#variable-peak_mag)  <br> |
|  float | [**snr\_est**](#variable-snr_est)  <br> |
|  float | [**test\_stat**](#variable-test_stat)  <br> |












































## Public Attributes Documentation




### variable code\_phase 

```C++
size_t acq_result_t::code_phase;
```



Peak col: code phase (0 … code\_bins-1). 


        

<hr>



### variable doppler\_bin 

```C++
size_t acq_result_t::doppler_bin;
```



Peak row: Doppler bin (0 … doppler\_bins-1). 


        

<hr>



### variable noise\_est 

```C++
float acq_result_t::noise_est;
```



CFAR noise estimate over `[noise_lo, noise_hi]`. 


        

<hr>



### variable peak\_mag 

```C++
float acq_result_t::peak_mag;
```



max `|R[i,j]|` over the surface (linear). 


        

<hr>



### variable snr\_est 

```C++
float acq_result_t::snr_est;
```



Estimated per-sample amplitude SNR of the burst. 


        

<hr>



### variable test\_stat 

```C++
float acq_result_t::test_stat;
```



peak\_mag / noise\_est; 0 if noise\_est == 0. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acq/acq_core.h`

