

# File carrier\_nda\_core.h



[**FileList**](files.md) **>** [**carrier\_nda**](dir_425637d1941eacd8ae8cdd8750b207f0.md) **>** [**carrier\_nda\_core.h**](carrier__nda__core_8h.md)

[Go to the source code of this file](carrier__nda__core_8h_source.md)

_Non-data-aided (NDA) M-th-power carrier-tracking loop._ [More...](#detailed-description)

* `#include "boxcar/boxcar_core.h"`
* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "lo/lo_core.h"`
* `#include "lockdet/lockdet_core.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include "dp_tlm/dp_tlm_core.h"`
* `#include <math.h>`
* `#include "telemetry/telemetry_core.h"`















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
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) void | [**carrier\_nda\_disc**](#function-carrier_nda_disc) (float complex z, int m, double \* pe, double \* lock) <br>_The M-th-power discriminator on an arm sample, normalized by its own amplitude law._  |
|  double | [**carrier\_nda\_get\_bn**](#function-carrier_nda_get_bn) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  double | [**carrier\_nda\_get\_last\_error**](#function-carrier_nda_get_last_error) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  double | [**carrier\_nda\_get\_lock**](#function-carrier_nda_get_lock) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  int | [**carrier\_nda\_get\_locked**](#function-carrier_nda_get_locked) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br>_Current lock decision (1 = locked, 0 = not), with the configured verify-count / hysteresis rule applied (see carrier\_nda\_configure\_lock)._  |
|  int | [**carrier\_nda\_get\_m**](#function-carrier_nda_get_m) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  int | [**carrier\_nda\_get\_n**](#function-carrier_nda_get_n) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  double | [**carrier\_nda\_get\_nco\_freq**](#function-carrier_nda_get_nco_freq) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br>_Instantaneous NCO frequency command = centre + full loop-filter output (integ + kp\*e), cycles/sample. Mean rides a ramp with no lag; variance is the loop stress. See the impl for the estimator-vs-command distinction._  |
|  double | [**carrier\_nda\_get\_norm\_freq**](#function-carrier_nda_get_norm_freq) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  size\_t | [**carrier\_nda\_get\_sps**](#function-carrier_nda_get_sps) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  void | [**carrier\_nda\_get\_state**](#function-carrier_nda_get_state) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state, void \* blob) <br>_Serialize the full loop state into_ `blob` _._ |
|  void | [**carrier\_nda\_init**](#function-carrier_nda_init) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* s, double bn, double zeta, double init\_norm\_freq, size\_t sps, int n, int m) <br>_Initialise an NDA carrier loop in place (no allocation)._  |
|  void | [**carrier\_nda\_reset**](#function-carrier_nda_reset) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br>_Re-seed the loop to its create-time frequency/phase; keep config._  |
|  void | [**carrier\_nda\_set\_bn**](#function-carrier_nda_set_bn) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state, double val) <br> |
|  void | [**carrier\_nda\_set\_norm\_freq**](#function-carrier_nda_set_norm_freq) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state, double val) <br> |
|  int | [**carrier\_nda\_set\_state**](#function-carrier_nda_set_state) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state, const void \* blob) <br>_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._  |
|  int | [**carrier\_nda\_set\_telemetry**](#function-carrier_nda_set_telemetry) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context and register the carrier loop's probes on it. Registers four probes, emitted once per input sample (this is a sample-rate loop — use_ `decim` _to thin the stream): "&lt;prefix&gt;.lock" (the lock-signal EMA, ~1 when phase-locked), "&lt;prefix&gt;.e" (the M-th-power phase discriminator — the loop stress), "&lt;prefix&gt;.freq" (the tracked carrier frequency, cycles/sample) and "&lt;prefix&gt;.locked" (the verify-counted lockdet decision, 0/1). Passing NULL detaches. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in_[_**dp\_tlm/dp\_tlm\_core.h**_](dp__tlm__core_8h.md) _)._ |
|  size\_t | [**carrier\_nda\_state\_bytes**](#function-carrier_nda_state_bytes) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**carrier\_nda\_steer**](#function-carrier_nda_steer) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* s, double pe) <br>_Steer the shared NCO with a phase error through the loop filter._  |
|  size\_t | [**carrier\_nda\_steps**](#function-carrier_nda_steps) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br>_De-rotate a cf32 block with the recovered carrier and return the de-rotated stream (one output per input sample)._  |
|  size\_t | [**carrier\_nda\_steps\_max\_out**](#function-carrier_nda_steps_max_out) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* state) <br> |
|  void | [**carrier\_nda\_tlm\_flush**](#function-carrier_nda_tlm_flush) (const [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* s) <br>_Emit the carrier loop's telemetry records for the current sample._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float complex | [**carrier\_nda\_wipeoff**](#function-carrier_nda_wipeoff) ([**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) \* s, float complex x) <br>_Per-sample carrier wipe-off: de-rotate_ `x` _by the NCO, advance it._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CARRIER\_NDA\_EPS**](carrier__nda__core_8h.md#define-carrier_nda_eps)  `1e-12`<br> |
| define  | [**CARRIER\_NDA\_INV\_2PI**](carrier__nda__core_8h.md#define-carrier_nda_inv_2pi)  `0.15915494309189535 /\* 1 / (2\*pi) \*/`<br> |
| define  | [**CARRIER\_NDA\_LOCK\_ALPHA**](carrier__nda__core_8h.md#define-carrier_nda_lock_alpha)  `0.05`<br> |
| define  | [**CARRIER\_NDA\_LOCK\_NORM\_SD**](carrier__nda__core_8h.md#define-carrier_nda_lock_norm_sd)  `0.11322770341445956`<br> |
| define  | [**CARRIER\_NDA\_STATE\_MAGIC**](carrier__nda__core_8h.md#define-carrier_nda_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('C', 'N', 'D', 'A')`<br> |
| define  | [**CARRIER\_NDA\_STATE\_VERSION**](carrier__nda__core_8h.md#define-carrier_nda_state_version)  `/* multi line expression */`<br> |

## Detailed Description


A carrier-recovery loop that locks **without data and without symbol timing** — the cold-start / acquisition counterpart to the decision-directed [**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) loop. Per sample it de-rotates the input with the integer-phase [**lo\_state\_t**](structlo__state__t.md) NCO (carrier wipe-off); it filters the de-rotated samples through a free-running I/Q **boxcar moving average** of `sps/n` samples (one output per input sample — no rate change), and on every sample runs the **M-th-power** phase discriminator, filters the error through an embedded [**loop\_filter\_state\_t**](structloop__filter__state__t.md), and steers the NCO frequency + phase.


Raising the arm sample `z` to the Mth power strips the M-PSK data modulation, leaving M times the carrier phase — so the discriminator is **independent of the data symbols and of symbol timing**. That is what lets it acquire a bare/unmodulated carrier, or a modulated carrier before timing lock. It is the M-fold-ambiguous acquisition aid; a decision-directed loop gives the low-jitter steady state (resolve the M-fold ambiguity downstream).


The M-th power is computed by **repeated complex squaring** (`z²`→`z⁴`→`z⁸`) of the **unit-magnitude** sample `z/|z|`. Each level yields a phase error and a lock signal:
* `phase_error` = `Im((z/|z|)^M)` scaled by `1, ½, ¼` for M = 2, 4, 8 — the scale normalizes the phase-detector gain so the S-curve slope at lock is 2 for every M (one `bn` behaves identically across M).
* `lock_signal` = `Re((z/|z|)^M)` — the M-th power of a **limited** sample, so it is bounded in ±1 and its H0 variance is 1/2 for **every** M. ~1 when phase-locked, zero-mean with no carrier. That M-independence is what makes one `lock_thresh` mean one Pfa at every order; the threshold chain is derived above `CARRIER_NDA_LOCK_ALPHA`. Its EMA (`lock`) is the carrier lock metric. See `docs/design/mpsk.md` §2.3 for the derivation.




The block API (carrier\_nda\_steps) is the Python face and emits the de-rotated sample stream; the JM\_FORCEINLINE [**carrier\_nda\_wipeoff()**](carrier__nda__core_8h.md#function-carrier_nda_wipeoff)/\_arm\_step()/\_steer() are the C composition API a receiver inlines into its own sample loop (it can also steer the shared NCO with its own decision-directed error on handover).




**Note:**

**The input level does not matter, and there is no AGC here.** The discriminator divides out its own amplitude law (`|z|^M`) exactly, so both outputs — and with them the loop gain — are invariant to input scale over the whole float range: measured identical to 5e-7 relative from an amplitude of 1e-5 to 1e15, at every M. This loop used to embed a slow arm AGC whose only job was to manufacture `|z| = 1` so a raw `Im(z^M)` would behave; that condition no longer has to be manufactured, and the AGC is gone. A receiver needs exactly one AGC, for its own signal path, and not one per detector (`docs/design/mpsk.md` §2.3).



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



The arm is a free-running boxcar **moving average** of the last `arm_len` de-rotated samples — one output per input sample, **no rate change** (not a decimating integrate-and-dump). It updates the running window sum in O(1) (add `d`, subtract the sample leaving the window), runs the M-th-power discriminator on the window average, writes `pe` and `lock`, and returns 1 every call.




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

_The M-th-power discriminator on an arm sample, normalized by its own amplitude law._ 
```C++
JM_FORCEINLINE void carrier_nda_disc (
    float complex z,
    int m,
    double * pe,
    double * lock
) 
```



Runs the repeated-squaring recursion `z²`→`z⁴`→`z⁸` on the **unit- magnitude** sample `z/|z|` and writes the phase error (= scaled `Im((z/|z|)^M)`) and the lock signal (`Re((z/|z|)^M)`). Both outputs are therefore invariant to the input's scale, which is what lets this loop run with no AGC in front of it — see the amplitude note at the top of this file, and docs/design/mpsk.md §2.3 for the squaring-loss measurement that says normalizing is equal or better from ~6 dB Es/N0 up.




**Parameters:**


* `z` Arm moving-average sample (any scale; only its phase is used). 
* `m` Constellation order (2, 4, 8). 
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



### function carrier\_nda\_get\_nco\_freq 

_Instantaneous NCO frequency command = centre + full loop-filter output (integ + kp\*e), cycles/sample. Mean rides a ramp with no lag; variance is the loop stress. See the impl for the estimator-vs-command distinction._ 
```C++
double carrier_nda_get_nco_freq (
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



Restores the object to its post-create state: the carrier NCO is reset to the seed frequency it was constructed with (init\_norm\_freq) with zero phase, the moving-average arm, the loop-filter integrator and the lock EMA are cleared, and the lock detector is dropped. The configured (bn, zeta), the arm geometry (sps, n) and the constellation order m are preserved, so the same object can re-acquire a fresh capture.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.track import CarrierNda
>>> c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0,
...                sps=8, n=4, m=4)
>>> rng = np.random.default_rng(0)
>>> k = np.arange(40000)
>>> x = (np.exp(2j * np.pi * 0.001 * k) + 0.05 * (
...      rng.standard_normal(k.size)
...      + 1j * rng.standard_normal(k.size))).astype(np.complex64)
>>> _ = c.steps(x)
>>> round(c.norm_freq, 4), round(c.lock, 2)   # acquired the carrier
(0.001, 0.99)
>>> c.reset()
>>> round(c.norm_freq, 4), round(c.lock, 2)   # back to seed, unlocked
(0.0, 0.0)
```
 




        

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

_Attach (or detach) a telemetry context and register the carrier loop's probes on it. Registers four probes, emitted once per input sample (this is a sample-rate loop — use_ `decim` _to thin the stream): "&lt;prefix&gt;.lock" (the lock-signal EMA, ~1 when phase-locked), "&lt;prefix&gt;.e" (the M-th-power phase discriminator — the loop stress), "&lt;prefix&gt;.freq" (the tracked carrier frequency, cycles/sample) and "&lt;prefix&gt;.locked" (the verify-counted lockdet decision, 0/1). Passing NULL detaches. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in_[_**dp\_tlm/dp\_tlm\_core.h**_](dp__tlm__core_8h.md) _)._
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

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take all four probes (the attach fails whole; everything stays detached). 
```C++
>>> import numpy as np
>>> from doppler.track import CarrierNda
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 14)
>>> c = CarrierNda(bn=0.01, sps=8, n=4, m=4)
>>> c.set_telemetry(tlm, "car", decim=8)
>>> sorted(tlm.probe_names)
['car.e', 'car.freq', 'car.lock', 'car.locked']
>>> x = np.exp(2j * np.pi * 0.005 * np.arange(4096)).astype(
...     np.complex64)
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

_De-rotate a cf32 block with the recovered carrier and return the de-rotated stream (one output per input sample)._ 
```C++
size_t carrier_nda_steps (
    carrier_nda_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```



Runs the non-data-aided carrier loop over the block: each sample is wiped off by the integer-phase NCO, the de-rotated sample slides the I/Q moving-average arm, and the M-th-power discriminator (which strips the M-PSK data modulation) steers the NCO frequency and phase. Because the discriminator is data- and timing-independent, this acquires the carrier with no symbol timing and no data present — a bare carrier, or a modulated carrier before timing lock. It resolves to one of m carrier phases (M-fold ambiguity, resolved downstream). Read norm\_freq for the tracked carrier (cycles/sample) and lock for the carrier lock metric.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input samples (average power at or below unity). 
* `x_len` Number of input samples. 
* `out` De-rotated samples, one per input. 
* `max_out` Capacity of `out`. 



**Returns:**

Number of de-rotated samples written to `out` (equals `x_len`). 
```C++
>>> import numpy as np
>>> from doppler.track import CarrierNda
>>> c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0,
...                sps=8, n=4, m=4)
>>> rng = np.random.default_rng(0)
>>> k = np.arange(40000)
>>> x = (np.exp(2j * np.pi * 0.001 * k) + 0.05 * (
...      rng.standard_normal(k.size)
...      + 1j * rng.standard_normal(k.size))).astype(np.complex64)
>>> y = c.steps(x)                 # de-rotated toward DC
>>> y.shape[0]
40000
>>> round(c.norm_freq, 4)          # tracked carrier, cycles/sample
0.001
>>> c.lock > 0.5                    # carrier lock metric, ~1 at lock
True
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



### define CARRIER\_NDA\_LOCK\_NORM\_SD 

```C++
#define CARRIER_NDA_LOCK_NORM_SD `0.11322770341445956`
```




<hr>



### define CARRIER\_NDA\_STATE\_MAGIC 

```C++
#define CARRIER_NDA_STATE_MAGIC `DP_FOURCC ('C', 'N', 'D', 'A')`
```




<hr>



### define CARRIER\_NDA\_STATE\_VERSION 

```C++
#define CARRIER_NDA_STATE_VERSION `/* multi line expression */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/carrier_nda/carrier_nda_core.h`

