

# File wfm\_dsp.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_dsp.h**](wfm__dsp_8h.md)

[Go to the source code of this file](wfm__dsp_8h_source.md)

_DSSS spreading + root-raised-cosine pulse shaping (Phase B)._ [More...](#detailed-description)

* `#include "clib_common.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**wfm\_dsss\_spread**](#function-wfm_dsss_spread) (const float \_Complex \* syms, size\_t n\_sym, const uint8\_t \* code, size\_t sf, float \_Complex \* out) <br>_Spread_ `n_sym` _complex data symbols by a binary PN code._ |
|  size\_t | [**wfm\_frame\_dsss\_chips**](#function-wfm_frame_dsss_chips) (const uint8\_t \* acq\_code, size\_t acq\_len, size\_t acq\_reps, const uint8\_t \* data\_code, size\_t data\_len, const uint8\_t \* sync, size\_t sync\_len, const uint8\_t \* payload, size\_t payload\_len, int crc, uint8\_t \* out) <br>_Build a two-code DSSS burst as one flat 0/1 chip pattern._  |
|  size\_t | [**wfm\_frame\_dsss\_nchips**](#function-wfm_frame_dsss_nchips) (size\_t acq\_len, size\_t acq\_reps, size\_t data\_len, size\_t sync\_len, size\_t payload\_len, int crc) <br>_Chip count of a DSSS burst frame (sizes_ `wfm_frame_dsss_chips` _)._ |
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



### function wfm\_frame\_dsss\_chips 

_Build a two-code DSSS burst as one flat 0/1 chip pattern._ 
```C++
size_t wfm_frame_dsss_chips (
    const uint8_t * acq_code,
    size_t acq_len,
    size_t acq_reps,
    const uint8_t * data_code,
    size_t data_len,
    const uint8_t * sync,
    size_t sync_len,
    const uint8_t * payload,
    size_t payload_len,
    int crc,
    uint8_t * out
) 
```



The transmit side of `burst_demod`'s frame contract, assembled in one place so TX and RX can never drift:


`[ acq_code × acq_reps | (sync | payload | crc16(payload)) ⊕ data_code ]`


The preamble is the _unmodulated_ repeated acquisition code (no data on it — a pure coherent-integration target). Every frame bit is then spread by the (distinct) data code: chip `j` of frame bit `b` is `b ^ data_code[j]`. The CRC-16-CCITT trailer ([**dp\_crc16.h**](dp__crc16_8h.md)) is computed over the payload bits only and spread MSB-first. Mapping chips to ±1 (BPSK) is the synth's job.




**Parameters:**


* `acq_code` preamble code (0/1), length `acq_len`; NULL when `acq_len*acq_reps == 0`. 
* `acq_len` preamble code length in chips. 
* `acq_reps` preamble repetitions. 
* `data_code` payload spreading code (0/1), length `data_len`. 
* `data_len` chips per frame symbol (the spreading factor). 
* `sync` frame-sync word bits (0/1), length `sync_len`; NULL ok. 
* `sync_len` sync word length in bits. 
* `payload` payload bits (0/1), length `payload_len`; NULL ok. 
* `payload_len` payload length in bits. 
* `crc` non-zero: append the CRC-16 trailer after the payload. 
* `out` output chip array (0/1) of `wfm_frame_dsss_nchips(...)` elements. 



**Returns:**

Chips written, or 0 on invalid geometry (see `wfm_frame_dsss_nchips`). 





        

<hr>



### function wfm\_frame\_dsss\_nchips 

_Chip count of a DSSS burst frame (sizes_ `wfm_frame_dsss_chips` _)._
```C++
size_t wfm_frame_dsss_nchips (
    size_t acq_len,
    size_t acq_reps,
    size_t data_len,
    size_t sync_len,
    size_t payload_len,
    int crc
) 
```



`acq_len*acq_reps + (sync_len + payload_len + crc_bits) * data_len`, where `crc_bits` is 16 when `crc` is set and there are payload bits, else 0 (a CRC over nothing protects nothing). Returns 0 when the geometry is invalid: frame bits present but no data code, or nothing to transmit at all.




**Parameters:**


* `acq_len` preamble code length in chips (0 = no preamble). 
* `acq_reps` preamble repetitions (0 = no preamble). 
* `data_len` payload spreading-code length (chips per symbol). 
* `sync_len` frame-sync word length in bits (0 = none). 
* `payload_len` payload length in bits. 
* `crc` non-zero: a CRC-16 trailer follows the payload. 



**Returns:**

Total burst chips, or 0 if the geometry is invalid/empty. 





        

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


* `beta` roll-off in `[0, 1]`. 
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

