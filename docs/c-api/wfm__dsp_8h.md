

# File wfm\_dsp.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_dsp.h**](wfm__dsp_8h.md)

[Go to the source code of this file](wfm__dsp_8h_source.md)

_DSSS spreading + root-raised-cosine pulse shaping (Phase B)._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/util/util_core.h"`
* `#include <math.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_wfm\_cont\_dsss\_chips**](#function-dp_wfm_cont_dsss_chips) (const uint8\_t \* code, size\_t code\_len, const uint8\_t \* data, size\_t n\_data, double chips\_per\_symbol, size\_t n\_chips, uint8\_t \* out) <br>_Build a CONTINUOUS, ASYNCHRONOUS DSSS chip pattern._  |
|  void | [**dp\_wfm\_dsss\_spread**](#function-dp_wfm_dsss_spread) (const float \_Complex \* syms, size\_t n\_sym, const uint8\_t \* code, size\_t sf, float \_Complex \* out) <br>_Spread_ `n_sym` _complex data symbols by a binary PN code._ |
|  void | [**dp\_wfm\_polyphase\_bank**](#function-dp_wfm_polyphase_bank) (const float \* proto, size\_t proto\_len, size\_t num\_phases, size\_t num\_taps, float \* bank) <br>_Deal an arbitrary FIR prototype into a polyphase interpolation bank._  |
|  void | [**dp\_wfm\_rrc\_polyphase\_bank**](#function-dp_wfm_rrc_polyphase_bank) (double beta, int sps, int span, float \* bank) <br>_Decompose the RRC pulse shape into a polyphase interpolation bank._  |
|  void | [**dp\_wfm\_rrc\_taps**](#function-dp_wfm_rrc_taps) (double beta, int sps, int span, float \* taps) <br>_Fill_ `taps` _with a unit-energy root-raised-cosine impulse response._ |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**wfm\_cont\_dsss\_nchips**](#function-wfm_cont_dsss_nchips) (size\_t n\_chips) <br>_Chip count for_ `dp_wfm_cont_dsss_chips` _: exactly_`n_chips` _._ |
|  double | [**wfm\_rc\_h**](#function-wfm_rc_h) (double t, double beta) <br>_The MATCHED pair's composite pulse:_ `rrc * rrc` _, in closed form._ |
|  size\_t | [**wfm\_rrc\_bank\_ntaps**](#function-wfm_rrc_bank_ntaps) (int span) <br>_Number of taps per phase in a_ `dp_wfm_rrc_polyphase_bank` _:_`2*span + 1` _._ |
|  double | [**wfm\_rrc\_h**](#function-wfm_rrc_h) (double t, double beta) <br>_Analytic root-raised-cosine impulse response at one instant._  |
|  size\_t | [**wfm\_rrc\_ntaps**](#function-wfm_rrc_ntaps) (int sps, int span) <br>_Number of taps a_ `dp_wfm_rrc_taps` _call produces:_`2*span*sps + 1` _._ |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**M\_PI**](wfm__dsp_8h.md#define-m_pi)  `3.14159265358979323846`<br> |

## Detailed Description


Two pure DSP primitives the engine/composer use to build spread-spectrum and band-limited waveforms:
* dp\_wfm\_dsss\_spread: multiply each data symbol by a PN chip code.
* dp\_wfm\_rrc\_taps: a unit-energy root-raised-cosine FIR (matched-filter pulse shape), applied by upsample + FIR. 




    
## Public Functions Documentation




### function dp\_wfm\_cont\_dsss\_chips 

_Build a CONTINUOUS, ASYNCHRONOUS DSSS chip pattern._ 
```C++
size_t dp_wfm_cont_dsss_chips (
    const uint8_t * code,
    size_t code_len,
    const uint8_t * data,
    size_t n_data,
    double chips_per_symbol,
    size_t n_chips,
    uint8_t * out
) 
```



The continuous counterpart to `dp_wfm_dsss_desc_chips`. Two differences, both required by a continuously-transmitting spread carrier (CCSDS command-link style) rather than a bounded burst:



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



### function dp\_wfm\_dsss\_spread 

_Spread_ `n_sym` _complex data symbols by a binary PN code._
```C++
void dp_wfm_dsss_spread (
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



### function dp\_wfm\_polyphase\_bank 

_Deal an arbitrary FIR prototype into a polyphase interpolation bank._ 
```C++
void dp_wfm_polyphase_bank (
    const float * proto,
    size_t proto_len,
    size_t num_phases,
    size_t num_taps,
    float * bank
) 
```



The pure decomposition shared by every polyphase-bank builder: phase `p` gets the prototype taps that land on output samples of residue `p`, so `bank[p*num_taps + t] = proto[t*num_phases + p]` (zero-padded past `proto_len`). Row-major, `num_phases * num_taps` floats — exactly the layout `dp_resamp_create_custom(num_phases, num_taps, bank, rate)` consumes. Interpolate an input stream by `num_phases` (rate = num\_phases) with the resulting bank and you recompute the dense `proto` convolution from only the nonzero upsampled contributions.




**Parameters:**


* `proto` prototype FIR taps. 
* `proto_len` number of prototype taps. 
* `num_phases` interpolation factor (bank rows). 
* `num_taps` taps per phase; must satisfy `num_phases * num_taps >= proto_len` (use `(proto_len + num_phases - 1) / num_phases`). 
* `bank` output bank, row-major, length `num_phases * num_taps`. 




        

<hr>



### function dp\_wfm\_rrc\_polyphase\_bank 

_Decompose the RRC pulse shape into a polyphase interpolation bank._ 
```C++
void dp_wfm_rrc_polyphase_bank (
    double beta,
    int sps,
    int span,
    float * bank
) 
```



The dense pulse shaper upsamples a symbol stream by `sps` (one impulse per `sps` samples, the rest hard zeros) then runs the full `dp_wfm_rrc_taps` FIR over it — `(sps-1)/sps` of every tap-multiply hits a structural zero. The _polyphase_ form computes the identical convolution from only the nonzero contributions: it splits the length-`wfm_rrc_ntaps(sps, span)` prototype into `sps` phases of `wfm_rrc_bank_ntaps(span)` taps each, so phase `p` selects the subset of prototype taps that land on output samples of residue `p`.


The prototype is `dp_wfm_rrc_taps(beta, sps, span)` scaled by `sqrt(sps)` — the same unit-average-power scaling `dp_wfm_synth_set_rrc` applies to the dense taps, folded in here so the two paths shape at byte-comparable amplitude. The row-major layout `bank[p*num_taps + t] = proto[t*sps + p]` (zero-padded past the final partial tap) is exactly the decomposition `resamp`'s own Kaiser bank uses, so the bank drops straight into `dp_resamp_create_custom(sps,
wfm_rrc_bank_ntaps(span), bank, sps)` as an interpolate-by-`sps` shaper.


Unlike `resamp`'s Kaiser prototype (which carries a `×num_phases` gain to compensate interpolation energy spreading), the RRC prototype carries no such gain: the interpolate path reproduces the dense FIR output to float precision with the raw scaled taps.




**Parameters:**


* `beta` roll-off in `[0, 1]`. 
* `sps` samples per symbol (&gt;= 1); also the number of phases. 
* `span` one-sided span in symbols (&gt;= 1). 
* `bank` output bank, row-major, length `sps * wfm_rrc_bank_ntaps(span)`. 




        

<hr>



### function dp\_wfm\_rrc\_taps 

_Fill_ `taps` _with a unit-energy root-raised-cosine impulse response._
```C++
void dp_wfm_rrc_taps (
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

_Chip count for_ `dp_wfm_cont_dsss_chips` _: exactly_`n_chips` _._
```C++
static inline size_t wfm_cont_dsss_nchips (
    size_t n_chips
) 
```



Trivial, but present so the two continuous entry points mirror the burst pair (`dp_wfm_dsss_desc_nchips` / `dp_wfm_dsss_desc_chips`, in `wfm/wfm_frame.h`) and callers size their buffer through a named function rather than an open-coded expression. 


        

<hr>



### function wfm\_rc\_h 

_The MATCHED pair's composite pulse:_ `rrc * rrc` _, in closed form._
```C++
static inline double wfm_rc_h (
    double t,
    double beta
) 
```



A root-raised cosine convolved with itself is a raised cosine, so the pulse a matched receiver actually sees needs no convolution and no table — which is what lets a constructor evaluate it. Normalised to `g(0) = 1`, the level a unity-gain matched cascade delivers (see [**dp\_RateConverter\_gain()**](RateConverter__core_8h.md#function-dp_rateconverter_gain)), so `g(t)` IS the recovered symbol amplitude at timing offset `t`.


Nyquist by construction: `g(k) = 0` at every non-zero integer `k`, which is why a timing error and not an amplitude error is what inter-symbol interference looks like here.


The removable singularity at `t = ±1/(2β)` is handled by its closed-form limit; `t = 0` needs none (the sinc limit is taken explicitly).




**Parameters:**


* `t` time in SYMBOL periods (T = 1), relative to the pulse centre. 
* `beta` roll-off in `[0, 1]`. 



**Returns:**

`g(t)`, with `g(0) = 1`.



```C++
printf ("%.4f %.6f\n", wfm_rc_h (0.0, 0.35), wfm_rc_h (1.0, 0.35));
// 1.0000 0.000000
```
 


        

<hr>



### function wfm\_rrc\_bank\_ntaps 

_Number of taps per phase in a_ `dp_wfm_rrc_polyphase_bank` _:_`2*span + 1` _._
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



The RRC formula itself, evaluated at an arbitrary continuous time — the single source of truth every RRC consumer samples. `dp_wfm_rrc_taps()` walks this on the uniform `1/sps` grid and normalises; a receiver's polyphase matched-filter bank (RateConverter's pulse-shaped terminal stage) samples it at `num_phases * num_taps` instants that are NOT a uniform sub-multiple of the input grid, which is why the point evaluator is public: an arbitrary (non-integer) samples-per-symbol bank cannot be built by decomposing an integer-oversampled prototype, and a second copy of this formula is exactly the kind of peer implementation that drifts.


Both removable singularities are handled by their closed-form limits: the `0/0` at `t = 0`, and the `0/0` at `t = ±1/(4β)` where the denominator's `1 - (4βt)^2` vanishes.




**Parameters:**


* `t` time in SYMBOL periods (T = 1), relative to the pulse centre. 
* `beta` roll-off in `[0, 1]`. 



**Returns:**

`h(t)`, unnormalised (peak ≈ `1 - β + 4β/π` at `t = 0`). 





        

<hr>



### function wfm\_rrc\_ntaps 

_Number of taps a_ `dp_wfm_rrc_taps` _call produces:_`2*span*sps + 1` _._
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
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_dsp.h`

