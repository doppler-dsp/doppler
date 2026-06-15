

# File measure\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**measure**](dir_4f61a452d1df39cf8c2e8be27f29f1f2.md) **>** [**measure\_core.h**](measure__core_8h.md)

[Go to the source code of this file](measure__core_8h_source.md)

_Measure module — shared result structs and module-level helpers._ [More...](#detailed-description)

* `#include "clib_common.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**imd\_meas\_t**](structimd__meas__t.md) <br>_Two-tone intermodulation result (IMD2/IMD3/TOI)._  |
| struct | [**npr\_meas\_t**](structnpr__meas__t.md) <br>_Noise Power Ratio (notched-noise loading) result._  |
| struct | [**time\_stats\_t**](structtime__stats__t.md) <br>_Time-domain capture statistics (AC-coupled crest/PAPR)._  |
| struct | [**tone\_meas\_t**](structtone__meas__t.md) <br>_Single-tone dynamic-measurement bag._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  double | [**dp\_coherent\_freq**](#function-dp_coherent_freq) (double fs, double f\_target, size\_t N) <br>_Nearest leakage-free coherent test frequency._  |
|  size\_t | [**measure\_min\_samples**](#function-measure_min_samples) (double fs, double target\_rbw, int window, float beta) <br>_Samples needed to reach a target resolution bandwidth._  |
|  double | [**measure\_proc\_gain**](#function-measure_proc_gain) (size\_t nfft) <br>_FFT processing gain in dB: 10\*log10(nfft / 2)._  |
|  size\_t | [**measure\_rec\_nfft**](#function-measure_rec_nfft) (size\_t n, size\_t pad) <br>_Recommended zero-padded transform length: next\_pow2(n \* max(pad,1))._  |




























## Detailed Description


The `doppler.measure` objects (ToneMeasure, …) each own a window + FFT and analyse a time-domain capture, returning one of these plain-C result bags by out-parameter. Every spectral metric integrates a component's power over its window MAIN LOBE (IEEE Std 1241) rather than reading a single peak bin, so the reading is independent of where the tone falls between FFT bins. 


    
## Public Functions Documentation




### function dp\_coherent\_freq 

_Nearest leakage-free coherent test frequency._ 
```C++
double dp_coherent_freq (
    double fs,
    double f_target,
    size_t N
) 
```



Snaps `f_target` to `J * fs / N` where J is the nearest integer cycle count that is coprime with N — an integer number of cycles in the capture (no leakage) with J coprime to N (so quantisation-noise correlation is minimised).




**Returns:**

The coherent frequency (Hz), or 0 on bad args. 





        

<hr>



### function measure\_min\_samples 

_Samples needed to reach a target resolution bandwidth._ 
```C++
size_t measure_min_samples (
    double fs,
    double target_rbw,
    int window,
    float beta
) 
```



RBW = ENBW \* fs / n, so n = ceil(ENBW \* fs / target\_rbw). The window ENBW is the Kaiser value for `beta` (measured via kaiser\_enbw on a reference window) or 1.5 for Hann.




**Parameters:**


* `fs` Sample rate (Hz). 
* `target_rbw` Desired resolution bandwidth (Hz). 
* `window` 0 = Hann, 1 = Kaiser. 
* `beta` Kaiser shape (ignored for Hann). 



**Returns:**

Required capture length, or 0 on bad args. 





        

<hr>



### function measure\_proc\_gain 

_FFT processing gain in dB: 10\*log10(nfft / 2)._ 
```C++
double measure_proc_gain (
    size_t nfft
) 
```




<hr>



### function measure\_rec\_nfft 

_Recommended zero-padded transform length: next\_pow2(n \* max(pad,1))._ 
```C++
size_t measure_rec_nfft (
    size_t n,
    size_t pad
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/measure/measure_core.h`

