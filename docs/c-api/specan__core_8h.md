

# File specan\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**specan**](dir_6ce576ad24803d600633e2545d7ab991.md) **>** [**specan\_core.h**](specan__core_8h.md)

[Go to the source code of this file](specan__core_8h_source.md)

_Specan — natural-parameter spectrum analyzer (DDC + averaging PSD)._ [More...](#detailed-description)

* `#include "doppler/ddc/ddc_core.h"`
* `#include "doppler/psd/psd_core.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/dp_complex.h"`
* `#include <stddef.h>`
* `#include "doppler/lo/lo_core.h"`
* `#include "doppler/RateConverter/RateConverter_core.h"`
* `#include "doppler/resamp/resamp_core.h"`
* `#include "doppler/hbdecim/hbdecim_core.h"`
* `#include "doppler/cic/cic_core.h"`
* `#include "doppler/fir/fir_core.h"`
* `#include "doppler/resample/resample_core.h"`
* `#include "doppler/acc_trace/acc_trace_core.h"`
* `#include "doppler/fft/fft_core.h"`
* `#include "doppler/spectral/spectral_core.h"`
* `#include "doppler/agc/agc_core.h"`
* `#include "doppler/dp_tlm/dp_tlm_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_specan\_state\_t**](structdp__specan__state__t.md) <br>_Specan state. Allocate with_ [_**dp\_specan\_create()**_](specan__core_8h.md#function-dp_specan_create) _._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_specan\_state\_t**](structdp__specan__state__t.md) \* | [**dp\_specan\_create**](#function-dp_specan_create) (double fs, double span, double rbw, double src\_center, double center, double offset\_db, double full\_scale, size\_t bits, int window, size\_t navg) <br>_Create a natural-parameter spectrum analyzer._  |
|  void | [**dp\_specan\_destroy**](#function-dp_specan_destroy) ([**dp\_specan\_state\_t**](structdp__specan__state__t.md) \* state) <br>_Destroy a Specan instance and release all memory._  |
|  size\_t | [**dp\_specan\_execute**](#function-dp_specan_execute) ([**dp\_specan\_state\_t**](structdp__specan__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, float \* out, size\_t max\_out) <br>_Mix, decimate, average and return one display spectrum, or nothing._  |
|  size\_t | [**dp\_specan\_execute\_max\_out**](#function-dp_specan_execute_max_out) ([**dp\_specan\_state\_t**](structdp__specan__state__t.md) \* state) <br>_Output capacity hint for_ [_**dp\_specan\_execute()**_](specan__core_8h.md#function-dp_specan_execute) _; equals disp\_n._ |
|  void | [**dp\_specan\_get\_state**](#function-dp_specan_get_state) (const [**dp\_specan\_state\_t**](structdp__specan__state__t.md) \* state, void \* blob) <br> |
|  void | [**dp\_specan\_reset**](#function-dp_specan_reset) ([**dp\_specan\_state\_t**](structdp__specan__state__t.md) \* state) <br>_Drop pending samples and the running average; LO/filter history zero._  |
|  void | [**dp\_specan\_retune**](#function-dp_specan_retune) ([**dp\_specan\_state\_t**](structdp__specan__state__t.md) \* state, double center) <br>_Retune the display center without rebuilding the chain._  |
|  int | [**dp\_specan\_set\_state**](#function-dp_specan_set_state) ([**dp\_specan\_state\_t**](structdp__specan__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**dp\_specan\_state\_bytes**](#function-dp_specan_state_bytes) (const [**dp\_specan\_state\_t**](structdp__specan__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**SPECAN\_STATE\_MAGIC**](specan__core_8h.md#define-specan_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('S','P','A','N')`<br> |
| define  | [**SPECAN\_STATE\_VERSION**](specan__core_8h.md#define-specan_state_version)  `1u`<br> |

## Detailed Description


A streaming spectrum analyzer that speaks the _instrument_ parameters an operator already knows — center frequency, span, resolution bandwidth (RBW), and reference level — instead of the DSP knobs (window length, Kaiser beta, zero-pad factor) underneath them. It is the C-first home for the mapping that `doppler.specan`'s engine used to hand-roll in Python.


It composes the existing library, re-implementing nothing:



```C++
cf32 in (fs_in)  →  Ddc  (mix center→DC, decimate to fs_out = span·1.28)
                 →  PSD (window → zero-pad FFT → cg²-normalised power,
                           averaged over `navg` segments)
                 →  crop to the central ±span/2 display band
                 →  dB + ref offset  →  float display spectrum
```




* [**dp\_ddc\_state\_t**](ddc__core_8h.md#typedef-dp_ddc_state_t) is the tuner/decimator (LO mix + RateConverter cascade); retuning the center is a cheap, seamless LO phase change.
* [**dp\_psd\_state\_t**](structdp__psd__state__t.md) is the one averaging-PSD core shared with the measurement suite; `navg = 1` gives a responsive single-periodogram frame, larger `navg` trades update rate for a smoother, lower-variance trace.




The display band length and the bin→frequency map are fixed at create time: bin `i` of the returned spectrum maps to `center + (i − disp_n/2)·fs_out/nfft` Hz. Peaks are intentionally NOT computed here — compose [**dp\_find\_peaks\_f32**](spectral__core_8h.md#function-dp_find_peaks_f32) on the returned trace.


Lifecycle: create → (execute / retune / reset)\* → destroy.



```C++
// 200 kHz span, 500 Hz RBW around DC of a 2.048 MHz cf32 stream
dp_specan_state_t *sa = dp_specan_create(2.048e6, 200e3, 500.0, 0.0, 0.0,
                                   0.0, 1, 1);
float disp[8192];
size_t n = dp_specan_execute(sa, iq, 65536, disp, 8192);  // 0 until a frame
dp_specan_destroy(sa);
```
 


    
## Public Functions Documentation




### function dp\_specan\_create 

_Create a natural-parameter spectrum analyzer._ 
```C++
dp_specan_state_t * dp_specan_create (
    double fs,
    double span,
    double rbw,
    double src_center,
    double center,
    double offset_db,
    double full_scale,
    size_t bits,
    int window,
    size_t navg
) 
```



Derives the DSP from the instrument parameters: `fs_out = min(span·1.28, fs)`, `n = next_pow_two(ceil(fs_out/rbw))` (the coarse RBW knob), a Kaiser `beta` solved so the window ENBW realises `rbw` (the fine knob), `nfft = next_pow_two(2·n)`, and the central display crop covering ±span/2.




**Parameters:**


* `fs` Input sample rate (Hz). Must be &gt; 0. 
* `span` Display span (Hz). Must be &gt; 0. 
* `rbw` Resolution bandwidth (Hz). Must be &gt; 0. 
* `src_center` Source center frequency (Hz); the input band is centred here, so the analyzer mixes (center − src\_center) to DC. 
* `center` Desired display center frequency (Hz). 
* `offset_db` Additive dB offset on the display spectrum, applied on top of dBFS (e.g. a dBm calibration the application computes from a reference level). 
* `full_scale` Amplitude that reads 0 dBFS (&gt; 0). Ignored if bits &gt; 0. 
* `bits` ADC depth: bits&gt;0 sets the 0-dBFS reference to 2^(bits-1) in the shared PSD core (the single source of truth for the dBFS reference). 
* `window` Window index: 0 = Hann, 1 = Kaiser (RBW-trimmable). 
* `navg` Segments averaged per emitted frame (&gt;= 1). 



**Returns:**

Heap-allocated state, or NULL on invalid argument or OOM. 




**Note:**

Caller must call [**dp\_specan\_destroy()**](specan__core_8h.md#function-dp_specan_destroy) when done. Argument order keeps the required parameters (fs, span, rbw) first, matching the generated constructor's hoisting of jm `required` init params.



```C++
>>> from doppler.analyzer import Specan
>>> sa = Specan(fs=2.048e6, span=200e3, rbw=500.0)
>>> sa.fs_out
256000.0
>>> sa.nfft == 2 * sa.n
True
```
 


        

<hr>



### function dp\_specan\_destroy 

_Destroy a Specan instance and release all memory._ 
```C++
void dp_specan_destroy (
    dp_specan_state_t * state
) 
```





**Parameters:**


* `state` May be NULL (no-op). 




        

<hr>



### function dp\_specan\_execute 

_Mix, decimate, average and return one display spectrum, or nothing._ 
```C++
size_t dp_specan_execute (
    dp_specan_state_t * state,
    const float _Complex * x,
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



```C++
>>> from doppler.analyzer import Specan
>>> import numpy as np
>>> sa = Specan(fs=2.048e6, span=200e3, rbw=500.0, navg=1)
>>> sa.execute(np.zeros(64, dtype=np.complex64)) is None  # too few
True
>>> frame = sa.execute(np.zeros(65536, dtype=np.complex64))
>>> frame.shape, frame.dtype
((801,), dtype('float32'))
```
 


        

<hr>



### function dp\_specan\_execute\_max\_out 

_Output capacity hint for_ [_**dp\_specan\_execute()**_](specan__core_8h.md#function-dp_specan_execute) _; equals disp\_n._
```C++
size_t dp_specan_execute_max_out (
    dp_specan_state_t * state
) 
```




<hr>



### function dp\_specan\_get\_state 

```C++
void dp_specan_get_state (
    const dp_specan_state_t * state,
    void * blob
) 
```




<hr>



### function dp\_specan\_reset 

_Drop pending samples and the running average; LO/filter history zero._ 
```C++
void dp_specan_reset (
    dp_specan_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function dp\_specan\_retune 

_Retune the display center without rebuilding the chain._ 
```C++
void dp_specan_retune (
    dp_specan_state_t * state,
    double center
) 
```



Updates the Ddc LO phase increment (seamless across blocks — no resampler or window reset) and drops pending samples so the next frame reflects only the new tuning. Changing the span or RBW requires a destroy + create (the decimation rate and window length change).




**Parameters:**


* `state` Must be non-NULL. 
* `center` New display center frequency (Hz). 




        

<hr>



### function dp\_specan\_set\_state 

```C++
int dp_specan_set_state (
    dp_specan_state_t * state,
    const void * blob
) 
```




<hr>



### function dp\_specan\_state\_bytes 

```C++
size_t dp_specan_state_bytes (
    const dp_specan_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define SPECAN\_STATE\_MAGIC 

```C++
#define SPECAN_STATE_MAGIC `DP_FOURCC ('S','P','A','N')`
```




<hr>



### define SPECAN\_STATE\_VERSION 

```C++
#define SPECAN_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/specan/specan_core.h`

