

# File timing\_est\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**timing\_est**](dir_ef0d777ccea6407e2e2ec8e704716d8f.md) **>** [**timing\_est\_core.h**](timing__est__core_8h.md)

[Go to the source code of this file](timing__est__core_8h_source.md)

_Feedforward symbol-timing estimator: a block of matched-filtered samples in, one estimate of the symbol timing out, with no loop._ [More...](#detailed-description)

* `#include "doppler/RateConverter/RateConverter_core.h"`
* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/nco/nco_core.h"`
* `#include "doppler/resamp/resamp_core.h"`
* `#include "doppler/hbdecim/hbdecim_core.h"`
* `#include "doppler/cic/cic_core.h"`
* `#include "doppler/fir/fir_core.h"`
* `#include "doppler/resample/resample_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_timing\_est\_state\_t**](structdp__timing__est__state__t.md) <br>_SymbolTimingEstimator state._  |
| struct | [**dp\_timing\_estimate\_t**](structdp__timing__estimate__t.md) <br>_One feedforward symbol-timing estimate._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) void | [**dp\_timing\_est\_accumulate**](#function-dp_timing_est_accumulate) ([**dp\_timing\_est\_state\_t**](structdp__timing__est__state__t.md) \* state, double power) <br>_Add one sample's power to the block in progress and advance the symbol clock by one sample._  |
|  [**dp\_timing\_est\_state\_t**](structdp__timing__est__state__t.md) \* | [**dp\_timing\_est\_create**](#function-dp_timing_est_create) (double sps) <br>_Create an estimator whose symbol clock runs at_ `sps` _samples a symbol._ |
|  [**dp\_timing\_est\_state\_t**](structdp__timing__est__state__t.md) \* | [**dp\_timing\_est\_create\_matched**](#function-dp_timing_est_create_matched) (double sps, int pulse, double beta, size\_t span) <br>_Create an estimator with a matched filter of its own, for a stream of_ `sps` _samples a symbol that nothing has filtered._ |
|  void | [**dp\_timing\_est\_destroy**](#function-dp_timing_est_destroy) ([**dp\_timing\_est\_state\_t**](structdp__timing__est__state__t.md) \* state) <br>_Destroy a timing\_est instance and release all memory._  |
|  [**dp\_timing\_estimate\_t**](structdp__timing__estimate__t.md) | [**dp\_timing\_est\_estimate**](#function-dp_timing_est_estimate) ([**dp\_timing\_est\_state\_t**](structdp__timing__est__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in) <br>_Estimate the symbol timing from one block of matched-filtered samples._  |
|  [**dp\_timing\_estimate\_t**](structdp__timing__estimate__t.md) | [**dp\_timing\_est\_finish**](#function-dp_timing_est_finish) ([**dp\_timing\_est\_state\_t**](structdp__timing__est__state__t.md) \* state) <br>_Close the block in progress and return its estimate._  |
|  void | [**dp\_timing\_est\_get\_state**](#function-dp_timing_est_get_state) (const [**dp\_timing\_est\_state\_t**](structdp__timing__est__state__t.md) \* state, void \* blob) <br>_Write the running state into_ `blob:` _the symbol clock, the sample count, the block in progress and the previous block._ |
|  void | [**dp\_timing\_est\_reset**](#function-dp_timing_est_reset) ([**dp\_timing\_est\_state\_t**](structdp__timing__est__state__t.md) \* state) <br>_Return the symbol clock to zero and forget the previous block and any block in progress; keep_ `sps` _._ |
|  int | [**dp\_timing\_est\_set\_state**](#function-dp_timing_est_set_state) ([**dp\_timing\_est\_state\_t**](structdp__timing__est__state__t.md) \* state, const void \* blob) <br>_Restore the running state from_ `blob` _._ |
|  size\_t | [**dp\_timing\_est\_state\_bytes**](#function-dp_timing_est_state_bytes) (const [**dp\_timing\_est\_state\_t**](structdp__timing__est__state__t.md) \* state) <br>_Bytes in the blob_ [_**dp\_timing\_est\_get\_state()**_](timing__est__core_8h.md#function-dp_timing_est_get_state) _writes._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**TIMING\_EST\_MATCHED\_PHASES**](timing__est__core_8h.md#define-timing_est_matched_phases)  `1024u`<br>_Arms in the matched flavor's terminal bank: the fixed-rate stage's timing resolution, 1 / 1024 of an output._  |
| define  | [**TIMING\_EST\_MATCHED\_SPS**](timing__est__core_8h.md#define-timing_est_matched_sps)  `4.0`<br>_Samples a symbol the matched flavor's converter delivers, and its symbol clock runs at._  |
| define  | [**TIMING\_EST\_STATE\_MAGIC**](timing__est__core_8h.md#define-timing_est_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('T', 'M', 'G', 'E')`<br>_State-blob identity (see_ [_**dp\_state.h**_](dp__state_8h.md) _)._ |
| define  | [**TIMING\_EST\_STATE\_VERSION**](timing__est__core_8h.md#define-timing_est_state_version)  `/* multi line expression */`<br>_State-blob layout version._  |

## Detailed Description


The power of a matched-filtered data stream is periodic at the symbol rate, with its maximum at the eye center. Over a block this object forms the Fourier coefficient of that power at the symbol rate,  and reads the timing from its phase and the estimate's quality from its size (Oerder and Meyr, 1988). `phi` is the object's SYMBOL CLOCK: a 32-bit phase word advanced once a sample by the NCO's own increment for `1 / sps` cycles a sample, so `sps` may be any real number above 2.


What it promises:



* **One estimate a block, in the block's own length.** There is no pull-in: the time an estimate takes is the block the caller hands it, whatever the timing was.
* **The carrier does not enter.** Only `|x|^2` is used, so the estimate is the same for any carrier phase and for a frequency offset, which is what lets it run before carrier recovery. (An offset still costs the caller's matched filter amplitude; that loss is the filter's.)
* **One symbol clock across calls.** The clock is not restarted between blocks, so consecutive estimates are comparable, and their difference is the symbol rate's error.
* **Nothing is decided.** No threshold is applied: `quality` is reported and the caller judges it.




It filters, resamples and decides nothing; the caller owns the matched filter. The reasoning, the predicted variance and what is not yet known are in docs/design/timing-estimator.md.


**With a matched filter of its own.** [**dp\_timing\_est\_create\_matched()**](timing__est__core_8h.md#function-dp_timing_est_create_matched) takes the pulse and the input's samples a symbol and puts a matched `RateConverter` ahead of the clock: its decimating cascade, then its matched terminal stage at a fixed rate to 4 samples a symbol, which nothing steers. [**dp\_timing\_est\_estimate()**](timing__est__core_8h.md#function-dp_timing_est_estimate) then takes the samples as they arrive, for any real number of them a symbol from 4 up. It skips the converter's own start ([**dp\_RateConverter\_get\_memory()**](RateConverter__core_8h.md#function-dp_rateconverter_get_memory)) before the first block counts, and every estimate says where the eye center sits on the INPUT's timeline (`eye`), which is what lets a second converter on the same samples be set to it.


Lifecycle: create -&gt; (estimate \| accumulate.. finish \| reset)\* -&gt; destroy


Example: 
```C++
dp_timing_est_state_t *te = dp_timing_est_create (4.0);
float _Complex x[512] = { 0 };       // 128 symbols of your stream
dp_timing_estimate_t e = dp_timing_est_estimate (te, x, 512);
(void)e.tau;                         // eye center, symbols
dp_timing_est_destroy (te);
```
 


    
## Public Functions Documentation




### function dp\_timing\_est\_accumulate 

_Add one sample's power to the block in progress and advance the symbol clock by one sample._ 
```C++
JM_FORCEINLINE void dp_timing_est_accumulate (
    dp_timing_est_state_t * state,
    double power
) 
```



The form a composing object uses when it has the power but not a block of samples (a matched bank read at several timing hypotheses a symbol hands its `|y|^2` here, one hypothesis a call). [**dp\_timing\_est\_estimate()**](timing__est__core_8h.md#function-dp_timing_est_estimate) is this in a loop followed by [**dp\_timing\_est\_finish()**](timing__est__core_8h.md#function-dp_timing_est_finish).




**Parameters:**


* `state` Must be non-NULL. 
* `power` `|x|^2` of the sample; not checked. 




        

<hr>



### function dp\_timing\_est\_create 

_Create an estimator whose symbol clock runs at_ `sps` _samples a symbol._
```C++
dp_timing_est_state_t * dp_timing_est_create (
    double sps
) 
```



The bound on `sps` is the line's own: the power is sampled `sps` times a symbol and its symbol-rate line sits at `1 / sps` cycles a sample, which is below the Nyquist rate only for `sps > 2`. How close to 2 a given pulse allows is a property of the pulse, not of this object.




**Parameters:**


* `sps` Samples per symbol of the stream handed to it; finite and above 2 (default: 4.0). 



**Returns:**

Heap-allocated state, or NULL when `sps` is not finite, is 2 or less, or allocation fails. 




**Note:**

Caller must call [**dp\_timing\_est\_destroy()**](timing__est__core_8h.md#function-dp_timing_est_destroy) when done. 
```C++
>>> from doppler.track import SymbolTimingEstimator
>>> te = SymbolTimingEstimator(sps=4.0)
>>> te.sps
4.0
```
 





        

<hr>



### function dp\_timing\_est\_create\_matched 

_Create an estimator with a matched filter of its own, for a stream of_ `sps` _samples a symbol that nothing has filtered._
```C++
dp_timing_est_state_t * dp_timing_est_create_matched (
    double sps,
    int pulse,
    double beta,
    size_t span
) 
```



Builds one matched `RateConverter` for the rate `TIMING_EST_MATCHED_SPS / sps`, droop compensation on, and runs the symbol clock on its output at TIMING\_EST\_MATCHED\_SPS samples a symbol. The converter's terminal stage is the matched filter and runs at that fixed rate: nothing steers it. [**dp\_timing\_est\_estimate()**](timing__est__core_8h.md#function-dp_timing_est_estimate) then takes the unfiltered samples, and its record's `symbols` counts the converter outputs the block gave, so a block that ends inside the lead-in reports fewer symbols than it was handed (none, while the lead-in lasts).




**Parameters:**


* `sps` Samples per symbol of the input; finite, and TIMING\_EST\_MATCHED\_SPS or more (the terminal stage does not interpolate). 
* `pulse` RC\_PULSE\_IANDD (rectangle) or RC\_PULSE\_RRC. 
* `beta` Root-raised-cosine roll-off, 0 to 1; ignored for the rectangle. 
* `span` One-sided root-raised-cosine span in symbols, 1 or more; ignored for the rectangle. 



**Returns:**

Heap-allocated state, or NULL when `sps` is not finite or is below TIMING\_EST\_MATCHED\_SPS, the pulse is neither, `beta` or `span` is out of range, or allocation fails. 




**Note:**

Caller must call [**dp\_timing\_est\_destroy()**](timing__est__core_8h.md#function-dp_timing_est_destroy) when done.



```C++
>>> import numpy as np
>>> from doppler.track import MatchedSymbolTimingEstimator
>>> n = np.arange(4096)
>>> # +1 -1 +1 ... through a root-raised cosine at 8 samples a symbol,
>>> # with a symbol instant at sample 3 and every 8 after it
>>> x = (0.25 * np.cos(np.pi * (n - 3.0) / 8.0)).astype(np.complex64)
>>> te = MatchedSymbolTimingEstimator(sps=8.0, pulse="rrc")
>>> e = te.estimate(x)
>>> bool(abs(e.eye % 8.0 - 3.0) < 0.1), e.symbols
(True, 490.25)
```
 


        

<hr>



### function dp\_timing\_est\_destroy 

_Destroy a timing\_est instance and release all memory._ 
```C++
void dp_timing_est_destroy (
    dp_timing_est_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_timing\_est\_estimate 

_Estimate the symbol timing from one block of matched-filtered samples._ 
```C++
dp_timing_estimate_t dp_timing_est_estimate (
    dp_timing_est_state_t * state,
    const float _Complex * in,
    size_t n_in
) 
```



The block is the whole call: its length sets the estimate's variance and the time the estimate takes. Consecutive calls stay on one symbol clock, and from the second call that had power the result carries the symbol rate's error.




**Parameters:**


* `state` Must be non-NULL. 
* `in` Matched-filtered samples, `sps` a symbol. 
* `n_in` Number of samples; 0 returns a record of zeros. 



**Returns:**

The block's estimate. 
```C++
>>> import numpy as np
>>> from doppler.track import SymbolTimingEstimator
>>> n = np.arange(4 * 256)
>>> # power peaking a quarter symbol into each symbol, on any carrier
>>> x = np.sqrt(1.0 + 0.5 * np.cos(2 * np.pi * (n / 4 - 0.25)))
>>> x = (x * np.exp(1j * 0.3 * n)).astype(np.complex64)
>>> e = SymbolTimingEstimator(sps=4.0).estimate(x)
>>> round(e.tau, 3), round(e.quality, 3), e.symbols, e.has_rate
(0.25, 0.25, 256.0, 0)
```
 





        

<hr>



### function dp\_timing\_est\_finish 

_Close the block in progress and return its estimate._ 
```C++
dp_timing_estimate_t dp_timing_est_finish (
    dp_timing_est_state_t * state
) 
```



A block with no samples returns a record of zeros and changes nothing. A block with no power returns `tau` 0 and `quality` 0 and is not kept as the previous block, so the next `rate` is taken against the last block that had power.




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The estimate for the samples accumulated since the last finish. 





        

<hr>



### function dp\_timing\_est\_get\_state 

_Write the running state into_ `blob:` _the symbol clock, the sample count, the block in progress and the previous block._
```C++
void dp_timing_est_get_state (
    const dp_timing_est_state_t * state,
    void * blob
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `blob` At least [**dp\_timing\_est\_state\_bytes()**](timing__est__core_8h.md#function-dp_timing_est_state_bytes) bytes. 




        

<hr>



### function dp\_timing\_est\_reset 

_Return the symbol clock to zero and forget the previous block and any block in progress; keep_ `sps` _._
```C++
void dp_timing_est_reset (
    dp_timing_est_state_t * state
) 
```



The next estimate is then the first again: its `tau` is on a clock that starts at the next sample, and it carries no `rate`.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.track import SymbolTimingEstimator
>>> n = np.arange(1024)
>>> x = np.sqrt(1.0 + 0.5 * np.cos(2 * np.pi * n / 4)).astype(np.complex64)
>>> te = SymbolTimingEstimator(sps=4.0)
>>> te.estimate(x).has_rate, te.estimate(x).has_rate
(0, 1)
>>> te.reset()
>>> te.estimate(x).has_rate
0
```
 




        

<hr>



### function dp\_timing\_est\_set\_state 

_Restore the running state from_ `blob` _._
```C++
int dp_timing_est_set_state (
    dp_timing_est_state_t * state,
    const void * blob
) 
```





**Parameters:**


* `state` Must be non-NULL; created with any `sps` (the blob carries no config, so the resumed clock runs at this object's). 
* `blob` A blob [**dp\_timing\_est\_get\_state()**](timing__est__core_8h.md#function-dp_timing_est_get_state) wrote. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID for a blob of another kind, version, size or byte order, or one whose fields are not a state this object reaches (a non-finite sum, a negative power, a previous `tau` outside (-1/2, 1/2], a block longer than the count); the state is then unchanged. 





        

<hr>



### function dp\_timing\_est\_state\_bytes 

_Bytes in the blob_ [_**dp\_timing\_est\_get\_state()**_](timing__est__core_8h.md#function-dp_timing_est_get_state) _writes._
```C++
size_t dp_timing_est_state_bytes (
    const dp_timing_est_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The blob's size: the envelope and the running fields. 





        

<hr>
## Macro Definition Documentation





### define TIMING\_EST\_MATCHED\_PHASES 

_Arms in the matched flavor's terminal bank: the fixed-rate stage's timing resolution, 1 / 1024 of an output._ 
```C++
#define TIMING_EST_MATCHED_PHASES `1024u`
```




<hr>



### define TIMING\_EST\_MATCHED\_SPS 

_Samples a symbol the matched flavor's converter delivers, and its symbol clock runs at._ 
```C++
#define TIMING_EST_MATCHED_SPS `4.0`
```




<hr>



### define TIMING\_EST\_STATE\_MAGIC 

_State-blob identity (see_ [_**dp\_state.h**_](dp__state_8h.md) _)._
```C++
#define TIMING_EST_STATE_MAGIC `DP_FOURCC ('T', 'M', 'G', 'E')`
```




<hr>



### define TIMING\_EST\_STATE\_VERSION 

_State-blob layout version._ 
```C++
#define TIMING_EST_STATE_VERSION `/* multi line expression */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/timing_est/timing_est_core.h`

