

# File mpsk\_receiver\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**mpsk\_receiver**](dir_a1dc26622ebd32726f4fc723db7ccb3b.md) **>** [**mpsk\_receiver\_core.h**](mpsk__receiver__core_8h.md)

[Go to the source code of this file](mpsk__receiver__core_8h_source.md)

_Pulse-shaped M-PSK receiver: a tuned matched front end and two loops._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "ddc/ddc_core.h"`
* `#include "ddcr/ddcr_core.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "mpsk_receiver/mpsk_rx_loops.h"`
* `#include <complex.h>`
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
| struct | [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) <br>_M-PSK receiver state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**mpsk\_receiver\_bits**](#function-mpsk_receiver_bits) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, const float complex \* x, size\_t x\_len, uint8\_t \* out, size\_t max\_out) <br>_Demodulate a cf32 block and emit hard Gray-coded bits._  |
|  size\_t | [**mpsk\_receiver\_bits\_max\_out**](#function-mpsk_receiver_bits_max_out) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  size\_t | [**mpsk\_receiver\_bits\_real**](#function-mpsk_receiver_bits_real) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, const float \* x, size\_t x\_len, uint8\_t \* out, size\_t max\_out) <br>_Demodulate a real f32 block and emit hard Gray-coded bits._  |
|  size\_t | [**mpsk\_receiver\_bits\_real\_max\_out**](#function-mpsk_receiver_bits_real_max_out) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  void | [**mpsk\_receiver\_configure\_lock**](#function-mpsk_receiver_configure_lock) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune the acquisition&lt;-&gt;tracking handover detector directly._  |
|  [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* | [**mpsk\_receiver\_create**](#function-mpsk_receiver_create) (int m, double sps, size\_t m\_out, int pulse, double rrc\_beta, int rrc\_span, double bn\_carrier, double zeta, double bn\_timing, int acq\_to\_track, double lock\_thresh, double init\_norm\_freq, int differential, size\_t num\_phases, int nda\_tap, int agc, double bn\_agc\_ratio) <br>_Create an M-PSK receiver._  |
|  [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* | [**mpsk\_receiver\_create\_bpsk**](#function-mpsk_receiver_create_bpsk) (double sample\_rate\_hz, double symbol\_rate\_hz, double carrier\_freq\_hz, int pulse, double rrc\_beta, int rrc\_span, double bn\_carrier, double bn\_timing, int acq\_to\_track, int differential, int agc) <br>_A BPSK receiver stated in the units a caller actually holds: Hz._  |
|  [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* | [**mpsk\_receiver\_create\_continuous**](#function-mpsk_receiver_create_continuous) (int m, double sps, int pulse, double rrc\_beta, int rrc\_span, double bn\_carrier, double bn\_timing, double init\_norm\_freq, int differential) <br>_The continuous flavor: one discriminator, and nothing waits._  |
|  [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* | [**mpsk\_receiver\_create\_real**](#function-mpsk_receiver_create_real) (int m, double sps, size\_t m\_out, int pulse, double rrc\_beta, int rrc\_span, double bn\_carrier, double zeta, double bn\_timing, int acq\_to\_track, double lock\_thresh, double init\_norm\_freq, int differential, size\_t num\_phases, int nda\_tap, int agc, double bn\_agc\_ratio) <br>_Create the same receiver behind an R2C halfband: a real IF in._  |
|  void | [**mpsk\_receiver\_destroy**](#function-mpsk_receiver_destroy) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Destroy an M-PSK receiver and release all memory._  |
|  double | [**mpsk\_receiver\_get\_agc\_gain\_db**](#function-mpsk_receiver_get_agc_gain_db) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Gain the front end's AGC is applying, in dB; 0.0 when_ `agc` _= 0._ |
|  double | [**mpsk\_receiver\_get\_bn\_agc\_ratio**](#function-mpsk_receiver_get_bn_agc_ratio) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_AGC bandwidth ratio in use — derived unless pinned (§8.1)._  |
|  int | [**mpsk\_receiver\_get\_clipped**](#function-mpsk_receiver_get_clipped) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Has the cascade's CIC stage clipped its input since the last reset? A CIC bounds its input to +-1.0 and clips silently past that, which costs ~25 dB of EVM behind a perfectly healthy lock._  |
|  double | [**mpsk\_receiver\_get\_last\_error**](#function-mpsk_receiver_get_last_error) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Carrier loop phase discriminator (rad) — the residual phase the loop is trying to null; loop stress._  |
|  double | [**mpsk\_receiver\_get\_lock**](#function-mpsk_receiver_get_lock) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  double | [**mpsk\_receiver\_get\_lock\_drop\_thresh**](#function-mpsk_receiver_get_lock_drop_thresh) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Carrier DROP threshold in use —_ `MPSK_RX_HANDOVER_DOWN` _x the declare threshold, the level hysteresis the pair is stated with._ |
|  double | [**mpsk\_receiver\_get\_lock\_thresh**](#function-mpsk_receiver_get_lock_thresh) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Handover lock threshold in use — derived unless pinned (§8.1)._  |
|  int64\_t | [**mpsk\_receiver\_get\_lock\_time**](#function-mpsk_receiver_get_lock_time) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Symbols from reset to the FIRST carrier-lock declaration, or -1 if the receiver has not locked yet._  |
|  int | [**mpsk\_receiver\_get\_locked**](#function-mpsk_receiver_get_locked) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Binary carrier-lock flag from the loop's hysteretic (up/down verify-counted) lock detector — de-chattered, unlike the raw metric._  |
|  int | [**mpsk\_receiver\_get\_m**](#function-mpsk_receiver_get_m) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  size\_t | [**mpsk\_receiver\_get\_m\_out**](#function-mpsk_receiver_get_m_out) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Terminal outputs per symbol (the old_ `n` _, now the cascade's)._ |
|  double | [**mpsk\_receiver\_get\_nco\_freq**](#function-mpsk_receiver_get_nco_freq) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Instantaneous NCO frequency command (carrier loop filter output, cycles/sample): mean tracks a ramp with no lag, variance is loop stress._  |
|  double | [**mpsk\_receiver\_get\_norm\_freq**](#function-mpsk_receiver_get_norm_freq) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Carrier frequency the receiver is tracking, cycles/sample at the input rate: the create-time centre plus the loop's own estimate._  |
|  size\_t | [**mpsk\_receiver\_get\_num\_phases**](#function-mpsk_receiver_get_num_phases) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Matched-filter bank arms in use — derived unless pinned (§8.1)._  |
|  double | [**mpsk\_receiver\_get\_sps**](#function-mpsk_receiver_get_sps) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  void | [**mpsk\_receiver\_get\_state**](#function-mpsk_receiver_get_state) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, void \* blob) <br> |
|  double | [**mpsk\_receiver\_get\_sync\_lock\_drop\_thresh**](#function-mpsk_receiver_get_sync_lock_drop_thresh) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Timing DROP threshold on_ `sync.lock` _. Equal to the declare threshold when the timing loop carries no level hysteresis._ |
|  double | [**mpsk\_receiver\_get\_sync\_lock\_thresh**](#function-mpsk_receiver_get_sync_lock_thresh) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Timing DECLARE threshold on_ `sync.lock` _, derived by symsync's own (rolloff, esno\_min, pfa, pd) geometry rather than pinned._ |
|  double | [**mpsk\_receiver\_get\_timing\_rate**](#function-mpsk_receiver_get_timing_rate) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Smoothed tracked samples per symbol — departs from the nominal_ `sps` _by exactly the sample-clock offset the timing loop is tracking._ |
|  int | [**mpsk\_receiver\_get\_tracking**](#function-mpsk_receiver_get_tracking) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  double | [**mpsk\_receiver\_get\_zeta**](#function-mpsk_receiver_get_zeta) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Loop damping in use — derived_ `1/sqrt(2)` _unless pinned (§8.1)._ |
|  void | [**mpsk\_receiver\_reset**](#function-mpsk_receiver_reset) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Re-seed the front end and both loops to their create-time state._  |
|  void | [**mpsk\_receiver\_set\_norm\_freq**](#function-mpsk_receiver_set_norm_freq) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, double val) <br>_Retune to_ `val` _cycles/sample: moves the LO centre there and zeroes the loop's residual estimate, so norm\_freq reads back exactly._ |
|  int | [**mpsk\_receiver\_set\_state**](#function-mpsk_receiver_set_state) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, const void \* blob) <br> |
|  int | [**mpsk\_receiver\_set\_telemetry**](#function-mpsk_receiver_set_telemetry) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context across the receiver. Registers the receiver's own "&lt;prefix&gt;.lock" probe (the carrier lock EMA) and "&lt;prefix&gt;.tracking" (the two-way handover decision, 0/1 — so a consumer sees exactly when the carrier was handed to the decision-directed discriminator or dropped back to NDA), then the carrier loop's "&lt;prefix&gt;.car.e" / ".freq" / ".locked" and the symbol-timing loop's "&lt;prefix&gt;.sync.e" / ".ctrl" / ".rate" / ".lock" / ".locked" / ".mu"_  _eleven probes emitted once per recovered symbol_ _then the front end's AGC under "&lt;prefix&gt;.agc" ("&lt;prefix&gt;.agc.gain\_db" and "&lt;prefix&gt;.agc.level\_db"; see_[_**agc\_set\_telemetry()**_](agc__core_8h.md#function-agc_set_telemetry) _). Thirteen probes total, all thinned by_`decim` _. Passing NULL detaches everything._ |
|  size\_t | [**mpsk\_receiver\_state\_bytes**](#function-mpsk_receiver_state_bytes) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**mpsk\_receiver\_step\_real\_ted**](#function-mpsk_receiver_step_real_ted) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* s, float x, float complex \* y\_out, int ted) <br>_Push one REAL input sample; emit a symbol if it completed one._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**mpsk\_receiver\_step\_ted**](#function-mpsk_receiver_step_ted) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* s, float complex x, float complex \* y\_out, int ted) <br>_Push one input sample; emit a symbol if it completed one._  |
|  size\_t | [**mpsk\_receiver\_steps**](#function-mpsk_receiver_steps) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br>_Demodulate a cf32 block and emit the recovered symbols._  |
|  size\_t | [**mpsk\_receiver\_steps\_max\_out**](#function-mpsk_receiver_steps_max_out) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  size\_t | [**mpsk\_receiver\_steps\_real**](#function-mpsk_receiver_steps_real) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, const float \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br>_Demodulate a real f32 block and emit the recovered symbols._  |
|  size\_t | [**mpsk\_receiver\_steps\_real\_max\_out**](#function-mpsk_receiver_steps_real_max_out) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**MPSK\_RECEIVER\_R\_STATE\_MAGIC**](mpsk__receiver__core_8h.md#define-mpsk_receiver_r_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('M', 'P', 'S', 'R')`<br> |
| define  | [**MPSK\_RECEIVER\_R\_STATE\_VERSION**](mpsk__receiver__core_8h.md#define-mpsk_receiver_r_state_version)  `2u`<br> |
| define  | [**MPSK\_RECEIVER\_STATE\_MAGIC**](mpsk__receiver__core_8h.md#define-mpsk_receiver_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('M', 'P', 'S', 'K')`<br> |
| define  | [**MPSK\_RECEIVER\_STATE\_VERSION**](mpsk__receiver__core_8h.md#define-mpsk_receiver_state_version)  `6u /\* v5: rebuilt on the matched DDC \*/`<br> |

## Detailed Description


A complete inline modem for a continuous (unspread) M-PSK signal at **any** input rate. It is the top of the polyphase family, and it is composition rather than machinery — it owns no filter, no NCO and no interpolator of its own:



```C++
x ──> MatchedDDC ──────────────────────────────> y ──> loops ──> symbols
       LO mix · CIC/HB cascade · matched filter        │
         ^                            ^                │
         └── freq_ctrl ── carrier ────┴── rate_ctrl ───┘
```



### One object, two front ends



A **real** IF — the usual output of a single-ended ADC — is the same receiver behind an R2C halfband, and it is a `real` flag on this state rather than a second type:



```C++
f32 in ──> MatchedDdcr ────────────────────────> y ──> the SAME loops
            halfband R2C (2:1) · LO mix · cascade · MF
```



Every loop, discriminator, handover rule and demapper decision is one implementation over one [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md). What the front end changes is exactly three things, and each is a rate convention rather than an algorithm:



* **The LO runs at half the input rate.** The R2C halfband decimates 2:1 (with the fs/4 shift baked in) _before_ the mix, so the LO sees `sps/2` samples per symbol — which is why [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) takes `lo_sps` separately from `sps`. `norm_freq` stays caller-facing in cycles/sample at the **input** rate, so the real face halves it on the way in and doubles it on the way out. Ddcr's tuning law is `norm_freq = -(2*f_c + 0.5)`.
* \*\*`sps` must exceed `2 * m_out`,\*\* strictly, against `sps >= m_out` for the complex face: the cascade behind the halfband runs at twice the overall rate and Ddcr requires that below 0.5.
* \*\*`init_norm_freq` means the real IF centre\*\* rather than a baseband residual.




The hot path is not tagged. There are two `step` entry points, each force- inlined onto [**mpsk\_rx\_fold**](mpsk__rx__loops_8h.md#function-mpsk_rx_fold), so the front end is a compile-time fact inside the sample loop and `real` is read only on cold paths (destroy, reset, telemetry, the frequency accessors and the state triplet).



* [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) (the matched flavor) mixes, decimates and matched-filters in the dot products it was already doing. Its terminal polyphase stage IS the matched filter, and the arm that stage selects IS the fractional symbol-timing delay.
* [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) closes a symbol-timing loop on the cascade's `rate_ctrl` port and a carrier loop on the LO's `freq_ctrl` port. The timing half is [**ratesync\_loop\_t**](structratesync__loop__t.md) — literally RateSync's loop, not a copy of it.




Carrier recovery follows the project rule, now structurally rather than by convention: **predetection de-rotation** happens in the LO at the front of the chain, and **postdetection discrimination** on the matched-filtered symbols at the end of it. Two discriminators steer the one LO:
* **acquisition** — the NDA M-th-power error on every terminal output, needing no data and no symbol timing (`tracking == 0`).
* **tracking** — a decision-directed error `e = Im(y·conj(â))/|y|` on the recovered symbols (lower jitter, at the symbol rate). The handover is opt-in (`acq_to_track`) and two-way, and the shared loop filter carries the frequency estimate across it in both directions, so a drop-back is a discriminator swap rather than a cold re-acquisition. See [**mpsk\_rx\_loops.h**](mpsk__rx__loops_8h.md) for why the two discriminators run at different rates and how the estimate survives the change.





### What the cascade buys



`sps` is a **double**, and the front end plans itself. At `sps = 8` the plan is a halfband or two and a terminal stage; at `sps = 256` it is a CIC followed by the same terminal stage, so the matched filter costs the same bank either way (~34 taps/arm at both ends of a 64x span of input rates, against the 4225 taps/arm a single-stage design would need). An irrational `sps` — a free-running ADC clock against the symbol clock — is no harder than an integer one, because the terminal accumulator is a double and the loop only has to steer the strobe.


The M-fold phase ambiguity is unchanged: resolve it with differential demapping (`bits(..., differential=1)`) or a sync word downstream. A DSSS-MPSK receiver is still `Dll(segments) -> MpskReceiver`.




**Warning:**

**This object's outputs are not bit-identical to releases before the cascade rebuild.** The matched filter became a polyphase bank instead of a dense FIR and the interpolator became a bank arm instead of a Farrow, so symbols move at the float level. `bn_carrier` also changed units: it is now normalised to the **symbol rate**, like `bn_timing`, rather than to the input sample rate — at the old default `sps = 8` the same number is now an 8x wider loop. Detection performance is unchanged (the fused matched filter measures on the Es/N0 bound); exact-output pins are not.


Lifecycle: `mpsk_receiver_create -> (steps / bits / reset)* -> _destroy`.



```C++
// QPSK, 8 samples/symbol, I&D matched filter, NDA acquisition
mpsk_receiver_state_t *rx = mpsk_receiver_create (
    4, 8.0, 4, MPSK_RX_PULSE_IANDD, 0.35, 8,
    0.01, 0.707, 0.01, 0, 0.5, 0.0, 100, 0, 1024,
    MPSK_RX_NDA_TAP_STROBE, 1, MPSK_RX_AGC_BW_RATIO);
float complex sym[256];
size_t k = mpsk_receiver_steps (rx, rx_in, rx_len, sym, 256);
double f = mpsk_receiver_get_norm_freq (rx);  // tracked residual carrier
mpsk_receiver_destroy (rx);
```
 



    
## Public Functions Documentation




### function mpsk\_receiver\_bits 

_Demodulate a cf32 block and emit hard Gray-coded bits._ 
```C++
size_t mpsk_receiver_bits (
    mpsk_receiver_state_t * state,
    const float complex * x,
    size_t x_len,
    uint8_t * out,
    size_t max_out
) 
```



Like [**mpsk\_receiver\_steps()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_steps), but each recovered symbol is sliced to its nearest M-PSK point and unpacked to log2(M) hard bits (LSB-first). With the differential option set at create time, the Gray label is taken from the phase _difference_ between consecutive symbols (rotation-invariant — it resolves the M-fold carrier ambiguity), else from the absolute (coherent) decision.




**Parameters:**


* `state` Receiver state. Must be non-NULL. 
* `x` Input cf32 samples. 
* `x_len` Number of input samples. 
* `out` Output bytes (0/1); caller provides `max_out` capacity. 
* `max_out` Output capacity. 



**Returns:**

Number of bits written. 
```C++
>>> import numpy as np
>>> from doppler.track import MpskReceiver
>>> rng = np.random.default_rng(3)
>>> idx = rng.integers(0, 2, 3000)                  # BPSK payload bits
>>> tx = np.repeat(np.exp(1j * np.pi * idx), 8).astype(np.complex64)
>>> rx = MpskReceiver(m=2, sps=8, m_out=4, bn_carrier=0.005)
>>> b = rx.bits(tx)                                 # 1 hard bit/symbol
>>> b.size
2998
>>> # settled tail matches the payload, up to the BPSK
>>> # inversion ambiguity and the pipeline's one-symbol lead
>>> tail = np.mean(b[1001:2001] != idx[1000:2000])
>>> round(float(min(tail, 1 - tail)), 3)
0.0
```
 





        

<hr>



### function mpsk\_receiver\_bits\_max\_out 

```C++
size_t mpsk_receiver_bits_max_out (
    mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_bits\_real 

_Demodulate a real f32 block and emit hard Gray-coded bits._ 
```C++
size_t mpsk_receiver_bits_real (
    mpsk_receiver_state_t * state,
    const float * x,
    size_t x_len,
    uint8_t * out,
    size_t max_out
) 
```



[**mpsk\_receiver\_bits()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_bits) taking real samples. Requires a state built by [**mpsk\_receiver\_create\_real()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create_real).




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



### function mpsk\_receiver\_bits\_real\_max\_out 

```C++
size_t mpsk_receiver_bits_real_max_out (
    mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_configure\_lock 

_Re-tune the acquisition&lt;-&gt;tracking handover detector directly._ 
```C++
void mpsk_receiver_configure_lock (
    mpsk_receiver_state_t * state,
    double up_thresh,
    double down_thresh,
    uint32_t n_up,
    uint32_t n_down
) 
```



Full lockdet control over the handover, mirroring [**costas\_configure\_lock()**](costas__core_8h.md#function-costas_configure_lock): a split declare/drop threshold pair on the carrier lock EMA (level hysteresis) and both verify counts (time hysteresis). A live handover survives the re-tune; the in-flight verify run restarts.




**Parameters:**


* `state` Must be non-NULL. 
* `up_thresh` Declare threshold on the carrier lock EMA. 
* `down_thresh` Drop threshold; choose &lt;= up\_thresh for level hysteresis. 
* `n_up` Consecutive above-threshold symbols to hand over to the decision-directed discriminator; clamped &gt;= 1. 
* `n_down` Consecutive below-threshold symbols to fall back to NDA acquisition; clamped &gt;= 1. 
```C++
>>> from doppler.track import MpskReceiver
>>> rx = MpskReceiver(m=4, sps=4, m_out=2, acq_to_track=1)
>>> rx.tracking
0
>>> rx.configure_lock(0.9, 0.72, 4, 16)   # tighter declare, fast drop
```
 




        

<hr>



### function mpsk\_receiver\_create 

_Create an M-PSK receiver._ 
```C++
mpsk_receiver_state_t * mpsk_receiver_create (
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





**Parameters:**


* `m` Constellation order M, 2/4/8 (default 4 = QPSK). 
* `sps` Samples per symbol; any double &gt;= `m_out` (8.0 by default, but 17.33389 is equally valid). 
* `m_out` Terminal outputs per symbol: even, 2..8. **0 (the default) derives it** — the largest even count in 2..8 the rate allows, via [**mpsk\_rx\_derive\_m\_out**](mpsk__rx__loops_8h.md#function-mpsk_rx_derive_m_out), which is `8` at the default `sps = 8`; pass a value only to pin one. Read it back with [**mpsk\_receiver\_get\_m\_out()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_get_m_out). Gardner needs the half-symbol gate. The derived answer reaches 8 for two reasons. The matched filter: the rectangle is one symbol wide, so its filter is an m\_out-tap sum spanning it, and a smaller m\_out samples the same integral more coarsely. Measured on QPSK at sps = 8 against EVM\_dB = -(Es/N0)\_dB, at 18 dB Es/N0: 0.41 dB off the bound at 8, 3.11 dB at 4. And the M-th-power discriminator: `z^M` auto-convolves the spectrum M times, spreading energy over ~`M*Rs`, and whatever exceeds the update rate folds back onto itself. A clean strobe raises to a constant with nothing to fold, but every departure from clean (ISI, timing error, noise) is splattered M-fold and aliased — so the nonlinearity's tolerance for a coarse matched filter COLLAPSES as M grows. The first reason is M-independent; the second is not. Measured (halving m\_out from 8 to 4, each M at its own SER=1e-3 anchor): BPSK 1.7 dB, QPSK 1.6 dB, **8PSK 3.0 dB** — the last also sitting 0.87 dB from the fully-scattered EVM floor, i.e. barely distinguishable from noise. **So m\_out = 8 is not optional at M = 8.** At `MPSK_RX_NDA_TAP_MF_OUT`, where the M-th power runs on the oversampled pulse rather than the strobe, the requirement is the blunt `m_out >= M`; since m\_out maxes at 8, 8PSK there is exactly critically sampled. **Never pair 2 with MPSK\_RX\_PULSE\_IANDD** — the filter degenerates to a two-tap sum, the eye barely opens and acquisition itself fails about half the time. Replaces the old `n` (NDA arm dumps/symbol), which the cascade's own outputs now serve. 
* `pulse` Matched-filter shape (default MPSK\_RX\_PULSE\_IANDD). 
* `rrc_beta` RRC roll-off in `[0, 1]` (default 0.35; RRC only). 
* `rrc_span` RRC one-sided span in symbols (default 8; RRC only). 
* `bn_carrier` Carrier loop noise bandwidth, **normalised to the symbol rate** (default 0.01). A carrier loop here closes around the matched filter, so its dead time is that filter's group delay — keep it a small fraction of the symbol rate, as a real receiver does. 
* `zeta` Damping factor for both loops. **0 (the default) derives it** as `1/sqrt(2)` ([**MPSK\_RX\_ZETA\_DEFAULT**](mpsk__rx__loops_8h.md#define-mpsk_rx_zeta_default)) — a constant rather than a computation, since nothing in this receiver moves the optimal damping and both loops already share one value. Read it back with [**mpsk\_receiver\_get\_zeta()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_get_zeta). 
* `bn_timing` Symbol-timing loop noise bandwidth, normalised to the symbol rate (default 0.01). 
* `acq_to_track` Enable the two-way NDA&lt;-&gt;decision-directed handover (default 0). 
* `lock_thresh` Handover declare threshold on the carrier lock metric. **0 (the default) derives it** as `sigma_H0 * eta(Pfa)` = `0.1132 * 4.4159` = `0.4999` ([**MPSK\_RX\_LOCK\_THRESH\_DEFAULT**](mpsk__rx__loops_8h.md#define-mpsk_rx_lock_thresh_default)), which is the 0.5 that used to be hand-picked — so the derivation changed no behaviour, and is here because a number that was picked and a number that was derived look identical until one has to move. Read it back with [**mpsk\_receiver\_get\_lock\_thresh()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_get_lock_thresh). The drop threshold sits at 0.8x for level hysteresis, and both directions are verify-counted (8 symbols up / 32 down). The metric is `Re((z/|z|)^M)` smoothed by an EMA, whose noise-only sd is 0.1132 for **every** M, so the threshold is 4.42 noise sigmas — a per-look false-alarm probability of 5e-6. To pin your own, divide your Pfa's z-score into 0.1132 rather than picking by feel; see [**carrier\_nda\_core.h**](carrier__nda__core_8h.md) for the derivation and the measured verification. 
* `init_norm_freq` Seed carrier frequency, cycles/sample at the input rate (default 0.0). This is the centre the LO is tuned to; the loop tracks the residual around it. 
* `differential` bits(): differential (rotation-invariant) demap (default 0 = coherent). 
* `num_phases` Terminal-stage bank arms; a power of two. **0 (the default) derives it** as 64 ([**MPSK\_RX\_NUM\_PHASES\_DEFAULT**](mpsk__rx__loops_8h.md#define-mpsk_rx_num_phases_default)), the measured saturation point — against the 1024 that used to be the default, a 16x bank for no measurable gain. Read it back with [**mpsk\_receiver\_get\_num\_phases()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_get_num_phases). Sets the timing resolution to `1/num_phases` of an output period. 
* `nda_tap` MPSK\_RX\_NDA\_TAP\_\* — where the NDA carrier discriminator reads, which sets its pull-in range and whether it needs symbol timing at all. An M-th-power detector updating at rate `F` can only observe `|df| < F/(2M)`, so the tap point IS the range:
  * `MPSK_RX_NDA_TAP_STROBE` (0) — the on-time strobe, at `Rs`. Cleanest input, narrowest range, and the ONLY tap whose input quality depends on the timing loop — it steers from the first strobe regardless, so pick another tap if the carrier must acquire first.
  * `MPSK_RX_NDA_TAP_MF_OUT` (1) — every terminal output, at `m_out*Rs`. No timing dependence, paid for with the ISI the between-symbol outputs carry (worst at 8PSK, where the decision margin is smallest). Measured unaided, QPSK at `sps = 8, m_out = 8`, each at its own best `bn_carrier`: `0.050*Rs` (strobe), `0.033*Rs` (mf\_out). `MF_IN`'s range is not measured yet (gh-766) — its update rate is a planner outcome, so it cannot be derived from the other two. Fixed at construction — nothing switches underneath the caller. Note `df = k*F/M` is a stable FALSE lock at every tap, reporting a healthy lock statistic that no self-referenced metric can flag. For more range than any tap gives, put a coarse frequency estimate in front and pass it as `init_norm_freq`. 


* `agc` Non-zero (default) puts the receiver's ONE AGC in the front-end cascade, immediately before the terminal matched stage. **It serves BOTH loops** — carrier and timing both run on its output, so it is a dynamic element inside both, which is why [**mpsk\_rx\_agc\_bn**](mpsk__rx__loops_8h.md#function-mpsk_rx_agc_bn) sizes it against the SLOWER of the two rather than against timing alone. What differs is only why the level matters to each: the timing detector normalises by a slope computed at construction for a unit-amplitude stream ([**symsync\_ted\_slope**](symsync__core_8h.md#function-symsync_ted_slope)), so a level error is a loop-gain error there directly; the carrier detector normalises by its own `|z|^M` ([**carrier\_nda\_disc**](carrier__nda__core_8h.md#function-carrier_nda_disc)), so it is immune to the level itself but still sees the AGC's transient. Pass 0 and the receiver is un-levelled: the timing loop is under-driven by `A^2`, which at an input amplitude of 0.25 is 16x. The reference is derived from the bank's own pulse energy, not chosen. 
* `bn_agc_ratio` That AGC's bandwidth as a fraction of the SLOWEST loop it feeds, `min(bn_carrier, bn_timing)` — see [**mpsk\_rx\_agc\_bn**](mpsk__rx__loops_8h.md#function-mpsk_rx_agc_bn). Must be in (0, 1); construction refuses 1 or above rather than warning, because at 1 the AGC is exactly as fast as a loop it feeds and past that it is faster, and two level-correcting loops at the same speed integrate against each other. **0 (the default) derives it** as `MPSK_RX_AGC_BW_RATIO` = 0.05, 20x slower than the slowest loop it feeds ([**MPSK\_RX\_AGC\_RATIO\_DEFAULT**](mpsk__rx__loops_8h.md#define-mpsk_rx_agc_ratio_default)); 0 is the one value below 1 that is a request rather than a rejection. Read it back with [**mpsk\_receiver\_get\_bn\_agc\_ratio()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_get_bn_agc_ratio). 



**Returns:**

Heap-allocated state, or NULL on invalid args / allocation failure.




**Note:**

**Zero means derive**, for `m_out`, `zeta`, `lock_thresh`, `num_phases` and `bn_agc_ratio` (gh-644). Every one of those validators previously REJECTED zero, so no working call site can be relying on it, which is what makes the derivation additive rather than a break. The derivation runs BEFORE the validation, so a derived answer faces the same guards a supplied one does. Each is reported back by a getter — without that, `0` would be an instruction whose result nobody can see. See docs/design/mpsk.md §8.1. 




**Note:**

Caller must call [**mpsk\_receiver\_destroy()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_destroy) when done. 





        

<hr>



### function mpsk\_receiver\_create\_bpsk 

_A BPSK receiver stated in the units a caller actually holds: Hz._ 
```C++
mpsk_receiver_state_t * mpsk_receiver_create_bpsk (
    double sample_rate_hz,
    double symbol_rate_hz,
    double carrier_freq_hz,
    int pulse,
    double rrc_beta,
    int rrc_span,
    double bn_carrier,
    double bn_timing,
    int acq_to_track,
    int differential,
    int agc
) 
```



Same core, same loops, same methods — this differs from [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create) only in what it ASKS FOR, and that is the point. A caller with a capture holds a sample rate, a symbol rate and a carrier frequency, all in Hz. They do not hold `sps`: that is `fs / Rs`, a ratio this library computes for its own use in selecting a cascade and in costing it. Requiring it makes the caller derive an internal quantity, and then it spreads — because `sps` is in the constructor, `init_norm_freq` has to be cycles per SAMPLE, so stating a carrier offset needs `sps` and `fs` both, while the loop bandwidth on the next line is normalised to the SYMBOL rate. One constructor, two normalisations, and the conversion between them is the caller's problem.


So the conversion happens here, once: `sps = sample_rate_hz / symbol_rate_hz` and the LO centre is `carrier_freq_hz / sample_rate_hz`. Nothing on this signature is normalised to anything.


\*\*`m` is absent because the type says it.\*\* That is the cheapest parameter to remove and the easiest to miss: a fact carried by the class name is not a parameter on that class.


Every argument this does not take is a derive-request in the delegate below — `m_out`, `zeta`, `lock_thresh`, `num_phases`, `bn_agc_ratio` all ask create() for the derived answer, and the NDA tap is the one measured to work at every battery point. They are absent because nobody has a use for them here, not because they are unavailable: `MpskReceiver` still takes every one.




**Parameters:**


* `sample_rate_hz` ADC sample rate, Hz. Must be &gt; 0. 
* `symbol_rate_hz` Symbol rate, Hz. Must be &gt; 0, and must leave `sample_rate_hz / symbol_rate_hz` at or above the derived `m_out` — a rate that cannot be strobed is refused rather than approximated. 
* `carrier_freq_hz` Carrier centre, Hz (default 0 — complex baseband). `|carrier_freq_hz|` must be under `sample_rate_hz / 2`; a centre outside Nyquist is a mis-stated capture, not a tuning request. 
* `pulse` Matched-filter shape (default MPSK\_RX\_PULSE\_IANDD). 
* `rrc_beta` RRC roll-off in `[0, 1]` (default 0.35; RRC only). 
* `rrc_span` RRC one-sided span in symbols (default 8; RRC only). 
* `bn_carrier` Carrier loop noise bandwidth, normalised to the symbol rate (default 0.01). 
* `bn_timing` Symbol-timing loop noise bandwidth, normalised to the symbol rate (default 0.01). 
* `acq_to_track` Hand the carrier over to a decision-directed discriminator once locked (default 0). 
* `differential` bits(): differential (rotation-invariant) demap (default 0, coherent). 
* `agc` Front-end AGC (default 1). 



**Returns:**

Heap-allocated state, or NULL on invalid args / allocation failure. Destroy with [**mpsk\_receiver\_destroy()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_destroy) like any other.



```C++
>>> from doppler.track import BpskReceiver
>>> rx = BpskReceiver(sample_rate_hz=8e6, symbol_rate_hz=1e6)
>>> rx.m                 # the type says it
2
>>> rx.sps               # derived from the two rates, not asked for
8.0
```
 


        

<hr>



### function mpsk\_receiver\_create\_continuous 

_The continuous flavor: one discriminator, and nothing waits._ 
```C++
mpsk_receiver_state_t * mpsk_receiver_create_continuous (
    int m,
    double sps,
    int pulse,
    double rrc_beta,
    int rrc_span,
    double bn_carrier,
    double bn_timing,
    double init_norm_freq,
    int differential
) 
```



Same object, same state, same methods — a second constructor that PINS the parameters a continuous receiver has no reason to vary, so the caller states the link and not the loops (docs/design/mpsk.md §2.1, §8). Nothing is removed: `mpsk_receiver_create()` still reaches every knob, and this is a [**ddc\_create\_matched**](ddc__core_8h.md#function-ddc_create_matched) -style flavor over the identical core rather than a second type.


**There is no handover, no warmup, no lock gate and no timing gate.** The NDA M-th-power error steers the LO from the first output to the last. That is a reliability argument rather than a simplicity one: there is no state in which the receiver can be wrong about which mode it is in, because there is one. No declaring on garbage, no drop-back that never fires, no metric that has to be trusted before the loop may act.


What it pins, and why each is not a choice here:



|pinned   |to   |because    |
|-----|-----|-----|
|`acq_to_track`   |0   |the handover IS the gate this flavor exists to remove    |
|`nda_tap`   |MPSK\_RX\_NDA\_TAP\_STROBE   |the only tap that acquires AND reports it at every point of the standard battery    |
|`agc`   |1   |load-bearing, not optional — it defines the level both loops run on    |
|`m_out`, `zeta`,   |0 (derive)   |not design axes; see the create() 

**Note:**




|
|`lock_thresh`,   ||with no handover it gates nothing — it is telemetry, so    |
|`num_phases`,   ||it is read back ([**mpsk\_receiver\_get\_lock\_thresh()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_get_lock_thresh)) rather    |
|`bn_agc_ratio`   ||than set   |






\*\*`mf_in` was pinned here first, and it was withdrawn on measurement.\*\* The argument was structural  a tap ahead of the matched filter needs no symbol timing, so nothing waits  and that structure is sound. What was wrong is that it was never measured on THIS flavor's waveform. The standard battery runs RRC with dense transitions throughout, which is the BURST flavor's signal; S0 describes this one as NRZ BPSK with periods of data modulation off but carrier on.


Measured on that scenario  `native/validation/rx_dynamics.c`, a coupled Doppler ramp across a data onset, NRZ/I&D/`m_out = 4`/DTTL at 12 dB Es/N0  `strobe` wins on every axis:



|tap   |lock, modulation OFF   |min at the data onset   |end    |
|-----|-----|-----|-----|
|`strobe`   |+0.935   |**+0.860**   |+0.920    |
|`mf_out`   |+0.934   |+0.478   |+0.802    |
|`mf_in`   |+0.761   |+0.417   |+0.714   |






\*\*`strobe`'s timing dependency costs nothing in the half where timing is impossible\*\*, and the reason is worth stating because it reads backwards: an unmodulated NRZ carrier is SAMPLING-PHASE INVARIANT. Every sample is the same constellation point, so the M-th-power discriminator does not care which one the timing loop would have nominated. Timing closure gates DEMODULATION, not carrier acquisition  so the tap that depends on it is free exactly where it looked most exposed. `mf_out` instead takes the largest hit the moment transitions exist, which is its ISI bias arriving on schedule, and `mf_in` reads lowest throughout (the excess noise bandwidth at its node  see the `nda_tap` enum, where that cost is measured and stated as the tap's price).


"Nothing waits" is untouched by the pin: it is a statement about GATES  the handover, the lock gate, the warmup  and `acq_to_track = 0` is what delivers it.


The M-fold ambiguity is **permanent** here — no decision-directed stage ever pins the absolute phase — so `differential` defaults to 1. Coherent demapping without a downstream sync word is a misconfiguration, not a choice.




**Parameters:**


* `m` Constellation order M, 2/4/8 (default 2 = BPSK). 
* `sps` Samples per symbol; any double (default 8.0). 
* `pulse` Matched-filter shape (default MPSK\_RX\_PULSE\_IANDD). 
* `rrc_beta` RRC roll-off in `[0, 1]` (default 0.35; RRC only). 
* `rrc_span` RRC one-sided span in symbols (default 8; RRC only). 
* `bn_carrier` Carrier loop noise bandwidth, normalised to the symbol rate (default 0.01). A design axis. 
* `bn_timing` Symbol-timing loop noise bandwidth, normalised to the symbol rate (default 0.01). A design axis. 
* `init_norm_freq` Seed carrier frequency, cycles/sample at the input rate (default 0.0). The centre the LO is tuned to; the loop tracks the residual around it. 
* `differential` bits(): differential (rotation-invariant) demap (default 1 — see above). 



**Returns:**

Heap-allocated state, or NULL on invalid args / allocation failure. Destroy with [**mpsk\_receiver\_destroy()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_destroy) like any other.



```C++
>>> from doppler.track import ContinuousMpskReceiver
>>> rx = ContinuousMpskReceiver(m=2, sps=8.0)
>>> rx.tracking          # one discriminator, forever
0
>>> rx.m_out             # derived, not defaulted
8
```
 


        

<hr>



### function mpsk\_receiver\_create\_real 

_Create the same receiver behind an R2C halfband: a real IF in._ 
```C++
mpsk_receiver_state_t * mpsk_receiver_create_real (
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



The real-input face. **Every parameter means what it means on [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create)** — same names, same order, same types, same derivations, the same "zero means derive" rule — because this is the same object and not a twin of it. Only the three rate conventions in this file's header block differ, and each is named against the parameter it touches below.


A real-valued IF is the usual output of a single-ended ADC, so this is the face that takes a digitiser's samples directly. Everything downstream — symbols, bits, telemetry, serialization — is one implementation shared with the complex face.




**Parameters:**


* `m` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create). 
* `sps` Samples per symbol at the REAL input; any double **strictly greater than `2 * m_out`**. The cascade behind the halfband runs at twice the overall rate, and Ddcr requires that rate below 0.5 — so where the complex face accepts `sps >= m_out`, this one needs twice the headroom. Derived `m_out` honours the same bound ([**mpsk\_rx\_derive\_m\_out**](mpsk__rx__loops_8h.md#function-mpsk_rx_derive_m_out) takes the constraint, not the rate), so a caller cannot pair an `sps` and an `m_out` that will not construct. 
* `m_out` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create); **0 derives** it against the strict `sps/2` cap above rather than `sps`. 
* `pulse` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create). 
* `rrc_beta` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create). 
* `rrc_span` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create). 
* `bn_carrier` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create). Still normalised to the SYMBOL rate: the halfband moves the LO's clock, not the loop's units. 
* `zeta` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create); 0 derives. 
* `bn_timing` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create). 
* `acq_to_track` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create). 
* `lock_thresh` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create); 0 derives. 
* `init_norm_freq` The real IF **centre**, cycles/sample at the real input rate. An IF at `0.2 * fs` is `0.2`; the halved value the LO actually uses is this object's business, not the caller's. A real IF must be tuned near — this face does not acquire from a cold zero the way the complex one does, so the centre is where the tap's pull-in range sits _around_. 
* `differential` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create). 
* `num_phases` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create); 0 derives. 
* `nda_tap` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create), all three taps included: `MF_IN`'s update rate is read from this front end's own cascade (`ddcr_get_bank_sps`), and `bank_sps` measures identical on both faces because it is symbol-relative — the halfband's 2:1 is absorbed by the plan. 
* `agc` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create). The AGC sits inside the cascade BEHIND the halfband, so it levels the analytic signal at the intermediate rate, which is also where the noise has already been filtered. 
* `bn_agc_ratio` As [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create); 0 derives. 



**Returns:**

Heap-allocated state, or NULL on invalid args / allocation failure. Destroy with [**mpsk\_receiver\_destroy()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_destroy) like any other.



```C++
// QPSK on a real IF at 0.2*fs, 32 samples/symbol, I&D matched filter
mpsk_receiver_state_t *rx = mpsk_receiver_create_real (
    4, 32.0, 0, MPSK_RX_PULSE_IANDD, 0.35, 8,
    0.01, 0.0, 0.01, 0, 0.0, 0.2, 0, 0,
    MPSK_RX_NDA_TAP_STROBE, 1, 0.0);
float complex sym[256];
size_t k = mpsk_receiver_steps_real (rx, rx_in, rx_len, sym, 256);
mpsk_receiver_destroy (rx);
```
 


        

<hr>



### function mpsk\_receiver\_destroy 

_Destroy an M-PSK receiver and release all memory._ 
```C++
void mpsk_receiver_destroy (
    mpsk_receiver_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function mpsk\_receiver\_get\_agc\_gain\_db 

_Gain the front end's AGC is applying, in dB; 0.0 when_ `agc` _= 0._
```C++
double mpsk_receiver_get_agc_gain_db (
    const mpsk_receiver_state_t * state
) 
```



The cascade's own level correction, read back rather than inferred. This is the diagnostic for a level problem: a receiver that will not lock with a healthy `lock` statistic, or one whose timing loop behaves differently at two input levels, is asking about this number. It settles at `-10*log10(P_in / P_ref)` where `P_ref` is the power a unit-amplitude symbol stream has where the AGC sits, so a reading far from 0 dB says the input is far from the level the cascade was built for  which is fine, and is exactly what the AGC is for, but is worth knowing.


Separate from the cascade's filter response ([**RateConverter\_gain()**](RateConverter__core_8h.md#function-rateconverter_gain)), which is computed from coefficients and stays 1.0; the two multiply. 


        

<hr>



### function mpsk\_receiver\_get\_bn\_agc\_ratio 

_AGC bandwidth ratio in use — derived unless pinned (§8.1)._ 
```C++
double mpsk_receiver_get_bn_agc_ratio (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_clipped 

_Has the cascade's CIC stage clipped its input since the last reset? A CIC bounds its input to +-1.0 and clips silently past that, which costs ~25 dB of EVM behind a perfectly healthy lock._ 
```C++
int mpsk_receiver_get_clipped (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_last\_error 

_Carrier loop phase discriminator (rad) — the residual phase the loop is trying to null; loop stress._ 
```C++
double mpsk_receiver_get_last_error (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_lock 

```C++
double mpsk_receiver_get_lock (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_lock\_drop\_thresh 

_Carrier DROP threshold in use —_ `MPSK_RX_HANDOVER_DOWN` _x the declare threshold, the level hysteresis the pair is stated with._
```C++
double mpsk_receiver_get_lock_drop_thresh (
    const mpsk_receiver_state_t * state
) 
```



Readable for the same reason the declare side is: a caller plotting the lock statistic needs both edges to know what the decision was reading, and deriving `0.8 *` in a plotting script puts a second copy of the hysteresis rule outside the object that owns it. 


        

<hr>



### function mpsk\_receiver\_get\_lock\_thresh 

_Handover lock threshold in use — derived unless pinned (§8.1)._ 
```C++
double mpsk_receiver_get_lock_thresh (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_lock\_time 

_Symbols from reset to the FIRST carrier-lock declaration, or -1 if the receiver has not locked yet._ 
```C++
int64_t mpsk_receiver_get_lock_time (
    const mpsk_receiver_state_t * state
) 
```



The acquisition time, as a number a caller can read rather than infer by polling `locked` in a loop. Dated by the same hysteretic detector `mpsk_receiver_get_locked()` reports, so the two cannot disagree.


In SYMBOLS, not seconds: `bn_carrier` and `bn_timing` are both normalised to the symbol rate, so a settling budget quoted in symbols is comparable across every input rate, and a caller with `Rs` divides once. Only the first declaration is dated — a drop and re-acquire does not restamp it, because the question this answers is "how long did this receiver take to
lock", not "when did it last hold". [**mpsk\_receiver\_reset()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_reset) clears it to -1. 


        

<hr>



### function mpsk\_receiver\_get\_locked 

_Binary carrier-lock flag from the loop's hysteretic (up/down verify-counted) lock detector — de-chattered, unlike the raw metric._ 
```C++
int mpsk_receiver_get_locked (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_m 

```C++
int mpsk_receiver_get_m (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_m\_out 

_Terminal outputs per symbol (the old_ `n` _, now the cascade's)._
```C++
size_t mpsk_receiver_get_m_out (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_nco\_freq 

_Instantaneous NCO frequency command (carrier loop filter output, cycles/sample): mean tracks a ramp with no lag, variance is loop stress._ 
```C++
double mpsk_receiver_get_nco_freq (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_norm\_freq 

_Carrier frequency the receiver is tracking, cycles/sample at the input rate: the create-time centre plus the loop's own estimate._ 
```C++
double mpsk_receiver_get_norm_freq (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_num\_phases 

_Matched-filter bank arms in use — derived unless pinned (§8.1)._ 
```C++
size_t mpsk_receiver_get_num_phases (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_sps 

```C++
double mpsk_receiver_get_sps (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_state 

```C++
void mpsk_receiver_get_state (
    const mpsk_receiver_state_t * state,
    void * blob
) 
```




<hr>



### function mpsk\_receiver\_get\_sync\_lock\_drop\_thresh 

_Timing DROP threshold on_ `sync.lock` _. Equal to the declare threshold when the timing loop carries no level hysteresis._
```C++
double mpsk_receiver_get_sync_lock_drop_thresh (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_sync\_lock\_thresh 

_Timing DECLARE threshold on_ `sync.lock` _, derived by symsync's own (rolloff, esno\_min, pfa, pd) geometry rather than pinned._
```C++
double mpsk_receiver_get_sync_lock_thresh (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_timing\_rate 

_Smoothed tracked samples per symbol — departs from the nominal_ `sps` _by exactly the sample-clock offset the timing loop is tracking._
```C++
double mpsk_receiver_get_timing_rate (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_tracking 

```C++
int mpsk_receiver_get_tracking (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_zeta 

_Loop damping in use — derived_ `1/sqrt(2)` _unless pinned (§8.1)._
```C++
double mpsk_receiver_get_zeta (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_reset 

_Re-seed the front end and both loops to their create-time state._ 
```C++
void mpsk_receiver_reset (
    mpsk_receiver_state_t * state
) 
```



Clears the cascade's filter memory, the carrier and timing NCOs, the loop-filter integrators and the lock detectors, and returns the carrier estimate to `init_norm_freq`. The configuration (order, rate, pulse, bandwidths) is untouched, so the same input fed twice around a reset reproduces the same output bit-for-bit.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.track import MpskReceiver
>>> rng = np.random.default_rng(0)
>>> idx = rng.integers(0, 4, 300)
>>> tx = np.repeat(np.exp(1j * (2 * np.pi * idx / 4 + np.pi / 4)), 8)
>>> tx = tx.astype(np.complex64)
>>> rx = MpskReceiver(m=4, sps=8, m_out=4)
>>> first = rx.steps(tx)
>>> rx.reset()                                # back to the cold state
>>> np.array_equal(first, rx.steps(tx))       # same input, same output
True
```
 




        

<hr>



### function mpsk\_receiver\_set\_norm\_freq 

_Retune to_ `val` _cycles/sample: moves the LO centre there and zeroes the loop's residual estimate, so norm\_freq reads back exactly._
```C++
void mpsk_receiver_set_norm_freq (
    mpsk_receiver_state_t * state,
    double val
) 
```




<hr>



### function mpsk\_receiver\_set\_state 

```C++
int mpsk_receiver_set_state (
    mpsk_receiver_state_t * state,
    const void * blob
) 
```




<hr>



### function mpsk\_receiver\_set\_telemetry 

_Attach (or detach) a telemetry context across the receiver. Registers the receiver's own "&lt;prefix&gt;.lock" probe (the carrier lock EMA) and "&lt;prefix&gt;.tracking" (the two-way handover decision, 0/1 — so a consumer sees exactly when the carrier was handed to the decision-directed discriminator or dropped back to NDA), then the carrier loop's "&lt;prefix&gt;.car.e" / ".freq" / ".locked" and the symbol-timing loop's "&lt;prefix&gt;.sync.e" / ".ctrl" / ".rate" / ".lock" / ".locked" / ".mu"_  _eleven probes emitted once per recovered symbol_ _then the front end's AGC under "&lt;prefix&gt;.agc" ("&lt;prefix&gt;.agc.gain\_db" and "&lt;prefix&gt;.agc.level\_db"; see_[_**agc\_set\_telemetry()**_](agc__core_8h.md#function-agc_set_telemetry) _). Thirteen probes total, all thinned by_`decim` _. Passing NULL detaches everything._
```C++
int mpsk_receiver_set_telemetry (
    mpsk_receiver_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```





**Warning:**

The two AGC probes are NOT at the symbol rate the other eleven are. That AGC sits pre-terminal in the cascade (RateConverter's tap, ahead of the stage the timing loop steers) and emits once per gain-update event, i.e. every `AGC_DECIM_DEFAULT` samples of that fixed-rate stream  so it reports on a grid that depends on the planned cascade, not on recovered symbols, and a run yields a different number of AGC records than carrier records. Compare the two by TIME, never by record index. This is deliberate: the AGC's bandwidth is quoted in the pre-terminal stream's units precisely so it is not coupled to the loop that is stretching the symbol grid (see [**RateConverter\_enable\_agc()**](RateConverter__core_8h.md#function-rateconverter_enable_agc)).


Instrumenting it matters because it is FIRST in the chain, and a level error is the one kind no downstream loop can correct for itself: a TED normalises by its own construct-time slope, so it reads a level error as a loop-gain error (A^2 Gardner, A DTTL) with no other reference to catch it. This receiver also makes the AGC the slowest of its three loops by construction  [**mpsk\_rx\_agc\_bn()**](mpsk__rx__loops_8h.md#function-mpsk_rx_agc_bn) derives its bandwidth as a fraction of the slowest loop it feeds, and bn\_agc\_ratio is validated to (0, 1)  but that is a choice of THIS composition, and slowest does not by itself mean longest: settling is set by the bandwidth AND by how far the level starts from the reference, which is unknown at construction. Which is exactly why it has to be measured rather than inferred; the zero-referenced "&lt;prefix&gt;.agc.level\_db" is what makes that possible.


With `agc` = 0 at construction there is no AGC to attach and the two probes are simply absent (fourteen, not sixteen); this still returns DP\_OK.


Setup path, never hot; the context is borrowed and must outlive the attachment (SPSC rules in [**dp\_tlm/dp\_tlm\_core.h**](dp__tlm__core_8h.md)). 

**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "rx". 
* `decim` Emit every decim-th symbol (every decim-th gain update for the two AGC probes); &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take the probes (the attach fails whole; everything detached). 
```C++
>>> import numpy as np
>>> from doppler.track import MpskReceiver
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 14)   # 16 probes x ~512 syms + headroom
>>> rx = MpskReceiver(m=4, sps=4, m_out=2)
>>> rx.set_telemetry(tlm, "rx")
>>> len(tlm.probe_names)
16
>>> rng = np.random.default_rng(7)
>>> syms = (1 - 2 * rng.integers(0, 2, 512)).astype(np.complex64)
>>> x = np.repeat(syms, 4)
>>> _ = rx.steps(x)
>>> recs = tlm.read()
>>> tlm.dropped        # size the ring, or the counts below diverge
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



### function mpsk\_receiver\_state\_bytes 

```C++
size_t mpsk_receiver_state_bytes (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_step\_real\_ted 

_Push one REAL input sample; emit a symbol if it completed one._ 
```C++
JM_FORCEINLINE  JM_HOT int mpsk_receiver_step_real_ted (
    mpsk_receiver_state_t * s,
    float x,
    float complex * y_out,
    int ted
) 
```



The real face's composition API — [**mpsk\_receiver\_step\_ted()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_step_ted) behind an R2C halfband. Only the front end and the input type differ; everything after the front end is [**mpsk\_rx\_fold**](mpsk__rx__loops_8h.md#function-mpsk_rx_fold), shared verbatim, which is what makes "the loops behave identically regardless of front end" a claim about one body of code rather than about two.




**Parameters:**


* `s` State, built by [**mpsk\_receiver\_create\_real()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create_real). Non-NULL. 
* `x` One real input sample. 
* `y_out` Receives the symbol when the return is 1. 
* `ted` RATESYNC\_TED\_GARDNER or RATESYNC\_TED\_DTTL — pass a literal for a specialised (branch-free) instantiation. 



**Returns:**

1 if a symbol was emitted (into `y_out`), 0 otherwise. 





        

<hr>



### function mpsk\_receiver\_step\_ted 

_Push one input sample; emit a symbol if it completed one._ 
```C++
JM_FORCEINLINE  JM_HOT int mpsk_receiver_step_ted (
    mpsk_receiver_state_t * s,
    float complex x,
    float complex * y_out,
    int ted
) 
```



The composition API: mixes, decimates and matched-filters `x` through the front end at the loops' current control values, then folds every output it produced into both loops. The cascade rate is `m_out/sps <= 1`, so one input can complete at most two output periods and therefore at most one on-time strobe.




**Parameters:**


* `s` State. Must be non-NULL. 
* `x` One input sample. 
* `y_out` Receives the symbol when the return is 1. 
* `ted` RATESYNC\_TED\_GARDNER or RATESYNC\_TED\_DTTL — pass a literal for a specialised (branch-free) instantiation. 



**Returns:**

1 if a symbol was emitted (into `y_out`), 0 otherwise. 





        

<hr>



### function mpsk\_receiver\_steps 

_Demodulate a cf32 block and emit the recovered symbols._ 
```C++
size_t mpsk_receiver_steps (
    mpsk_receiver_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```



Runs the per-sample loop (mix + cascade + matched filter, then the carrier and timing loops) over `x` and writes one cf32 symbol per recovered symbol period — roughly `x_len / sps` outputs. Read norm\_freq for the tracked carrier and lock for the carrier lock metric.




**Parameters:**


* `state` Receiver state. Must be non-NULL. 
* `x` Input cf32 samples. 
* `x_len` Number of input samples. 
* `out` Output symbols; caller provides `max_out` capacity. 
* `max_out` Output capacity. 



**Returns:**

Number of symbols written. 
```C++
>>> import numpy as np
>>> from doppler.track import MpskReceiver
>>> rng = np.random.default_rng(0)
>>> idx = rng.integers(0, 4, 3000)                  # QPSK symbols
>>> tx = np.repeat(np.exp(1j * (2 * np.pi * idx / 4 + np.pi / 4)), 8)
>>> tx = tx.astype(np.complex64)                    # 8 samples/symbol
>>> rx = MpskReceiver(m=4, sps=8, m_out=4, bn_carrier=0.02)
>>> sym = rx.steps(tx)                              # blind NDA acquire
>>> sym.size                                        # ~ x_len / sps
2998
>>> rx.lock > 0.8                                   # carrier locked
True
```
 





        

<hr>



### function mpsk\_receiver\_steps\_max\_out 

```C++
size_t mpsk_receiver_steps_max_out (
    mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_steps\_real 

_Demodulate a real f32 block and emit the recovered symbols._ 
```C++
size_t mpsk_receiver_steps_real (
    mpsk_receiver_state_t * state,
    const float * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```



[**mpsk\_receiver\_steps()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_steps) taking real samples: the R2C halfband makes them complex before anything else touches them, and the per-sample body is the same one. Requires a state built by [**mpsk\_receiver\_create\_real()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create_real).




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



### function mpsk\_receiver\_steps\_real\_max\_out 

```C++
size_t mpsk_receiver_steps_real_max_out (
    mpsk_receiver_state_t * state
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



### define MPSK\_RECEIVER\_STATE\_MAGIC 

```C++
#define MPSK_RECEIVER_STATE_MAGIC `DP_FOURCC ('M', 'P', 'S', 'K')`
```




<hr>



### define MPSK\_RECEIVER\_STATE\_VERSION 

```C++
#define MPSK_RECEIVER_STATE_VERSION `6u /* v5: rebuilt on the matched DDC */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/mpsk_receiver/mpsk_receiver_core.h`

