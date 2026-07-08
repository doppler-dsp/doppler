

# File dll\_core.h



[**FileList**](files.md) **>** [**dll**](dir_f3da3e2048ea3a8b9e723d3c5367d8f8.md) **>** [**dll\_core.h**](dll__core_8h.md)

[Go to the source code of this file](dll__core_8h_source.md)

_Delay-lock loop (DLL) — non-coherent early/prompt/late code tracking._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include <complex.h>`
* `#include "detection/detection_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dll\_state\_t**](structdll__state__t.md) <br>_DLL state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**dll\_accumulate**](#function-dll_accumulate) ([**dll\_state\_t**](structdll__state__t.md) \* s, float complex d) <br>_Per-sample early/prompt/late correlate + code-phase advance._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float | [**dll\_chip\_sign**](#function-dll_chip_sign) (uint8\_t c) <br> |
|  void | [**dll\_configure**](#function-dll_configure) ([**dll\_state\_t**](structdll__state__t.md) \* state, double bn, double zeta) <br> |
|  void | [**dll\_configure\_lock**](#function-dll_configure_lock) ([**dll\_state\_t**](structdll__state__t.md) \* state, double threshold, size\_t n\_looks, double alpha) <br>_Configure the always-on code-lock detector._  |
|  [**dll\_state\_t**](structdll__state__t.md) \* | [**dll\_create**](#function-dll_create) (const uint8\_t \* code, size\_t code\_len, size\_t sps, double init\_chip, double bn, double zeta, double spacing, size\_t segments) <br>_Create a DLL instance (COPIES_ `code` _)._ |
|  void | [**dll\_destroy**](#function-dll_destroy) ([**dll\_state\_t**](structdll__state__t.md) \* state) <br>_Destroy a DLL instance and release all memory (incl. the code copy)._  |
|  double | [**dll\_get\_bn**](#function-dll_get_bn) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  double | [**dll\_get\_code\_phase**](#function-dll_get_code_phase) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  double | [**dll\_get\_code\_rate**](#function-dll_get_code_rate) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  double | [**dll\_get\_last\_error**](#function-dll_get_last_error) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  double | [**dll\_get\_lock\_stat**](#function-dll_get_lock_stat) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br>_Last lock test statistic R (compare against the configured eta)._  |
|  int | [**dll\_get\_locked**](#function-dll_get_locked) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br>_Last lock decision (1 = locked, 0 = not)._  |
|  double | [**dll\_get\_noise\_est**](#function-dll_get_noise_est) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br>_Current CFAR noise-power estimate E\|O\|^2 (offset-tap EMA)._  |
|  size\_t | [**dll\_get\_segments**](#function-dll_get_segments) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  void | [**dll\_get\_state**](#function-dll_get_state) (const [**dll\_state\_t**](structdll__state__t.md) \* state, void \* blob) <br> |
|  void | [**dll\_init**](#function-dll_init) ([**dll\_state\_t**](structdll__state__t.md) \* s, const uint8\_t \* code, size\_t code\_len, size\_t sps, double init\_chip, double bn, double zeta, double spacing) <br>_Initialise a DLL in place (no allocation); BORROWS_ `code` _._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float | [**dll\_replica**](#function-dll_replica) (const [**dll\_state\_t**](structdll__state__t.md) \* s, double c, double adv) <br>_Sub-chip code replica at fractional code phase_ `c` _(one tap)._ |
|  void | [**dll\_reset**](#function-dll_reset) ([**dll\_state\_t**](structdll__state__t.md) \* state) <br>_Re-seed the loop to its create-time code phase; keep config._  |
|  void | [**dll\_set\_bn**](#function-dll_set_bn) ([**dll\_state\_t**](structdll__state__t.md) \* state, double val) <br> |
|  int | [**dll\_set\_state**](#function-dll_set_state) ([**dll\_state\_t**](structdll__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**dll\_state\_bytes**](#function-dll_state_bytes) (const [**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  size\_t | [**dll\_steps**](#function-dll_steps) ([**dll\_state\_t**](structdll__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br> |
|  size\_t | [**dll\_steps\_max\_out**](#function-dll_steps_max_out) ([**dll\_state\_t**](structdll__state__t.md) \* state) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**dll\_update**](#function-dll_update) ([**dll\_state\_t**](structdll__state__t.md) \* s) <br>_Per-period code discriminator + loop update + phase wrap._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DLL\_EPS**](dll__core_8h.md#define-dll_eps)  `1e-12`<br> |
| define  | [**DLL\_STATE\_MAGIC**](dll__core_8h.md#define-dll_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('D','L','L',' ')`<br> |
| define  | [**DLL\_STATE\_VERSION**](dll__core_8h.md#define-dll_state_version)  `1u`<br> |

## Detailed Description


Tracks the code phase of a continuous, repeating spreading code (e.g. a PN / Gold sequence) on a _carrier-wiped_ sample stream. Per sample it correlates the input against three taps of the local code — early (`+spacing` chips), prompt, late (`-spacing` chips) — accumulating an integrate-and-dump over one code period; per period it runs the non-coherent envelope discriminator `(|E| - |L|) / (|E| + |L|)`, filters it through an embedded 2nd-order [**loop\_filter\_state\_t**](structloop__filter__state__t.md), and steers the code rate + phase.


It pairs with the carrier loop ([**costas\_core.h**](costas__core_8h.md)): the carrier loop wipes the carrier, the DLL wipes the code. The block API (dll\_steps) is the Python face; the JM\_FORCEINLINE [**dll\_accumulate()**](dll__core_8h.md#function-dll_accumulate)/dll\_update() are the C composition API a tracking channel inlines into its own sample loop.


Lifecycle: dll\_create -&gt; [steps / configure / reset]\* -&gt; dll\_destroy, or embed by value with [**dll\_init()**](dll__core_8h.md#function-dll_init) (which BORROWS the caller-owned code).



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

_Configure the always-on code-lock detector._ 
```C++
void dll_configure_lock (
    dll_state_t * state,
    double threshold,
    size_t n_looks,
    double alpha
) 
```



The DLL carries a lock detector that reuses acquisition's non-coherent test statistic. Every emitted look (a partial in segments mode, or the full-epoch prompt when segments == 1) is also correlated at a _random off-peak_ code phase — re-drawn each epoch and kept `noise_guard` chips clear of the prompt/early/late lobe — to give a signal-free CFAR noise sample (valid for a low-sidelobe code, e.g. Gold). The offset power feeds an EMA reference `E|O|^2`; the prompt powers of `n_looks` consecutive looks are summed into `S = sum|P_k|^2`, and the detector declares lock when


R = sqrt(2 \* S / E\|O\|^2) &gt; `threshold` 


which under H0 has `P(R > threshold) = marcum_q(n_looks, 0, threshold)` — so a caller sizes `threshold` = det\_threshold\_noncoherent(pfa, n\_looks) and `n_looks` = det\_n\_noncoh(snr, ...) to meet a target (Pfa, Pd). The threshold is passed in (not derived) so the core stays dependency-free; the Python binding converts a `pfa` via the detection module. The EMA must average many more cells than the test integrates (`1/alpha >> n_looks`) or the noise estimate's own variance inflates Pfa; the binding defaults `1/alpha` to `max(1024, 32*n_looks)`.




**Parameters:**


* `state` DLL state. Must be non-NULL. 
* `threshold` CFAR threshold eta on the statistic R. 
* `n_looks` Non-coherent integration depth N (looks); clamped to &gt;= 1. 
* `alpha` EMA coefficient for the noise reference, in (0, 1]. 




        

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

_Last lock decision (1 = locked, 0 = not)._ 
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
#define DLL_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dll/dll_core.h`

