

# File wfm\_synth\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm\_synth**](dir_3fd4fbfbc7cedac951bbaaf096533da9.md) **>** [**wfm\_synth\_core.h**](wfm__synth__core_8h.md)

[Go to the source code of this file](wfm__synth__core_8h_source.md)

_Synth component API._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/fir/fir_core.h"`
* `#include "doppler/lo/lo_core.h"`
* `#include "doppler/awgn/awgn_core.h"`
* `#include "doppler/pn/pn_core.h"`
* `#include "doppler/resamp/resamp_core.h"`
* `#include <math.h>`
* `#include "doppler/gold/gold_core.h"`
* `#include "doppler/wfm/wfm_dsp.h"`
* `#include "doppler/mpsk/mpsk_core.h"`
* `#include "doppler/cvt/cvt_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) <br> |
| struct | [**wfm\_synth\_refill\_state\_t**](structwfm__synth__refill__state__t.md) <br>_A frame source's own state triplet, over its_ `user` _pointer._ |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**wfm\_\_synth\_\_core\_8h\_1abc5c98fcc1211af2b80116dd6e0a035d**](#enum-wfm__synth__core_8h_1abc5c98fcc1211af2b80116dd6e0a035d)  <br> |
| enum  | [**wfm\_\_synth\_\_core\_8h\_1ac36f475ca5b446f4fde4c9b90bec77c8**](#enum-wfm__synth__core_8h_1ac36f475ca5b446f4fde4c9b90bec77c8)  <br> |
| typedef int(\* | [**wfm\_synth\_refill\_fn**](#typedef-wfm_synth_refill_fn)  <br>_Synth state._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* | [**dp\_wfm\_synth\_create**](#function-dp_wfm_synth_create) (int type, double fs, double freq, double snr, int snr\_mode, uint32\_t seed, int sps, int pn\_length, uint64\_t pn\_poly, int lfsr, double f\_end) <br>_Allocate and configure a waveform synthesiser. The synthesiser combines a local oscillator (LO), optional AWGN, and an optional PN LFSR into a single streaming source. One call to_ [_**dp\_wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-dp_wfm_synth_step) _or_[_**dp\_wfm\_synth\_steps()**_](wfm__synth__core_8h.md#function-dp_wfm_synth_steps) _advances all sub-components in lock-step. SNR &gt;= WFM\_SYNTH\_SNR\_CLEAN (100 dB) skips AWGN entirely — clean waveforms pay no noise overhead. When_`snr_mode` _is "auto" the library picks the natural reference: Es/No for modulated types (BPSK, QPSK), fs-band SNR for tone/noise/PN._ |
|  int | [**dp\_wfm\_synth\_data\_ended**](#function-dp_wfm_synth_data_ended) (const [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state) <br>_Non-zero once an attached frame source has reported its end._  |
|  void | [**dp\_wfm\_synth\_destroy**](#function-dp_wfm_synth_destroy) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state) <br>_Destroy a synth instance and release all memory. Recursively frees the LO, AWGN, and PN sub-objects, then the struct itself. Safe to call with NULL (no-op)._  |
|  float | [**dp\_wfm\_synth\_get\_cur\_im**](#function-dp_wfm_synth_get_cur_im) (const [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state) <br>_Return the imaginary part of the current held symbol. For QPSK this is the Q component (±1/√2); for BPSK/PN it is always 0; for tone/noise it is 0._  |
|  float | [**dp\_wfm\_synth\_get\_cur\_re**](#function-dp_wfm_synth_get_cur_re) (const [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state) <br>_Return the real part of the current held symbol. For modulated types this is the I component latched at the last symbol boundary (±1 for BPSK/PN, ±1/√2 for QPSK). For tone the synthesiser initialises cur\_re to 1.0 so that the held symbol is a clean unit-power carrier; for noise it is 0.0 (noise has no held symbol)._  |
|  int | [**dp\_wfm\_synth\_get\_nsps**](#function-dp_wfm_synth_get_nsps) (const [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state) <br>_Return the samples-per-symbol count. For modulated types (BPSK, QPSK, PN) each symbol is held for nsps consecutive output samples. For tone/noise this field is present but unused by the synthesis path._  |
|  void | [**dp\_wfm\_synth\_get\_state**](#function-dp_wfm_synth_get_state) (const [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, void \* blob) <br> |
|  int | [**dp\_wfm\_synth\_get\_sym\_pos**](#function-dp_wfm_synth_get_sym_pos) (const [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state) <br>_Return the current position within the current symbol (0..nsps-1). Reaches nsps and wraps to 0 each time a new symbol is consumed from the PN LFSR. Useful for frame alignment: sym\_pos==0 on a step boundary means the very next sample begins a fresh symbol._  |
|  int | [**dp\_wfm\_synth\_get\_wtype**](#function-dp_wfm_synth_get_wtype) (const [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state) <br>_Return the active waveform type discriminant. Maps to the WFM\_SYNTH\_\* enum: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk. Use this to inspect which synthesis path is active at runtime._  |
|  void | [**dp\_wfm\_synth\_noise\_steps**](#function-dp_wfm_synth_noise_steps) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, float \_Complex \* output, size\_t n) <br>_Generate n noise-only samples — the synth's additive-AWGN term with no signal — continuing the same noise RNG stream_ [_**dp\_wfm\_synth\_steps()**_](wfm__synth__core_8h.md#function-dp_wfm_synth_steps) _draws from (no reseed, identical chunked awgn call pattern, so a gap rendered here is the seamless continuation of the on-time noise). Writes exact zeros and advances nothing for a clean synth (no AWGN child). Used by the composer to carry a segment's noise floor through its off-time gap._ |
|  void | [**dp\_wfm\_synth\_reseed\_noise**](#function-dp_wfm_synth_reseed_noise) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, uint32\_t seed) <br>_Reseed only the additive-noise (AWGN) generator, leaving the signal (LO / PN code / data / pulse shaping) untouched. A no-op for a synth with no noise. Used by the composer to give each repeat a fresh noise realization while the underlying waveform stays bit-identical._  |
|  void | [**dp\_wfm\_synth\_reset**](#function-dp_wfm_synth_reset) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state) <br>_Reset Synth to its post-create state. Resets the LO phase accumulator, AWGN internal state, and PN LFSR register to their initial values so the output sequence is perfectly reproducible from sample 0._  |
|  int | [**dp\_wfm\_synth\_set\_bits**](#function-dp_wfm_synth_set_bits) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, const uint8\_t \* bits, size\_t n, int modulation) <br>_Attach a user bit pattern to a type=bits synth (no-op otherwise)._  |
|  void | [**dp\_wfm\_synth\_set\_chirp\_span**](#function-dp_wfm_synth_set_chirp_span) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, size\_t span) <br>_Pin a chirp's sweep span to_ `span` _samples (no-op for non-chirp)._ |
|  void | [**dp\_wfm\_synth\_set\_cur\_im**](#function-dp_wfm_synth_set_cur_im) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, float val) <br>_Override the held-symbol imaginary (Q) component in-place. Takes effect on the next_ [_**dp\_wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-dp_wfm_synth_step) _within the current symbol hold._ |
|  void | [**dp\_wfm\_synth\_set\_cur\_re**](#function-dp_wfm_synth_set_cur_re) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, float val) <br>_Override the held-symbol real (I) component in-place. Takes effect on the next_ [_**dp\_wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-dp_wfm_synth_step) _within the current symbol hold._ |
|  int | [**dp\_wfm\_synth\_set\_dsss\_chips**](#function-dp_wfm_synth_set_dsss_chips) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, const uint8\_t \* chips, size\_t n\_chips) <br>_Install an assembled two-code DSSS burst as the chip pattern._  |
|  int | [**dp\_wfm\_synth\_set\_dsss\_cont**](#function-dp_wfm_synth_set_dsss_cont) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, const uint8\_t \* code, size\_t code\_len, double chips\_per\_symbol, int data\_mode, const uint8\_t \* data, size\_t n\_data) <br>_Configure a type=dsss synth for CONTINUOUS ASYNCHRONOUS generation._  |
|  int | [**dp\_wfm\_synth\_set\_dsss\_window**](#function-dp_wfm_synth_set_dsss_window) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, size\_t code\_only\_symbols, size\_t frame\_symbols) <br>_Give the continuous DSSS stream a frame with a pure-code window._  |
|  void | [**dp\_wfm\_synth\_set\_nsps**](#function-dp_wfm_synth_set_nsps) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, int val) <br>_Override the samples-per-symbol count in-place. Does not flush the symbol-position counter (sym\_pos); set sym\_pos=0 as well when changing sps mid-stream._  |
|  int | [**dp\_wfm\_synth\_set\_refill**](#function-dp_wfm_synth_set_refill) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, [**wfm\_synth\_refill\_fn**](wfm__synth__core_8h.md#typedef-wfm_synth_refill_fn) fn, void \* user, void(\*)(void \*) free\_user) <br>_Pull each frame from_ `fn` _instead of cycling the pattern._ |
|  int | [**dp\_wfm\_synth\_set\_refill\_state**](#function-dp_wfm_synth_set_refill_state) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, const [**wfm\_synth\_refill\_state\_t**](structwfm__synth__refill__state__t.md) \* ops) <br>_Give the attached refill its state triplet, so the synth serializes._  |
|  int | [**dp\_wfm\_synth\_set\_rrc**](#function-dp_wfm_synth_set_rrc) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, const float \* taps, size\_t ntaps) <br>_Enable RRC pulse shaping on a symbol synth (pn/bpsk/qpsk/bits)._  |
|  int | [**dp\_wfm\_synth\_set\_state**](#function-dp_wfm_synth_set_state) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, const void \* blob) <br> |
|  void | [**dp\_wfm\_synth\_set\_sym\_pos**](#function-dp_wfm_synth_set_sym_pos) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, int val) <br>_Override the symbol-position counter in-place. Injecting 0 forces the next_ [_**dp\_wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-dp_wfm_synth_step) _to latch a new PN chip; any other value fast-forwards into the middle of the current symbol hold._ |
|  int | [**dp\_wfm\_synth\_set\_symbols**](#function-dp_wfm_synth_set_symbols) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, const float \_Complex \* symbols, size\_t n) <br>_Attach a complex-symbol stream to a type=symbols synth (no-op else)._  |
|  void | [**dp\_wfm\_synth\_set\_wtype**](#function-dp_wfm_synth_set_wtype) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, int val) <br>_Override the waveform type discriminant in-place. Changing wtype does not reinitialise sub-objects; use with care._  |
|  size\_t | [**dp\_wfm\_synth\_state\_bytes**](#function-dp_wfm_synth_state_bytes) (const [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state) <br> |
|  const char \* | [**dp\_wfm\_synth\_state\_refusal**](#function-dp_wfm_synth_state_refusal) (const [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state) <br>_Why the synth's state cannot be serialized, or NULL when it can._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float \_Complex | [**dp\_wfm\_synth\_step**](#function-dp_wfm_synth_step) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state) <br>_Generate one output sample from internal state. Advances the PN LFSR (modulated types only, on symbol boundaries), the LO phase accumulator, and the AWGN engine, then returns the mixed result:_ `sym * carrier + noise` _. Inlined and hot-path annotated so tight per-sample loops pay no call overhead._ |
|  void | [**dp\_wfm\_synth\_steps**](#function-dp_wfm_synth_steps) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* state, float \_Complex \* output, size\_t n) <br>_Generate a block of output samples. Calls_ [_**dp\_wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-dp_wfm_synth_step) _in a tight loop, writing each cf32 sample into_`output` _. The Python binding returns a freshly allocated NumPy complex64 array; ownership is transferred to the caller._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) int | [**wfm\_synth\_bit\_next**](#function-wfm_synth_bit_next) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* s, unsigned \* bit) <br>_The next bit of the pattern, or none once the data has ended._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float \_Complex | [**wfm\_synth\_bit\_symbol**](#function-wfm_synth_bit_symbol) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* s) <br>_Next symbol from the user bit pattern — one mapping, every M._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) int | [**wfm\_synth\_bps**](#function-wfm_synth_bps) (int type) <br>_Bits carried by one symbol of_ `type` _— the_`bps` _an Eb/No needs._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float | [**wfm\_synth\_cont\_dsss\_chip**](#function-wfm_synth_cont_dsss_chip) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* s) <br>_One continuous-DSSS chip:_ `code[n % n_code] ^ data` _, as a BPSK sign._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) uint64\_t | [**wfm\_synth\_mls\_poly**](#function-wfm_synth_mls_poly) (uint32\_t n) <br>_The MLS primitive polynomial table — pn's, reached by its old name._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float \_Complex | [**wfm\_synth\_next\_symbol**](#function-wfm_synth_next_symbol) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* s) <br>_Pull the next constellation symbol from the active shaped source._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) void | [**wfm\_synth\_shape**](#function-wfm_synth_shape) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* s, float \_Complex \* out, size\_t m, float \_Complex \* syms) <br>_Produce_ `m` _polyphase-shaped baseband samples into_`out` _._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) void | [**wfm\_synth\_shaper\_prime**](#function-wfm_synth_shaper_prime) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* s) <br>_Prime the shaper's delay line so its output aligns with the dense FIR._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**wfm\_synth\_snr\_over\_fs**](#function-wfm_synth_snr_over_fs) (int mode, int bps, double span, double snr) <br>_Convert a per-symbol or per-bit SNR to SNR over the full sample rate._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**WFM\_DSSS\_ENDED**](wfm__synth__core_8h.md#define-wfm_dsss_ended)  `2u`<br> |
| define  | [**WFM\_SYNTH\_SNR\_CLEAN**](wfm__synth__core_8h.md#define-wfm_synth_snr_clean)  `100.0`<br> |
| define  | [**WFM\_SYNTH\_STATE\_MAGIC**](wfm__synth__core_8h.md#define-wfm_synth_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('W','F','M','S')`<br> |
| define  | [**WFM\_SYNTH\_STATE\_VERSION**](wfm__synth__core_8h.md#define-wfm_synth_state_version)  `3u`<br> |

## Detailed Description


Lifecycle: create -&gt; `[step / steps / reset]*` -&gt; destroy


Example: 
```C++
dp_wfm_synth_state_t *obj = dp_wfm_synth_create(0, 1000000.0, 0.0, 100.0, 0, 1, 8, 7, 0);
float _Complex y = dp_wfm_synth_step(obj);
dp_wfm_synth_destroy(obj);
```
 


    
## Public Types Documentation




### enum wfm\_\_synth\_\_core\_8h\_1abc5c98fcc1211af2b80116dd6e0a035d 

```C++
enum wfm__synth__core_8h_1abc5c98fcc1211af2b80116dd6e0a035d {
    WFM_SYNTH_TONE = 0,
    WFM_SYNTH_NOISE = 1,
    WFM_SYNTH_PN = 2,
    WFM_SYNTH_BPSK = 3,
    WFM_SYNTH_QPSK = 4,
    WFM_SYNTH_CHIRP = 5,
    WFM_SYNTH_BITS = 6,
    WFM_SYNTH_SYMBOLS = 7,
    WFM_SYNTH_DSSS = 8
};
```



Waveform type discriminant (the `type` create argument / type choice). 


        

<hr>



### enum wfm\_\_synth\_\_core\_8h\_1ac36f475ca5b446f4fde4c9b90bec77c8 

```C++
enum wfm__synth__core_8h_1ac36f475ca5b446f4fde4c9b90bec77c8 {
    WFM_DSSS_DATA_NONE = 0,
    WFM_DSSS_DATA_BITS = 1,
    WFM_DSSS_DATA_PRBS = 2
};
```



Continuous-DSSS data-symbol source (dp\_wfm\_synth\_set\_dsss\_cont's data\_mode). 


        

<hr>



### typedef wfm\_synth\_refill\_fn 

_Synth state._ 
```C++
typedef int(* wfm_synth_refill_fn) (void *user, uint8_t *bits, size_t n);
```



Allocate with [**dp\_wfm\_synth\_create()**](wfm__synth__core_8h.md#function-dp_wfm_synth_create).


Where a BITS synth's next frame (or a dsss burst's next burst) comes from, in place of the cycle.


Called when the synth has read the last of its `n` bits: write the next frame's `n` bits into `bits` and return 0, or return non-zero when the data has ended. The frame length never changes, which is what lets the synth refill its own buffer in place. See [**dp\_wfm\_synth\_set\_refill()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_refill). 


        

<hr>
## Public Functions Documentation




### function dp\_wfm\_synth\_create 

_Allocate and configure a waveform synthesiser. The synthesiser combines a local oscillator (LO), optional AWGN, and an optional PN LFSR into a single streaming source. One call to_ [_**dp\_wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-dp_wfm_synth_step) _or_[_**dp\_wfm\_synth\_steps()**_](wfm__synth__core_8h.md#function-dp_wfm_synth_steps) _advances all sub-components in lock-step. SNR &gt;= WFM\_SYNTH\_SNR\_CLEAN (100 dB) skips AWGN entirely — clean waveforms pay no noise overhead. When_`snr_mode` _is "auto" the library picks the natural reference: Es/No for modulated types (BPSK, QPSK), fs-band SNR for tone/noise/PN._
```C++
dp_wfm_synth_state_t * dp_wfm_synth_create (
    int type,
    double fs,
    double freq,
    double snr,
    int snr_mode,
    uint32_t seed,
    int sps,
    int pn_length,
    uint64_t pn_poly,
    int lfsr,
    double f_end
) 
```





**Parameters:**


* `type` Waveform type: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk, 5=chirp, 6=bits, 7=symbols, 8=dsss. The Python binding accepts strings "tone"\|"noise"\|"pn"\|"bpsk"\|"qpsk"\|"chirp"\|"bits"\|"symbols"\|"dsss". For "bits" attach the pattern with [**dp\_wfm\_synth\_set\_bits()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_bits); for "symbols" attach the complex stream with [**dp\_wfm\_synth\_set\_symbols()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_symbols); for "dsss" attach the burst with [**dp\_wfm\_synth\_set\_dsss\_chips()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_dsss_chips) after create(). 
* `fs` Sample rate in Hz. Sets the carrier frequency normalisation and the noise bandwidth. Default 1 000 000.0. 
* `freq` Carrier frequency offset in Hz (−fs/2 … fs/2). A complex LO is created only when freq != 0. For a chirp this is the start frequency f\_start (the instantaneous frequency at t=0). Default 0.0. 
* `snr` Target SNR in dB, interpreted per `snr_mode`. Values &gt;= WFM\_SYNTH\_SNR\_CLEAN (100) disable AWGN. Default 100.0. 
* `snr_mode` SNR reference: 0=auto, 1=fs (full-band), 2=ebno, 3=esno. The Python binding accepts strings "auto"\|"fs"\|"ebno"\|"esno". Default 0. 
* `seed` PRNG seed shared by AWGN and the PN LFSR. Default 1. 
* `sps` Samples per symbol for modulated types (BPSK, QPSK, PN). Ignored for tone/noise. Default 8. 
* `pn_length` LFSR register length (1..64); period = 2^pn\_length - 1. Default 7 (period 127). 
* `pn_poly` Galois tap polynomial for the LFSR. 0 means "look up
             the canonical MLS polynomial for pn\_length" from the wfm\_synth\_mls\_poly table. Default 0. 
* `lfsr` LFSR realization: PN\_GALOIS (0) or PN\_FIBONACCI (1). 
* `f_end` Chirp end frequency in Hz (type=chirp only; ignored otherwise). With `freq` as the start, the instantaneous frequency sweeps linearly from `freq` to `f_end` over the span set by [**dp\_wfm\_synth\_set\_chirp\_span()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_chirp_span), then holds at `f_end`. Until a span is pinned the slope is 0 (a CW tone at `freq`). `f_end < freq` is a down-chirp. Default 0.0. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**dp\_wfm\_synth\_destroy()**](wfm__synth__core_8h.md#function-dp_wfm_synth_destroy) when done. 
```C++
>>> from doppler.wfm import _SynthEngine
>>> import numpy as np
>>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> x = s.steps(4)
>>> x.dtype
dtype('complex64')
>>> x.tolist()
[(1+0j), (1+0j), (1+0j), (1+0j)]
```
 





        

<hr>



### function dp\_wfm\_synth\_data\_ended 

_Non-zero once an attached frame source has reported its end._ 
```C++
int dp_wfm_synth_data_ended (
    const dp_wfm_synth_state_t * state
) 
```




<hr>



### function dp\_wfm\_synth\_destroy 

_Destroy a synth instance and release all memory. Recursively frees the LO, AWGN, and PN sub-objects, then the struct itself. Safe to call with NULL (no-op)._ 
```C++
void dp_wfm_synth_destroy (
    dp_wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Pointer to heap-allocated state; may be NULL. 
```C++
>>> from doppler.wfm import _SynthEngine
>>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> s.destroy()   # explicit teardown; no exception
```
 




        

<hr>



### function dp\_wfm\_synth\_get\_cur\_im 

_Return the imaginary part of the current held symbol. For QPSK this is the Q component (±1/√2); for BPSK/PN it is always 0; for tone/noise it is 0._ 
```C++
float dp_wfm_synth_get_cur_im (
    const dp_wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Current symbol imaginary (Q) component. 





        

<hr>



### function dp\_wfm\_synth\_get\_cur\_re 

_Return the real part of the current held symbol. For modulated types this is the I component latched at the last symbol boundary (±1 for BPSK/PN, ±1/√2 for QPSK). For tone the synthesiser initialises cur\_re to 1.0 so that the held symbol is a clean unit-power carrier; for noise it is 0.0 (noise has no held symbol)._ 
```C++
float dp_wfm_synth_get_cur_re (
    const dp_wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Current symbol real (I) component. 





        

<hr>



### function dp\_wfm\_synth\_get\_nsps 

_Return the samples-per-symbol count. For modulated types (BPSK, QPSK, PN) each symbol is held for nsps consecutive output samples. For tone/noise this field is present but unused by the synthesis path._ 
```C++
int dp_wfm_synth_get_nsps (
    const dp_wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Samples per symbol (nsps &gt;= 1). 





        

<hr>



### function dp\_wfm\_synth\_get\_state 

```C++
void dp_wfm_synth_get_state (
    const dp_wfm_synth_state_t * state,
    void * blob
) 
```




<hr>



### function dp\_wfm\_synth\_get\_sym\_pos 

_Return the current position within the current symbol (0..nsps-1). Reaches nsps and wraps to 0 each time a new symbol is consumed from the PN LFSR. Useful for frame alignment: sym\_pos==0 on a step boundary means the very next sample begins a fresh symbol._ 
```C++
int dp_wfm_synth_get_sym_pos (
    const dp_wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Symbol position counter (0 &lt;= sym\_pos &lt; nsps). 





        

<hr>



### function dp\_wfm\_synth\_get\_wtype 

_Return the active waveform type discriminant. Maps to the WFM\_SYNTH\_\* enum: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk. Use this to inspect which synthesis path is active at runtime._ 
```C++
int dp_wfm_synth_get_wtype (
    const dp_wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Integer waveform type index (WFM\_SYNTH\_TONE .. WFM\_SYNTH\_QPSK). 





        

<hr>



### function dp\_wfm\_synth\_noise\_steps 

_Generate n noise-only samples — the synth's additive-AWGN term with no signal — continuing the same noise RNG stream_ [_**dp\_wfm\_synth\_steps()**_](wfm__synth__core_8h.md#function-dp_wfm_synth_steps) _draws from (no reseed, identical chunked awgn call pattern, so a gap rendered here is the seamless continuation of the on-time noise). Writes exact zeros and advances nothing for a clean synth (no AWGN child). Used by the composer to carry a segment's noise floor through its off-time gap._
```C++
void dp_wfm_synth_noise_steps (
    dp_wfm_synth_state_t * state,
    float _Complex * output,
    size_t n
) 
```





**Parameters:**


* `state` Synth state (may be NULL — no-op). 
* `output` n complex samples out. 
* `n` Sample count. 




        

<hr>



### function dp\_wfm\_synth\_reseed\_noise 

_Reseed only the additive-noise (AWGN) generator, leaving the signal (LO / PN code / data / pulse shaping) untouched. A no-op for a synth with no noise. Used by the composer to give each repeat a fresh noise realization while the underlying waveform stays bit-identical._ 
```C++
void dp_wfm_synth_reseed_noise (
    dp_wfm_synth_state_t * state,
    uint32_t seed
) 
```





**Parameters:**


* `state` Synth state (may be NULL). 
* `seed` New noise RNG seed. 




        

<hr>



### function dp\_wfm\_synth\_reset 

_Reset Synth to its post-create state. Resets the LO phase accumulator, AWGN internal state, and PN LFSR register to their initial values so the output sequence is perfectly reproducible from sample 0._ 
```C++
void dp_wfm_synth_reset (
    dp_wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> from doppler.wfm import _SynthEngine
>>> import numpy as np
>>> s = _SynthEngine(type="qpsk", sps=4, seed=1, snr=100.0)
>>> a = s.steps(16).copy()
>>> s.reset()
>>> np.array_equal(a, s.steps(16))
True
```
 




        

<hr>



### function dp\_wfm\_synth\_set\_bits 

_Attach a user bit pattern to a type=bits synth (no-op otherwise)._ 
```C++
int dp_wfm_synth_set_bits (
    dp_wfm_synth_state_t * state,
    const uint8_t * bits,
    size_t n,
    int modulation
) 
```



Copies `n` bits (each 0/1) into the synth; `modulation` maps them to symbols (0=none → 0/1 amplitude, 1=bpsk → ±1, 2=qpsk → Gray-coded ±1/√2, two bits per symbol). The pattern is oversampled by the create-time `sps` and sent ONCE: one pass is `n * sps` samples (`n/2 * sps` for qpsk), and the output is silent after it (doppler#1718 deleted the cycle; a payload that goes on is a data source, dp\_wfm\_synth\_set\_refill). Replaces any previous pattern; resets the read position. Safe to call repeatedly.




**Parameters:**


* `state` Must be non-NULL. 
* `bits` Array of `n` bytes, each 0 or 1. 
* `n` Number of bits (&gt; 0). 
* `modulation` 0=none, 1=bpsk, 2=qpsk. 



**Returns:**

0 on success; -1 on bad args or allocation failure. 





        

<hr>



### function dp\_wfm\_synth\_set\_chirp\_span 

_Pin a chirp's sweep span to_ `span` _samples (no-op for non-chirp)._
```C++
void dp_wfm_synth_set_chirp_span (
    dp_wfm_synth_state_t * state,
    size_t span
) 
```



A linear chirp's slope is `(f_end − f_start) / span`, so the span — the number of samples the sweep occupies — must be known before generation. The composer calls this with the source's declared span or the segment length. A synth that is never pinned does not sweep: it holds the start frequency on [**dp\_wfm\_synth\_step()**](wfm__synth__core_8h.md#function-dp_wfm_synth_step) and [**dp\_wfm\_synth\_steps()**](wfm__synth__core_8h.md#function-dp_wfm_synth_steps) alike, so the waveform never depends on how reads are chunked. Only the first pin (while the span is still 0) takes effect, so it is safe to call unconditionally after [**dp\_wfm\_synth\_create()**](wfm__synth__core_8h.md#function-dp_wfm_synth_create); `span` 0 is a no-op.


The span is configuration, not running state: [**dp\_wfm\_synth\_get\_state()**](wfm__synth__core_8h.md#function-dp_wfm_synth_get_state) does not carry it, so pin a resumed instance exactly as the original was pinned.




**Parameters:**


* `state` Must be non-NULL. 
* `span` Sweep length in samples (&gt; 0). 




        

<hr>



### function dp\_wfm\_synth\_set\_cur\_im 

_Override the held-symbol imaginary (Q) component in-place. Takes effect on the next_ [_**dp\_wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-dp_wfm_synth_step) _within the current symbol hold._
```C++
void dp_wfm_synth_set_cur_im (
    dp_wfm_synth_state_t * state,
    float val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New cur\_im value. 




        

<hr>



### function dp\_wfm\_synth\_set\_cur\_re 

_Override the held-symbol real (I) component in-place. Takes effect on the next_ [_**dp\_wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-dp_wfm_synth_step) _within the current symbol hold._
```C++
void dp_wfm_synth_set_cur_re (
    dp_wfm_synth_state_t * state,
    float val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New cur\_re value. 




        

<hr>



### function dp\_wfm\_synth\_set\_dsss\_chips 

_Install an assembled two-code DSSS burst as the chip pattern._ 
```C++
int dp_wfm_synth_set_dsss_chips (
    dp_wfm_synth_state_t * state,
    const uint8_t * chips,
    size_t n_chips
) 
```



The burst is assembled from a frame DESCRIPTION by `dp_wfm_dsss_desc_chips()` (`wfm/wfm_frame.h`)  an unmodulated preamble (`acq_code` repeated `acq_reps` times, the coherent acquisition target) followed by every bit of the assembled frame XOR-spread by the distinct `data_code`  and installed here as the synth's BPSK chip stream, each chip held for the create-time `sps` samples, i.e. `sps` is samples per _chip_ here. The common frame `sync | payload | CRC-16` is `dp_wfm_frame_fixed()`; a coded burst is any other description. This is the transmit side of `BurstDemod`'s frame contract: the same codes, sync word, and payload length hand to `dp_burst_demod_set_preamble`/`set_sync` on receive.


One pass of the pattern is one burst (`n_chips * sps` samples), sent once like the bits pattern, and silence after it  the composer sizes a dsss segment's on-time to exactly one burst, or one per frame of a data source (dp\_wfm\_synth\_set\_refill). Replaces any previous pattern; resets the read position. Chips are copied; `chips` stays the caller's.


NOTE: `snr_mode` semantics — the raw engine's create-time esno refers to the _chip_ (the output symbol). The Segment/Synth faces convert a data-symbol Es/N0 (`snr_mode="esno"`) to the over-fs value with `10*log10(sf*sps)` before create; see `dp_wfm_snr_over_fs()`.




**Parameters:**


* `state` Synth (no-op unless `wtype == WFM_SYNTH_DSSS`). 
* `chips` Burst chips, one per byte (0/1), BPSK-mapped by the synth. 
* `n_chips` Chip count; must be non-zero. 



**Returns:**

0 on success, -1 on a NULL/empty pattern or allocation failure. 





        

<hr>



### function dp\_wfm\_synth\_set\_dsss\_cont 

_Configure a type=dsss synth for CONTINUOUS ASYNCHRONOUS generation._ 
```C++
int dp_wfm_synth_set_dsss_cont (
    dp_wfm_synth_state_t * state,
    const uint8_t * code,
    size_t code_len,
    double chips_per_symbol,
    int data_mode,
    const uint8_t * data,
    size_t n_data
) 
```



The continuous counterpart to [**dp\_wfm\_synth\_set\_dsss\_chips()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_dsss_chips): the same `type="dsss"` waveform, switched to the endless mode by supplying `chips_per_symbol` (= `chip_rate / symbol_rate`). One waveform type, one discriminator — no tenth entry in the five hand-maintained name tables `wfm_names.h` records rotting once already.


**Lazy, not materialised.** Chips are generated per sample by `wfm_synth_cont_dsss_chip` off a running counter, so the stream is genuinely endless — there is no pattern length to pick and the standalone `Synth` face works unbounded. The data-symbol source is chosen by `data_mode:` 
* `WFM_DSSS_DATA_NONE` — code-only: the pure spreading code, no data.
* `WFM_DSSS_DATA_BITS` — `data`, one bit per data symbol, sent ONCE: the chips are silent after its last bit (doppler#1718).
* `WFM_DSSS_DATA_PRBS` — the synth's own seeded PN (create it in create(); a receiver regenerates the bits via `doppler.wfm.PN`).




The burst frame parameters have no meaning here (no preamble, sync, or CRC); the caller rejects that combination upstream rather than ignoring it (see wfmgen's `--symbol-rate` validation), so this function does not revisit it.




**Parameters:**


* `state` Synth (no-op unless `wtype == WFM_SYNTH_DSSS`). 
* `code` Spreading code chips (0/1), length `code_len`; copied. 
* `code_len` Spreading code length in chips (&gt; 0) — the SF. 
* `chips_per_symbol` Chips per data symbol (&gt;= 1), `chip_rate / symbol_rate`. Non-integer is the normal, asynchronous case. 
* `data_mode` WFM\_DSSS\_DATA\_{NONE,BITS,PRBS}. 
* `data` Payload bits (0/1) for WFM\_DSSS\_DATA\_BITS, length `n_data`; copied. Ignored (may be NULL) otherwise. 
* `n_data` Payload length in bits (&gt; 0 for WFM\_DSSS\_DATA\_BITS). 



**Returns:**

0 on success; -1 on invalid geometry or allocation failure. 





        

<hr>



### function dp\_wfm\_synth\_set\_dsss\_window 

_Give the continuous DSSS stream a frame with a pure-code window._ 
```C++
int dp_wfm_synth_set_dsss_window (
    dp_wfm_synth_state_t * state,
    size_t code_only_symbols,
    size_t frame_symbols
) 
```



The frame is on the DATA clock: of every `frame_symbols` symbols, the first `code_only_symbols` carry the pure spreading code and no data, and the rest carry the payload, running on from the previous frame. The symbol clock is the stream's own free-running one (see wfm\_synth\_cont\_dsss\_chip), so a frame edge lands at whatever chip phase it lands at: the chip and data clocks have no fixed relation, and no frame edge is synchronous with a code epoch. This is the multi-emitter waveform's frame — 450 code-only symbols then 4500 of data in the application it was written for — and the searcher's coherent depth is what the window makes possible. Configuration, not running state: it is kept by reset() and is not serialized. The order against [**dp\_wfm\_synth\_set\_dsss\_cont()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_dsss_cont) does not matter.




**Parameters:**


* `state` Synth (no-op unless `wtype == WFM_SYNTH_DSSS`). 
* `code_only_symbols` Pure-code symbols opening each frame, at most `frame_symbols`. Equal to it means code only, for ever. 
* `frame_symbols` Frame length in symbols; **0 means no window** — the stream exactly as without this call. 



**Returns:**

0 on success (and for a non-dsss synth); -1 if `code_only_symbols` exceeds a non-zero `frame_symbols`. 





        

<hr>



### function dp\_wfm\_synth\_set\_nsps 

_Override the samples-per-symbol count in-place. Does not flush the symbol-position counter (sym\_pos); set sym\_pos=0 as well when changing sps mid-stream._ 
```C++
void dp_wfm_synth_set_nsps (
    dp_wfm_synth_state_t * state,
    int val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New nsps value (&gt;= 1). 




        

<hr>



### function dp\_wfm\_synth\_set\_refill 

_Pull each frame from_ `fn` _instead of cycling the pattern._
```C++
int dp_wfm_synth_set_refill (
    dp_wfm_synth_state_t * state,
    wfm_synth_refill_fn fn,
    void * user,
    void(*)(void *) free_user
) 
```



The pattern set by [**dp\_wfm\_synth\_set\_bits()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_bits) is the FIRST frame; when its last bit has been read, `fn` writes the next frame's bits into the synth's buffer (the same `n`), and so on. When `fn` reports the end, the synth latches it ([**dp\_wfm\_synth\_data\_ended()**](wfm__synth__core_8h.md#function-dp_wfm_synth_data_ended)) and emits zero  silence  from then on. A later [**dp\_wfm\_synth\_set\_bits()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_bits) detaches the refill.


A dsss BURST plays its chips through the same cursor, so it takes a refill too: the pattern is the burst [**dp\_wfm\_synth\_set\_dsss\_chips()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_dsss_chips) installed, and `fn` writes the next burst's chips (doppler#1719). A later [**dp\_wfm\_synth\_set\_dsss\_chips()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_dsss_chips) detaches it. A CONTINUOUS dsss stream set up with `WFM_DSSS_DATA_BITS` takes one as well: each data symbol reads the next bit through the cursor instead of cycling its payload, the refill writing the next `n` bits, and the chips are silent once it reports the end.


A refill attached here has no state of its own, so the synth REFUSES serialization  [**dp\_wfm\_synth\_state\_bytes()**](wfm__synth__core_8h.md#function-dp_wfm_synth_state_bytes) returns 0 and [**dp\_wfm\_synth\_set\_state()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_state) is DP\_ERR\_INVALID  until the source's triplet is attached with [**dp\_wfm\_synth\_set\_refill\_state()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_refill_state): the pulled frame and the source's position are state, and a blob without them would resume the wrong data. [**dp\_wfm\_synth\_state\_refusal()**](wfm__synth__core_8h.md#function-dp_wfm_synth_state_refusal) says why.




**Parameters:**


* `state` a type=bits synth with a pattern set, a type=dsss burst with its chips set, or a continuous one whose data\_mode is `WFM_DSSS_DATA_BITS`. 
* `fn` the frame source; NULL detaches. 
* `user` passed to `fn`; owned by the synth when `free_user` is given, which it calls on detach or destroy. 
* `free_user` frees `user`, or NULL. 



**Returns:**

0, or -1 if the synth is none of those, or has no pattern.



```C++
#include "doppler/wfm_synth/wfm_synth_core.h"
#include <complex.h>

// One more 4-bit frame of ones after the first, then the end.
static int
one_more (void *u, uint8_t *bits, size_t n)
{
  int *left = u;
  if ((*left)-- <= 0)
    return 1; // the end
  for (size_t i = 0; i < n; i++)
    bits[i] = 1;
  return 0;
}

int
main (void)
{
  dp_wfm_synth_state_t *s = dp_wfm_synth_create (
      WFM_SYNTH_BITS, 1e6, 0.0, 200.0, 0, 1, 1, 7, 0, 0, 0.0);
  int left = 1;
  dp_wfm_synth_set_bits (s, (const uint8_t[]){ 0, 1, 0, 1 }, 4, 0);
  dp_wfm_synth_set_refill (s, one_more, &left, NULL);
  float _Complex x[12];
  dp_wfm_synth_steps (s, x, 12); // 0101, then 1111, then silence
  const int ok = crealf (x[1]) > 0.5f && crealf (x[4]) > 0.5f
                 && cabsf (x[11]) < 1e-3f && dp_wfm_synth_data_ended (s);
  dp_wfm_synth_destroy (s);
  return ok ? 0 : 1;
}
```
 


        

<hr>



### function dp\_wfm\_synth\_set\_refill\_state 

_Give the attached refill its state triplet, so the synth serializes._ 
```C++
int dp_wfm_synth_set_refill_state (
    dp_wfm_synth_state_t * state,
    const wfm_synth_refill_state_t * ops
) 
```



The blob then carries the frame in play (`bits`, which a refill rewrites and so is state, not config) and nests `ops'` sub-blob over the refill's `user`. A later [**dp\_wfm\_synth\_set\_refill()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_refill)  any attach or detach  drops `ops` with the refill it belonged to.




**Parameters:**


* `state` a synth with a refill attached. 
* `ops` the triplet, in static storage; NULL takes it away. 



**Returns:**

0, or -1 when no refill is attached. 





        

<hr>



### function dp\_wfm\_synth\_set\_rrc 

_Enable RRC pulse shaping on a symbol synth (pn/bpsk/qpsk/bits)._ 
```C++
int dp_wfm_synth_set_rrc (
    dp_wfm_synth_state_t * state,
    const float * taps,
    size_t ntaps
) 
```



Replaces the default rectangular sample-and-hold with a root-raised-cosine pulse: the symbol-rate impulse train is filtered by `taps` (a real FIR of `ntaps` coefficients, typically `dp_wfm_rrc_taps(beta, sps, span)`). The taps are scaled by sqrt(sps) internally for unit transmit power, so every caller passes the raw taps and gets byte-identical shaping. No-op for types with no symbol stream (tone/noise/chirp). Replaces any existing shaper and clears its delay line.




**Parameters:**


* `state` Must be non-NULL. 
* `taps` Real FIR taps (copied). 
* `ntaps` Number of taps (&gt; 0). 



**Returns:**

0 on success; -1 on bad args / allocation failure. 





        

<hr>



### function dp\_wfm\_synth\_set\_state 

```C++
int dp_wfm_synth_set_state (
    dp_wfm_synth_state_t * state,
    const void * blob
) 
```




<hr>



### function dp\_wfm\_synth\_set\_sym\_pos 

_Override the symbol-position counter in-place. Injecting 0 forces the next_ [_**dp\_wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-dp_wfm_synth_step) _to latch a new PN chip; any other value fast-forwards into the middle of the current symbol hold._
```C++
void dp_wfm_synth_set_sym_pos (
    dp_wfm_synth_state_t * state,
    int val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New sym\_pos value (0 &lt;= val &lt; nsps). 




        

<hr>



### function dp\_wfm\_synth\_set\_symbols 

_Attach a complex-symbol stream to a type=symbols synth (no-op else)._ 
```C++
int dp_wfm_synth_set_symbols (
    dp_wfm_synth_state_t * state,
    const float _Complex * symbols,
    size_t n
) 
```



Copies `n` complex symbols into the synth. Each symbol **is** the constellation point — there is no bit→symbol mapping, so this generalises every modulation (pi/4-QPSK, QAM, custom shaping) into "compute the symbols,
pass them in". The stream is oversampled by the create-time `sps` and **cycled** to fill whatever length `dp_wfm_synth_steps()` requests (one pass is `n * sps` samples), and is RRC-shaped when `dp_wfm_synth_set_rrc()` is active. Replaces any previous stream; resets the read position. Safe to call repeatedly.




**Parameters:**


* `state` Must be non-NULL. 
* `symbols` Array of `n` complex symbols (copied). 
* `n` Number of symbols (&gt; 0). 



**Returns:**

0 on success; -1 on bad args or allocation failure. 
```C++
>>> import numpy as np
>>> from doppler.wfm import _SynthEngine, rrc_taps
>>> s = _SynthEngine(
...     type="symbols", fs=1.0, freq=0.0, snr=100.0, sps=4)
>>> s.set_symbols(np.array([1+0j, 1j, -1+0j, -1j], np.complex64))
>>> s.steps(4)[::4].tolist()   # symbol centres (rect hold)
[(1+0j), (1+0j), (1+0j), (1+0j)]
```
 





        

<hr>



### function dp\_wfm\_synth\_set\_wtype 

_Override the waveform type discriminant in-place. Changing wtype does not reinitialise sub-objects; use with care._ 
```C++
void dp_wfm_synth_set_wtype (
    dp_wfm_synth_state_t * state,
    int val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` New wtype value (WFM\_SYNTH\_TONE .. WFM\_SYNTH\_QPSK). 




        

<hr>



### function dp\_wfm\_synth\_state\_bytes 

```C++
size_t dp_wfm_synth_state_bytes (
    const dp_wfm_synth_state_t * state
) 
```




<hr>



### function dp\_wfm\_synth\_state\_refusal 

_Why the synth's state cannot be serialized, or NULL when it can._ 
```C++
const char * dp_wfm_synth_state_refusal (
    const dp_wfm_synth_state_t * state
) 
```



A refill with no triplet ([**dp\_wfm\_synth\_set\_refill\_state()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_refill_state)), or a source whose triplet refuses  a pipe, whose delivered octets are gone. The reason is static, as every refusal's is. 


        

<hr>



### function dp\_wfm\_synth\_step 

_Generate one output sample from internal state. Advances the PN LFSR (modulated types only, on symbol boundaries), the LO phase accumulator, and the AWGN engine, then returns the mixed result:_ `sym * carrier + noise` _. Inlined and hot-path annotated so tight per-sample loops pay no call overhead._
```C++
JM_FORCEINLINE  JM_HOT float _Complex dp_wfm_synth_step (
    dp_wfm_synth_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Next output sample (float \_Complex). 
```C++
>>> from doppler.wfm import _SynthEngine
>>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> s.step()
(1+0j)
```
 





        

<hr>



### function dp\_wfm\_synth\_steps 

_Generate a block of output samples. Calls_ [_**dp\_wfm\_synth\_step()**_](wfm__synth__core_8h.md#function-dp_wfm_synth_step) _in a tight loop, writing each cf32 sample into_`output` _. The Python binding returns a freshly allocated NumPy complex64 array; ownership is transferred to the caller._
```C++
void dp_wfm_synth_steps (
    dp_wfm_synth_state_t * state,
    float _Complex * output,
    size_t n
) 
```





**Parameters:**


* `state` Initialised Synth state returned by `dp_wfm_synth_create`. 
* `output` Output buffer of at least `n` cf32 elements. 
* `n` Number of samples to generate. 
```C++
>>> from doppler.wfm import _SynthEngine
>>> import numpy as np
>>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
>>> x = s.steps(4)
>>> x.shape, x.dtype
((4,), dtype('complex64'))
>>> x.tolist()
[(1+0j), (1+0j), (1+0j), (1+0j)]
```
 




        

<hr>



### function wfm\_synth\_bit\_next 

_The next bit of the pattern, or none once the data has ended._ 
```C++
JM_FORCEINLINE int wfm_synth_bit_next (
    dp_wfm_synth_state_t * s,
    unsigned * bit
) 
```



The cursor STOPS at `n_bits`; it never wraps (doppler#1718: the cycle that sent one pattern again and again is gone). With a refill, that one bounds check finds the frame boundary: the next frame is drawn lazily, when its first bit is due  the source's counts are frames started, and a paced source is asked when the frame is due. A refill that reports the end latches `data_ended`, and so does the end of a pattern with no refill: it was the whole of the data, sent once. Either way every later call is the slow path's immediate "no bit", and the caller sends silence  never a line nobody sent. The one place the cursor moves, so the per-sample and block paths cannot disagree about a frame boundary.




**Parameters:**


* `s` the synth; a type=bits synth with a pattern set. 
* `bit` receives the next bit, 0 or 1, when one is returned. 



**Returns:**

1 with the bit in `*bit`, or 0: the data has ended. 





        

<hr>



### function wfm\_synth\_bit\_symbol 

_Next symbol from the user bit pattern — one mapping, every M._ 
```C++
JM_FORCEINLINE float _Complex wfm_synth_bit_symbol (
    dp_wfm_synth_state_t * s
) 
```



**The single home for the bits-&gt;symbol map.** It had four copies: two in this header (`wfm_synth_next_symbol` and `dp_wfm_synth_step`) and two in `dp_wfm_synth_steps()`. `wfm_synth_next_symbol`'s own comment says the kernel is shared "so the single-sample and block paths cannot diverge -- they call
the SAME function rather than each inlining the arithmetic", and the arithmetic was inlined four times anyway.


`bit_mod` is BITS PER SYMBOL, which is what its existing values already mean (1 = BPSK, 2 = QPSK), so M = 1 &lt;&lt; bit\_mod and 3 = 8PSK extends the numbering rather than reinterpreting it. One symbol's bits are read **MSB-first** into a Gray label and handed to `mpsk_constellation()`  the library's canonical mapping, and the one `dp_ber_score()` inverts to score bit errors.


That shared mapping is the point. The QPSK branches this replaces put `b0` on the I sign and `b1` on the Q sign: the same CONSTELLATION, but two of the four labels swapped against `mpsk_constellation()`. Nothing scored a QPSK bit pattern against truth, so it never produced a wrong number  but a framed QPSK stream read through the canonical scorer would have shown about half its symbols wrong on a perfectly working receiver, which is the plausible-number failure docs/design/rx-test.md exists to stop.


`bit_mod == 0` is not PSK  it is the 0/1 amplitude line this type has always emitted  so it keeps its own branch.




**Parameters:**


* `s` Synth state; `bits`/`n_bits` must be non-empty, `bit_idx` advances. 



**Returns:**

Unit-modulus constellation point (a unit-amplitude line at `bit_mod == 0`), which is what Synth's unit-power SNR reference needs. 





        

<hr>



### function wfm\_synth\_bps 

_Bits carried by one symbol of_ `type` _— the_`bps` _an Eb/No needs._
```C++
JM_FORCEINLINE int wfm_synth_bps (
    int type
) 
```



QPSK carries two, everything else one. DSSS is one because its payload is BPSK, which is what makes `ebno == esno` for a DSSS source. 


        

<hr>



### function wfm\_synth\_cont\_dsss\_chip 

_One continuous-DSSS chip:_ `code[n % n_code] ^ data` _, as a BPSK sign._
```C++
JM_FORCEINLINE float wfm_synth_cont_dsss_chip (
    dp_wfm_synth_state_t * s
) 
```



The per-chip kernel shared by `dp_wfm_synth_step` and `dp_wfm_synth_steps` (and the manifest `impl`), so the single-sample and block paths cannot diverge — they call the SAME function rather than each inlining the arithmetic. Advances the code clock (`n % n_code`) and the INDEPENDENT symbol clock (`floor(n / chips_per_symbol)`) off one running chip counter; at each symbol boundary it refreshes the data bit from the configured source (constant 0 for code-only, the next payload bit, or the next PN bit). Non-integer `chips_per_symbol` is what makes symbol edges land mid-epoch — the asynchronicity.


With a frame set (`dp_wfm_synth_set_dsss_window`), the frame lives on the SYMBOL clock: of every `frame_symbols` symbols, the first `code_only_symbols` carry data 0 — the pure code — and the rest carry the payload, whose index counts data symbols only, so the bits run on across frames. The symbol clock never restarts: it is the same free-running `floor(n / chips_per_symbol)` with or without a window, so a frame edge falls at whatever chip phase that clock puts it — the chip and data clocks have no fixed relation, and no frame edge is synchronous with a code epoch. `frame_symbols == 0` is the windowless stream, bit for bit.


Requires `chips_per_symbol >= 1` (chip rate &gt;= symbol rate, always true for a real DSSS waveform), so the symbol index advances by 0 or 1 per chip and the PN is never asked to skip. 


        

<hr>



### function wfm\_synth\_mls\_poly 

_The MLS primitive polynomial table — pn's, reached by its old name._ 
```C++
JM_FORCEINLINE uint64_t wfm_synth_mls_poly (
    uint32_t n
) 
```



The table itself moved to `pn/pn_core.h` (`pn_mls_poly`), because the convention it encodes is [**dp\_pn\_create()**](pn__core_8h.md#function-dp_pn_create)'s tap mask and not the synth's. This spelling is retained for the call sites that already use it; it forwards and holds no table of its own, so the two cannot disagree. 


        

<hr>



### function wfm\_synth\_next\_symbol 

_Pull the next constellation symbol from the active shaped source._ 
```C++
JM_FORCEINLINE float _Complex wfm_synth_next_symbol (
    dp_wfm_synth_state_t * s
) 
```



The single symbol-generation point the polyphase pulse shaper feeds from, dispatching on the waveform type exactly as `dp_wfm_synth_step`'s symbol latch does — the PN LFSR (pn/bpsk one chip, qpsk two Gray chips), the user bit pattern (bits, per bit\_mod, sent once), the continuous asynchronous DSSS chip, or the cycled complex-symbol stream — and advancing that source's read cursor by one symbol. Only the shaped types (pn/bpsk/qpsk/bits/symbols/dsss, the set `dp_wfm_synth_set_rrc` accepts) reach here, so the shaper draws the _same_ symbol sequence the dense-FIR path would; only the pulse-shaping filter differs. 


        

<hr>



### function wfm\_synth\_shape 

_Produce_ `m` _polyphase-shaped baseband samples into_`out` _._
```C++
JM_FORCEINLINE void wfm_synth_shape (
    dp_wfm_synth_state_t * s,
    float _Complex * out,
    size_t m,
    float _Complex * syms
) 
```



The one shaping kernel shared by `dp_wfm_synth_step` (m == 1) and `dp_wfm_synth_steps` (m == block): prime once, generate exactly the `dp_resamp_interp_inputs_needed(shaper, m)` symbols this call consumes into the caller's `syms` scratch, and fill `m` outputs. Because the resampler is block-boundary invariant and both faces call this identical routine, a single m-sample call and m one-sample calls produce bit-identical output — the step()==steps() guarantee. Carrier mix and noise are applied by the caller.




**Parameters:**


* `s` Shaper-attached synth state (`s->shaper != NULL`). 
* `out` Output buffer, capacity &gt;= `m`. 
* `m` Number of baseband samples to produce. 
* `syms` Caller scratch, capacity &gt;= dp\_resamp\_interp\_inputs\_needed(s, m). 




        

<hr>



### function wfm\_synth\_shaper\_prime 

_Prime the shaper's delay line so its output aligns with the dense FIR._ 
```C++
JM_FORCEINLINE void wfm_synth_shaper_prime (
    dp_wfm_synth_state_t * s
) 
```



The polyphase interpolator emits its first meaningful sample only after the delay line fills, so its output lags the dense-FIR path by exactly `nsps` samples. Discarding that many leading outputs once, at stream start (which consumes exactly the first source symbol into the delay line), realigns the shaped waveform to the dense path to float precision — so switching a source to polyphase shaping does not shift downstream sample timing. Idempotent via the `primed` flag; re-armed by `dp_wfm_synth_reset`. 


        

<hr>



### function wfm\_synth\_snr\_over\_fs 

_Convert a per-symbol or per-bit SNR to SNR over the full sample rate._ 
```C++
JM_FORCEINLINE double wfm_synth_snr_over_fs (
    int mode,
    int bps,
    double span,
    double snr
) 
```



**The one place this arithmetic lives.** A noise amplitude is always referenced to fs, so every SNR mode is a conversion into that: an Es/N0 spreads the symbol's energy over `span` samples, and an Eb/No does the same after first multiplying by the bits the symbol carries. Getting it wrong is silent — the waveform is still a waveform, at an SNR nobody asked for — so having it written twice is how a generator and a composer come to place different noise for the same requested number.




**Parameters:**


* `mode` RESOLVED mode: 1 fs, 2 Eb/No, 3 Es/No. Never 0 (auto) — see below. 
* `bps` Bits per symbol, from [**wfm\_synth\_bps()**](wfm__synth__core_8h.md#function-wfm_synth_bps). 
* `span` Samples one symbol's energy is spread over. 
* `snr` The requested figure, in dB, in `mode's` reference. 



**Returns:**

SNR in dB over fs, ready for [**dp\_awgn\_amplitude\_for\_snr()**](awgn__core_8h.md#function-dp_awgn_amplitude_for_snr).


\*\*`auto` and `span` are deliberately the CALLER's\*\*, and that is not an oversight: they are the two things that legitimately differ. `wfm_synth` resolves `auto` to fs for a DSSS source because at create() time it cannot do better — the codes attach afterwards, so the spreading factor that sets the symbol span is not yet known — while the composer resolves the same source to Es/No and passes the true span (`sf * sps` for a burst, or `fs/symbol_rate` for a continuous asynchronous stream, which coincide only in the synchronous case that mode exists to avoid). Those differences are inputs, not a second formula. 
```C++
// Es/No 12 dB at 8 samples/symbol -> 2.969 dB over fs
double fs_db = wfm_synth_snr_over_fs (3, 1, 8.0, 12.0);
// the same figure read as Eb/No on QPSK is 3.010 dB hotter
double eb_db = wfm_synth_snr_over_fs (2, wfm_synth_bps (WFM_SYNTH_QPSK),
                                      8.0, 12.0);
```
 


        

<hr>
## Macro Definition Documentation





### define WFM\_DSSS\_ENDED 

```C++
#define WFM_DSSS_ENDED `2u`
```



`cur_data` once a continuous stream's data has ended: not a bit, so the chips are silent from then on, and it is a byte of the serialized state like the bit it replaces. 


        

<hr>



### define WFM\_SYNTH\_SNR\_CLEAN 

```C++
#define WFM_SYNTH_SNR_CLEAN `100.0`
```




<hr>



### define WFM\_SYNTH\_STATE\_MAGIC 

```C++
#define WFM_SYNTH_STATE_MAGIC `DP_FOURCC ('W','F','M','S')`
```




<hr>



### define WFM\_SYNTH\_STATE\_VERSION 

```C++
#define WFM_SYNTH_STATE_VERSION `3u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm_synth/wfm_synth_core.h`

