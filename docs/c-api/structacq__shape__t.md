

# Struct acq\_shape\_t



[**ClassList**](annotated.md) **>** [**acq\_shape\_t**](structacq__shape__t.md)



_What the engine knows about the SHAPE of the repeated preamble, beyond its samples (doppler#1470)._ [More...](#detailed-description)

* `#include <acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**delay\_loss**](#variable-delay_loss)  <br> |
|  double | [**delay\_loss\_mean**](#variable-delay_loss_mean)  <br> |
|  double | [**off\_peak**](#variable-off_peak)  <br> |
|  double | [**off\_peak\_amp**](#variable-off_peak_amp)  <br> |
|  size\_t | [**zone**](#variable-zone)  <br> |












































## Detailed Description


The engine's framing, slow-time transform and wideband tiling hold for any periodic reference; only two things read the waveform's correlation shape, and they read it from here rather than from the chip count:



* the width of the correlation mainlobe, which sets the peak list's exclusion zone and the twin rule's reach ([**zone**](structacq__shape__t.md#variable-zone));
* the amplitude a peak keeps when the true delay falls between two samples, which the Pd model averages over a uniform half-sample prior ([**delay\_loss**](structacq__shape__t.md#variable-delay_loss), [**delay\_loss\_mean**](structacq__shape__t.md#variable-delay_loss_mean)).




The constructor that owns the waveform fills it once. A PN code of `spc` samples per chip has a triangular autocorrelation one chip wide, so its descriptor is analytic: `zone = spc`, and a peak `delta` samples off the grid keeps `1 - delta/spc`. 


    
## Public Attributes Documentation




### variable delay\_loss 

```C++
double acq_shape_t::delay_loss[ACQ_DELAY_LOSS_NODES];
```



Amplitude kept at the midpoint nodes (k + 1/2)/nodes of that half-sample range, k = 0 … nodes-1. 
 


        

<hr>



### variable delay\_loss\_mean 

```C++
double acq_shape_t::delay_loss_mean;
```



Mean amplitude kept over a uniform delay straddle of 0 to 1/2 sample. 
 


        

<hr>



### variable off\_peak 

```C++
double acq_shape_t::off_peak;
```



Correlation energy OFF the peak, relative to it: sum over m != 0 of \|R(m)\|^2 / R(0)^2 for the periodic autocorrelation R. It lands in the cells the CFAR reference averages, so the Pd model charges it (doppler#1501): ~0 for a perfect sequence. Filled by the constructor from the replica, whatever built the rest. 


        

<hr>



### variable off\_peak\_amp 

```C++
double acq_shape_t::off_peak_amp;
```



The same lags' AMPLITUDE: sum over m != 0 of \|R(m)\| / R(0). With off\_peak it says how concentrated that energy is, which sets how much mean it adds to the reference. 


        

<hr>



### variable zone 

```C++
size_t acq_shape_t::zone;
```



Correlation mainlobe half-width, samples: peaks closer than this in delay are one emitter. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acq/acq_core.h`

