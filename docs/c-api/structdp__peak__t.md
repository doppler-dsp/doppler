

# Struct dp\_peak\_t



[**ClassList**](annotated.md) **>** [**dp\_peak\_t**](structdp__peak__t.md)



_One spectral peak returned by_ [_**find\_peaks\_f32()**_](spectral__core_8h.md#function-find_peaks_f32) _._[More...](#detailed-description)

* `#include <spectral_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float | [**amplitude\_db**](#variable-amplitude_db)  <br> |
|  float | [**freq\_norm**](#variable-freq_norm)  <br> |












































## Detailed Description


freq\_norm is the DC-centred normalised frequency in [−0.5, +0.5). amplitude\_db is the parabola-corrected peak value in the same dB units as the input spectrum. 


    
## Public Attributes Documentation




### variable amplitude\_db 

```C++
float dp_peak_t::amplitude_db;
```



Parabola-corrected peak amplitude in dB. 


        

<hr>



### variable freq\_norm 

```C++
float dp_peak_t::freq_norm;
```



Normalised frequency −0.5..+0.5 (DC-centred). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/spectral/spectral_core.h`

