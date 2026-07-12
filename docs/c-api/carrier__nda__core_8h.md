

# File carrier\_nda\_core.h



[**FileList**](files.md) **>** [**carrier\_nda**](dir_425637d1941eacd8ae8cdd8750b207f0.md) **>** [**carrier\_nda\_core.h**](carrier__nda__core_8h.md)

[Go to the source code of this file](carrier__nda__core_8h_source.md)

_Non-data-aided (NDA) M-th-power carrier-tracking loop._ [More...](#detailed-description)

* `#include "agc/agc_core.h"`
* `#include "boxcar/boxcar_core.h"`
* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "lo/lo_core.h"`
* `#include "lockdet/lockdet_core.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include "telemetry/telemetry.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) <br>_NDA M-th-power carrier loop state._  |
| struct | [**carrier\_nda\_tlm\_t**](structcarrier__nda__tlm__t.md) <br>_Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — the probe site is then a single predicted-not-taken branch per block loop. Zeroed in state blobs and preserved across set\_state (DP\_DEFINE\_POD\_STATE\_TLM)._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**carrier\_nda\_arm\_step**](#function-carrier_nda_arm_step) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* s, float complex d, double \* pe, double \* lock) <br>_Slide the moving-average arm by one sample; discriminate the output._  |
|  void | [**carrier\_nda\_configure\_lock**](#function-carrier_nda_configure_lock) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune the carrier lock detector's geometry directly._  |
|  [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* | [**carrier\_nda\_create**](#function-carrier_nda_create) (double bn, double zeta, double init\_norm\_freq, size\_t sps, int n, int m) <br>_Create an NDA carrier loop instance._  |
|  void | [**carrier\_nda\_destroy**](#function-carrier_nda_destroy) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br>_Destroy an NDA carrier loop instance and release all memory._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) void | [**carrier\_nda\_disc**](#function-carrier_nda_disc) (float complex z, int m, double scale, double \* pe, double \* lock) <br>_The M-th-power discriminator on an arm sample (raw, no per-dump limit)._  |
|  double | [**carrier\_nda\_get\_bn**](#function-carrier_nda_get_bn) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  double | [**carrier\_nda\_get\_last\_error**](#function-carrier_nda_get_last_error) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  double | [**carrier\_nda\_get\_lock**](#function-carrier_nda_get_lock) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  int | [**carrier\_nda\_get\_locked**](#function-carrier_nda_get_locked) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br>_Current lock decision (1 = locked, 0 = not), with the configured verify-count / hysteresis rule applied (see carrier\_nda\_configure\_lock)._  |
|  int | [**carrier\_nda\_get\_m**](#function-carrier_nda_get_m) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  int | [**carrier\_nda\_get\_n**](#function-carrier_nda_get_n) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  double | [**carrier\_nda\_get\_norm\_freq**](#function-carrier_nda_get_norm_freq) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  size\_t | [**carrier\_nda\_get\_sps**](#function-carrier_nda_get_sps) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  void | [**carrier\_nda\_get\_state**](#function-carrier_nda_get_state) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state, void \* blob) <br>_Serialize the full loop state into_ `blob` _._ |
|  void | [**carrier\_nda\_init**](#function-carrier_nda_init) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* s, double bn, double zeta, double init\_norm\_freq, size\_t sps, int n, int m) <br>_Initialise an NDA carrier loop in place (no allocation)._  |
|  void | [**carrier\_nda\_reset**](#function-carrier_nda_reset) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br>_Re-seed the loop to its create-time frequency/phase; keep config._  |
|  void | [**carrier\_nda\_set\_bn**](#function-carrier_nda_set_bn) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state, double val) <br> |
|  void | [**carrier\_nda\_set\_norm\_freq**](#function-carrier_nda_set_norm_freq) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state, double val) <br> |
|  int | [**carrier\_nda\_set\_state**](#function-carrier_nda_set_state) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state, const void \* blob) <br>_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._  |
|  int | [**carrier\_nda\_set\_telemetry**](#function-carrier_nda_set_telemetry) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state, [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context and register the carrier loop's probes on it — including the embedded arm AGC's. Registers four probes of its own, emitted once per input sample (this is a sample-rate loop — use_ `decim` _to thin the stream) plus the embedded AGC's "&lt;prefix&gt;.agc.gain\_db" (emitted at the AGC's own amortized gain-update rate): "&lt;prefix&gt;.lock" (the lock-signal EMA, ~1 when phase-locked), "&lt;prefix&gt;.e" (the M-th-power phase discriminator — the loop stress), "&lt;prefix&gt;.freq" (the tracked carrier frequency, cycles/sample) and "&lt;prefix&gt;.locked" (the verify-counted lockdet decision, 0/1). Passing NULL detaches the loop and the embedded AGC. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in_[_**telemetry/telemetry.h**_](telemetry_8h.md) _)._ |
|  size\_t | [**carrier\_nda\_state\_bytes**](#function-carrier_nda_state_bytes) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**carrier\_nda\_steer**](#function-carrier_nda_steer) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* s, double pe) <br>_Steer the shared NCO with a phase error through the loop filter._  |
|  size\_t | [**carrier\_nda\_steps**](#function-carrier_nda_steps) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br> |
|  size\_t | [**carrier\_nda\_steps\_max\_out**](#function-carrier_nda_steps_max_out) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  void | [**carrier\_nda\_tlm\_flush**](#function-carrier_nda_tlm_flush) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* s) <br>_Emit the carrier loop's telemetry records for the current sample._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float complex | [**carrier\_nda\_wipeoff**](#function-carrier_nda_wipeoff) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* s, float complex x) <br>_Per-sample carrier wipe-off: de-rotate_ `x` _by the NCO, advance it._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CARRIER\_NDA\_AGC\_ALPHA**](carrier__nda__core_8h.md#define-carrier_nda_agc_alpha)  `0.01`<br> |
| define  | [**CARRIER\_NDA\_AGC\_BW\_RATIO**](carrier__nda__core_8h.md#define-carrier_nda_agc_bw_ratio)  `0.01`<br> |
| define  | [**CARRIER\_NDA\_AGC\_CLIP\_DB**](carrier__nda__core_8h.md#define-carrier_nda_agc_clip_db)  `10.0`<br> |
| define  | [**CARRIER\_NDA\_AGC\_REF\_DB**](carrier__nda__core_8h.md#define-carrier_nda_agc_ref_db)  `0.0`<br> |
| define  | [**CARRIER\_NDA\_EPS**](carrier__nda__core_8h.md#define-carrier_nda_eps)  `1e-12`<br> |
| define  | [**CARRIER\_NDA\_INV\_2PI**](carrier__nda__core_8h.md#define-carrier_nda_inv_2pi)  `0.15915494309189535 /\* 1 / (2\*pi) \*/`<br> |
| define  | [**CARRIER\_NDA\_LOCK\_ALPHA**](carrier__nda__core_8h.md#define-carrier_nda_lock_alpha)  `0.05`<br> |
| define  | [**CARRIER\_NDA\_STATE\_MAGIC**](carrier__nda__core_8h.md#define-carrier_nda_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('C', 'N', 'D', 'A')`<br> |
| define  | [**CARRIER\_NDA\_STATE\_VERSION**](carrier__nda__core_8h.md#define-carrier_nda_state_version)  `4u /\* v4: lockdet decision rule (verify counters) \*/`<br> |

## Detailed Description


A carrier-recovery loop that locks **without data and without symbol timing** — the cold-start / acquisition counterpart to the decision-directed [**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) loop. Per sample it de-rotates the input with the integer-phase [**lo\_state\_t**](structlo__state__t.md) NCO (carrier wipe-off); it filters the de-rotated samples through a free-running I/Q **boxcar moving average** of `sps/n` samples (one output per input sample — no rate change), and on every sample runs the **M-th-power** phase discriminator, filters the error through an embedded [**loop\_filter\_state\_t**](structloop__filter__state__t.md), and steers the NCO frequency + phase.


Raising the (unit-normalized) arm sample `z` to the Mth power strips the M-PSK data modulation, leaving M times the carrier phase — so the discriminator is **independent of the data symbols and of symbol timing**. That is what lets it acquire a bare/unmodulated carrier, or a modulated carrier before timing lock. It is the M-fold-ambiguous acquisition aid; a decision-directed loop gives the low-jitter steady state (resolve the M-fold ambiguity downstream).


The M-th power is computed by **repeated complex squaring** (`z²`→`z⁴`→`z⁸`). Each level yields a phase error and a lock signal:
* `phase_error` = `Im(z^M)` scaled by `1, ½, ¼` for M = 2, 4, 8 — the scale normalizes the phase-detector gain so the S-curve slope at lock is 2 for every M (one `bn` behaves identically across M).
* `lock_signal` = `Re(z^M)` (× a per-M `lock_scale`) for M ≤ 4, and a faithful monotone lock detector for M = 8 — ~1 when phase-locked, ~0 with no carrier. Its EMA (`lock`) is the carrier lock metric. See `docs/design/mpsk.md` §2.3 for the derivation.




The block API (carrier\_nda\_steps) is the Python face and emits the de-rotated sample stream; the JM\_FORCEINLINE [**carrier\_nda\_wipeoff()**](carrier__nda__core_8h.md#function-carrier_nda_wipeoff)/\_arm\_step()/\_steer() are the C composition API a receiver inlines into its own sample loop (it can also steer the shared NCO with its own decision-directed error on handover).




**Note:**

**Input average power must be at or below unity.** The arm sample feeding the M-th-power detector is normalized to unit average power by an internal AGC (bandwidth = 0.01\*bn) with a 10 dB square clip, so the loop gain is amplitude-invariant. This is the normal convention for captured/scaled baseband (and holds for the DSSS despreader's correlation gain). A cold input more than ~10 dB above unity is out of spec: the deliberately slow AGC cannot normalize it before the discriminator reacts, and the loop can false-lock. The AGC absorbs residual/slow level variation, not a large cold offset.



```C++
// QPSK NDA carrier loop, 8 samples/symbol, 2-sample moving-average arm
carrier_nda_state_t *c = carrier_nda_create(0.01, 0.707, 0.0, 8, 4, 4);
float complex derot[1024];
size_t k = carrier_nda_steps(c, rx, rx_len, derot, 1024);
double f = carrier_nda_get_norm_freq(c); // tracked carrier (cyc/sample)
carrier_nda_destroy(c);
```
 


    
## Public Functions Documentation




### function carrier\_nda\_arm\_step 

_Slide the moving-average arm by one sample; discriminate the output._ 
```C++
JM_FORCEINLINE  JM_HOT int carrier_nda_arm_step (
    carrier_nda_state_t * s,
    float complex d,
    double * pe,
    double * lock
) 
```



The arm is a free-running boxcar **moving average** of the last `arm_len` de-rotated samples — one output per input sample, **no rate change** (not a decimating integrate-and-dump). It updates the running window sum in O(1) (add `d`, subtract the sample leaving the window), runs the M-th-power discriminator on the AGC-normalized window average, writes `pe` and `lock`, and returns 1 every call.




**Parameters:**


* `s` Carrier loop state. Must be non-NULL. 
* `d` One de-rotated sample (from carrier\_nda\_wipeoff). 
* `pe` Receives the phase error. 
* `lock` Receives the lock signal. 



**Returns:**

Always 1 (one discriminator output per input sample). 





        

<hr>



### function carrier\_nda\_configure\_lock 

_Re-tune the carrier lock detector's geometry directly._ 
```C++
void carrier_nda_configure_lock (
    carrier_nda_state_t * state,
    double up_thresh,
    double down_thresh,
    uint32_t n_up,
    uint32_t n_down
) 
```



Full lockdet control, mirroring [**costas\_configure\_lock()**](costas__core_8h.md#function-costas_configure_lock): a split declare/drop threshold pair on the lock-signal EMA (level hysteresis) and both verify counts (time hysteresis). Defaults (0.5/0.4, 64 up / 32 down) start from MpskReceiver's own pre-existing acquisition&lt;-&gt; tracking handover thresholds, but size n\_up independently: `lock` is a fast per-sample EMA, so consecutive looks are highly autocorrelated and MpskReceiver's own n\_up=8 does not compound the false-declare rate the way it would for independent looks (direct Monte Carlo against a noise-only, no-carrier input found real false locks at n\_up=8; n\_up=64 was the smallest verify count that reliably eliminated them  see carrier\_nda\_core.c's CARRIER\_NDA\_LOCK\_DEFAULT\_\* comment for the exact trial data). A live lock survives the re-tune; the in-flight verify run restarts.




**Parameters:**


* `state` Must be non-NULL. 
* `up_thresh` Declare threshold on the lock-signal EMA. 
* `down_thresh` Drop threshold; choose &lt;= up\_thresh for level hysteresis. 
* `n_up` Consecutive above-threshold samples to declare; clamped &gt;= 1. 
* `n_down` Consecutive below-threshold samples to drop; clamped &gt;= 1. 
```C++
>>> from doppler.track import CarrierNda
>>> c = CarrierNda(bn=0.01, sps=8, n=4, m=4)
>>> c.locked
False
>>> c.configure_lock(0.6, 0.5, 16, 64)   # tighter declare, slower drop
```
 




        

<hr>



### function carrier\_nda\_create 

_Create an NDA carrier loop instance._ 
```C++
carrier_nda_state_t * carrier_nda_create (
    double bn,
    double zeta,
    double init_norm_freq,
    size_t sps,
    int n,
    int m
) 
```





**Parameters:**


* `bn` Loop noise bandwidth (default 0.01). 
* `zeta` Damping factor (default 0.707). 
* `init_norm_freq` Seed carrier frequency, cycles/sample (default 0.0). 
* `sps` Samples per symbol (default 8). 
* `n` MA window divisor: window = sps/n (default 4; spsn==0). 
* `m` Constellation order M, 2/4/8 (default 4 = QPSK). 



**Returns:**

Heap-allocated state, or NULL on invalid args / allocation failure. 




**Note:**

Caller must call [**carrier\_nda\_destroy()**](carrier__nda__core_8h.md#function-carrier_nda_destroy) when done. 





        

<hr>



### function carrier\_nda\_destroy 

_Destroy an NDA carrier loop instance and release all memory._ 
```C++
void carrier_nda_destroy (
    carrier_nda_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function carrier\_nda\_disc 

_The M-th-power discriminator on an arm sample (raw, no per-dump limit)._ 
```C++
JM_FORCEINLINE void carrier_nda_disc (
    float complex z,
    int m,
    double scale,
    double * pe,
    double * lock
) 
```



Runs the repeated-squaring recursion `z²`→`z⁴`→`z⁸` directly on the arm sample `z` (the "conventional Costas" / linear-arm form) and writes the phase error (= scaled `Im(z^M)`) and lock signal. The arm sample is expected to be AGC-normalized to unit average power upstream (carrier\_nda\_arm\_step runs the window average through an embedded agc\_core AGC) — so a clean window is `|z|≈1` and a transition-straddling window is `|z|<1` and is _down-weighted naturally_. This is deliberate: a per-sample unit-magnitude normalization is Yuen's "polarity-type" hard limiter, the worst nonlinearity (≈2.5–4 dB extra squaring loss, and non-monotonic in SNR — see docs/design/mpsk.md §2.3). On a unit-magnitude `z` the raw and normalized forms coincide, so the S-curve and lock-scale calibration are unchanged.




**Parameters:**


* `z` Arm moving-average sample (AGC-normalized, ~unit at lock). 
* `m` Constellation order (2, 4, 8). 
* `scale` Per-M lock scale (1 / 0.619 / 0.412). 
* `pe` Receives the phase error. 
* `lock` Receives the lock signal. 




        

<hr>



### function carrier\_nda\_get\_bn 

```C++
double carrier_nda_get_bn (
    const carrier_nda_state_t * state
) 
```




<hr>



### function carrier\_nda\_get\_last\_error 

```C++
double carrier_nda_get_last_error (
    const carrier_nda_state_t * state
) 
```




<hr>



### function carrier\_nda\_get\_lock 

```C++
double carrier_nda_get_lock (
    const carrier_nda_state_t * state
) 
```




<hr>



### function carrier\_nda\_get\_locked 

_Current lock decision (1 = locked, 0 = not), with the configured verify-count / hysteresis rule applied (see carrier\_nda\_configure\_lock)._ 
```C++
int carrier_nda_get_locked (
    const carrier_nda_state_t * state
) 
```




<hr>



### function carrier\_nda\_get\_m 

```C++
int carrier_nda_get_m (
    const carrier_nda_state_t * state
) 
```




<hr>



### function carrier\_nda\_get\_n 

```C++
int carrier_nda_get_n (
    const carrier_nda_state_t * state
) 
```




<hr>



### function carrier\_nda\_get\_norm\_freq 

```C++
double carrier_nda_get_norm_freq (
    const carrier_nda_state_t * state
) 
```




<hr>



### function carrier\_nda\_get\_sps 

```C++
size_t carrier_nda_get_sps (
    const carrier_nda_state_t * state
) 
```




<hr>



### function carrier\_nda\_get\_state 

_Serialize the full loop state into_ `blob` _._
```C++
void carrier_nda_get_state (
    const carrier_nda_state_t * state,
    void * blob
) 
```




<hr>



### function carrier\_nda\_init 

_Initialise an NDA carrier loop in place (no allocation)._ 
```C++
void carrier_nda_init (
    carrier_nda_state_t * s,
    double bn,
    double zeta,
    double init_norm_freq,
    size_t sps,
    int n,
    int m
) 
```





**Parameters:**


* `s` State to initialise. Must be non-NULL. 
* `bn` Loop noise bandwidth, cycles/sample (per-sample loop). 
* `zeta` Damping factor (0.707 = critically damped). 
* `init_norm_freq` Seed carrier frequency, cycles/sample. 
* `sps` Samples per symbol. 
* `n` MA window divisor: window = sps/n samples (sps % n == 0, sps/n &lt;= BOXCAR\_MAX\_LEN). 
* `m` Constellation order M (2, 4, 8). 




        

<hr>



### function carrier\_nda\_reset 

_Re-seed the loop to its create-time frequency/phase; keep config._ 
```C++
void carrier_nda_reset (
    carrier_nda_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function carrier\_nda\_set\_bn 

```C++
void carrier_nda_set_bn (
    carrier_nda_state_t * state,
    double val
) 
```




<hr>



### function carrier\_nda\_set\_norm\_freq 

```C++
void carrier_nda_set_norm_freq (
    carrier_nda_state_t * state,
    double val
) 
```




<hr>



### function carrier\_nda\_set\_state 

_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._ 
```C++
int carrier_nda_set_state (
    carrier_nda_state_t * state,
    const void * blob
) 
```




<hr>



### function carrier\_nda\_set\_telemetry 

_Attach (or detach) a telemetry context and register the carrier loop's probes on it — including the embedded arm AGC's. Registers four probes of its own, emitted once per input sample (this is a sample-rate loop — use_ `decim` _to thin the stream) plus the embedded AGC's "&lt;prefix&gt;.agc.gain\_db" (emitted at the AGC's own amortized gain-update rate): "&lt;prefix&gt;.lock" (the lock-signal EMA, ~1 when phase-locked), "&lt;prefix&gt;.e" (the M-th-power phase discriminator — the loop stress), "&lt;prefix&gt;.freq" (the tracked carrier frequency, cycles/sample) and "&lt;prefix&gt;.locked" (the verify-counted lockdet decision, 0/1). Passing NULL detaches the loop and the embedded AGC. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in_[_**telemetry/telemetry.h**_](telemetry_8h.md) _)._
```C++
int carrier_nda_set_telemetry (
    carrier_nda_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "car" or "rx.car". 
* `decim` Emit every decim-th sample; &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take all five probes (the attach fails whole; everything stays detached). 
```C++
>>> import numpy as np
>>> from doppler.track import CarrierNda
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 14)
>>> c = CarrierNda(bn=0.01, sps=8, n=4, m=4)
>>> c.set_telemetry(tlm, "car", decim=8)
>>> sorted(tlm.probe_names())
['car.agc.gain_db', 'car.e', 'car.freq', 'car.lock', 'car.locked']
>>> x = np.exp(2j * np.pi * 0.005 * np.arange(4096)).astype(np.complex64)
>>> _ = c.steps(x)
>>> recs = tlm.read()
>>> len(recs[recs["probe"] == tlm.probe_id("car.e")]) == 4096 // 8
True
```
 





        

<hr>



### function carrier\_nda\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t carrier_nda_state_bytes (
    const carrier_nda_state_t * state
) 
```




<hr>



### function carrier\_nda\_steer 

_Steer the shared NCO with a phase error through the loop filter._ 
```C++
JM_FORCEINLINE  JM_HOT void carrier_nda_steer (
    carrier_nda_state_t * s,
    double pe
) 
```



Filters `pe` and updates the NCO frequency (per sample) + a proportional phase nudge. Shared by the NDA acquisition path and a composing receiver's decision-directed tracking path (handover writes the same NCO).




**Parameters:**


* `s` Carrier loop state. Must be non-NULL. 
* `pe` Phase error (NDA discriminator, or a decision-directed error). 




        

<hr>



### function carrier\_nda\_steps 

```C++
size_t carrier_nda_steps (
    carrier_nda_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```




<hr>



### function carrier\_nda\_steps\_max\_out 

```C++
size_t carrier_nda_steps_max_out (
    carrier_nda_state_t * state
) 
```




<hr>



### function carrier\_nda\_tlm\_flush 

_Emit the carrier loop's telemetry records for the current sample._ 
```C++
void carrier_nda_tlm_flush (
    const carrier_nda_state_t * s
) 
```



Out-of-line on purpose: the emit machinery must not inline into the per-sample hot loop (inlined ring-write expansions bloat the loop body and an extern call site forces per-iteration state reloads — both measured ~20% slower detached on other loops). Callers gate on `s->tlm.ctx`. This loop updates every sample, so the natural call rate is per sample — decim (set at attach) is the throttle. Records "&lt;prefix&gt;.lock" (the lock-signal EMA), "&lt;prefix&gt;.e" (the M-th-power phase discriminator — the loop stress), "&lt;prefix&gt;.freq" (the tracked carrier, NCO centre + integrated correction, cycles/sample) and "&lt;prefix&gt;.locked" (the verify-counted lockdet decision, 0/1). A composing receiver (the MPSK receiver) calls this once per recovered symbol instead.




**Parameters:**


* `s` State with a non-NULL tlm.ctx (caller-checked). 




        

<hr>



### function carrier\_nda\_wipeoff 

_Per-sample carrier wipe-off: de-rotate_ `x` _by the NCO, advance it._
```C++
JM_FORCEINLINE  JM_HOT float complex carrier_nda_wipeoff (
    carrier_nda_state_t * s,
    float complex x
) 
```





**Parameters:**


* `s` Carrier loop state. Must be non-NULL. 
* `x` One input sample. 



**Returns:**

The de-rotated sample to feed the moving-average arm. 





        

<hr>
## Macro Definition Documentation





### define CARRIER\_NDA\_AGC\_ALPHA 

```C++
#define CARRIER_NDA_AGC_ALPHA `0.01`
```




<hr>



### define CARRIER\_NDA\_AGC\_BW\_RATIO 

```C++
#define CARRIER_NDA_AGC_BW_RATIO `0.01`
```




<hr>



### define CARRIER\_NDA\_AGC\_CLIP\_DB 

```C++
#define CARRIER_NDA_AGC_CLIP_DB `10.0`
```




<hr>



### define CARRIER\_NDA\_AGC\_REF\_DB 

```C++
#define CARRIER_NDA_AGC_REF_DB `0.0`
```




<hr>



### define CARRIER\_NDA\_EPS 

```C++
#define CARRIER_NDA_EPS `1e-12`
```




<hr>



### define CARRIER\_NDA\_INV\_2PI 

```C++
#define CARRIER_NDA_INV_2PI `0.15915494309189535 /* 1 / (2*pi) */`
```




<hr>



### define CARRIER\_NDA\_LOCK\_ALPHA 

```C++
#define CARRIER_NDA_LOCK_ALPHA `0.05`
```




<hr>



### define CARRIER\_NDA\_STATE\_MAGIC 

```C++
#define CARRIER_NDA_STATE_MAGIC `DP_FOURCC ('C', 'N', 'D', 'A')`
```




<hr>



### define CARRIER\_NDA\_STATE\_VERSION 

```C++
#define CARRIER_NDA_STATE_VERSION `4u /* v4: lockdet decision rule (verify counters) */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/carrier_nda/carrier_nda_core.h`

