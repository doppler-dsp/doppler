

# File wfm\_dsp.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_dsp.h**](wfm__dsp_8h.md)

[Go to the source code of this file](wfm__dsp_8h_source.md)

_DSSS spreading + root-raised-cosine pulse shaping (Phase B)._ [More...](#detailed-description)

* `#include "clib_common.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**wfm\_dsss\_spread**](#function-wfm_dsss_spread) (const float \_Complex \* syms, size\_t n\_sym, const uint8\_t \* code, size\_t sf, float \_Complex \* out) <br>_Spread_ `n_sym` _complex data symbols by a binary PN code._ |
|  void | [**wfm\_rrc\_taps**](#function-wfm_rrc_taps) (double beta, int sps, int span, float \* taps) <br>_Fill_ `taps` _with a unit-energy root-raised-cosine impulse response._ |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**wfm\_rrc\_ntaps**](#function-wfm_rrc_ntaps) (int sps, int span) <br>_Number of taps a_ `wfm_rrc_taps` _call produces:_`2*span*sps + 1` _._ |


























## Detailed Description


Two pure DSP primitives the engine/composer use to build spread-spectrum and band-limited waveforms:
* wfm\_dsss\_spread: multiply each data symbol by a PN chip code.
* wfm\_rrc\_taps: a unit-energy root-raised-cosine FIR (matched-filter pulse shape), applied by upsample + FIR. 




    
## Public Functions Documentation




### function wfm\_dsss\_spread 

_Spread_ `n_sym` _complex data symbols by a binary PN code._
```C++
void wfm_dsss_spread (
    const float _Complex * syms,
    size_t n_sym,
    const uint8_t * code,
    size_t sf,
    float _Complex * out
) 
```



`out[i*sf + j] = syms[i] * (code[j] ? -1 : +1)` — each symbol is repeated across `sf` chips, sign-flipped per code chip. Output length is `n_sym*sf`. Works for BPSK (real syms) and QPSK (complex syms).




**Parameters:**


* `syms` complex data symbols;
* `n_sym` their count. 
* `code` PN chip code (0/1), length `sf`;
* `sf` spreading factor. 
* `out` output chips, length `n_sym * sf`. 




        

<hr>



### function wfm\_rrc\_taps 

_Fill_ `taps` _with a unit-energy root-raised-cosine impulse response._
```C++
void wfm_rrc_taps (
    double beta,
    int sps,
    int span,
    float * taps
) 
```



Length is `wfm_rrc_ntaps(sps, span)`; the response is symmetric about the centre tap and normalised so `sum(taps^2) == 1` (so cascading TX·RX gives a Nyquist raised cosine). The `t = 0` and `t = ±1/(4β)` singularities are handled by their closed-form limits.




**Parameters:**


* `beta` roll-off in [0, 1]. 
* `sps` samples per symbol (&gt;= 1). 
* `span` one-sided span in symbols (&gt;= 1). 
* `taps` output array of length `wfm_rrc_ntaps(sps, span)`. 




        

<hr>
## Public Static Functions Documentation




### function wfm\_rrc\_ntaps 

_Number of taps a_ `wfm_rrc_taps` _call produces:_`2*span*sps + 1` _._
```C++
static inline size_t wfm_rrc_ntaps (
    int sps,
    int span
) 
```





**Parameters:**


* `sps` samples per symbol (&gt;= 1). 
* `span` one-sided filter span in symbols (&gt;= 1). 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_dsp.h`

