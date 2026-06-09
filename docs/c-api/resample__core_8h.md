

# File resample\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**resample**](dir_430486ea22038fad478027f2dc6550c6.md) **>** [**resample\_core.h**](resample__core_8h.md)

[Go to the source code of this file](resample__core_8h_source.md)

_Resample module — public C API._ 

* `#include "clib_common.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**ciccompmf**](#function-ciccompmf) (double \* out, uint32\_t N, uint32\_t R, uint32\_t M) <br>_Design a CIC passband-droop compensator FIR filter. Implements the closed-form Bernoulli-series maximally-flat-error method from Molnar & Vucic (IEEE TCAS-II 58(12):926-930, 2011, DOI 10.1109/TCSII.2011.2172522). The compensator runs at the_ _decimated_ _(output) rate and should be applied after the CIC stage. DC gain is exactly 1.0. Odd M gives symmetric linear-phase taps; even M gives half-sample-shifted linear-phase taps._ |
|  double | [**kaiser\_beta**](#function-kaiser_beta) (double atten) <br>_Compute the Kaiser window beta parameter from stopband attenuation. Uses the standard Kaiser-Hamming formulae: atten &gt; 50 dB: beta = 0.1102 \* (atten - 8.7) 21 &lt;= atten &lt;= 50 dB: beta = 0.5842\*(atten-21)^0.4 + 0.07886\*(atten-21) atten &lt; 21 dB: beta = 0.0 (rectangular window)._  |
|  int | [**kaiser\_num\_taps**](#function-kaiser_num_taps) (int num\_phases, double atten, double pb, double sb) <br>_Estimate the taps-per-phase count for a polyphase Kaiser FIR bank. Applies the Kaiser length formula to the per-phase normalised prototype (pb/num\_phases, sb/num\_phases), rounds up to the next odd symmetrical length, then divides by num\_phases to give taps per branch. The result is the minimum num\_taps argument to pass to_ [_**Resampler\_create\_custom()**_](Resampler__core_8h.md#function-resampler_create_custom) _._ |




























## Public Functions Documentation




### function ciccompmf 

_Design a CIC passband-droop compensator FIR filter. Implements the closed-form Bernoulli-series maximally-flat-error method from Molnar & Vucic (IEEE TCAS-II 58(12):926-930, 2011, DOI 10.1109/TCSII.2011.2172522). The compensator runs at the_ _decimated_ _(output) rate and should be applied after the CIC stage. DC gain is exactly 1.0. Odd M gives symmetric linear-phase taps; even M gives half-sample-shifted linear-phase taps._
```C++
void ciccompmf (
    double * out,
    uint32_t N,
    uint32_t R,
    uint32_t M
) 
```





**Parameters:**


* `out` Output buffer; must hold at least M doubles. M outside the Bernoulli table range leaves out unmodified. 
* `N` CIC filter order (number of integrator/comb stages, &gt;= 1). 
* `R` CIC decimation factor (&gt;= 2). 
* `M` Number of compensator taps in [1, 19] (odd or even).


```C++
>>> from doppler.resample import ciccompmf
>>> import numpy as np
>>> h = ciccompmf(4, 16, 5)
>>> h.shape, h.dtype
((5,), dtype('float64'))
>>> [round(float(v), 4) for v in h]
[0.029, -0.282, 1.5061, -0.282, 0.029]
```
 


        

<hr>



### function kaiser\_beta 

_Compute the Kaiser window beta parameter from stopband attenuation. Uses the standard Kaiser-Hamming formulae: atten &gt; 50 dB: beta = 0.1102 \* (atten - 8.7) 21 &lt;= atten &lt;= 50 dB: beta = 0.5842\*(atten-21)^0.4 + 0.07886\*(atten-21) atten &lt; 21 dB: beta = 0.0 (rectangular window)._ 
```C++
double kaiser_beta (
    double atten
) 
```





**Parameters:**


* `atten` Desired stopband attenuation in dB (positive value). 



**Returns:**

Kaiser beta parameter (&gt;= 0.0).



```C++
>>> from doppler.resample import kaiser_beta
>>> round(kaiser_beta(60.0), 4)
5.6533
>>> kaiser_beta(20.0)
0.0
```
 


        

<hr>



### function kaiser\_num\_taps 

_Estimate the taps-per-phase count for a polyphase Kaiser FIR bank. Applies the Kaiser length formula to the per-phase normalised prototype (pb/num\_phases, sb/num\_phases), rounds up to the next odd symmetrical length, then divides by num\_phases to give taps per branch. The result is the minimum num\_taps argument to pass to_ [_**Resampler\_create\_custom()**_](Resampler__core_8h.md#function-resampler_create_custom) _._
```C++
int kaiser_num_taps (
    int num_phases,
    double atten,
    double pb,
    double sb
) 
```





**Parameters:**


* `num_phases` Number of polyphase branches (power of two). 
* `atten` Desired stopband attenuation in dB. 
* `pb` Normalised passband edge (0 &lt; pb &lt; sb &lt; 1). 
* `sb` Normalised stopband edge. 



**Returns:**

Taps per polyphase branch (&gt;= 1).



```C++
>>> from doppler.resample import kaiser_num_taps
>>> kaiser_num_taps(4096, 60.0, 0.4, 0.6)
19
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/resample/resample_core.h`

