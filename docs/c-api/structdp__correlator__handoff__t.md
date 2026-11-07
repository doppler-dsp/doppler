

# Struct dp\_correlator\_handoff\_t



[**ClassList**](annotated.md) **>** [**dp\_correlator\_handoff\_t**](structdp__correlator__handoff__t.md)



_Wire-ready hand-off record built from one_ [_**dp\_correlator\_hit\_t**_](structdp__correlator__hit__t.md) _hit._[More...](#detailed-description)

* `#include <correlator_core.h>`





















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


Every field is in a physical unit and the record is flat and pointer-free, so it can cross a thread or process boundary. It carries no timestamp: whatever feeds the engine samples owns the clock and stamps the record at its `samples_consumed`. The fields are tabulated in docs/design/async-dsss-receiver.md §2.2. 


    
## Public Attributes Documentation




### variable chip\_phase 

```C++
double dp_correlator_handoff_t::chip_phase;
```



Chips, Dll's own instantaneous-phase convention (the mirror image of dp\_correlator\_hit\_t::code\_phase's correlation-lag convention  see [**dp\_correlator\_build\_handoff()**](correlator__core_8h.md#function-dp_correlator_build_handoff)'s doc comment). 


        

<hr>



### variable cn0\_dbhz\_est 

```C++
double dp_correlator_handoff_t::cn0_dbhz_est;
```



Estimated carrier-to-noise density, dB-Hz. 


        

<hr>



### variable doppler\_hz\_est 

```C++
double dp_correlator_handoff_t::doppler_hz_est;
```



Folded/signed Doppler estimate, Hz. 


        

<hr>



### variable doppler\_res\_hz 

```C++
double dp_correlator_handoff_t::doppler_res_hz;
```



Width of the estimate (+/- half this). 


        

<hr>



### variable noise\_est 

```C++
float dp_correlator_handoff_t::noise_est;
```



Raw CFAR noise-floor estimate, diagnostic. 


        

<hr>



### variable peak\_mag 

```C++
float dp_correlator_handoff_t::peak_mag;
```



Raw CFAR peak magnitude, diagnostic. 


        

<hr>



### variable samples\_consumed 

```C++
uint64_t dp_correlator_handoff_t::samples_consumed;
```



Raw-sample offset this hit ended at. 


        

<hr>



### variable test\_stat 

```C++
float dp_correlator_handoff_t::test_stat;
```



Raw CFAR gating statistic, diagnostic. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/correlator/correlator_core.h`

