

# Struct burst\_capture\_event\_t



[**ClassList**](annotated.md) **>** [**burst\_capture\_event\_t**](structburst__capture__event__t.md)



_One captured burst's event, as_ `events()` _hands it back._[More...](#detailed-description)

* `#include <burst_capture_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  double | [**doppler\_hz\_est**](#variable-doppler_hz_est)  <br> |
|  double | [**doppler\_res\_hz**](#variable-doppler_res_hz)  <br> |
|  uint64\_t | [**preamble\_start**](#variable-preamble_start)  <br> |
|  double | [**refine\_margin**](#variable-refine_margin)  <br> |












































## Detailed Description


push() returns the SAMPLES of every burst it completed, concatenated; this is the parallel record for row `i` of that return. It exists because a single push() can complete many bursts and each one needs its own event  a single set of scalar read-backs would describe only the last (docs/design/dsss-burst-receiver.md §4: the record must be sufficient on its own, for EVERY burst, not just the most recent).


Everything here is acquisition's or refine's. A consumer's own estimates (a demodulator's residual frequency, its post-decode SNR) belong to the consumer's record, not to this one. 


    
## Public Attributes Documentation




### variable cn0\_dbhz\_est 

```C++
double burst_capture_event_t::cn0_dbhz_est;
```



C/N0 lower bound from the hit, dB-Hz. 
 


        

<hr>



### variable doppler\_hz\_est 

```C++
double burst_capture_event_t::doppler_hz_est;
```



Signed coarse Doppler, Hz. 
 


        

<hr>



### variable doppler\_res\_hz 

```C++
double burst_capture_event_t::doppler_res_hz;
```



Acquisition's native bin width, Hz. 
 


        

<hr>



### variable preamble\_start 

```C++
uint64_t burst_capture_event_t::preamble_start;
```



Exact stream position of the preamble. 
 


        

<hr>



### variable refine\_margin 

```C++
double burst_capture_event_t::refine_margin;
```



Runner-up period over the winner. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/burst_capture/burst_capture_core.h`

