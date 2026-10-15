

# Struct burst\_capture\_event\_t



[**ClassList**](annotated.md) **>** [**burst\_capture\_event\_t**](structburst__capture__event__t.md)



_One captured burst's event, as_ `events()` _hands it back._[More...](#detailed-description)

* `#include <burst_capture_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  double | [**doppler\_hz\_est**](#variable-doppler_hz_est)  <br> |
|  double | [**doppler\_rate\_hz\_s**](#variable-doppler_rate_hz_s)  <br> |
|  double | [**doppler\_res\_hz**](#variable-doppler_res_hz)  <br> |
|  uint64\_t | [**preamble\_start**](#variable-preamble_start)  <br> |












































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



### variable doppler\_rate\_hz\_s 

```C++
double burst_capture_event_t::doppler_rate_hz_s;
```



Refine's winning rate hypothesis, Hz/s: the rate whose de-chirp made the whole preamble's coherent sum largest at preamble\_start. One of refine's R grid points from -doppler\_rate to +doppler\_rate, spaced 2\*doppler\_rate/(R - 1); 0 without a declared rate (doppler\_rate 0). Refine searches the grid whenever a rate is declared, with one acquisition hypothesis or many (doppler#2219). A hypothesis, not a fit, from the whole preamble rather than one dwell: it is the grid point nearest the carrier's rate in every event 23 dB over the design point, on or off the grid. At the design point it is in 82% to 89% of events whose rate is ON a grid point, but in 51% of those 0.4 of a step off one, where the neighbouring point is only 0.6 of a step away (validate\_acq\_rate\_pd's event-rate rows; docs/design/burst-capture.md). Appended last, so positional initialisers do not shift (doppler#2219). 


        

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

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/burst_capture/burst_capture_core.h`

