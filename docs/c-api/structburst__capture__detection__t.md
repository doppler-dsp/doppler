

# Struct burst\_capture\_detection\_t



[**ClassList**](annotated.md) **>** [**burst\_capture\_detection\_t**](structburst__capture__detection__t.md)



_One raw detection, as the search reported it._ [More...](#detailed-description)

* `#include <burst_capture_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**cn0\_dbhz**](#variable-cn0_dbhz)  <br> |
|  double | [**doppler\_hz**](#variable-doppler_hz)  <br> |
|  uint64\_t | [**epoch**](#variable-epoch)  <br> |
|  double | [**peak\_mag**](#variable-peak_mag)  <br> |
|  double | [**test\_stat**](#variable-test_stat)  <br> |












































## Detailed Description


What `detections()` hands back: everything acquisition found in the last push, BEFORE the claim rule merged anything and before the suppression window dropped anything. `events()` is the other end of the same pipe  the bursts that survived all of that and whose windows arrived.


Both exist because they answer different questions and arrive at different times. A detection is available the moment a frame clears threshold; a burst is not available until its LAST sample has, which is `retain_span` later and never for a burst that was cut off. A caller watching a band for activity wants the first; a caller decoding wants the second; a bank doing both would otherwise have to run two acquisition engines over one stream.


The epoch is stream-absolute, which `acq_result_t::code_phase` is not  that is a lag modulo one code period, and making it absolute is the first thing this object does with a hit. 


    
## Public Attributes Documentation




### variable cn0\_dbhz 

```C++
double burst_capture_detection_t::cn0_dbhz;
```



C/N0 lower bound from the hit, dB-Hz. 
 


        

<hr>



### variable doppler\_hz 

```C++
double burst_capture_detection_t::doppler_hz;
```



Signed coarse Doppler, folded, Hz. 
 


        

<hr>



### variable epoch 

```C++
uint64_t burst_capture_detection_t::epoch;
```



Stream-absolute code epoch of the hit. 
 


        

<hr>



### variable peak\_mag 

```C++
double burst_capture_detection_t::peak_mag;
```



Raw CFAR peak magnitude. 
 


        

<hr>



### variable test\_stat 

```C++
double burst_capture_detection_t::test_stat;
```



The CFAR gating statistic, peak over noise. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/burst_capture/burst_capture_core.h`

