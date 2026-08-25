

# Struct dsss\_br\_pending\_t



[**ClassList**](annotated.md) **>** [**dsss\_br\_pending\_t**](structdsss__br__pending__t.md)



_One detection between acquisition and demodulation._ [More...](#detailed-description)

* `#include <dsss_burst_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**anchor**](#variable-anchor)  <br> |
|  double | [**cn0\_dbhz**](#variable-cn0_dbhz)  <br> |
|  double | [**doppler\_hz**](#variable-doppler_hz)  <br> |
|  double | [**margin**](#variable-margin)  <br> |
|  int | [**refined**](#variable-refined)  <br> |
|  uint64\_t | [**start**](#variable-start)  <br> |












































## Detailed Description


A hit cannot always be refined the moment it arrives  the refine window reaches BACKWARDS and forwards, so some of it may not have been pushed yet  and a detection dropped because its window was incomplete is a lost burst. So a hit is queued here with the event fields acquisition supplied, refined when its window is reachable, and demodulated when the burst has fully arrived. 


    
## Public Attributes Documentation




### variable anchor 

```C++
uint64_t dsss_br_pending_t::anchor;
```



Coarse code epoch from the hit (stream-absolute). 


        

<hr>



### variable cn0\_dbhz 

```C++
double dsss_br_pending_t::cn0_dbhz;
```



C/N0 lower bound from the hit, dB-Hz. 
 


        

<hr>



### variable doppler\_hz 

```C++
double dsss_br_pending_t::doppler_hz;
```



Signed coarse Doppler, Hz. 
 


        

<hr>



### variable margin 

```C++
double dsss_br_pending_t::margin;
```



Refine runner-up ratio; valid once `refined`. 
 


        

<hr>



### variable refined 

```C++
int dsss_br_pending_t::refined;
```



Non-zero once `start` is known. 
 


        

<hr>



### variable start 

```C++
uint64_t dsss_br_pending_t::start;
```



Refined preamble start; valid once `refined`. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dsss_burst_receiver/dsss_burst_receiver_core.h`

