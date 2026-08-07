

# Struct acq\_handoff\_t



[**ClassList**](annotated.md) **>** [**acq\_handoff\_t**](structacq__handoff__t.md)



_Wire-ready hand-off record built from one_ [_**acq\_result\_t**_](structacq__result__t.md) _hit._[More...](#detailed-description)

* `#include <acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**chip\_phase**](#variable-chip_phase)  <br> |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  double | [**doppler\_hz\_est**](#variable-doppler_hz_est)  <br> |
|  double | [**doppler\_res\_hz**](#variable-doppler_res_hz)  <br> |
|  float | [**noise\_est**](#variable-noise_est)  <br> |
|  float | [**peak\_mag**](#variable-peak_mag)  <br> |
|  uint64\_t | [**samples\_consumed**](#variable-samples_consumed)  <br> |
|  float | [**test\_stat**](#variable-test_stat)  <br> |












































## Detailed Description


The C twin of the acquisition hand-off prototype's `DetectionEvent` (minus `timestamp_ns`, which depends on an optional `dp_sample_clock_t*` only that Python layer currently threads through). Fields are named/shaped to match `SPEC.md`'s own `DetectionEvent` table so a future wire encoding needs no renaming. 


    
## Public Attributes Documentation




### variable chip\_phase 

```C++
double acq_handoff_t::chip_phase;
```



Chips, Dll's own instantaneous-phase convention (the mirror image of [**acq\_result\_t::code\_phase**](structacq__result__t.md#variable-code_phase)'s correlation-lag convention  see [**acq\_build\_handoff()**](acq__core_8h.md#function-acq_build_handoff)'s doc comment). 


        

<hr>



### variable cn0\_dbhz\_est 

```C++
double acq_handoff_t::cn0_dbhz_est;
```



Estimated carrier-to-noise density, dB-Hz. 


        

<hr>



### variable doppler\_hz\_est 

```C++
double acq_handoff_t::doppler_hz_est;
```



Folded/signed Doppler estimate, Hz. 


        

<hr>



### variable doppler\_res\_hz 

```C++
double acq_handoff_t::doppler_res_hz;
```



Width of the estimate (+/- half this). 


        

<hr>



### variable noise\_est 

```C++
float acq_handoff_t::noise_est;
```



Raw CFAR noise-floor estimate, diagnostic. 


        

<hr>



### variable peak\_mag 

```C++
float acq_handoff_t::peak_mag;
```



Raw CFAR peak magnitude, diagnostic. 


        

<hr>



### variable samples\_consumed 

```C++
uint64_t acq_handoff_t::samples_consumed;
```



Raw-sample offset this hit ended at. 


        

<hr>



### variable test\_stat 

```C++
float acq_handoff_t::test_stat;
```



Raw CFAR gating statistic, diagnostic. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acq/acq_core.h`

