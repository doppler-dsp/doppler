

# File carrier\_acq\_core.h



[**FileList**](files.md) **>** [**carrier\_acq**](dir_fda2da85aa46b94cfd09d911f4a8e3eb.md) **>** [**carrier\_acq\_core.h**](carrier__acq__core_8h.md)

[Go to the source code of this file](carrier__acq__core_8h_source.md)

_CarrierAcquisition — PSDMF residual-carrier frequency refinement._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "psd/psd_core.h"`
* `#include "detector/detector_core.h"`
* `#include "detection/detection_core.h"`
* `#include "spectral/spectral_core.h"`
* `#include "corr/corr_core.h"`
* `#include "fft/fft_core.h"`
* `#include "acc_trace/acc_trace_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**carrier\_acq\_state\_t**](structcarrier__acq__state__t.md) <br>_CarrierAcquisition state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**carrier\_acq\_state\_t**](structcarrier__acq__state__t.md) \* | [**carrier\_acq\_create**](#function-carrier_acq_create) (double sample\_rate\_hz, double symbol\_rate\_hz, double resolution\_hz, size\_t zero\_pad, int window, float beta, const float \* psd\_template, size\_t psd\_template\_len, double pfa, double pd, double design\_snr, bool sequential, size\_t max\_n\_blocks) <br>_Create a carrier\_acq instance._  |
|  void | [**carrier\_acq\_destroy**](#function-carrier_acq_destroy) ([**carrier\_acq\_state\_t**](structcarrier__acq__state__t.md) \* state) <br>_Destroy a carrier\_acq instance and release all memory._  |
|  void | [**carrier\_acq\_get\_state**](#function-carrier_acq_get_state) (const [**carrier\_acq\_state\_t**](structcarrier__acq__state__t.md) \* state, void \* blob) <br> |
|  void | [**carrier\_acq\_reset**](#function-carrier_acq_reset) ([**carrier\_acq\_state\_t**](structcarrier__acq__state__t.md) \* state) <br>_Reset to the post-create state: discard the running PSD average and detection state; n\_blocks/ready/residual\_hz return to their initial values. Config (psd/detector/dwell\_target) is untouched._  |
|  int | [**carrier\_acq\_set\_state**](#function-carrier_acq_set_state) ([**carrier\_acq\_state\_t**](structcarrier__acq__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**carrier\_acq\_state\_bytes**](#function-carrier_acq_state_bytes) (const [**carrier\_acq\_state\_t**](structcarrier__acq__state__t.md) \* state) <br> |
|  void | [**carrier\_acq\_steps**](#function-carrier_acq_steps) ([**carrier\_acq\_state\_t**](structcarrier__acq__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len) <br>_Fold raw complex samples into the running PSD average and test for a detection. Accepts any chunk size across repeated calls_  _a partial trailing block is carried to the next call. A no-op once ready is true or the give-up cap (max\_n\_blocks in sequential mode, dwell\_target otherwise) has been reached._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CARRIER\_ACQ\_STATE\_MAGIC**](carrier__acq__core_8h.md#define-carrier_acq_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc)('C', 'A', 'Q', 'R')`<br> |
| define  | [**CARRIER\_ACQ\_STATE\_VERSION**](carrier__acq__core_8h.md#define-carrier_acq_state_version)  `1u`<br> |

## Detailed Description


Runs AFTER Acquisition's own coarse Doppler search, as a one-shot matched-filter refinement stage: feed the already-despread symbol-rate stream via steps(), and once enough non-coherent looks have accumulated to cross a Pfa/Pd-driven detection threshold, ready becomes true and residual\_hz holds the sub-bin-refined residual carrier estimate, Hz.


Composes existing primitives rather than reimplementing them:



* [**psd\_state\_t**](structpsd__state__t.md): FFT + window + zero-pad + non-coherent power averaging (Welch's method)  the entire "measure the average
    power spectrum of what's coming in" half of the algorithm.
* [**detector\_state\_t**](structdetector__state__t.md): FFT-based circular correlation of the averaged power spectrum against a known template (the average PSD shape of a random rectangular-pulse BPSK symbol stream by default, or a caller-supplied override for a different pulse/modulation) plus a noise-referenced test statistic and argmax lag.
* detection\_core's [**det\_n\_noncoh()**](detection__core_8h.md#function-det_n_noncoh)/det\_threshold(): det\_n\_noncoh (the same chi-square statistic Acquisition's own auto-config uses) drives the precomputed fixed-dwell/give-up cap; det\_threshold (sqrt(-2\*ln(pfa))) is reused as the tail-quantile stand-in inside the per-block CFAR ratio threshold below.




The per-block CFAR ratio threshold is NOT [**det\_threshold\_noncoherent()**](detection__core_8h.md#function-det_threshold_noncoherent) (that statistic  a classic complex-correlator peak/noise envelope ratio  does not transfer to this object's real statistic, a power-spectrum-vs-known-template correlation; confirmed via Monte Carlo, ~5x too conservative). carrier\_acq\_ratio\_threshold() (carrier\_acq\_core.c) instead uses the derived H0 model for this specific statistic (an exact Gamma-sum mean/variance for the averaged, template-correlated periodogram) plus ONE empirically-calibrated tail-inflation constant (kappa, standing in for the argmax-over-nfft-correlated-lags extreme- value quantile a full closed form hasn't cleanly reduced to yet  see FINISHING\_PLAN.md's CarrierAcquisition section / the derive\_carrier\_acq\_statistic.py derivation for the full story, and revisit/refine kappa when time allows).


Only the default template generator (a sinc^2 shape, DC-centred to match [**psd\_power\_twosided()**](psd__core_8h.md#function-psd_power_twosided)'s own bin order) and the 3-point parabolic sub-bin peak refinement (read directly off [**detector\_state\_t**](structdetector__state__t.md)'s own out\_buf  no second correlation pass needed) are new leaf code.


Lifecycle: create -&gt; steps()\* -&gt; (ready ? residual\_hz : keep feeding) -&gt; reset()/destroy



```C++
carrier_acq_state_t *ca = carrier_acq_create(
    4.092e6, 100e3, 0.0, 4, 0, 0.0f, NULL, 0, 1e-3, 0.9, 2.0, true,
    100000);
while (!ca->ready && ca->n_blocks < ca->max_n_blocks)
    carrier_acq_steps(ca, block, block_len);
double hz = ca->residual_hz; // valid only when ca->ready
carrier_acq_destroy(ca);
```
 


    
## Public Functions Documentation




### function carrier\_acq\_create 

_Create a carrier\_acq instance._ 
```C++
carrier_acq_state_t * carrier_acq_create (
    double sample_rate_hz,
    double symbol_rate_hz,
    double resolution_hz,
    size_t zero_pad,
    int window,
    float beta,
    const float * psd_template,
    size_t psd_template_len,
    double pfa,
    double pd,
    double design_snr,
    bool sequential,
    size_t max_n_blocks
) 
```





**Parameters:**


* `sample_rate_hz` Sample rate of the input stream, Hz (required). 
* `symbol_rate_hz` Symbol rate, Hz  builds the default template (required). 
* `resolution_hz` Desired FFT frequency resolution, Hz. &lt;= 0.0 is a sentinel meaning "auto": symbol\_rate\_hz/10.0. 
* `zero_pad` PSD zero-pad factor (&gt;= 1); see [**psd\_core.h**](psd__core_8h.md). 
* `window` Enum index; 0=hann, 1=kaiser, 2=blackman-harris. 
* `beta` Kaiser beta (ignored for hann/blackman-harris). 
* `psd_template` Known PSD-shape template override, length must equal nfft = next\_pow2(round(sample\_rate\_hz /resolution\_hz) \* zero\_pad); NULL/length-0 means "not supplied"  the default rectangular-pulse sinc^2 template (from symbol\_rate\_hz) is used. 
* `psd_template_len` Length of `psd_template` (0 if not supplied). 
* `pfa` Target per-test false-alarm probability. 
* `pd` Target detection probability. 
* `design_snr` Assumed per-sample amplitude SNR used ONLY to precompute dwell\_target via [**det\_n\_noncoh()**](detection__core_8h.md#function-det_n_noncoh); not a live measurement. An optimistic guess only affects NON-sequential mode (which trusts this one-shot wait count outright)  sequential mode's own give-up bound is max\_n\_blocks, not dwell\_target, precisely so a wrong design\_snr can't stop it from trying more blocks once real data shows it needs to. 
* `sequential` True: test for a detection after EVERY block (the per-block CFAR ratio threshold  see carrier\_acq\_ratio\_threshold() in carrier\_acq\_core.c  tightens as more looks accumulate), stopping the moment one fires or max\_n\_blocks is reached. False: accumulate silently and test once, at dwell\_target. 
* `max_n_blocks` Sequential mode's own give-up cap (ignored by non-sequential mode, which stops at dwell\_target instead)  deliberately a SEPARATE, generous bound from dwell\_target; capping sequential mode at design\_snr's own point estimate would defeat the reason to test every block in the first place. 



**Returns:**

Heap-allocated state, or NULL on invalid argument or allocation failure. 




**Note:**

Caller must call [**carrier\_acq\_destroy()**](carrier__acq__core_8h.md#function-carrier_acq_destroy) when done. 
```C++
>>> import numpy as np
>>> from doppler.acquire import CarrierAcquisition
>>> rng = np.random.default_rng(12345)
>>> bits = np.where(rng.integers(0, 2, 4000), 1.0, -1.0)
>>> data = np.repeat(bits, 8)                 # 8 samples/symbol BPSK
>>> t = np.arange(len(data))
>>> x = (data * np.exp(2j * np.pi * 123.0 * t / 8000.0)).astype(
...     np.complex64)  # residual carrier at 123 Hz
>>> ca = CarrierAcquisition(
...     sample_rate_hz=8000.0, symbol_rate_hz=1000.0,
...     psd_template=np.array([], dtype=np.float32))
>>> ca.steps(x)                   # fold the stream, testing each block
>>> ca.ready                      # detection fired
True
>>> round(ca.residual_hz, 0)      # recovered residual carrier, Hz
123.0
```
 





        

<hr>



### function carrier\_acq\_destroy 

_Destroy a carrier\_acq instance and release all memory._ 
```C++
void carrier_acq_destroy (
    carrier_acq_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function carrier\_acq\_get\_state 

```C++
void carrier_acq_get_state (
    const carrier_acq_state_t * state,
    void * blob
) 
```




<hr>



### function carrier\_acq\_reset 

_Reset to the post-create state: discard the running PSD average and detection state; n\_blocks/ready/residual\_hz return to their initial values. Config (psd/detector/dwell\_target) is untouched._ 
```C++
void carrier_acq_reset (
    carrier_acq_state_t * state
) 
```



Use it to reuse one detector across successive captures: after a detection (or a give-up) the running average and counters are cleared, so the next steps() starts folding a fresh stream from zero.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.acquire import CarrierAcquisition
>>> ca = CarrierAcquisition(
...     sample_rate_hz=8000.0, symbol_rate_hz=1000.0,
...     psd_template=np.array([], dtype=np.float32))
>>> ca.steps(np.zeros(2048, dtype=np.complex64))  # accumulate looks
>>> ca.n_blocks > 0
True
>>> ca.reset()                    # discard the running PSD average
>>> ca.n_blocks
0
```
 




        

<hr>



### function carrier\_acq\_set\_state 

```C++
int carrier_acq_set_state (
    carrier_acq_state_t * state,
    const void * blob
) 
```




<hr>



### function carrier\_acq\_state\_bytes 

```C++
size_t carrier_acq_state_bytes (
    const carrier_acq_state_t * state
) 
```




<hr>



### function carrier\_acq\_steps 

_Fold raw complex samples into the running PSD average and test for a detection. Accepts any chunk size across repeated calls_  _a partial trailing block is carried to the next call. A no-op once ready is true or the give-up cap (max\_n\_blocks in sequential mode, dwell\_target otherwise) has been reached._
```C++
void carrier_acq_steps (
    carrier_acq_state_t * state,
    const float _Complex * x,
    size_t x_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Raw complex input samples (cf32). 
* `x_len` Number of samples in `x`. 
```C++
>>> import numpy as np
>>> from doppler.acquire import CarrierAcquisition
>>> rng = np.random.default_rng(12345)
>>> bits = np.where(rng.integers(0, 2, 4000), 1.0, -1.0)
>>> data = np.repeat(bits, 8)                 # 8 samples/symbol BPSK
>>> t = np.arange(len(data))
>>> x = (data * np.exp(2j * np.pi * 123.0 * t / 8000.0)).astype(
...     np.complex64)  # residual carrier at 123 Hz
>>> ca = CarrierAcquisition(
...     sample_rate_hz=8000.0, symbol_rate_hz=1000.0,
...     psd_template=np.array([], dtype=np.float32))
>>> ca.steps(x)                   # fold the stream, testing each block
>>> ca.ready
True
>>> round(ca.residual_hz, 0)      # recovered residual carrier, Hz
123.0
```
 




        

<hr>
## Macro Definition Documentation





### define CARRIER\_ACQ\_STATE\_MAGIC 

```C++
#define CARRIER_ACQ_STATE_MAGIC `DP_FOURCC ('C', 'A', 'Q', 'R')`
```




<hr>



### define CARRIER\_ACQ\_STATE\_VERSION 

```C++
#define CARRIER_ACQ_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/carrier_acq/carrier_acq_core.h`

