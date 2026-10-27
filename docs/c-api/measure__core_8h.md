

# File measure\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**measure**](dir_0caaff9683efc5ec33140a5572570268.md) **>** [**measure\_core.h**](measure__core_8h.md)

[Go to the source code of this file](measure__core_8h_source.md)

_Measure module — shared result structs and module-level helpers._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`















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
|  size\_t | [**dp\_measure\_min\_samples**](#function-dp_measure_min_samples) (double fs, double target\_rbw, size\_t bits, double dynamic\_range\_db, int complex\_input) <br>_Samples needed to reach a target resolution bandwidth._  |
|  double | [**dp\_measure\_proc\_gain**](#function-dp_measure_proc_gain) (size\_t nfft) <br>_FFT processing gain in dB: 10\*log10(nfft / 2)._  |
|  size\_t | [**dp\_measure\_rec\_nfft**](#function-dp_measure_rec_nfft) (size\_t n, size\_t pad) <br>_Recommended zero-padded transform length: next\_pow\_two(n \* max(pad, 1))._  |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  double | [**measure\_dr\_from\_bits**](#function-measure_dr_from_bits) (size\_t bits) <br>_Default dynamic-range target (dB) implied by an ADC bit depth._  |
|  double | [**measure\_resolve\_dr**](#function-measure_resolve_dr) (double dynamic\_range\_db, size\_t bits) <br>_Resolve the dynamic-range target from the override/bits/default chain._  |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**MEASURE\_DR\_DEFAULT\_DB**](measure__core_8h.md#define-measure_dr_default_db)  `120.0`<br>_Dynamic-range target (dB) when neither_ `bits` _nor an explicit override is given (general DUT): deep enough for ~19-bit measurements. The fallback of measure\_resolve\_dr()._ |
| define  | [**MEASURE\_DR\_MARGIN\_DB**](measure__core_8h.md#define-measure_dr_margin_db)  `12.0`<br>_Sidelobe headroom (dB) below the ideal converter SNR._  |
| define  | [**MEASURE\_PAD**](measure__core_8h.md#define-measure_pad)  `2u`<br>_Zero-pad factor every measurement object hands its PSD core: nfft = next\_pow\_two(n \* MEASURE\_PAD), so a capture of n samples is transformed at no less than twice its own length._  |
| define  | [**MEASURE\_SPUR\_SIDELOBES**](measure__core_8h.md#define-measure_spur_sidelobes)  `1.0`<br>_Extra main-lobe widths excluded past the first null when searching for spurs (or, in IMDMeasure, for the second tone), so a component's near-in sidelobes are never mistaken for a spur._  |

## Detailed Description


The `doppler.measure` objects (ToneMeasure, …) each own a window + FFT and analyse a time-domain capture, returning one of these plain-C result bags by value. Every spectral metric integrates a component's power over its window MAIN LOBE (IEEE Std 1241) rather than reading a single peak bin, so the reading is independent of where the tone falls between FFT bins. 


    
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


J starts at round(f\_target \* N / fs), is clamped to `[1, N-1]`, then moved by the smallest distance that makes it coprime with N; on a tie the higher J wins.




**Parameters:**


* `fs` Sample rate (Hz, &gt; 0). 
* `f_target` Desired test frequency (Hz). 
* `N` Capture length (samples, &gt;= 2). 



**Returns:**

The coherent frequency J \* fs / N (Hz), or 0 if N &lt; 2 or fs &lt;= 0. 





        

<hr>



### function dp\_measure\_min\_samples 

_Samples needed to reach a target resolution bandwidth._ 
```C++
size_t dp_measure_min_samples (
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



### function dp\_measure\_proc\_gain 

_FFT processing gain in dB: 10\*log10(nfft / 2)._ 
```C++
double dp_measure_proc_gain (
    size_t nfft
) 
```



How far a single bin's noise sits below the total noise power for an nfft-point transform of a real capture. No argument check: nfft = 0 returns -inf.




**Parameters:**


* `nfft` Transform length (bins). 



**Returns:**

Processing gain (dB). 





        

<hr>



### function dp\_measure\_rec\_nfft 

_Recommended zero-padded transform length: next\_pow\_two(n \* max(pad, 1))._ 
```C++
size_t dp_measure_rec_nfft (
    size_t n,
    size_t pad
) 
```



The same rule the measurement objects apply (with pad = [**MEASURE\_PAD**](measure__core_8h.md#define-measure_pad)), exposed so a caller can plan an FFT size before building an analyser.




**Parameters:**


* `n` Capture length (samples). 
* `pad` Zero-pad factor; 0 is treated as 1. 



**Returns:**

The smallest power of two &gt;= n \* pad (1 when n is 0). 





        

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

_Dynamic-range target (dB) when neither_ `bits` _nor an explicit override is given (general DUT): deep enough for ~19-bit measurements. The fallback of measure\_resolve\_dr()._
```C++
#define MEASURE_DR_DEFAULT_DB `120.0`
```




<hr>



### define MEASURE\_DR\_MARGIN\_DB 

_Sidelobe headroom (dB) below the ideal converter SNR._ 
```C++
#define MEASURE_DR_MARGIN_DB `12.0`
```



A B-bit ADC's spur/noise floor sits at -(6.02\*B+1.76) dBc, so measure\_dr\_from\_bits() targets sidelobes this much deeper to be sure window leakage stays under the floor being measured. 


        

<hr>



### define MEASURE\_PAD 

_Zero-pad factor every measurement object hands its PSD core: nfft = next\_pow\_two(n \* MEASURE\_PAD), so a capture of n samples is transformed at no less than twice its own length._ 
```C++
#define MEASURE_PAD `2u`
```




<hr>



### define MEASURE\_SPUR\_SIDELOBES 

_Extra main-lobe widths excluded past the first null when searching for spurs (or, in IMDMeasure, for the second tone), so a component's near-in sidelobes are never mistaken for a spur._ 
```C++
#define MEASURE_SPUR_SIDELOBES `1.0`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/measure/measure_core.h`

