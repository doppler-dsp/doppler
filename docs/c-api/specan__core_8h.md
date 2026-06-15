

# File specan\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**specan**](dir_6d702d949620e4073485867cfd9038e4.md) **>** [**specan\_core.h**](specan__core_8h.md)

[Go to the source code of this file](specan__core_8h_source.md)

_Specan — natural-parameter spectrum analyzer (DDC + averaging PSD)._ [More...](#detailed-description)

* `#include "ddc/ddc_core.h"`
* `#include "welch/welch_core.h"`
* `#include <complex.h>`
* `#include <stddef.h>`
* `#include "lo/lo_core.h"`
* `#include "RateConverter/RateConverter_core.h"`
* `#include "resamp/resamp_core.h"`
* `#include "hbdecim/hbdecim_core.h"`
* `#include "cic/cic_core.h"`
* `#include "fir/fir_core.h"`
* `#include "resample/resample_core.h"`
* `#include "acc_trace/acc_trace_core.h"`
* `#include "fft/fft_core.h"`
* `#include "spectral/spectral_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**specan\_state\_t**](structspecan__state__t.md) <br>_Specan state. Allocate with_ [_**specan\_create()**_](specan__core_8h.md#function-specan_create) _._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**specan\_state\_t**](structspecan__state__t.md) \* | [**specan\_create**](#function-specan_create) (double fs, double span, double rbw, double src\_center, double center, double ref\_db, int window, size\_t navg) <br>_Create a natural-parameter spectrum analyzer._  |
|  void | [**specan\_destroy**](#function-specan_destroy) ([**specan\_state\_t**](structspecan__state__t.md) \* state) <br>_Destroy a Specan instance and release all memory._  |
|  size\_t | [**specan\_execute**](#function-specan_execute) ([**specan\_state\_t**](structspecan__state__t.md) \* state, const float complex \* x, size\_t x\_len, float \* out, size\_t max\_out) <br>_Mix, decimate, average and return one display spectrum, or nothing._  |
|  size\_t | [**specan\_execute\_max\_out**](#function-specan_execute_max_out) ([**specan\_state\_t**](structspecan__state__t.md) \* state) <br>_Output capacity hint for_ [_**specan\_execute()**_](specan__core_8h.md#function-specan_execute) _; equals disp\_n._ |
|  void | [**specan\_reset**](#function-specan_reset) ([**specan\_state\_t**](structspecan__state__t.md) \* state) <br>_Drop pending samples and the running average; LO/filter history zero._  |
|  void | [**specan\_retune**](#function-specan_retune) ([**specan\_state\_t**](structspecan__state__t.md) \* state, double center) <br>_Retune the display center without rebuilding the chain._  |




























## Detailed Description


A streaming spectrum analyzer that speaks the _instrument_ parameters an operator already knows — center frequency, span, resolution bandwidth (RBW), and reference level — instead of the DSP knobs (window length, Kaiser beta, zero-pad factor) underneath them. It is the C-first home for the mapping that `doppler.specan`'s engine used to hand-roll in Python.


It composes the existing library, re-implementing nothing:



```C++
cf32 in (fs_in)  →  Ddc  (mix center→DC, decimate to fs_out = span·1.28)
                 →  Welch (window → zero-pad FFT → cg²-normalised power,
                           averaged over `navg` segments)
                 →  crop to the central ±span/2 display band
                 →  dB + ref offset  →  float display spectrum
```




* [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) is the tuner/decimator (LO mix + RateConverter cascade); retuning the center is a cheap, seamless LO phase change.
* [**welch\_state\_t**](structwelch__state__t.md) is the one averaging-PSD core shared with the measurement suite; `navg = 1` gives a responsive single-periodogram frame, larger `navg` trades update rate for a smoother, lower-variance trace.




The display band length and the bin→frequency map are fixed at create time: bin `i` of the returned spectrum maps to `center + (i − disp_n/2)·fs_out/nfft` Hz. Peaks are intentionally NOT computed here — compose [**find\_peaks\_f32**](spectral__core_8h.md#function-find_peaks_f32) on the returned trace.


Lifecycle: create → (execute / retune / reset)\* → destroy.



```C++
// 200 kHz span, 500 Hz RBW around DC of a 2.048 MHz cf32 stream
specan_state_t *sa = specan_create(2.048e6, 200e3, 500.0, 0.0, 0.0,
                                   0.0, 1, 1);
float disp[8192];
size_t n = specan_execute(sa, iq, 65536, disp, 8192);  // 0 until a frame
specan_destroy(sa);
```
 


    
## Public Functions Documentation




### function specan\_create 

_Create a natural-parameter spectrum analyzer._ 
```C++
specan_state_t * specan_create (
    double fs,
    double span,
    double rbw,
    double src_center,
    double center,
    double ref_db,
    int window,
    size_t navg
) 
```



Derives the DSP from the instrument parameters: `fs_out = min(span·1.28, fs)`, `n = next_pow2(ceil(fs_out/rbw))` (the coarse RBW knob), a Kaiser `beta` solved so the window ENBW realises `rbw` (the fine knob), `nfft = next_pow2(2·n)`, and the central display crop covering ±span/2.




**Parameters:**


* `fs` Input sample rate (Hz). Must be &gt; 0. 
* `span` Display span (Hz). Must be &gt; 0. 
* `rbw` Resolution bandwidth (Hz). Must be &gt; 0. 
* `src_center` Source center frequency (Hz); the input band is centred here, so the analyzer mixes (center − src\_center) to DC. 
* `center` Desired display center frequency (Hz). 
* `ref_db` dB offset added to the display spectrum (e.g. a dBm calibration the application computes from a ref level). 
* `window` Window index: 0 = Hann, 1 = Kaiser (RBW-trimmable). 
* `navg` Segments averaged per emitted frame (&gt;= 1). 



**Returns:**

Heap-allocated state, or NULL on invalid argument or OOM. 




**Note:**

Caller must call [**specan\_destroy()**](specan__core_8h.md#function-specan_destroy) when done. Argument order keeps the required parameters (fs, span, rbw) first, matching the generated constructor's hoisting of jm `required` init params.



```C++
>>> from doppler.analyzer import Specan
>>> sa = Specan(fs=2.048e6, span=200e3, rbw=500.0)
>>> sa.fs_out
256000.0
>>> sa.nfft == 2 * sa.n
True
```
 


        

<hr>



### function specan\_destroy 

_Destroy a Specan instance and release all memory._ 
```C++
void specan_destroy (
    specan_state_t * state
) 
```





**Parameters:**


* `state` May be NULL (no-op). 




        

<hr>



### function specan\_execute 

_Mix, decimate, average and return one display spectrum, or nothing._ 
```C++
size_t specan_execute (
    specan_state_t * state,
    const float complex * x,
    size_t x_len,
    float * out,
    size_t max_out
) 
```



Feeds `x` through the Ddc, buffers the decimated output, and once `n·navg` decimated samples are available windows + FFTs + averages them into a fresh frame, crops the central ±span/2 band and writes it in dB (+ ref\_db). Returns 0 (writing nothing) until a frame is ready — the binding maps that to Python `None`.




**Parameters:**


* `state` Must be non-NULL. 
* `x` cf32 input block (C-only; the binding passes it). 
* `x_len` Number of input samples (C-only). 
* `out` Display-spectrum buffer, dB (C-only). 
* `max_out` Capacity of `out` (C-only); &gt;= disp\_n is sufficient. 



**Returns:**

Display bins written (disp\_n), or 0 if no frame is ready yet. 





        

<hr>



### function specan\_execute\_max\_out 

_Output capacity hint for_ [_**specan\_execute()**_](specan__core_8h.md#function-specan_execute) _; equals disp\_n._
```C++
size_t specan_execute_max_out (
    specan_state_t * state
) 
```




<hr>



### function specan\_reset 

_Drop pending samples and the running average; LO/filter history zero._ 
```C++
void specan_reset (
    specan_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function specan\_retune 

_Retune the display center without rebuilding the chain._ 
```C++
void specan_retune (
    specan_state_t * state,
    double center
) 
```



Updates the Ddc LO phase increment (seamless across blocks — no resampler or window reset) and drops pending samples so the next frame reflects only the new tuning. Changing the span or RBW requires a destroy + create (the decimation rate and window length change).




**Parameters:**


* `state` Must be non-NULL. 
* `center` New display center frequency (Hz). 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/specan/specan_core.h`

