

# File filter\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**filter**](dir_820039e5f1fe5ce84fe724a5385272f3.md) **>** [**filter\_core.h**](filter__core_8h.md)

[Go to the source code of this file](filter__core_8h_source.md)

_Filter module — public C API._ 

* `#include "doppler/clib_common.h"`
* `#include "doppler/resample/resample_core.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_design\_lowpass**](#function-dp_design_lowpass) (double fpass, double fstop, double atten\_db, float \* out) <br>_Design a Kaiser-windowed-sinc lowpass FIR (float32 taps)._  |




























## Public Functions Documentation




### function dp\_design\_lowpass 

_Design a Kaiser-windowed-sinc lowpass FIR (float32 taps)._ 
```C++
void dp_design_lowpass (
    double fpass,
    double fstop,
    double atten_db,
    float * out
) 
```



The tap count is chosen automatically from the band edges and the attenuation with the Kaiser FIR-length formula, `n = dp_kaiser_num_taps(1, atten_db, fpass / 2, fstop / 2) | 1` (always odd, so the filter is type-I linear phase with an integer group delay of `(n - 1) / 2` samples). The window shape comes from [**dp\_kaiser\_beta()**](resample__core_8h.md#function-dp_kaiser_beta), the FIR-design beta formula, not the window-sidelobe one. The ideal cutoff sits at the midpoint of the transition band, `(fpass + fstop) / 2`. Taps are `2 fc sinc(2 fc m) w[m]` with `fc` in cycles/sample; they are not renormalized afterwards, so the DC gain is close to, but not exactly, 1.




**Parameters:**


* `fpass` Passband edge, Nyquist-normalized (1.0 == fs/2), `0 < fpass < fstop`. 
* `fstop` Stopband edge, Nyquist-normalized (1.0 == fs/2). 
* `atten_db` Target stopband attenuation in dB (positive). 
* `out` Output buffer for the taps. The caller must size it to `dp_kaiser_num_taps(1, atten_db, fpass / 2.0, fstop / 2.0) | 1` floats; that many are written. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/filter/filter_core.h`

