

# Struct acq\_result\_t



[**ClassList**](annotated.md) **>** [**acq\_result\_t**](structacq__result__t.md)



_One acquisition detection event._ 

* `#include <acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  size\_t | [**code\_phase**](#variable-code_phase)  <br> |
|  size\_t | [**doppler\_bin**](#variable-doppler_bin)  <br> |
|  float | [**noise\_est**](#variable-noise_est)  <br> |
|  float | [**peak\_mag**](#variable-peak_mag)  <br> |
|  float | [**test\_stat**](#variable-test_stat)  <br> |












































## Public Attributes Documentation




### variable cn0\_dbhz\_est 

```C++
float acq_result_t::cn0_dbhz_est;
```



Estimated carrier-to-noise density (dB-Hz), backed out of test\_stat via the same C/N0 &lt;-&gt; per-sample-amplitude-SNR relationship used to size the engine (see [**acq\_create()**](acq__core_8h.md#function-acq_create)). Tracks the true C/N0 while receiver AWGN dominates the CFAR noise estimate; saturates at the code's own autocorrelation-sidelobe floor once the true C/N0 exceeds what this code/geometry can resolve — a real ceiling, not a fault. 


        

<hr>



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



### variable test\_stat 

```C++
float acq_result_t::test_stat;
```



The gating CFAR statistic: peak\_mag / noise\_est under coherent-only detection (n\_noncoh == 1); scaled by sqrt(2\*n\_noncoh) once non-coherent looks are combined. 0 if noise\_est == 0. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acq/acq_core.h`

