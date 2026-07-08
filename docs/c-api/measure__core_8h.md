

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
|  size\_t | [**measure\_min\_samples**](#function-measure_min_samples) (double fs, double target\_rbw, size\_t bits, double dynamic\_range\_db, int complex\_input) <br>_Samples needed to reach a target resolution bandwidth._  |
|  double | [**measure\_proc\_gain**](#function-measure_proc_gain) (size\_t nfft) <br>_FFT processing gain in dB: 10\*log10(nfft / 2)._  |
|  size\_t | [**measure\_rec\_nfft**](#function-measure_rec_nfft) (size\_t n, size\_t pad) <br>_Recommended zero-padded transform length: next\_pow2(n \* max(pad,1))._  |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  double | [**measure\_dr\_from\_bits**](#function-measure_dr_from_bits) (size\_t bits) <br>_Default dynamic-range target (dB) implied by an ADC bit depth._  |
|  double | [**measure\_resolve\_dr**](#function-measure_resolve_dr) (double dynamic\_range\_db, size\_t bits) <br>_Resolve the dynamic-range target from the override/bits/default chain._  |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**MEASURE\_DR\_DEFAULT\_DB**](measure__core_8h.md#define-measure_dr_default_db)  `120.0`<br> |
| define  | [**MEASURE\_DR\_MARGIN\_DB**](measure__core_8h.md#define-measure_dr_margin_db)  `12.0`<br> |
| define  | [**MEASURE\_PAD**](measure__core_8h.md#define-measure_pad)  `2u`<br> |
| define  | [**MEASURE\_SPUR\_SIDELOBES**](measure__core_8h.md#define-measure_spur_sidelobes)  `1.0`<br> |

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
    size_t bits,
    double dynamic_range_db,
    int complex_input
) 
```



Plans a capture for the same auto-Kaiser window the measurement objects use: the dynamic-range target (from `dynamic_range_db`, else `bits`) selects the Kaiser beta, whose ENBW (measured via kaiser\_enbw) sets the bins-per-RBW. RBW = ENBW \* fs / n, so n = ceil(ENBW \* fs / target\_rbw).




**Parameters:**


* `fs` Sample rate (Hz, &gt; 0). 
* `target_rbw` Desired resolution bandwidth (Hz). When &lt;= 0 it defaults to span/1000, where span = fs/2 for real captures and fs for complex (`complex_input`). 
* `bits` ADC depth: sets the dynamic-range target when no explicit override is given. 
* `dynamic_range_db` Explicit dynamic-range target (dB); used when &gt; 0. 
* `complex_input` Non-zero if the capture is complex (span = fs). 



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
## Public Static Functions Documentation




### function measure\_dr\_from\_bits 

_Default dynamic-range target (dB) implied by an ADC bit depth._ 
```C++
static inline double measure_dr_from_bits (
    size_t bits
) 
```



Ideal quantisation SNR is 6.02\*bits + 1.76 dB; add MEASURE\_DR\_MARGIN\_DB of headroom so the window's first sidelobe sits below the converter's own floor.




**Parameters:**


* `bits` ADC depth in bits (&gt; 0). 



**Returns:**

Dynamic-range / sidelobe-attenuation target in dB. 





        

<hr>



### function measure\_resolve\_dr 

_Resolve the dynamic-range target from the override/bits/default chain._ 
```C++
static inline double measure_resolve_dr (
    double dynamic_range_db,
    size_t bits
) 
```





**Parameters:**


* `dynamic_range_db` Explicit target (dB); used when &gt; 0. 
* `bits` ADC depth; used (via measure\_dr\_from\_bits) when the override is unset and bits &gt; 0. 



**Returns:**

Dynamic-range / sidelobe-attenuation target in dB. 





        

<hr>
## Macro Definition Documentation





### define MEASURE\_DR\_DEFAULT\_DB 

```C++
#define MEASURE_DR_DEFAULT_DB `120.0`
```




<hr>



### define MEASURE\_DR\_MARGIN\_DB 

```C++
#define MEASURE_DR_MARGIN_DB `12.0`
```




<hr>



### define MEASURE\_PAD 

```C++
#define MEASURE_PAD `2u`
```




<hr>



### define MEASURE\_SPUR\_SIDELOBES 

```C++
#define MEASURE_SPUR_SIDELOBES `1.0`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/measure/measure_core.h`

