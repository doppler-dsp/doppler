

# File mpsk\_receiver\_r\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**mpsk\_receiver\_r**](dir_2235ea4ae040991d93c0b2870a03660e.md) **>** [**mpsk\_receiver\_r\_core.h**](mpsk__receiver__r__core_8h.md)

[Go to the source code of this file](mpsk__receiver__r__core_8h_source.md)

_Real-input M-PSK receiver: the complex twin behind an R2C front end._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "ddcr/ddcr_core.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "mpsk_receiver/mpsk_rx_loops.h"`
* `#include <complex.h>`
* `#include "ddc/ddc_core.h"`
* `#include "mpsk_receiver/mpsk_receiver_core.h"`
* `#include "ratesync/ratesync_core.h"`
* `#include "RateConverter/RateConverter_core.h"`
* `#include "resamp/resamp_core.h"`
* `#include "hbdecim/hbdecim_core.h"`
* `#include "cic/cic_core.h"`
* `#include "fir/fir_core.h"`
* `#include "resample/resample_core.h"`
* `#include "lo/lo_core.h"`
* `#include "nco/nco_core.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include "lockdet/lockdet_core.h"`
* `#include "symsync/symsync_core.h"`
* `#include "agc/agc_core.h"`
* `#include "dp_tlm/dp_tlm_core.h"`
* `#include "ber/ber_core.h"`
* `#include "telemetry/telemetry_core.h"`
* `#include "boxcar/boxcar_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) <br>_Real-input M-PSK receiver state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**mpsk\_receiver\_r\_bits**](#function-mpsk_receiver_r_bits) ([**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state, const float \* x, size\_t x\_len, uint8\_t \* out, size\_t max\_out) <br>_Demodulate a real f32 block and emit hard Gray-coded bits._  |
|  size\_t | [**mpsk\_receiver\_r\_bits\_max\_out**](#function-mpsk_receiver_r_bits_max_out) ([**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br> |
|  void | [**mpsk\_receiver\_r\_configure\_lock**](#function-mpsk_receiver_r_configure_lock) ([**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune the acquisition&lt;-&gt;tracking handover detector directly._  |
|  [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* | [**mpsk\_receiver\_r\_create**](#function-mpsk_receiver_r_create) (int m, double sps, size\_t m\_out, int pulse, double rrc\_beta, int rrc\_span, double bn\_carrier, double zeta, double bn\_timing, int acq\_to\_track, double lock\_thresh, double init\_norm\_freq, int differential, size\_t num\_phases, int nda\_tap, int agc, double bn\_agc\_ratio) <br>_Create a real-input M-PSK receiver._  |
|  void | [**mpsk\_receiver\_r\_destroy**](#function-mpsk_receiver_r_destroy) ([**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br>_Destroy and release all memory._  |
|  double | [**mpsk\_receiver\_r\_get\_agc\_gain\_db**](#function-mpsk_receiver_r_get_agc_gain_db) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br>_Gain the front end's AGC is applying, in dB; 0.0 when_ `agc` _= 0._ |
|  int | [**mpsk\_receiver\_r\_get\_clipped**](#function-mpsk_receiver_r_get_clipped) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br>_Has the cascade's CIC clipped its input since the last reset?_  |
|  double | [**mpsk\_receiver\_r\_get\_last\_error**](#function-mpsk_receiver_r_get_last_error) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br> |
|  double | [**mpsk\_receiver\_r\_get\_lock**](#function-mpsk_receiver_r_get_lock) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br> |
|  int64\_t | [**mpsk\_receiver\_r\_get\_lock\_time**](#function-mpsk_receiver_r_get_lock_time) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br>_Symbols from reset to the FIRST carrier-lock declaration, or -1 if the receiver has not locked yet._  |
|  int | [**mpsk\_receiver\_r\_get\_locked**](#function-mpsk_receiver_r_get_locked) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br> |
|  int | [**mpsk\_receiver\_r\_get\_m**](#function-mpsk_receiver_r_get_m) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br> |
|  size\_t | [**mpsk\_receiver\_r\_get\_m\_out**](#function-mpsk_receiver_r_get_m_out) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br> |
|  double | [**mpsk\_receiver\_r\_get\_nco\_freq**](#function-mpsk_receiver_r_get_nco_freq) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br>_Instantaneous NCO frequency command at the real input rate._  |
|  double | [**mpsk\_receiver\_r\_get\_norm\_freq**](#function-mpsk_receiver_r_get_norm_freq) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br>_Tracked carrier, cycles/sample at the REAL input rate._  |
|  double | [**mpsk\_receiver\_r\_get\_sps**](#function-mpsk_receiver_r_get_sps) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br> |
|  void | [**mpsk\_receiver\_r\_get\_state**](#function-mpsk_receiver_r_get_state) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state, void \* blob) <br> |
|  double | [**mpsk\_receiver\_r\_get\_timing\_rate**](#function-mpsk_receiver_r_get_timing_rate) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* s) <br> |
|  int | [**mpsk\_receiver\_r\_get\_tracking**](#function-mpsk_receiver_r_get_tracking) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br> |
|  void | [**mpsk\_receiver\_r\_reset**](#function-mpsk_receiver_r_reset) ([**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br>_Re-seed the front end and both loops to their create-time state._  |
|  void | [**mpsk\_receiver\_r\_set\_norm\_freq**](#function-mpsk_receiver_r_set_norm_freq) ([**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state, double val) <br>_Retune to_ `val` _cycles/sample at the real input rate._ |
|  int | [**mpsk\_receiver\_r\_set\_state**](#function-mpsk_receiver_r_set_state) ([**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state, const void \* blob) <br> |
|  int | [**mpsk\_receiver\_r\_set\_telemetry**](#function-mpsk_receiver_r_set_telemetry) ([**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context across the receiver._  |
|  size\_t | [**mpsk\_receiver\_r\_state\_bytes**](#function-mpsk_receiver_r_state_bytes) (const [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**mpsk\_receiver\_r\_step\_ted**](#function-mpsk_receiver_r_step_ted) ([**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* s, float x, float complex \* y\_out, int ted) <br>_Push one real input sample; emit a symbol if it completed one._  |
|  size\_t | [**mpsk\_receiver\_r\_steps**](#function-mpsk_receiver_r_steps) ([**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state, const float \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br>_Demodulate a real f32 block and emit the recovered symbols._  |
|  size\_t | [**mpsk\_receiver\_r\_steps\_max\_out**](#function-mpsk_receiver_r_steps_max_out) ([**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**MPSK\_RECEIVER\_R\_STATE\_MAGIC**](mpsk__receiver__r__core_8h.md#define-mpsk_receiver_r_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('M', 'P', 'S', 'R')`<br> |
| define  | [**MPSK\_RECEIVER\_R\_STATE\_VERSION**](mpsk__receiver__r__core_8h.md#define-mpsk_receiver_r_state_version)  `2u`<br> |

## Detailed Description


[**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md)'s real-input counterpart, and the same object in every way that matters — it owns a [**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) (the matched flavor) instead of a [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t), and drives the identical [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md). Everything the loops do — the symbol-timing loop on the cascade's `rate_ctrl` port, the M-th-power NDA and decision-directed carrier discriminators on the LO's `freq_ctrl` port, the two-way handover, the demapper — is one shared implementation, not a copy. Read [**mpsk\_rx\_loops.h**](mpsk__rx__loops_8h.md) for all of it; only what the front end changes is described here.



```C++
f32 in ──> MatchedDdcr ─────────────────────────────> y ──> loops ──> syms
            halfband R2C (2:1) · LO mix · cascade · MF
```



Two consequences follow from the halfband, and they are the whole difference:


**The LO runs at half the input rate.** The R2C halfband decimates 2:1 (with the fs/4 shift baked in) _before_ the mix, so the LO sees `sps/2` samples per symbol. `freq_ctrl` is in cycles per sample at that intermediate rate, which is why [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) takes `lo_sps` as a parameter rather than assuming it equals `sps`. `norm_freq` stays caller-facing in cycles/sample at the **input** rate, so it is halved on the way in and doubled on the way out — the conversion lives in this file and nowhere else.


\*\*`sps` must exceed `2 * m_out`.\*\* The cascade behind the halfband runs at `2 * rate`, and Ddcr requires `rate < 0.5`; the receiver asks for `rate = m_out/sps`, so `sps > 2 * m_out` (against `sps >= m_out` for the complex type). At the default `m_out = 8` that is `sps > 16`, which is why this type's `sps` default is 32.0 where the complex twin's is 8.0.


A real-valued IF is the usual output of a single-ended ADC, so this is the type that takes a digitiser's samples directly. Everything downstream — symbols, bits, telemetry, serialization — is identical to the complex twin.


Lifecycle: `mpsk_receiver_r_create -> (steps / bits / reset)* -> _destroy`.



```C++
// QPSK on a real IF at 0.2*fs, 16 samples/symbol, RRC matched filter
mpsk_receiver_r_state_t *rx = mpsk_receiver_r_create (
    4, 16.0, 4, MPSK_RX_PULSE_RRC, 0.35, 8,
    0.005, 0.707, 0.01, 0, 0.5, 0.2, 100, 0, 1024);
float complex sym[256];
size_t k = mpsk_receiver_r_steps (rx, rx_in, rx_len, sym, 256);
mpsk_receiver_r_destroy (rx);
```
 


    
## Public Functions Documentation




### function mpsk\_receiver\_r\_bits 

_Demodulate a real f32 block and emit hard Gray-coded bits._ 
```C++
size_t mpsk_receiver_r_bits (
    mpsk_receiver_r_state_t * state,
    const float * x,
    size_t x_len,
    uint8_t * out,
    size_t max_out
) 
```



As [**mpsk\_receiver\_bits()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_bits), taking real samples.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Real f32 input samples. 
* `x_len` Number of input samples. 
* `out` Output bytes (0/1); caller provides `max_out` capacity. 
* `max_out` Output capacity. 



**Returns:**

Number of bits written. 
```C++
>>> import numpy as np
>>> from doppler.track import MpskReceiverR
>>> rng = np.random.default_rng(3)
>>> idx = rng.integers(0, 2, 2400)                  # BPSK payload bits
>>> bb = np.repeat(np.exp(1j * np.pi * idx), 32)
>>> n = np.arange(bb.size)
>>> x = (0.4 * bb * np.exp(2j * np.pi * 0.25 * n)).real  # IF at fs/4
>>> x = np.ascontiguousarray(x.astype(np.float32))
>>> rx = MpskReceiverR(m=2, sps=32, m_out=8, init_norm_freq=0.25,
...                    bn_carrier=0.005)
>>> b = rx.bits(x)                                  # 1 hard bit/symbol
>>> b.size
2398
>>> # settled tail matches the payload, up to the BPSK
>>> # inversion ambiguity
>>> tail = np.mean(b[1500:2300] != idx[1500:2300])
>>> round(float(min(tail, 1 - tail)), 3)
0.0
```
 





        

<hr>



### function mpsk\_receiver\_r\_bits\_max\_out 

```C++
size_t mpsk_receiver_r_bits_max_out (
    mpsk_receiver_r_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_r\_configure\_lock 

_Re-tune the acquisition&lt;-&gt;tracking handover detector directly._ 
```C++
void mpsk_receiver_r_configure_lock (
    mpsk_receiver_r_state_t * state,
    double up_thresh,
    double down_thresh,
    uint32_t n_up,
    uint32_t n_down
) 
```



The real-input twin of [**mpsk\_receiver\_configure\_lock()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_configure_lock), whose contract it shares exactly: a split declare/drop threshold pair on the carrier lock EMA (level hysteresis) plus both verify counts (time hysteresis). A live handover survives the re-tune; the in-flight verify run restarts.




**Parameters:**


* `state` Must be non-NULL. 
* `up_thresh` Declare threshold on the carrier lock EMA. 
* `down_thresh` Drop threshold; choose &lt;= up\_thresh for level hysteresis. 
* `n_up` Consecutive above-threshold symbols to hand over to the decision-directed discriminator; clamped &gt;= 1. 
* `n_down` Consecutive below-threshold symbols to fall back to NDA acquisition; clamped &gt;= 1. 
```C++
>>> from doppler.track import MpskReceiverR
>>> rx = MpskReceiverR(m=4, sps=10, m_out=2, acq_to_track=1)
>>> rx.tracking
0
>>> rx.configure_lock(0.9, 0.72, 4, 16)   # tighter declare, fast drop
```
 




        

<hr>



### function mpsk\_receiver\_r\_create 

_Create a real-input M-PSK receiver._ 
```C++
mpsk_receiver_r_state_t * mpsk_receiver_r_create (
    int m,
    double sps,
    size_t m_out,
    int pulse,
    double rrc_beta,
    int rrc_span,
    double bn_carrier,
    double zeta,
    double bn_timing,
    int acq_to_track,
    double lock_thresh,
    double init_norm_freq,
    int differential,
    size_t num_phases,
    int nda_tap,
    int agc,
    double bn_agc_ratio
) 
```



Parameters match [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create) exactly; only two behave differently, both because of the R2C halfband:




**Parameters:**


* `m` Constellation order M, 2/4/8 (default 4 = QPSK). 
* `sps` Samples per symbol at the REAL input; any double **strictly greater than `2 * m_out`** (default 32.0). The cascade behind the halfband runs at twice the overall rate, and Ddcr requires that rate below 0.5. 
* `m_out` Terminal outputs per symbol: even, 2..8 (default 8) — where an I&D matched filter reaches the coherent bound; see the complex twin's create() for the measurements. It is this default that forces `sps`'s to 32.0. 
* `pulse` Matched-filter shape (default MPSK\_RX\_PULSE\_IANDD). 
* `rrc_beta` RRC roll-off in `[0, 1]` (default 0.35; RRC only). 
* `rrc_span` RRC one-sided span in symbols (default 8; RRC only). 
* `bn_carrier` Carrier loop noise bandwidth, normalised to the symbol rate (default 0.005). 
* `zeta` Damping factor for both loops (default 0.707). 
* `bn_timing` Timing loop noise bandwidth, per symbol (0.01). 
* `acq_to_track` Enable the two-way handover (default 0). 
* `lock_thresh` Handover declare threshold (default 0.5). 
* `init_norm_freq` Carrier frequency to tune to, cycles/sample **at the real input rate** (default 0.0). A real IF at `0.2 * fs` is `0.2`; the halved value the LO actually uses is this object's business, not the caller's. 
* `differential` bits(): differential demap (default 0). 
* `num_phases` Terminal-stage bank arms, a power of two (1024). 
* `nda_tap` MPSK\_RX\_NDA\_TAP\_\* — where the NDA carrier discriminator reads, and so its pull-in range: `_STROBE` (0, default) at `Rs` and the only tap needing symbol timing, or `_MF_OUT` (1) at `m_out*Rs`. See [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create) for the full trade and the measured ranges.

`_MF_IN` is NOT accepted here yet: it reads the cascade's `bank_sps` rate, which this front end does not publish (its `ddcr` carries the same RateConverter, so wiring it is small — measured, `bank_sps` comes out identical on both types). Construction refuses it rather than falling back to a rate that would mis-size the loop.


One further difference: this type does not acquire from a cold zero the way the complex twin does — a real IF must be tuned near, so `init_norm_freq` is the centre and a tap buys pull-in _around_ it, not from nothing. 

**Parameters:**


* `agc` Non-zero (default) puts this receiver's ONE AGC in the front-end cascade, before the terminal matched stage — the same placement and the same reason as the complex twin ([**mpsk\_receiver\_create**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create)): it serves BOTH loops, since carrier and timing both run on its output. The timing detector is the one whose gain depends on the level (its slope is a construct-time constant for a unit-amplitude stream); the carrier detector normalises itself but still sees the AGC's transient. Pass 0 and the timing loop is under-driven by `A^2`. 
* `bn_agc_ratio` That AGC's bandwidth as a fraction of the SLOWEST loop it feeds, `min(bn_carrier, bn_timing)` — see [**mpsk\_rx\_agc\_bn**](mpsk__rx__loops_8h.md#function-mpsk_rx_agc_bn). In (0, 1), refused at 1 or above; `MPSK_RX_AGC_BW_RATIO` (0.05) by default. 



**Returns:**

Heap-allocated state, or NULL on invalid args / allocation failure. 




**Note:**

Caller must call [**mpsk\_receiver\_r\_destroy()**](mpsk__receiver__r__core_8h.md#function-mpsk_receiver_r_destroy) when done. 





        

<hr>



### function mpsk\_receiver\_r\_destroy 

_Destroy and release all memory._ 
```C++
void mpsk_receiver_r_destroy (
    mpsk_receiver_r_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function mpsk\_receiver\_r\_get\_agc\_gain\_db 

_Gain the front end's AGC is applying, in dB; 0.0 when_ `agc` _= 0._
```C++
double mpsk_receiver_r_get_agc_gain_db (
    const mpsk_receiver_r_state_t * state
) 
```



The twin of [**mpsk\_receiver\_get\_agc\_gain\_db()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_get_agc_gain_db), and the same diagnostic: a receiver that will not lock with a healthy `lock` statistic, or one whose timing loop behaves differently at two input levels, is asking about this number. Note the AGC sits inside the cascade BEHIND the R2C halfband, so it levels the analytic signal at the intermediate rate, not the real input  which is what makes its reference the same derived `bank_e0 / bank_sps` the complex twin uses. 


        

<hr>



### function mpsk\_receiver\_r\_get\_clipped 

_Has the cascade's CIC clipped its input since the last reset?_ 
```C++
int mpsk_receiver_r_get_clipped (
    const mpsk_receiver_r_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_r\_get\_last\_error 

```C++
double mpsk_receiver_r_get_last_error (
    const mpsk_receiver_r_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_r\_get\_lock 

```C++
double mpsk_receiver_r_get_lock (
    const mpsk_receiver_r_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_r\_get\_lock\_time 

_Symbols from reset to the FIRST carrier-lock declaration, or -1 if the receiver has not locked yet._ 
```C++
int64_t mpsk_receiver_r_get_lock_time (
    const mpsk_receiver_r_state_t * state
) 
```



The acquisition time, as a number a caller can read rather than infer by polling `locked` in a loop. Dated by the same hysteretic detector `mpsk_receiver_r_get_locked()` reports, so the two cannot disagree.


In SYMBOLS, not seconds: `bn_carrier` and `bn_timing` are both normalised to the symbol rate, so a settling budget quoted in symbols is comparable across every input rate, and a caller with `Rs` divides once. Only the first declaration is dated — a drop and re-acquire does not restamp it, because the question this answers is "how long did this receiver take to
lock", not "when did it last hold". [**mpsk\_receiver\_r\_reset()**](mpsk__receiver__r__core_8h.md#function-mpsk_receiver_r_reset) clears it to -1. 


        

<hr>



### function mpsk\_receiver\_r\_get\_locked 

```C++
int mpsk_receiver_r_get_locked (
    const mpsk_receiver_r_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_r\_get\_m 

```C++
int mpsk_receiver_r_get_m (
    const mpsk_receiver_r_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_r\_get\_m\_out 

```C++
size_t mpsk_receiver_r_get_m_out (
    const mpsk_receiver_r_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_r\_get\_nco\_freq 

_Instantaneous NCO frequency command at the real input rate._ 
```C++
double mpsk_receiver_r_get_nco_freq (
    const mpsk_receiver_r_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_r\_get\_norm\_freq 

_Tracked carrier, cycles/sample at the REAL input rate._ 
```C++
double mpsk_receiver_r_get_norm_freq (
    const mpsk_receiver_r_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_r\_get\_sps 

```C++
double mpsk_receiver_r_get_sps (
    const mpsk_receiver_r_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_r\_get\_state 

```C++
void mpsk_receiver_r_get_state (
    const mpsk_receiver_r_state_t * state,
    void * blob
) 
```




<hr>



### function mpsk\_receiver\_r\_get\_timing\_rate 

```C++
double mpsk_receiver_r_get_timing_rate (
    const mpsk_receiver_r_state_t * s
) 
```




<hr>



### function mpsk\_receiver\_r\_get\_tracking 

```C++
int mpsk_receiver_r_get_tracking (
    const mpsk_receiver_r_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_r\_reset 

_Re-seed the front end and both loops to their create-time state._ 
```C++
void mpsk_receiver_r_reset (
    mpsk_receiver_r_state_t * state
) 
```



Identical in effect to [**mpsk\_receiver\_reset()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_reset) — clears the R2C halfband and cascade memory, the carrier and timing NCOs, the loop integrators and the lock detectors, and returns the carrier estimate to `init_norm_freq`. Configuration is untouched, so a burst fed twice around a reset reproduces bit-for-bit.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.track import MpskReceiverR
>>> rng = np.random.default_rng(0)
>>> idx = rng.integers(0, 4, 300)
>>> bb = np.repeat(np.exp(2j * np.pi * idx / 4), 32)
>>> n = np.arange(bb.size)
>>> x = (0.4 * bb * np.exp(2j * np.pi * 0.25 * n)).real  # IF at fs/4
>>> x = np.ascontiguousarray(x.astype(np.float32))
>>> rx = MpskReceiverR(m=4, sps=32, m_out=8, init_norm_freq=0.25)
>>> first = rx.steps(x)
>>> rx.reset()                                # back to the cold state
>>> np.array_equal(first, rx.steps(x))        # same input, same output
True
```
 




        

<hr>



### function mpsk\_receiver\_r\_set\_norm\_freq 

_Retune to_ `val` _cycles/sample at the real input rate._
```C++
void mpsk_receiver_r_set_norm_freq (
    mpsk_receiver_r_state_t * state,
    double val
) 
```




<hr>



### function mpsk\_receiver\_r\_set\_state 

```C++
int mpsk_receiver_r_set_state (
    mpsk_receiver_r_state_t * state,
    const void * blob
) 
```




<hr>



### function mpsk\_receiver\_r\_set\_telemetry 

_Attach (or detach) a telemetry context across the receiver._ 
```C++
int mpsk_receiver_r_set_telemetry (
    mpsk_receiver_r_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```



Registers the same thirteen probes as [**mpsk\_receiver\_set\_telemetry()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_set_telemetry), whose contract it shares in full: the receiver's own "&lt;prefix&gt;.lock" and "&lt;prefix&gt;.tracking", the carrier loop's "&lt;prefix&gt;.car.e" / ".freq" / ".locked", and the symbol-timing loop's "&lt;prefix&gt;.sync.e" / ".ctrl" / ".rate" / ".lock" / ".locked" / ".mu" — eleven emitted once per recovered symbol — then the front end's AGC under "&lt;prefix&gt;.agc" (".gain\_db" and ".level\_db"), forwarded through [**ddcr\_set\_telemetry()**](ddcr__core_8h.md#function-ddcr_set_telemetry). All thinned by `decim`. Passing NULL detaches everything. Setup path, never hot; the context is borrowed and must outlive the attachment.




**Warning:**

As on the complex twin, the two AGC probes are on the cascade's MFR-input grid rather than the symbol grid, so their record count differs from the other eleven — compare by time, not by index. See [**mpsk\_receiver\_set\_telemetry()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_set_telemetry) for why, and for why that AGC is the slowest loop in the receiver.




**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "rx". 
* `decim` Emit every decim-th symbol (every decim-th gain update for the two AGC probes); &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take the probes (the attach fails whole; everything detached). 
```C++
>>> import numpy as np
>>> from doppler.track import MpskReceiverR
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 14)
>>> rx = MpskReceiverR(m=4, sps=10, m_out=2, init_norm_freq=0.25)
>>> rx.set_telemetry(tlm, "rx")
>>> len(tlm.probe_names)
13
>>> rng = np.random.default_rng(7)
>>> idx = rng.integers(0, 4, 512)
>>> bb = np.repeat(np.exp(2j * np.pi * idx / 4), 10)
>>> n = np.arange(bb.size)
>>> x = (0.4 * bb * np.exp(2j * np.pi * 0.25 * n)).real
>>> x = np.ascontiguousarray(x.astype(np.float32))
>>> _ = rx.steps(x)
>>> recs = tlm.read()
>>> tlm.dropped            # size the ring, or the counts below diverge
0
>>> n_sync = len(recs[recs["probe"] == tlm.probe_id("rx.sync.e")])
>>> n_car = len(recs[recs["probe"] == tlm.probe_id("rx.car.e")])
>>> n_sync > 0 and n_sync == n_car
True
>>> n_agc = len(recs[recs["probe"] == tlm.probe_id("rx.agc.gain_db")])
>>> n_agc > 0 and n_agc != n_sync   # cascade grid, not symbol grid
True
```
 





        

<hr>



### function mpsk\_receiver\_r\_state\_bytes 

```C++
size_t mpsk_receiver_r_state_bytes (
    const mpsk_receiver_r_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_r\_step\_ted 

_Push one real input sample; emit a symbol if it completed one._ 
```C++
JM_FORCEINLINE  JM_HOT int mpsk_receiver_r_step_ted (
    mpsk_receiver_r_state_t * s,
    float x,
    float complex * y_out,
    int ted
) 
```



The composition API, identical in shape to [**mpsk\_receiver\_step\_ted()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_step_ted) — only the front end and the input type differ.




**Parameters:**


* `s` State. Must be non-NULL. 
* `x` One real input sample. 
* `y_out` Receives the symbol when the return is 1. 
* `ted` RATESYNC\_TED\_GARDNER or RATESYNC\_TED\_DTTL — pass a literal for a specialised (branch-free) instantiation. 



**Returns:**

1 if a symbol was emitted (into `y_out`), 0 otherwise. 





        

<hr>



### function mpsk\_receiver\_r\_steps 

_Demodulate a real f32 block and emit the recovered symbols._ 
```C++
size_t mpsk_receiver_r_steps (
    mpsk_receiver_r_state_t * state,
    const float * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```



As [**mpsk\_receiver\_steps()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_steps), taking real samples: the R2C halfband makes them complex before anything else touches them.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Real f32 input samples. 
* `x_len` Number of input samples. 
* `out` Output symbols; caller provides `max_out` capacity. 
* `max_out` Output capacity. 



**Returns:**

Number of symbols written. 
```C++
>>> import numpy as np
>>> from doppler.track import MpskReceiverR
>>> rng = np.random.default_rng(3)
>>> idx = rng.integers(0, 4, 2400)                  # QPSK symbols
>>> bb = np.repeat(np.exp(2j * np.pi * idx / 4), 32)  # 32 sps
>>> n = np.arange(bb.size)
>>> x = (0.4 * bb * np.exp(2j * np.pi * 0.25 * n)).real  # IF at fs/4
>>> x = np.ascontiguousarray(x.astype(np.float32))
>>> rx = MpskReceiverR(m=4, sps=32, m_out=8, init_norm_freq=0.25)
>>> sym = rx.steps(x)
>>> sym.size                                        # ~ x_len / sps
2398
>>> rx.lock > 0.8                                   # carrier locked
True
```
 





        

<hr>



### function mpsk\_receiver\_r\_steps\_max\_out 

```C++
size_t mpsk_receiver_r_steps_max_out (
    mpsk_receiver_r_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define MPSK\_RECEIVER\_R\_STATE\_MAGIC 

```C++
#define MPSK_RECEIVER_R_STATE_MAGIC `DP_FOURCC ('M', 'P', 'S', 'R')`
```




<hr>



### define MPSK\_RECEIVER\_R\_STATE\_VERSION 

```C++
#define MPSK_RECEIVER_R_STATE_VERSION `2u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/mpsk_receiver_r/mpsk_receiver_r_core.h`

