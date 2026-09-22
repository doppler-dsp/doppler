

# Struct acq\_shape\_t



[**ClassList**](annotated.md) **>** [**acq\_shape\_t**](structacq__shape__t.md)



_What the engine knows about the SHAPE of the repeated preamble, beyond its samples (doppler#1470)._ [More...](#detailed-description)

* `#include <acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**delay\_loss**](#variable-delay_loss)  <br> |
|  double | [**delay\_loss\_mean**](#variable-delay_loss_mean)  <br> |
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



### variable zone 

```C++
size_t acq_shape_t::zone;
```



Correlation mainlobe half-width, samples: peaks closer than this in delay are one emitter. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acq/acq_core.h`

