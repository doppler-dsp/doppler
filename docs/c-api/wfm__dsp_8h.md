

# File wfm\_dsp.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_dsp.h**](wfm__dsp_8h.md)

[Go to the source code of this file](wfm__dsp_8h_source.md)

_DSSS spreading + root-raised-cosine pulse shaping (Phase B)._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include <math.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**wfm\_cont\_dsss\_chips**](#function-wfm_cont_dsss_chips) (const uint8\_t \* code, size\_t code\_len, const uint8\_t \* data, size\_t n\_data, double chips\_per\_symbol, size\_t n\_chips, uint8\_t \* out) <br>_Build a CONTINUOUS, ASYNCHRONOUS DSSS chip pattern._  |
|  void | [**wfm\_dsss\_spread**](#function-wfm_dsss_spread) (const float \_Complex \* syms, size\_t n\_sym, const uint8\_t \* code, size\_t sf, float \_Complex \* out) <br>_Spread_ `n_sym` _complex data symbols by a binary PN code._ |
|  size\_t | [**wfm\_frame\_dsss\_chips**](#function-wfm_frame_dsss_chips) (const uint8\_t \* acq\_code, size\_t acq\_len, size\_t acq\_reps, const uint8\_t \* data\_code, size\_t data\_len, const uint8\_t \* sync, size\_t sync\_len, const uint8\_t \* payload, size\_t payload\_len, int crc, uint8\_t \* out) <br>_Build a two-code DSSS burst as one flat 0/1 chip pattern._  |
|  size\_t | [**wfm\_frame\_dsss\_nchips**](#function-wfm_frame_dsss_nchips) (size\_t acq\_len, size\_t acq\_reps, size\_t data\_len, size\_t sync\_len, size\_t payload\_len, int crc) <br>_Chip count of a DSSS burst frame (sizes_ `wfm_frame_dsss_chips` _)._ |
|  void | [**wfm\_polyphase\_bank**](#function-wfm_polyphase_bank) (const float \* proto, size\_t proto\_len, size\_t num\_phases, size\_t num\_taps, float \* bank) <br>_Deal an arbitrary FIR prototype into a polyphase interpolation bank._  |
|  void | [**wfm\_rrc\_polyphase\_bank**](#function-wfm_rrc_polyphase_bank) (double beta, int sps, int span, float \* bank) <br>_Decompose the RRC pulse shape into a polyphase interpolation bank._  |
|  void | [**wfm\_rrc\_taps**](#function-wfm_rrc_taps) (double beta, int sps, int span, float \* taps) <br>_Fill_ `taps` _with a unit-energy root-raised-cosine impulse response._ |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**wfm\_cont\_dsss\_nchips**](#function-wfm_cont_dsss_nchips) (size\_t n\_chips) <br>_Chip count for_ `wfm_cont_dsss_chips` _: exactly_`n_chips` _._ |
|  size\_t | [**wfm\_rrc\_bank\_ntaps**](#function-wfm_rrc_bank_ntaps) (int span) <br>_Number of taps per phase in a_ `wfm_rrc_polyphase_bank` _:_`2*span + 1` _._ |
|  double | [**wfm\_rrc\_h**](#function-wfm_rrc_h) (double t, double beta) <br>_Analytic root-raised-cosine impulse response at one instant._  |
|  size\_t | [**wfm\_rrc\_ntaps**](#function-wfm_rrc_ntaps) (int sps, int span) <br>_Number of taps a_ `wfm_rrc_taps` _call produces:_`2*span*sps + 1` _._ |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**M\_PI**](wfm__dsp_8h.md#define-m_pi)  `3.14159265358979323846`<br> |

## Detailed Description


Two pure DSP primitives the engine/composer use to build spread-spectrum and band-limited waveforms:
* wfm\_dsss\_spread: multiply each data symbol by a PN chip code.
* wfm\_rrc\_taps: a unit-energy root-raised-cosine FIR (matched-filter pulse shape), applied by upsample + FIR. 




    
## Public Functions Documentation




### function wfm\_cont\_dsss\_chips 

_Build a CONTINUOUS, ASYNCHRONOUS DSSS chip pattern._ 
```C++
size_t wfm_cont_dsss_chips (
    const uint8_t * code,
    size_t code_len,
    const uint8_t * data,
    size_t n_data,
    double chips_per_symbol,
    size_t n_chips,
    uint8_t * out
) 
```



The continuous counterpart to `wfm_frame_dsss_chips`. Two differences, both required by a continuously-transmitting spread carrier (CCSDS command-link style) rather than a bounded burst:



* **Continuous**: no preamble, no sync word, no CRC. The spreading code repeats end to end and data rides on it the whole way.
* **Asynchronous**: the data-symbol clock is independent of the code epoch, so `chips_per_symbol` is a non-integer `double` and symbol boundaries land _inside_ code epochs. The burst builder spreads exactly one bit per full code period — synchronous by construction, integer always.




Chip `i` carries `code[i % code_len] ^ data[floor(i / chips_per_symbol)]`, so both clocks advance independently off the same chip index. Because the symbol index is a floor of a fractional quotient, consecutive symbols legitimately span different numbers of chips (1136 or 1137 at SPEC.md's 3.069 Mcps / 2700 bps) — that jitter IS the asynchronicity, not an artifact.


Materialising the pattern up front, exactly as the burst builder does, is what lets the synth's existing cyclic chip latch play it back unchanged: no new per-sample branch, no new running state, no serialization change.




**Parameters:**


* `code` spreading code chips (0/1), length `code_len`. 
* `code_len` spreading code length in chips (&gt; 0). 
* `data` data bits (0/1), length `n_data`; cycled if exhausted. 
* `n_data` data bit count (&gt; 0). 
* `chips_per_symbol` chips per data symbol (&gt; 0, typically non-integer). 
* `n_chips` chips to produce (the caller's requested span). 
* `out` output chip array (0/1) of `n_chips` elements. 



**Returns:**

Chips written (== `n_chips`), or 0 on invalid geometry. 





        

<hr>



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



### function wfm\_polyphase\_bank 

_Deal an arbitrary FIR prototype into a polyphase interpolation bank._ 
```C++
void wfm_polyphase_bank (
    const float * proto,
    size_t proto_len,
    size_t num_phases,
    size_t num_taps,
    float * bank
) 
```



The pure decomposition shared by every polyphase-bank builder: phase `p` gets the prototype taps that land on output samples of residue `p`, so `bank[p*num_taps + t] = proto[t*num_phases + p]` (zero-padded past `proto_len`). Row-major, `num_phases * num_taps` floats — exactly the layout `resamp_create_custom(num_phases, num_taps, bank, rate)` consumes. Interpolate an input stream by `num_phases` (rate = num\_phases) with the resulting bank and you recompute the dense `proto` convolution from only the nonzero upsampled contributions.




**Parameters:**


* `proto` prototype FIR taps. 
* `proto_len` number of prototype taps. 
* `num_phases` interpolation factor (bank rows). 
* `num_taps` taps per phase; must satisfy `num_phases * num_taps >= proto_len` (use `(proto_len + num_phases - 1) / num_phases`). 
* `bank` output bank, row-major, length `num_phases * num_taps`. 




        

<hr>



### function wfm\_rrc\_polyphase\_bank 

_Decompose the RRC pulse shape into a polyphase interpolation bank._ 
```C++
void wfm_rrc_polyphase_bank (
    double beta,
    int sps,
    int span,
    float * bank
) 
```



The dense pulse shaper upsamples a symbol stream by `sps` (one impulse per `sps` samples, the rest hard zeros) then runs the full `wfm_rrc_taps` FIR over it — `(sps-1)/sps` of every tap-multiply hits a structural zero. The _polyphase_ form computes the identical convolution from only the nonzero contributions: it splits the length-`wfm_rrc_ntaps(sps, span)` prototype into `sps` phases of `wfm_rrc_bank_ntaps(span)` taps each, so phase `p` selects the subset of prototype taps that land on output samples of residue `p`.


The prototype is `wfm_rrc_taps(beta, sps, span)` scaled by `sqrt(sps)` — the same unit-average-power scaling `wfm_synth_set_rrc` applies to the dense taps, folded in here so the two paths shape at byte-comparable amplitude. The row-major layout `bank[p*num_taps + t] = proto[t*sps + p]` (zero-padded past the final partial tap) is exactly the decomposition `resamp`'s own Kaiser bank uses, so the bank drops straight into `resamp_create_custom(sps,
wfm_rrc_bank_ntaps(span), bank, sps)` as an interpolate-by-`sps` shaper.


Unlike `resamp`'s Kaiser prototype (which carries a `×num_phases` gain to compensate interpolation energy spreading), the RRC prototype carries no such gain: the interpolate path reproduces the dense FIR output to float precision with the raw scaled taps.




**Parameters:**


* `beta` roll-off in `[0, 1]`. 
* `sps` samples per symbol (&gt;= 1); also the number of phases. 
* `span` one-sided span in symbols (&gt;= 1). 
* `bank` output bank, row-major, length `sps * wfm_rrc_bank_ntaps(span)`. 




        

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




### function wfm\_cont\_dsss\_nchips 

_Chip count for_ `wfm_cont_dsss_chips` _: exactly_`n_chips` _._
```C++
static inline size_t wfm_cont_dsss_nchips (
    size_t n_chips
) 
```



Trivial, but present so the two continuous entry points mirror the burst pair (`wfm_frame_dsss_nchips` / `wfm_frame_dsss_chips`) and callers size their buffer through a named function rather than an open-coded expression. 


        

<hr>



### function wfm\_rrc\_bank\_ntaps 

_Number of taps per phase in a_ `wfm_rrc_polyphase_bank` _:_`2*span + 1` _._
```C++
static inline size_t wfm_rrc_bank_ntaps (
    int span
) 
```





**Parameters:**


* `span` one-sided filter span in symbols (&gt;= 1). 




        

<hr>



### function wfm\_rrc\_h 

_Analytic root-raised-cosine impulse response at one instant._ 
```C++
static inline double wfm_rrc_h (
    double t,
    double beta
) 
```



The RRC formula itself, evaluated at an arbitrary continuous time — the single source of truth every RRC consumer samples. `wfm_rrc_taps()` walks this on the uniform `1/sps` grid and normalises; a receiver's polyphase matched-filter bank (RateConverter's pulse-shaped terminal stage) samples it at `num_phases * num_taps` instants that are NOT a uniform sub-multiple of the input grid, which is why the point evaluator is public: an arbitrary (non-integer) samples-per-symbol bank cannot be built by decomposing an integer-oversampled prototype, and a second copy of this formula is exactly the kind of peer implementation that drifts.


Both removable singularities are handled by their closed-form limits: the `0/0` at `t = 0`, and the `0/0` at `t = ±1/(4β)` where the denominator's `1 - (4βt)^2` vanishes.




**Parameters:**


* `t` time in SYMBOL periods (T = 1), relative to the pulse centre. 
* `beta` roll-off in `[0, 1]`. 



**Returns:**

`h(t)`, unnormalised (peak ≈ `1 - β + 4β/π` at `t = 0`). 





        

<hr>



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
## Macro Definition Documentation





### define M\_PI 

```C++
#define M_PI `3.14159265358979323846`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_dsp.h`

