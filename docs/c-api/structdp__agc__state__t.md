

# Struct dp\_agc\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_agc\_state\_t**](structdp__agc__state__t.md)



_AGC state._ [More...](#detailed-description)

* `#include <agc_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**alpha**](#variable-alpha)  <br> |
|  double | [**clip\_db**](#variable-clip_db)  <br> |
|  float | [**clip\_lin**](#variable-clip_lin)  <br> |
|  size\_t | [**decim**](#variable-decim)  <br> |
|  double | [**g\_last**](#variable-g_last)  <br> |
|  double | [**gain\_db**](#variable-gain_db)  <br> |
|  size\_t | [**gain\_phase**](#variable-gain_phase)  <br> |
|  size\_t | [**gain\_update\_period**](#variable-gain_update_period)  <br> |
|  double | [**loop\_bw**](#variable-loop_bw)  <br> |
|  double | [**p\_avg**](#variable-p_avg)  <br> |
|  double | [**ref\_db**](#variable-ref_db)  <br> |
|  [**agc\_tlm\_t**](structagc__tlm__t.md) | [**tlm**](#variable-tlm)  <br> |












































## Detailed Description


Allocate with [**dp\_agc\_create()**](agc__core_8h.md#function-dp_agc_create). `ref_db`, `loop_bw`, `alpha`, `decim`, `clip_db` and `gain_update_period` are configuration (readable and writable at runtime); `gain_db`, `p_avg`, `g_last`, `gain_phase` and `clip_lin` are the loop's internal memory. 


    
## Public Attributes Documentation




### variable alpha 

```C++
double dp_agc_state_t::alpha;
```



Power-detector EMA coefficient in (0, 1] (config, writable). 
 


        

<hr>



### variable clip\_db 

```C++
double dp_agc_state_t::clip_db;
```



Output square-clip level, dB: each of I and Q is limited to +/- 10^(clip\_db/20) (config, writable; default AGC\_CLIP\_DB\_DEFAULT). 
 


        

<hr>



### variable clip\_lin 

```C++
float dp_agc_state_t::clip_lin;
```



Cached 10^(clip\_db/20), refreshed at each period boundary and at create/reset. 
 


        

<hr>



### variable decim 

```C++
size_t dp_agc_state_t::decim;
```



Envelope decimation: the detector and loop filter run once per chunk of this many samples, &gt;= 1 (typically 8, 16 or 32). 
 


        

<hr>



### variable g\_last 

```C++
double dp_agc_state_t::g_last;
```



Linear gain applied to the latest sample, held across the update period (running state; 1.0 at create/reset). 
 


        

<hr>



### variable gain\_db 

```C++
double dp_agc_state_t::gain_db;
```



Loop-filter integrator: commanded gain, dB (running state; 0 at create/reset). 
 


        

<hr>



### variable gain\_phase 

```C++
size_t dp_agc_state_t::gain_phase;
```



[**dp\_agc\_step()**](agc__core_8h.md#function-dp_agc_step) position in the update period, [0, gain\_update\_period) (running state). 
 


        

<hr>



### variable gain\_update\_period 

```C++
size_t dp_agc_state_t::gain_update_period;
```



[**dp\_agc\_step()**](agc__core_8h.md#function-dp_agc_step) control-update period, samples (config, &gt;= 1): the detector + gain-apply run every sample, but the loop-filter command (the exp10/log10 work) refreshes once per this many samples — a zero-order hold on the gain that amortises the transcendentals on a sample-rate hot loop. 1 (default) is the exact per-sample loop; &gt;1 trades gain-update latency for speed (keep well below 1/(4\*loop\_bw)). The same shape as decim's `4*decim*loop_bw <= 0.05`, but only decim's is measured — this one is a zero-order hold on a different quantity. 


        

<hr>



### variable loop\_bw 

```C++
double dp_agc_state_t::loop_bw;
```



Loop noise bandwidth, cycles/sample (config, writable); filter time constant 1/(4\*loop\_bw). 


        

<hr>



### variable p\_avg 

```C++
double dp_agc_state_t::p_avg;
```



Power-detector EMA of OUTPUT power, linear (running state; seeded to 10^(ref\_db/10) at create/reset). Anything setting this by hand must use the REFERENCE power, not a measured input power: the loop filter's error is (ref\_db - 10log10(p\_avg)), so seeding it with an input measurement hands the loop an error equal to the whole gain and it integrates it. Measured on this object at loop\_bw 0.002 / alpha 0.01, a 4x-hot input drove the gain a further 13.4 dB past its correct value before recovering (+2.4 dB at 0.25x). [**dp\_agc\_create()**](agc__core_8h.md#function-dp_agc_create)/dp\_agc\_reset() already do the right thing; this note is for anyone tempted to shortcut them. 


        

<hr>



### variable ref\_db 

```C++
double dp_agc_state_t::ref_db;
```



Target output power, dB (config, writable). 
 


        

<hr>



### variable tlm 

```C++
agc_tlm_t dp_agc_state_t::tlm;
```



Live telemetry attachment; zeroed in state blobs and kept across set\_state. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/agc/agc_core.h`

