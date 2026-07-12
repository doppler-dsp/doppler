

# File dll\_core.h



[**FileList**](files.md) **>** [**dll**](dir_f3da3e2048ea3a8b9e723d3c5367d8f8.md) **>** [**dll\_core.h**](dll__core_8h.md)

[Go to the source code of this file](dll__core_8h_source.md)

_Delay-lock loop (DLL) — non-coherent early/prompt/late code tracking._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "lockdet/lockdet_core.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include "telemetry/telemetry.h"`
* `#include <complex.h>`
* `#include "detection/detection_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dll\_state\_t**](structdll__state__t.md) <br>_DLL state._  |
| struct | [**dll\_tlm\_t**](structdll__tlm__t.md) <br>_Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then a single predicted-not-taken branch per code epoch. Zeroed in state blobs and preserved across set\_state (the hand-written triplet treats it like the borrowed_ `code` _)._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**dll\_accumulate**](#function-dll_accumulate) ([**dll\_state\_t**](structdll__state__t.md) \* s, float complex d) <br>_Per-sample early/prompt/late correlate + code-phase advance._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float | [**dll\_chip\_sign**](#function-dll_chip_sign) (uint8\_t c) <br> |
|  void | [**dll\_configure**](#function-dll_configure) ([**dll\_state\_t**](structdll__state__t.md) \* state, double bn, double zeta) <br> |
|  int | [**dll\_configure\_lock**](#function-dll_configure_lock) ([**dll\_state\_t**](structdll__state__t.md) \* state, double pfa, size\_t n\_looks, double ref\_snr\_db) <br>_Tune the always-on code-lock detector to a target (pfa, n\_looks)._  |
|  void | [**dll\_configure\_lock\_raw**](#function-dll_configure_lock_raw) ([**dll\_state\_t**](structdll__state__t.md) \* state, double up\_thresh, double down\_thresh, size\_t n\_looks, double alpha, uint32\_t n\_up, uint32\_t n\_down) <br>_Set the lock detector's raw geometry directly._  |
|  [**dll\_state\_t**](structdll__state__t.md) \* | [**dll\_create**](#function-dll_create) (const uint8\_t \* code, size\_t code\_len, size\_t sps, double init\_chip, double bn, double zeta, double spacing, size\_t segments) <br>_Create a DLL instance (COPIES_ `code` _)._ |
|  void | [**dll\_destroy**](#function-dll_destroy) ([**dll\_state\_t**](structdll__state__t.md) \* state) <br>_Destroy a DLL instance and release all memory (incl. the code copy)._  |
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
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**dll\_lock\_accumulate**](#function-dll_lock_accumulate) ([**dll\_state\_t**](structdll__state__t.md) \* s, float complex d) <br>_Per-sample offset (noise) tap for the always-on lock detector._  |
|  void | [**dll\_lock\_epoch**](#function-dll_lock_epoch) ([**dll\_state\_t**](structdll__state__t.md) \* s) <br>_Per-epoch lock-detector housekeeping: re-draw the noise offset._  |
|  void | [**dll\_lock\_look**](#function-dll_lock_look) ([**dll\_state\_t**](structdll__state__t.md) \* s, double norm) <br>_Fold one look into the lock detector; clear the offset tap._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float | [**dll\_replica**](#function-dll_replica) (const [**dll\_state\_t**](structdll__state__t.md) \* s, double c, double adv) <br>_Sub-chip code replica at fractional code phase_ `c` _(one tap)._ |
|  void | [**dll\_reset**](#function-dll_reset) ([**dll\_state\_t**](structdll__state__t.md) \* state) <br>_Re-seed the loop to its create-time code phase; keep config._  |
|  void | [**dll\_set\_bn**](#function-dll_set_bn) ([**dll\_state\_t**](structdll__state__t.md) \* state, double val) <br> |
|  int | [**dll\_set\_state**](#function-dll_set_state) ([**dll\_state\_t**](structdll__state__t.md) \* state, const void \* blob) <br> |
|  int | [**dll\_set\_telemetry**](#function-dll_set_telemetry) ([**dll\_state\_t**](structdll__state__t.md) \* state, [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context and register the code loop's probes on it. Registers four probes, emitted once per code epoch (period) and further thinned by decim: "&lt;prefix&gt;.e" (the early-minus-late envelope discriminator — the loop stress), "&lt;prefix&gt;.rate" (the tracked code rate, chips advanced per nominal chip, ~1.0 at lock), "&lt;prefix&gt;.lock" (the CFAR lock statistic R; compare against the configured threshold) and "&lt;prefix&gt;.locked" (the verify-counted lock decision, 0/1 — the lockdet output, so a consumer sees where the declare/drop rule fired without re-deriving it from the statistic). Passing NULL detaches. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in_ [_**telemetry/telemetry.h**_](telemetry_8h.md) _)._ |
|  size\_t | [**dll\_state\_bytes**](#function-dll_state_bytes) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  size\_t | [**dll\_steps**](#function-dll_steps) ([**dll\_state\_t**](structdll__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br> |
|  size\_t | [**dll\_steps\_max\_out**](#function-dll_steps_max_out) ([**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  void | [**dll\_tlm\_flush**](#function-dll_tlm_flush) (const [**dll\_state\_t**](structdll__state__t.md) \* s) <br>_Emit the code loop's telemetry records for the epoch just closed._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**dll\_update**](#function-dll_update) ([**dll\_state\_t**](structdll__state__t.md) \* s) <br>_Per-period code discriminator + loop update + phase wrap._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DLL\_EPS**](dll__core_8h.md#define-dll_eps)  `1e-12`<br> |
| define  | [**DLL\_STATE\_MAGIC**](dll__core_8h.md#define-dll_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('D','L','L',' ')`<br> |
| define  | [**DLL\_STATE\_VERSION**](dll__core_8h.md#define-dll_state_version)  `3u /\* v3: lockdet decision rule (verify counters) \*/`<br> |

## Detailed Description


Tracks the code phase of a continuous, repeating spreading code (e.g. a PN / Gold sequence) on a _carrier-wiped_ sample stream. Per sample it correlates the input against three taps of the local code — early (`+spacing` chips), prompt, late (`-spacing` chips) — accumulating an integrate-and-dump over one code period; per period it runs the non-coherent envelope discriminator `(|E| - |L|) / (|E| + |L|)`, filters it through an embedded 2nd-order [**loop\_filter\_state\_t**](structloop__filter__state__t.md), and steers the code rate + phase.


It pairs with the carrier loop ([**costas\_core.h**](costas__core_8h.md)): the carrier loop wipes the carrier, the DLL wipes the code. The block API (dll\_steps) is the Python face; the JM\_FORCEINLINE [**dll\_accumulate()**](dll__core_8h.md#function-dll_accumulate)/dll\_update() are the C composition API a tracking channel inlines into its own sample loop.


Lifecycle: `dll_create -> (steps / configure / reset)* -> dll_destroy`, or embed by value with [**dll\_init()**](dll__core_8h.md#function-dll_init) (which BORROWS the caller-owned code).



```C++
uint8_t code[31] = { ... };  // 0/1 chips, one period
dll_state_t *d = dll_create(code, 31, 2, 0.0, 0.01, 0.707, 0.5);
float complex sym[16];
size_t k = dll_steps(d, rx, rx_len, sym, 16);  // one prompt per period
double phase = d->chip_pos;                     // tracked code phase, chips
dll_destroy(d);
```
 


    
## Public Functions Documentation




### function dll\_accumulate 

_Per-sample early/prompt/late correlate + code-phase advance._ 
```C++
JM_FORCEINLINE  JM_HOT void dll_accumulate (
    dll_state_t * s,
    float complex d
) 
```



Correlates the carrier-wiped sample `d` against the early, prompt and late code taps (wrapped over the periodic code) and advances the code phase by `code_rate / sps` chips. Inline, zero call overhead.




**Parameters:**


* `s` DLL state. Must be non-NULL. 
* `d` One carrier-wiped input sample. 




        

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

```C++
void dll_configure (
    dll_state_t * state,
    double bn,
    double zeta
) 
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




        

<hr>



### function dll\_create 

_Create a DLL instance (COPIES_ `code` _)._
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
    float complex d
) 
```



The composition sibling of [**dll\_accumulate()**](dll__core_8h.md#function-dll_accumulate): correlates the input against the code at this epoch's random off-peak offset, feeding the CFAR noise reference. Call it on the same sample stream as [**dll\_accumulate()**](dll__core_8h.md#function-dll_accumulate) and BEFORE it (both taps evaluate the pre-advance chip phase). A composer that skips this (and [**dll\_lock\_look()**](dll__core_8h.md#function-dll_lock_look)/dll\_lock\_epoch()) simply leaves the lock detector idle — locked stays 0, lock\_stat/noise\_est stay 0.




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



### function dll\_replica 

_Sub-chip code replica at fractional code phase_ `c` _(one tap)._
```C++
JM_FORCEINLINE float dll_replica (
    const dll_state_t * s,
    double c,
    double adv
) 
```



Evaluates the ±1 code at chip phase `c` over the chip-phase extent `[c, c + adv)` swept by one input sample (`adv = code_rate / sps`). Away from a chip transition this is just `sign(code[floor(c)])`. When the sample's extent straddles the boundary into the next chip, it returns the overlap-weighted blend of the two chip signs — `frac` of the sample lies in chip `floor(c)`, `1 - frac` in the next chip. Because the chips are ±1 and constant away from a transition, this is the _exact_ matched-filter integral over a window whose edge falls at a fractional sample position: it makes the correlation (hence the E-L discriminator) vary continuously with sub-sample code phase instead of stepping in integer-sample quanta, with no loss of despread SNR (the chip interior is still fully integrated). The blend is the linear (trapezoidal) interpolant of the correlation; for ±1 chips no signal interpolation is needed — only the lone straddling sample is reweighted.




**Parameters:**


* `s` DLL state (for the code and period length). 
* `c` Code phase of the tap, chips, in [0, sf). 
* `adv` Chip-phase advance per input sample (`code_rate / sps`). 



**Returns:**

Blended ±1 replica value for this tap and sample. 





        

<hr>



### function dll\_reset 

_Re-seed the loop to its create-time code phase; keep config._ 
```C++
void dll_reset (
    dll_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function dll\_set\_bn 

```C++
void dll_set_bn (
    dll_state_t * state,
    double val
) 
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

_Attach (or detach) a telemetry context and register the code loop's probes on it. Registers four probes, emitted once per code epoch (period) and further thinned by decim: "&lt;prefix&gt;.e" (the early-minus-late envelope discriminator — the loop stress), "&lt;prefix&gt;.rate" (the tracked code rate, chips advanced per nominal chip, ~1.0 at lock), "&lt;prefix&gt;.lock" (the CFAR lock statistic R; compare against the configured threshold) and "&lt;prefix&gt;.locked" (the verify-counted lock decision, 0/1 — the lockdet output, so a consumer sees where the declare/drop rule fired without re-deriving it from the statistic). Passing NULL detaches. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in_ [_**telemetry/telemetry.h**_](telemetry_8h.md) _)._
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
>>> sorted(tlm.probe_names())
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

```C++
size_t dll_steps (
    dll_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
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

_Per-period code discriminator + loop update + phase wrap._ 
```C++
JM_FORCEINLINE  JM_HOT void dll_update (
    dll_state_t * s
) 
```



Runs the non-coherent early-minus-late envelope discriminator on the dumped accumulators, filters it, updates the code rate, and wraps the prompt phase to the next period (plus a proportional phase nudge). Call at a period boundary after reading the prompt; the caller resets the accumulators. Inline.




**Parameters:**


* `s` DLL state. Must be non-NULL. 




        

<hr>
## Macro Definition Documentation





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
#define DLL_STATE_VERSION `3u /* v3: lockdet decision rule (verify counters) */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dll/dll_core.h`

