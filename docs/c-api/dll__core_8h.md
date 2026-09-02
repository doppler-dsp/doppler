

# File dll\_core.h



[**FileList**](files.md) **>** [**dll**](dir_f3da3e2048ea3a8b9e723d3c5367d8f8.md) **>** [**dll\_core.h**](dll__core_8h.md)

[Go to the source code of this file](dll__core_8h_source.md)

_Delay-lock loop (DLL) — non-coherent early/prompt/late code tracking._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "lockdet/lockdet_core.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include "nco/nco_core.h"`
* `#include "dp_tlm/dp_tlm_core.h"`
* `#include <complex.h>`
* `#include <math.h>`
* `#include "detection/detection_core.h"`
* `#include "telemetry/telemetry_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dll\_state\_t**](structdll__state__t.md) <br>_DLL state._  |
| struct | [**dll\_tlm\_t**](structdll__tlm__t.md) <br>_Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then a single predicted-not-taken branch per code epoch. Zeroed in state blobs and preserved across set\_state (the hand-written triplet treats it like the borrowed_ `code` _)._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**dll\_accumulate**](#function-dll_accumulate) ([**dll\_state\_t**](structdll__state__t.md) \* s, float \_Complex d) <br>_Per-sample early/prompt/late correlate + fixed-point code-phase advance._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float | [**dll\_chip\_sign**](#function-dll_chip_sign) (uint8\_t c) <br> |
|  void | [**dll\_configure**](#function-dll_configure) ([**dll\_state\_t**](structdll__state__t.md) \* state, double bn, double zeta) <br>_Recompute the loop gains for a new (bn, zeta); keep the code state._  |
|  int | [**dll\_configure\_lock**](#function-dll_configure_lock) ([**dll\_state\_t**](structdll__state__t.md) \* state, double pfa, size\_t n\_looks, double ref\_snr\_db) <br>_Tune the always-on code-lock detector to a target (pfa, n\_looks)._  |
|  void | [**dll\_configure\_lock\_raw**](#function-dll_configure_lock_raw) ([**dll\_state\_t**](structdll__state__t.md) \* state, double up\_thresh, double down\_thresh, size\_t n\_looks, double alpha, uint32\_t n\_up, uint32\_t n\_down) <br>_Set the lock detector's raw geometry directly._  |
|  [**dll\_state\_t**](structdll__state__t.md) \* | [**dll\_create**](#function-dll_create) (const uint8\_t \* code, size\_t code\_len, size\_t sps, double init\_chip, double bn, double zeta, double spacing, size\_t segments) <br>_Create a code/timing delay-locked loop over a spreading code._  |
|  void | [**dll\_destroy**](#function-dll_destroy) ([**dll\_state\_t**](structdll__state__t.md) \* state) <br>_Destroy a DLL instance and release all memory (incl. the code copy)._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**dll\_dwell\_center\_chip\_pos**](#function-dll_dwell_center_chip_pos) (const [**dll\_state\_t**](structdll__state__t.md) \* s) <br>_This sample's dwell-CENTER code phase, chips._  |
|  double | [**dll\_get\_bn**](#function-dll_get_bn) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  double | [**dll\_get\_code\_phase**](#function-dll_get_code_phase) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  double | [**dll\_get\_code\_rate**](#function-dll_get_code_rate) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  double | [**dll\_get\_last\_error**](#function-dll_get_last_error) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  double | [**dll\_get\_lock\_stat**](#function-dll_get_lock_stat) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br>_Last lock test statistic R (compare against the configured eta)._  |
|  int | [**dll\_get\_locked**](#function-dll_get_locked) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br>_Current lock decision (1 = locked, 0 = not), with the configured verify-count / hysteresis rule applied (see dll\_configure\_lock)._  |
|  double | [**dll\_get\_noise\_est**](#function-dll_get_noise_est) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br>_Current CFAR noise-power estimate E\|O\|^2 (offset-tap EMA)._  |
|  size\_t | [**dll\_get\_segments**](#function-dll_get_segments) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  void | [**dll\_get\_state**](#function-dll_get_state) (const [**dll\_state\_t**](structdll__state__t.md) \* state, void \* blob) <br> |
|  void | [**dll\_init**](#function-dll_init) ([**dll\_state\_t**](structdll__state__t.md) \* s, const uint8\_t \* code, size\_t code\_len, size\_t sps, double init\_chip, double bn, double zeta, double spacing) <br>_Initialise a DLL in place (no allocation); BORROWS_ `code` _._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**dll\_lock\_accumulate**](#function-dll_lock_accumulate) ([**dll\_state\_t**](structdll__state__t.md) \* s, float \_Complex d) <br>_Per-sample offset (noise) tap for the always-on lock detector._  |
|  void | [**dll\_lock\_epoch**](#function-dll_lock_epoch) ([**dll\_state\_t**](structdll__state__t.md) \* s) <br>_Per-epoch lock-detector housekeeping: re-draw the noise offset._  |
|  void | [**dll\_lock\_look**](#function-dll_lock_look) ([**dll\_state\_t**](structdll__state__t.md) \* s, double norm) <br>_Fold one look into the lock detector; clear the offset tap._  |
|  size\_t | [**dll\_lookback\_segments**](#function-dll_lookback_segments) (size\_t tsamps, double max\_error\_db) <br>_Derive a principled_ `segments` _count from a max tolerable async-lookback correlation-power loss, instead of hand-picking one._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float | [**dll\_replica**](#function-dll_replica) (const [**dll\_state\_t**](structdll__state__t.md) \* s, double c) <br>_Sub-chip code replica at fractional code phase_ `c` _(one tap)._ |
|  void | [**dll\_reset**](#function-dll_reset) ([**dll\_state\_t**](structdll__state__t.md) \* state) <br>_Re-seed the loop to its create-time code phase; keep config._  |
|  void | [**dll\_set\_bn**](#function-dll_set_bn) ([**dll\_state\_t**](structdll__state__t.md) \* state, double val) <br> |
|  void | [**dll\_set\_rate\_aid**](#function-dll_set_rate_aid) ([**dll\_state\_t**](structdll__state__t.md) \* state, double rate\_aid) <br>_Set the carrier-aiding code-rate deviation (ratio; 0 = off)._  |
|  int | [**dll\_set\_state**](#function-dll_set_state) ([**dll\_state\_t**](structdll__state__t.md) \* state, const void \* blob) <br> |
|  int | [**dll\_set\_telemetry**](#function-dll_set_telemetry) ([**dll\_state\_t**](structdll__state__t.md) \* state, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context and register the code loop's probes on it. Registers four probes, emitted once per code epoch (period) and further thinned by decim: "&lt;prefix&gt;.e" (the early-minus-late envelope discriminator — the loop stress), "&lt;prefix&gt;.rate" (the tracked code rate, chips advanced per nominal chip, ~1.0 at lock), "&lt;prefix&gt;.lock" (the CFAR lock statistic R; compare against the configured threshold) and "&lt;prefix&gt;.locked" (the verify-counted lock decision, 0/1 — the lockdet output, so a consumer sees where the declare/drop rule fired without re-deriving it from the statistic). Passing NULL detaches. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in_ [_**dp\_tlm/dp\_tlm\_core.h**_](dp__tlm__core_8h.md) _)._ |
|  size\_t | [**dll\_state\_bytes**](#function-dll_state_bytes) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  size\_t | [**dll\_steps**](#function-dll_steps) ([**dll\_state\_t**](structdll__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, float \_Complex \* out, size\_t max\_out) <br>_Correlate a carrier-wiped block against the local code and steer the code NCO once per code period._  |
|  size\_t | [**dll\_steps\_max\_out**](#function-dll_steps_max_out) ([**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  void | [**dll\_tlm\_flush**](#function-dll_tlm_flush) (const [**dll\_state\_t**](structdll__state__t.md) \* s) <br>_Emit the code loop's telemetry records for the epoch just closed._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**dll\_update**](#function-dll_update) ([**dll\_state\_t**](structdll__state__t.md) \* s) <br>_Per-period code discriminator + loop update + NCO steer._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DLL\_DISC\_CLAMP**](dll__core_8h.md#define-dll_disc_clamp)  `1.0`<br> |
| define  | [**DLL\_EPS**](dll__core_8h.md#define-dll_eps)  `1e-12`<br> |
| define  | [**DLL\_STATE\_MAGIC**](dll__core_8h.md#define-dll_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('D','L','L',' ')`<br> |
| define  | [**DLL\_STATE\_VERSION**](dll__core_8h.md#define-dll_state_version)  `/* multi line expression */`<br> |

## Detailed Description


Tracks the code phase of a continuous, repeating spreading code (e.g. a PN / Gold sequence) on a _carrier-wiped_ sample stream. Per sample it correlates the input against three taps of a 2-samples/chip interpolated local-code replica — early (`+spacing` chips), prompt, late (`-spacing` chips) — accumulating an integrate-and-dump over one code period; per period it runs the power-domain non-coherent early-minus-late discriminator `0.5 * (|E|^2 - |L|^2) / |P|^2`, filters it through an embedded 2nd-order [**loop\_filter\_state\_t**](structloop__filter__state__t.md), and steers an embedded fixed-point NCO ([**nco\_state\_t**](structnco__state__t.md)) that tracks the code phase — a 32-bit phase accumulator, exact integer wraparound, no open-ended floating-point drift.


It pairs with the carrier loop ([**costas\_core.h**](costas__core_8h.md)): the carrier loop wipes the carrier, the DLL wipes the code. The block API (dll\_steps) is the Python face; the JM\_FORCEINLINE [**dll\_accumulate()**](dll__core_8h.md#function-dll_accumulate)/dll\_update() are the C composition API a tracking channel inlines into its own sample loop.


Lifecycle: `dll_create -> (steps / configure / reset)* -> dll_destroy`, or embed by value with [**dll\_init()**](dll__core_8h.md#function-dll_init) (which BORROWS the caller-owned code, and always runs with `segments == 1` — there is no by-value counterpart to `dll_create()`'s `segments` parameter).



```C++
uint8_t code[31] = { ... };  // 0/1 chips, one period
dll_state_t *d = dll_create(code, 31, 2, 0.0, 0.01, 0.707, 0.5);
float _Complex sym[16];
size_t k = dll_steps(d, rx, rx_len, sym, 16);  // one prompt per period
double phase = d->chip_pos;                     // tracked code phase, chips
dll_destroy(d);
```
 


    
## Public Functions Documentation




### function dll\_accumulate 

_Per-sample early/prompt/late correlate + fixed-point code-phase advance._ 
```C++
JM_FORCEINLINE  JM_HOT int dll_accumulate (
    dll_state_t * s,
    float _Complex d
) 
```



Correlates the carrier-wiped sample `d` against the early, prompt and late code taps at this sample's dwell-CENTER phase ([**dll\_dwell\_center\_chip\_pos**](dll__core_8h.md#function-dll_dwell_center_chip_pos), wrapped over the periodic code), then advances the embedded fixed-point NCO by one sample and re-derives `chip_pos` from its POST-advance (dwell-START-of-next-sample) phase (never independently accumulated — see [**dll\_state\_t::chip\_pos**](structdll__state__t.md#variable-chip_pos)). Inline, zero call overhead.




**Parameters:**


* `s` DLL state. Must be non-NULL. 
* `d` One carrier-wiped input sample. 



**Returns:**

1 if this sample's advance wrapped the code period (a period boundary), 0 otherwise. A plain return value, not a persistent struct field — a stored wrap flag that a caller forgets to consume is exactly the class of bug an earlier attempt at this redesign hit (a stale flag causing an infinite loop under segments&gt;1 stress); a local value cannot go stale. 





        

<hr>



### function dll\_chip\_sign 

```C++
JM_FORCEINLINE float dll_chip_sign (
    uint8_t c
) 
```



0/1 chip -&gt; +1/-1 BPSK sign. 


        

<hr>



### function dll\_configure 

_Recompute the loop gains for a new (bn, zeta); keep the code state._ 
```C++
void dll_configure (
    dll_state_t * state,
    double bn,
    double zeta
) 
```



Re-derives the 2nd-order loop filter's proportional and integral gains for a new noise bandwidth and damping, leaving the tracked code phase, code rate and correlator accumulators untouched — retune the loop mid-run (e.g. narrow the bandwidth once pulled in) without dropping lock.




**Parameters:**


* `state` DLL state. Must be non-NULL. 
* `bn` Loop noise bandwidth, normalised to the code-period rate. 
* `zeta` Damping factor (0.707 = critically damped). 
```C++
>>> import numpy as np
>>> from doppler.track import Dll
>>> rng = np.random.default_rng(1)
>>> code = rng.integers(0, 2, 31).astype(np.uint8)
>>> d = Dll(code=code, sps=2, bn=0.01)
>>> d.configure(bn=0.02, zeta=0.707)   # widen the bandwidth mid-run
>>> round(d.bn, 3)
0.02
```
 




        

<hr>



### function dll\_configure\_lock 

_Tune the always-on code-lock detector to a target (pfa, n\_looks)._ 
```C++
int dll_configure_lock (
    dll_state_t * state,
    double pfa,
    size_t n_looks,
    double ref_snr_db
) 
```



The DLL carries a lock detector that reuses acquisition's non-coherent test statistic. Every emitted look (a partial in segments mode, or the full-epoch prompt when segments == 1) is also correlated at a _random off-peak_ code phase — re-drawn each epoch and kept `noise_guard` chips clear of the prompt/early/late lobe — to give a signal-free CFAR noise sample (valid for a low-sidelobe code, e.g. Gold). The offset power feeds an EMA reference `E|O|^2`; the prompt powers of `n_looks` consecutive looks are summed into `S = sum|P_k|^2`, and the detector declares lock when


R = sqrt(2 \* S / E\|O\|^2) &gt; det\_threshold\_noncoherent(pfa, n\_looks)


which under H0 has `P(R > eta) = marcum_q(n_looks, 0, eta)`. Size `n_looks` with det\_n\_noncoh(snr, ...) for the operating C/N0.


The noise-reference EMA bandwidth is sized probabilistically via [**det\_ema\_alpha()**](detection__core_8h.md#function-det_ema_alpha): the signal-free `|O|^2` samples are exponential (0 dB estimator SNR per sample — a DC level in fluctuation of equal power), and `ref_snr_db` chooses the EMA output's estimator SNR (mean^2/variance). Passing 0 derives it from `n_looks:` the reference's relative std is held to an eighth of the statistic's intrinsic H0 spread (`1/sqrt(N)`), floored at ~33 dB — which reproduces the classic `1/alpha = max(1024, 32*N)` sizing exactly, now as a consequence instead of a constant.


The detector needs an off-peak code phase to sample noise from: with a very short code (fewer than ~2\*(spacing+2)+1 chips, i.e. sf &lt;= 6 at the default spacing) no offset clears the prompt/early/late lobe, the noise tap aliases the prompt, and the statistic pins below threshold — locked stays 0 (fail-closed) no matter the signal. Use a code of &gt;= 7 chips (real spreading codes are far longer) for a meaningful lock decision.


The decision itself runs through an embedded lock detector ([**lockdet\_core.h**](lockdet__core_8h.md)) rather than a single-comparison latch: `locked` flips up only after det\_verify\_count(pfa, pfa\*1e-3) CONSECUTIVE above-threshold decisions (the false-declare budget held three decades under the per-decision `pfa` — 2 straight for the default 1e-3), and drops only after 2 straight below-threshold decisions, so a statistic grazing the threshold cannot chatter the flag. Full control of the verify counts and a split declare/drop threshold pair is C-only via [**dll\_configure\_lock\_raw()**](dll__core_8h.md#function-dll_configure_lock_raw).




**Parameters:**


* `state` DLL state. Must be non-NULL. 
* `pfa` Per-decision false-alarm probability, in (0, 1). 
* `n_looks` Non-coherent integration depth N (looks); clamped &gt;= 1. 
* `ref_snr_db` Noise-reference estimator SNR in dB (&gt; 0), or 0 to derive from `n_looks` as above. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when `pfa` is outside (0, 1). 
```C++
>>> import numpy as np
>>> from doppler.track import Dll
>>> d = Dll(code=np.zeros(31, dtype=np.uint8), sps=2)
>>> d.configure_lock(1e-3, 20)
>>> d.locked
False
>>> d.configure_lock(1e-3, 20, ref_snr_db=20.0)   # ~50-look reference
>>> d.configure_lock(2.0, 20)
Traceback (most recent call last):
    ...
ValueError: configure_lock failed (rc=-4)
```
 





        

<hr>



### function dll\_configure\_lock\_raw 

_Set the lock detector's raw geometry directly._ 
```C++
void dll_configure_lock_raw (
    dll_state_t * state,
    double up_thresh,
    double down_thresh,
    size_t n_looks,
    double alpha,
    uint32_t n_up,
    uint32_t n_down
) 
```



The escape hatch under [**dll\_configure\_lock()**](dll__core_8h.md#function-dll_configure_lock) for a composing C caller that derives its own threshold/EMA/hysteresis geometry — the full lockdet decision rule is exposed: a split declare/drop threshold pair (level hysteresis) and both verify counts (time hysteresis; size them with [**det\_verify\_count()**](detection__core_8h.md#function-det_verify_count)). Re-tuning clears the in-flight statistic and drops the lock so the next decision uses only looks gathered under the new config.




**Parameters:**


* `state` DLL state. Must be non-NULL. 
* `up_thresh` Declare threshold on the statistic R (e.g. the CFAR eta from [**det\_threshold\_noncoherent()**](detection__core_8h.md#function-det_threshold_noncoherent)). 
* `down_thresh` Drop threshold on R; choose &lt;= up\_thresh for level hysteresis. 
* `n_looks` Non-coherent integration depth N (looks); clamped &gt;= 1. 
* `alpha` EMA coefficient for the noise reference, in (0, 1]. 
* `n_up` Consecutive above-threshold decisions to declare lock; clamped to &gt;= 1. 
* `n_down` Consecutive below-threshold decisions to drop it; clamped to &gt;= 1. 
```C++
>>> import numpy as np
>>> from doppler.track import Dll
>>> rng = np.random.default_rng(1)
>>> # >= 7 chips gives a usable lock statistic
>>> code = rng.integers(0, 2, 63).astype(np.uint8)
>>> chip = np.where(code & 1, -1.0, 1.0)
>>> x = np.tile(np.repeat(chip, 4), 400).astype(np.complex64)
>>> d = Dll(code, sps=4, bn=0.005)
>>> # raw geometry: declare at R>3, drop at R<2.5, 8-look,
>>> # 2-of-2 hysteresis
>>> d.configure_lock_raw(3.0, 2.5, 8, 1.0 / 1024, 2, 2)
>>> _ = d.steps(x)
>>> d.locked                       # cleared the declare threshold
True
>>> bool(d.lock_stat > 3.0)
True
```
 




        

<hr>



### function dll\_create 

_Create a code/timing delay-locked loop over a spreading code._ 
```C++
dll_state_t * dll_create (
    const uint8_t * code,
    size_t code_len,
    size_t sps,
    double init_chip,
    double bn,
    double zeta,
    double spacing,
    size_t segments
) 
```



A non-coherent early/prompt/late DLL that tracks the code phase of a repeating spreading sequence (PN / Gold) on a carrier-wiped sample stream. Each code period it correlates the input against three replica taps — early (`+spacing` chips), prompt, late (`-spacing` chips) — runs the power-domain early-minus-late discriminator, filters it through a 2nd-order loop, and steers a fixed-point code-phase NCO. With `segments == 1` it emits one prompt symbol per period (a coherent full-epoch integrate-and-dump); with `segments > 1` it splits each epoch into that many partial correlations and tracks non-coherently across them, robust to an asynchronous data-symbol clock. An always-on CFAR lock detector (see [**dll\_configure\_lock**](dll__core_8h.md#function-dll_configure_lock)) reports whether the loop is tracking; carrier-aiding ([**dll\_set\_rate\_aid**](dll__core_8h.md#function-dll_set_rate_aid)) lets the code NCO ride a physically-coupled Doppler the discriminator alone cannot pull in at low SNR.




**Parameters:**


* `code` Spreading code (0/1 chips), one period; copied internally. 
* `code_len` Code length (chips per period). 
* `sps` Samples per chip (default 2). 
* `init_chip` Seed code phase, chips (default 0.0). 
* `bn` Loop noise bandwidth (default 0.01). 
* `zeta` Damping factor (default 0.707). 
* `spacing` Early/late tap offset, chips (default 0.5). 
* `segments` Partial correlations per code epoch (default 1). 1 = a coherent full-epoch integrate-and-dump (one prompt/period). &gt;1 splits each epoch into that many sub-epoch partials: it emits that many partial prompts/period and tracks the code non-coherently across them (robust to an asynchronous data-symbol clock). segments/epoch ~ samples/symbol at a downstream SymbolSync when the symbol rate is near the code rate, so choose &gt;= 2 for symbol-timing recovery. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**dll\_destroy()**](dll__core_8h.md#function-dll_destroy) when done. 
```C++
>>> import numpy as np
>>> from doppler.track import Dll
>>> rng = np.random.default_rng(1)
>>> code = rng.integers(0, 2, 31).astype(np.uint8)  # a 31-chip PN code
>>> chip = np.where(code & 1, -1.0, 1.0)    # BPSK spreading code
>>> x = np.tile(np.repeat(chip, 2), 60).astype(np.complex64)
>>> d = Dll(code=code, sps=2)               # 2 samples/chip loop
>>> sym = d.steps(x)                        # one prompt per period
>>> sym.shape                               # 60 despread symbols
(60,)
>>> round(float(np.mean(sym.real[-10:])), 1)  # despread to a clean +1
1.0
>>> round(d.code_rate, 3)                   # code NCO at nominal rate
1.0
```
 





        

<hr>



### function dll\_destroy 

_Destroy a DLL instance and release all memory (incl. the code copy)._ 
```C++
void dll_destroy (
    dll_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dll\_dwell\_center\_chip\_pos 

_This sample's dwell-CENTER code phase, chips._ 
```C++
JM_FORCEINLINE double dll_dwell_center_chip_pos (
    const dll_state_t * s
) 
```



A received sample is a zero-order hold over its dwell interval `[chip_pos, chip_pos + step)`, `step` = one `phase_inc` in chip units (~1/sps). Evaluating the replica taps at the dwell's START (the raw pre-advance `chip_pos`) treats every sample as landing at the very first instant of its hold interval rather than at the interval's continuous-time representative point — this biases the correlation by half a sample's worth of chip phase (0.5/sps chips). Found via a direct symmetry check: the autocorrelation of a real signal must satisfy R(tau) = R(-tau), and the S-curve didn't — it was offset from tau=0 by exactly 0.5/sps, vanishing when taps are evaluated at the dwell midpoint instead. Advancing by half of `phase_inc` before deriving the chip position gives that midpoint.




**Parameters:**


* `s` DLL state. 



**Returns:**

Dwell-center code phase, chips. 





        

<hr>



### function dll\_get\_bn 

```C++
double dll_get_bn (
    const dll_state_t * state
) 
```




<hr>



### function dll\_get\_code\_phase 

```C++
double dll_get_code_phase (
    const dll_state_t * state
) 
```




<hr>



### function dll\_get\_code\_rate 

```C++
double dll_get_code_rate (
    const dll_state_t * state
) 
```




<hr>



### function dll\_get\_last\_error 

```C++
double dll_get_last_error (
    const dll_state_t * state
) 
```




<hr>



### function dll\_get\_lock\_stat 

_Last lock test statistic R (compare against the configured eta)._ 
```C++
double dll_get_lock_stat (
    const dll_state_t * state
) 
```




<hr>



### function dll\_get\_locked 

_Current lock decision (1 = locked, 0 = not), with the configured verify-count / hysteresis rule applied (see dll\_configure\_lock)._ 
```C++
int dll_get_locked (
    const dll_state_t * state
) 
```




<hr>



### function dll\_get\_noise\_est 

_Current CFAR noise-power estimate E\|O\|^2 (offset-tap EMA)._ 
```C++
double dll_get_noise_est (
    const dll_state_t * state
) 
```




<hr>



### function dll\_get\_segments 

```C++
size_t dll_get_segments (
    const dll_state_t * state
) 
```




<hr>



### function dll\_get\_state 

```C++
void dll_get_state (
    const dll_state_t * state,
    void * blob
) 
```




<hr>



### function dll\_init 

_Initialise a DLL in place (no allocation); BORROWS_ `code` _._
```C++
void dll_init (
    dll_state_t * s,
    const uint8_t * code,
    size_t code_len,
    size_t sps,
    double init_chip,
    double bn,
    double zeta,
    double spacing
) 
```



The by-value counterpart to [**dll\_create()**](dll__core_8h.md#function-dll_create): a tracking channel that embeds a [**dll\_state\_t**](structdll__state__t.md) initialises it here and retains ownership of `code` (it is not copied or freed). `code` must hold `code_len` chips for the loop's lifetime.




**Parameters:**


* `s` State to initialise. Must be non-NULL. 
* `code` Spreading code (0/1 chips), one period; borrowed. 
* `code_len` Code length (chips per period); must be &gt;= 1. 
* `sps` Samples per chip. 
* `init_chip` Seed code phase, chips. 
* `bn` Loop noise bandwidth, normalised to the code-period rate. 
* `zeta` Damping factor (0.707 = critically damped). 
* `spacing` Early/late tap offset, chips (0.5 = half-chip). 




        

<hr>



### function dll\_lock\_accumulate 

_Per-sample offset (noise) tap for the always-on lock detector._ 
```C++
JM_FORCEINLINE  JM_HOT void dll_lock_accumulate (
    dll_state_t * s,
    float _Complex d
) 
```



The composition sibling of [**dll\_accumulate()**](dll__core_8h.md#function-dll_accumulate): correlates the input against the code at this epoch's random off-peak offset, feeding the CFAR noise reference. Call it on the same sample stream as [**dll\_accumulate()**](dll__core_8h.md#function-dll_accumulate) and BEFORE it (both taps evaluate this sample's dwell-CENTER chip phase, [**dll\_dwell\_center\_chip\_pos**](dll__core_8h.md#function-dll_dwell_center_chip_pos) — [**dll\_accumulate()**](dll__core_8h.md#function-dll_accumulate) hasn't advanced the NCO yet, so the two calls see the same phase). A composer that skips this (and [**dll\_lock\_look()**](dll__core_8h.md#function-dll_lock_look)/dll\_lock\_epoch()) simply leaves the lock detector idle — locked stays 0, lock\_stat/noise\_est stay 0.




**Parameters:**


* `s` DLL state. Must be non-NULL. 
* `d` One carrier-wiped input sample (same sample as dll\_accumulate). 




        

<hr>



### function dll\_lock\_epoch 

_Per-epoch lock-detector housekeeping: re-draw the noise offset._ 
```C++
void dll_lock_epoch (
    dll_state_t * s
) 
```



Call once per code epoch (after the period's [**dll\_lock\_look()**](dll__core_8h.md#function-dll_lock_look)) so the next epoch's noise tap lands at a fresh random off-peak code phase.




**Parameters:**


* `s` DLL state. Must be non-NULL. 




        

<hr>



### function dll\_lock\_look 

_Fold one look into the lock detector; clear the offset tap._ 
```C++
void dll_lock_look (
    dll_state_t * s,
    double norm
) 
```



Normalises the prompt and offset accumulators by `norm` (the number of samples integrated into them — one full period for a full-epoch composer), folds the offset power into the CFAR noise reference and the prompt power into the running N-look sum, and — at every n\_looks-th look — forms the statistic R and steps the verify-counted lock detector. Call at each look boundary BEFORE zeroing the correlator accumulators (it reads acc\_p and acc\_o; acc\_o is cleared here). Out of line: per-look rate, never hot.




**Parameters:**


* `s` DLL state. Must be non-NULL. 
* `norm` Samples integrated into acc\_p/acc\_o this look (&gt; 0). 




        

<hr>



### function dll\_lookback\_segments 

_Derive a principled_ `segments` _count from a max tolerable async-lookback correlation-power loss, instead of hand-picking one._
```C++
size_t dll_lookback_segments (
    size_t tsamps,
    double max_error_db
) 
```



Ports the coupled-despreader prototype's `async_lookback_windows()` verbatim (itself ported from `~/legacy-commz`'s `asynchronous_correlation_loss`): derives an ideal segment size from `max_error_db`, then snaps to the nearest exact divisor of `tsamps` so `tsamps/segment_size` is always an integer count (ties broken toward the smaller/earlier divisor, matching `numpy.argmin`'s own first-occurrence convention). This project found hand-picked `segments`/`windows` values go stale silently (a `WINDOWS=62` constant was carried for a long time implying an unexamined ~0.07-0.1dB loss tolerance, ~5-7x tighter than this formula's own validated 0.5dB default) — this function is the canonical, derived replacement.




**Parameters:**


* `tsamps` One code period, in samples (`code_len * sps`). 
* `max_error_db` Maximum tolerable correlation-power loss from splitting the epoch into this many segments (dB, &gt; 0); 0.5 is this project's own validated default. 



**Returns:**

Segment count in `[1, tsamps]` that evenly divides `tsamps` (1 if `tsamps` == 0). 





        

<hr>



### function dll\_replica 

_Sub-chip code replica at fractional code phase_ `c` _(one tap)._
```C++
JM_FORCEINLINE float dll_replica (
    const dll_state_t * s,
    double c
) 
```



The code is treated as held at a fixed 2 samples/chip, sampled at chip-relative positions {0.25, 0.75} (a quarter and three-quarters into each chip) and linearly interpolated at any fractional position — NOT at {0, 0.5}, which would confine the whole linear-interpolation transition zone to one side of each chip boundary (found during the initial port: a real bug, not a cosmetic asymmetry — it broke E/L symmetry around P badly enough to leave a large discriminator offset at perfect lock). Quarter/three-quarter placement centers the transition zone symmetrically on each boundary (spanning `[chip-0.25, chip+0.25)`), and both quarter-chip flat regions are pure `sign(code[chip])` (no table is materialised — the 2x-oversampled index converts straight back to a chip index via `>> 1`, same as an unshifted grid would, only the phase origin moves). A point-sample interpolation, not a dwell-width-aware blend: this replaces the earlier exact matched-filter integral (which varied its blend width with the sample's chip-phase dwell time) with the simpler, validated design from `docs/design/async-dsss-receiver.md` §3.6 — the dwell-integral model was mathematically fancier but did not, on its own, fix the long-run false-lock that motivated this redesign; this one does.




**Parameters:**


* `s` DLL state (for the code and period length). 
* `c` Code phase of the tap, chips (any real value; wrapped mod sf). 



**Returns:**

Linearly-interpolated ±1 replica value for this tap and sample. 





        

<hr>



### function dll\_reset 

_Re-seed the loop to its create-time code phase; keep config._ 
```C++
void dll_reset (
    dll_state_t * state
) 
```



Restores the code phase, loop filter, correlator accumulators and lock detector to their post-construction state while preserving the tuned configuration (bn/zeta, spacing, segments, lock geometry). Re-running the same input after a reset therefore reproduces the same tracked state bit-for-bit — the basis of a deterministic replay.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.track import Dll
>>> rng = np.random.default_rng(21)
>>> code = rng.integers(0, 2, 63).astype(np.uint8)
>>> idx = (np.arange(63 * 4 * 300) * (1 + 3e-4) / 4).astype(
...     np.int64) % 63
>>> x = np.where(code[idx] & 1, -1.0, 1.0).astype(np.complex64)
>>> d = Dll(code, sps=4, bn=0.005)
>>> _ = d.steps(x)
>>> first = round(d.code_rate, 6)
>>> d.reset()                     # back to the create-time code phase
>>> _ = d.steps(x)                # same input -> same tracked rate
>>> round(d.code_rate, 6) == first
True
```
 




        

<hr>



### function dll\_set\_bn 

```C++
void dll_set_bn (
    dll_state_t * state,
    double val
) 
```




<hr>



### function dll\_set\_rate\_aid 

_Set the carrier-aiding code-rate deviation (ratio; 0 = off)._ 
```C++
void dll_set_rate_aid (
    dll_state_t * state,
    double rate_aid
) 
```



A fixed fractional rate bias summed into the sample-and-hold `phase_inc` on top of the loop's own control every epoch  for physically-coupled Doppler, `carrier_offset_hz / carrier_freq_hz`, so the code NCO rides the code-rate dilation the discriminator alone can't pull in at low SNR. Applied continuously across the epoch (via `phase_inc`), not as a phase pulse. Also nudges the current `phase_inc` so the aid takes effect before the first period update. `code_rate` stays the loop's own observable and is unaffected.




**Parameters:**


* `state` DLL state. Must be non-NULL. 
* `rate_aid` Fractional code-rate deviation (e.g. 8e-6). 0 disables. 
```C++
>>> import numpy as np
>>> from doppler.track import Dll
>>> rng = np.random.default_rng(11)
>>> code = rng.integers(0, 2, 63).astype(np.uint8)
>>> delta = 5e-4                                   # code-rate Doppler
>>> idx = (np.arange(63 * 4 * 300) * (1 + delta) / 4).astype(
...     np.int64) % 63
>>> x = np.where(code[idx] & 1, -1.0, 1.0).astype(np.complex64)
>>> plain = Dll(code, sps=4, bn=0.005)
>>> _ = plain.steps(x)
>>> round(plain.code_rate, 4)      # loop had to pull the whole Doppler
1.0005
>>> aided = Dll(code, sps=4, bn=0.005)
>>> aided.set_rate_aid(delta)      # feed the Doppler forward instead
>>> _ = aided.steps(x)
>>> round(aided.code_rate, 4)      # loop integrator stays at nominal
1.0
```
 




        

<hr>



### function dll\_set\_state 

```C++
int dll_set_state (
    dll_state_t * state,
    const void * blob
) 
```




<hr>



### function dll\_set\_telemetry 

_Attach (or detach) a telemetry context and register the code loop's probes on it. Registers four probes, emitted once per code epoch (period) and further thinned by decim: "&lt;prefix&gt;.e" (the early-minus-late envelope discriminator — the loop stress), "&lt;prefix&gt;.rate" (the tracked code rate, chips advanced per nominal chip, ~1.0 at lock), "&lt;prefix&gt;.lock" (the CFAR lock statistic R; compare against the configured threshold) and "&lt;prefix&gt;.locked" (the verify-counted lock decision, 0/1 — the lockdet output, so a consumer sees where the declare/drop rule fired without re-deriving it from the statistic). Passing NULL detaches. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in_ [_**dp\_tlm/dp\_tlm\_core.h**_](dp__tlm__core_8h.md) _)._
```C++
int dll_set_telemetry (
    dll_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "code" or "ch0.code". 
* `decim` Emit every decim-th epoch; &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take all four probes (the attach fails whole; the object stays detached). 
```C++
>>> import numpy as np
>>> from doppler.track import Dll
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> code = np.zeros(31, dtype=np.uint8)
>>> d = Dll(code=code, sps=2)
>>> d.set_telemetry(tlm, "code")
>>> sorted(tlm.probe_names)
['code.e', 'code.lock', 'code.locked', 'code.rate']
>>> x = np.ones(31 * 2 * 50, dtype=np.complex64)
>>> _ = d.steps(x)
>>> recs = tlm.read()   # four records per code epoch
>>> len(recs) > 0 and len(recs) % 4 == 0
True
```
 





        

<hr>



### function dll\_state\_bytes 

```C++
size_t dll_state_bytes (
    const dll_state_t * state
) 
```




<hr>



### function dll\_steps 

_Correlate a carrier-wiped block against the local code and steer the code NCO once per code period._ 
```C++
size_t dll_steps (
    dll_state_t * state,
    const float _Complex * x,
    size_t x_len,
    float _Complex * out,
    size_t max_out
) 
```



The Python face of the loop. Each code period the early/prompt/late correlators dump, the power-domain non-coherent early-minus-late discriminator runs, and the fixed-point code-phase NCO is re-steered; the prompt correlator value is emitted as one output symbol per period (or `segments` partial prompts per period when `segments > 1`). The loop is carrier-blind — it tracks with a residual carrier still on the input, so carrier recovery (Costas) and symbol-timing recovery are downstream stages fed from this output. Returned blocks are block-size invariant and safe to keep across calls (a block still referenced is never overwritten, jm gh-437).




**Parameters:**


* `state` DLL state. Must be non-NULL. 
* `x` Carrier-wiped input samples (one contiguous block). 
* `x_len` Number of input samples. 
* `out` Output buffer for the emitted prompt symbols. 
* `max_out` Capacity of `out` in elements; emission stops there. 



**Returns:**

Number of prompt symbols written — one per completed code period (`segments` per period when `segments > 1`) — up to `max_out`. 
```C++
>>> import numpy as np
>>> from doppler.track import Dll
>>> rng = np.random.default_rng(1)
>>> code = rng.integers(0, 2, 31).astype(np.uint8)
>>> chip = np.where(code & 1, -1.0, 1.0)    # BPSK spreading code
>>> x = np.tile(np.repeat(chip, 2), 40).astype(np.complex64)
>>> d = Dll(code=code, sps=2)
>>> sym = d.steps(x)                        # one prompt per period
>>> sym.dtype
dtype('complex64')
>>> round(float(np.mean(sym.real[-10:])), 1)  # despread to a clean +1
1.0
>>> round(d.code_rate, 3)                   # locked at nominal rate
1.0
```
 





        

<hr>



### function dll\_steps\_max\_out 

```C++
size_t dll_steps_max_out (
    dll_state_t * state
) 
```




<hr>



### function dll\_tlm\_flush 

_Emit the code loop's telemetry records for the epoch just closed._ 
```C++
void dll_tlm_flush (
    const dll_state_t * s
) 
```



Out-of-line on purpose: the emit machinery must not inline into the per-sample correlator loop (inlined ring-write expansions bloat the loop body and an extern call site forces per-iteration state reloads — both measured ~20% slower detached on other loops). Callers gate on `s->tlm.ctx` and call this once per code-epoch update. Records "&lt;prefix&gt;.e" (the E-L envelope discriminator — the loop stress), "&lt;prefix&gt;.rate" (the tracked code rate, chips per nominal chip), "&lt;prefix&gt;.lock" (the CFAR lock statistic R, refreshed every n\_looks looks) and "&lt;prefix&gt;.locked" (the verify-counted lockdet decision, 0/1 — plotted against .lock it shows exactly where the declare/drop rule fired). A composing tracking channel (the DSSS despreader) calls this from its own per-epoch update.




**Parameters:**


* `s` State with a non-NULL tlm.ctx (caller-checked). 




        

<hr>



### function dll\_update 

_Per-period code discriminator + loop update + NCO steer._ 
```C++
JM_FORCEINLINE  JM_HOT void dll_update (
    dll_state_t * s
) 
```



Runs the power-domain non-coherent early-minus-late discriminator `0.5 * (|E|^2 - |L|^2) / |P|^2` on the dumped accumulators (the prompt power is the normalizing "signal + noise power" reference — the validated design from `docs/design/async-dsss-receiver.md` §3.6, not a magnitude-domain `(|E|-|L|)/(|E|+|L|)` ratio), filters it, and steers `phase_inc` (sample-and-hold — held constant until the next call, exactly one epoch later) from BOTH the integrator (`code_rate`, a sustained rate) and the proportional term, spread smoothly across the whole next period rather than kicked directly into `phase`. Two things were tried and rejected while porting this design: (1) folding the loop filter's full combined control (`integ + kp*e`) into `phase_inc` as a single rate (mirroring `symsync_core.h`) massively over-corrects — a rate held for a whole `sf*sps`-sample period turns a `kp*e`-chip correction into a `kp*e*sf`-chip one, unstable at any non-tiny `bn`; (2) kicking `phase` directly once per period (mirroring `costas_core.c`) is unsafe right after a wrap (when `phase` is near zero) — a negative kick pushes phase backward across the just-crossed boundary, and the very next sample's forward step re-crosses it, registering a second, spurious wrap. Spreading the _same total_ `kp*e`-chip correction over the next period's `phase_inc` (not `phase` directly) reproduces the original double-accumulator design's `chip_pos += kp*e` exactly, without either failure mode. The period wrap itself is NOT handled here — it falls out of [**dll\_accumulate()**](dll__core_8h.md#function-dll_accumulate)'s own NCO advance (which wraps mod 2^32 on its own); call this at a period boundary ([**dll\_accumulate()**](dll__core_8h.md#function-dll_accumulate) returned 1) after reading the prompt, then the caller resets the accumulators. Inline.


The NCO free-runs at its own nominal rate (`1/tsamps` cycles/sample, set once in seed() and never touched directly); this function only ever computes a pure correction (`ctrl`, built entirely from the loop filter's integrator and proportional term  no "1.0"/nominal anywhere in it) and adds it on top when steering `phase_inc`. Keeping the control path free of the nominal rate is what lets a future second correction source (e.g. a carrier-aiding term) sum in cleanly, and is what `code_rate` (a _public_, ratio-form observable  1.0 = nominal, unrelated to how phase\_inc is actually steered) must never be substituted for internally.




**Parameters:**


* `s` DLL state. Must be non-NULL. 




        

<hr>
## Macro Definition Documentation





### define DLL\_DISC\_CLAMP 

```C++
#define DLL_DISC_CLAMP `1.0`
```




<hr>



### define DLL\_EPS 

```C++
#define DLL_EPS `1e-12`
```




<hr>



### define DLL\_STATE\_MAGIC 

```C++
#define DLL_STATE_MAGIC `DP_FOURCC ('D','L','L',' ')`
```




<hr>



### define DLL\_STATE\_VERSION 

```C++
#define DLL_STATE_VERSION `/* multi line expression */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dll/dll_core.h`

