

# File dsss\_receiver\_core.h



[**FileList**](files.md) **>** [**dsss\_receiver**](dir_39e39d42b234cb6483b3a80e996300fe.md) **>** [**dsss\_receiver\_core.h**](dsss__receiver__core_8h.md)

[Go to the source code of this file](dsss__receiver__core_8h_source.md)

_Composed continuous DSSS receiver: Acquisition -&gt; Costas(bn\_fll) pre-despread carrier wipeoff -&gt; Dll(segments) -&gt; RateConverter -&gt; MpskReceiver, one object._ [More...](#detailed-description)

* `#include "RateConverter/RateConverter_core.h"`
* `#include "acq/acq_core.h"`
* `#include "cic/cic_core.h"`
* `#include "dll/dll_core.h"`
* `#include "dp_state.h"`
* `#include "hbdecim/hbdecim_core.h"`
* `#include "mpsk_receiver/mpsk_receiver_core.h"`
* `#include "resamp/resamp_core.h"`
* `#include "resample/resample_core.h"`
* `#include <complex.h>`
* `#include <stddef.h>`
* `#include "costas/costas_core.h"`
* `#include "snr/snr_core.h"`
* `#include "ber/ber_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dsss\_receiver\_extra\_t**](structdsss__receiver__extra__t.md) <br> |
| struct | [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) <br>_Composed receiver state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dsss\_receiver\_configure\_chain\_raw**](#function-dsss_receiver_configure_chain_raw) ([**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state, size\_t segments, size\_t sps, int n) <br>_Pin the despread/resample/demod grid directly, bypassing the create-time_ `segments` _/_`sps` _defaults._ |
|  void | [**dsss\_receiver\_configure\_lock\_raw**](#function-dsss_receiver_configure_lock_raw) ([**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state, double up\_thresh, double down\_thresh, size\_t n\_looks, double alpha, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune the embedded Dll's code-lock detector directly. Forwards to_ `dll_configure_lock_raw()` _. Only meaningful once tracking has begun (_`dll` _is NULL before then); a no-op while searching. The detector is the hysteretic lockdet over the DLL's per-N-look CFAR statistic —_`up_thresh` _/_`down_thresh` _set the declare/drop levels and_`n_up` _/_`n_down` _the consecutive-look verify counts, trading declare latency against false-alarm rate._ |
|  int | [**dsss\_receiver\_configure\_search\_raw**](#function-dsss_receiver_configure_search_raw) ([**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state, size\_t doppler\_bins, size\_t n\_noncoh) <br>_Pin the embedded Acquisition's search grid directly. Forwards to_ `acq_configure_search_raw()` _— the escape hatch under this object's own_`symbol_rate` _-driven auto-sizing, for a power user who wants a specific_`(doppler_bins, n_noncoh)` _instead. Only meaningful while searching (a no-op has already happened once tracking has begun; the acquisition search doesn't run again until the next_`reset()` _)._ |
|  [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* | [**dsss\_receiver\_create**](#function-dsss_receiver_create) (const uint8\_t \* code, size\_t code\_len, double chip\_rate, double symbol\_rate, size\_t spc, int m, double cn0\_dbhz, double pfa, double pd, double doppler\_uncertainty, size\_t segments, size\_t sps, int differential) <br>_Create a DSSS receiver in the searching state._  |
|  void | [**dsss\_receiver\_destroy**](#function-dsss_receiver_destroy) ([**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state) <br>_Destroy a receiver and release all four children._  |
|  double | [**dsss\_receiver\_get\_chip\_phase**](#function-dsss_receiver_get_chip_phase) (const [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state) <br>_Dll's live tracked code phase (chips); 0.0 while searching._  |
|  double | [**dsss\_receiver\_get\_cn0\_dbhz\_est**](#function-dsss_receiver_get_cn0_dbhz_est) (const [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_receiver\_get\_code\_rate**](#function-dsss_receiver_get_code_rate) (const [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state) <br>_Dll's own tracking-quality indicator; 1.0 while searching._  |
|  double | [**dsss\_receiver\_get\_doppler\_hz**](#function-dsss_receiver_get_doppler_hz) (const [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_receiver\_get\_lock**](#function-dsss_receiver_get_lock) (const [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state) <br>_MpskReceiver's carrier lock EMA; 0.0 while searching._  |
|  int | [**dsss\_receiver\_get\_n**](#function-dsss_receiver_get_n) (const [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state) <br> |
|  double | [**dsss\_receiver\_get\_norm\_freq**](#function-dsss_receiver_get_norm_freq) (const [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state) <br>_MpskReceiver's tracked carrier frequency; 0.0 while searching._  |
|  size\_t | [**dsss\_receiver\_get\_segments**](#function-dsss_receiver_get_segments) (const [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state) <br> |
|  size\_t | [**dsss\_receiver\_get\_sps**](#function-dsss_receiver_get_sps) (const [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state) <br> |
|  void | [**dsss\_receiver\_get\_state**](#function-dsss_receiver_get_state) (const [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state, void \* blob) <br> |
|  int | [**dsss\_receiver\_get\_tracking**](#function-dsss_receiver_get_tracking) (const [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state) <br> |
|  void | [**dsss\_receiver\_reset**](#function-dsss_receiver_reset) ([**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state) <br>_Return to the searching state. Resets the embedded Acquisition and frees_ `dll` _/_`rc` _/_`rx` _(rebuilt from scratch on the next hit) — a receiver that has locked cannot be "reset back to tracking the same signal," only back to searching, matching every other object's reset() semantics in this codebase._ |
|  int | [**dsss\_receiver\_set\_state**](#function-dsss_receiver_set_state) ([**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**dsss\_receiver\_state\_bytes**](#function-dsss_receiver_state_bytes) (const [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state) <br> |
|  size\_t | [**dsss\_receiver\_steps**](#function-dsss_receiver_steps) ([**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br>_Stream raw cf32 samples; emit demodulated symbols once locked._  |
|  size\_t | [**dsss\_receiver\_steps\_max\_out**](#function-dsss_receiver_steps_max_out) ([**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DSSS\_RECEIVER\_STATE\_MAGIC**](dsss__receiver__core_8h.md#define-dsss_receiver_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('D', 'S', 'R', 'X')`<br> |
| define  | [**DSSS\_RECEIVER\_STATE\_VERSION**](dsss__receiver__core_8h.md#define-dsss_receiver_state_version)  `/* multi line expression */`<br> |
| define  | [**DSSS\_RX\_BN\_CARRIER**](dsss__receiver__core_8h.md#define-dsss_rx_bn_carrier)  `0.01`<br> |
| define  | [**DSSS\_RX\_BN\_FLL**](dsss__receiver__core_8h.md#define-dsss_rx_bn_fll)  `0.03`<br> |

## Detailed Description


The single-object form of the chain validated across this repo's "continuous async-DSSS receiver" story (`docs/gallery/dsss-acq-async-data.md`, `docs/gallery/dsss-despread-async-data.md`, `docs/gallery/dsss-receiver.md`): a continuous, non-bursty spreading code whose data-symbol clock need not be synchronous to the code-epoch clock. `steps()` streams raw samples through whichever child is currently active:



* **searching** (`tracking() == 0`): samples feed the embedded `Acquisition`. Nothing is emitted. On a hit, the carrier loop/ `Dll`/`RateConverter`/`MpskReceiver` are built from the hit's code phase and Doppler estimate (the exact `dll_init_chip_from_acq` phase-inversion and `RateConverter`-bridged sample-rate hand-off this repo's gallery pages validated by hand), and the **unconsumed tail** of the same `steps()` call is handed straight to them — no samples are dropped at the transition.
* **tracking** (`tracking() == 1`): samples are first derotated by a pre-despread carrier loop (`costas_wipeoff`/`costas_update`, one update per code period, `bn_fll`-assisted  removes BULK Doppler and its RATE OF CHANGE before the code loop ever sees it; a fixed/ bounded residual alone is fine downstream-only, per `docs/design/async-symbol-despreader.md` §4, but an unbounded Doppler RATE is not), one code period at a time (a small internal carry buffer holds any leftover partial-period tail across calls  `steps()` still accepts any block size), then feed `Dll -> RateConverter -> MpskReceiver` in sequence  the C-level equivalent of hand-composing those four objects (plus the new carrier stage)  and demodulated symbols are emitted. This is a NEW composition living entirely in this object  deliberately NOT a swap to the existing `Despreader` object (which fuses Costas+Dll per-sample), because `Despreader` embeds `Dll` via `dll_init()`, hardcoded to `segments==1`; it cannot carry this object's own `segments>1` async-lookback tracking. `dll_steps()` itself is called completely unmodified.




Per `[[feedback_despread_resample_demod_separation]]` (this story's own hard-won lesson): `segments` (the despreader's own tracking parameter) and `sps` (the demodulator's own sample-rate need) are independently configurable and bridged by an explicit `RateConverter`, never coupled to each other.



```C++
// "Just works": only the signal's own physical parameters are required.
dsss_receiver_state_t *rx = dsss_receiver_create(
    code, code_len, 3.0e6, 2100.0,   // chip_rate, symbol_rate
    2, 2,                            // spc, m (BPSK)
    55.0, 1e-3, 0.9, 100.0,          // cn0_dbhz, pfa, pd,
                                     // doppler_uncertainty
    4, 8,                            // segments, sps
    0);                              // differential
float complex syms[4096];
size_t n = dsss_receiver_steps(rx, x, x_len, syms, 4096);
dsss_receiver_destroy(rx);
```
 


    
## Public Functions Documentation




### function dsss\_receiver\_configure\_chain\_raw 

_Pin the despread/resample/demod grid directly, bypassing the create-time_ `segments` _/_`sps` _defaults._
```C++
int dsss_receiver_configure_chain_raw (
    dsss_receiver_state_t * state,
    size_t segments,
    size_t sps,
    int n
) 
```



The escape hatch for the one composition-specific knob this object adds beyond its children's own: `segments` (Dll's tracking parameter) and `sps`/`n` (MpskReceiver's sample-rate/carrier-arm parameters) are indepen­dently overridable here, still bridged by a freshly-sized `RateConverter` — never coupled to each other (see the module docstring). Rebuilds `dll`/`rc`/`rx` with every replacement allocated first, only freeing and adopting the old ones once every allocation has succeeded (mirrors `Acquisition`'s own `acq_regrid()` discipline) — a failed pin leaves the receiver tracking on its prior grid, not half-destroyed. Only meaningful once tracking (the grid defaults still apply to create-time auto-sizing for the next hit while searching; call `dsss_receiver_create()` with different `segments`/`sps` for that, or re-pin here again after the next hit).




**Parameters:**


* `state` The receiver. 
* `segments` Dll tracking segments per code period. 
* `sps` MpskReceiver samples per symbol (the resample target). 
* `n` MpskReceiver's carrier-arm count; must divide `sps`. 



**Returns:**

0 on success, -1 on invalid grid or an allocation failure (the receiver is left usable at its prior grid on failure). 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)
>>> rx.configure_chain_raw(segments=6, sps=8, n=8)  # re-pin the chain
>>> rx.segments                       # tracking grid updated in place
6
```
 





        

<hr>



### function dsss\_receiver\_configure\_lock\_raw 

_Re-tune the embedded Dll's code-lock detector directly. Forwards to_ `dll_configure_lock_raw()` _. Only meaningful once tracking has begun (_`dll` _is NULL before then); a no-op while searching. The detector is the hysteretic lockdet over the DLL's per-N-look CFAR statistic —_`up_thresh` _/_`down_thresh` _set the declare/drop levels and_`n_up` _/_`n_down` _the consecutive-look verify counts, trading declare latency against false-alarm rate._
```C++
void dsss_receiver_configure_lock_raw (
    dsss_receiver_state_t * state,
    double up_thresh,
    double down_thresh,
    size_t n_looks,
    double alpha,
    uint32_t n_up,
    uint32_t n_down
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `up_thresh` CFAR-statistic level to declare code lock (hit when the statistic exceeds it). 
* `down_thresh` Level below which a look is a miss; choose &lt;= `up_thresh` for level hysteresis. 
* `n_looks` Looks per decision — the DLL's non-coherent integration depth feeding one statistic. 
* `alpha` EMA smoothing coefficient on the lock statistic (0..1); smaller is smoother/slower. 
* `n_up` Consecutive hits required to declare lock. 
* `n_down` Consecutive misses required to drop lock. 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)
>>> rx.configure_lock_raw(up_thresh=0.4, down_thresh=0.2, n_looks=20,
...                       alpha=0.1, n_up=5, n_down=3)
>>> rx.tracking                # a no-op until a hit builds the Dll
0
```
 




        

<hr>



### function dsss\_receiver\_configure\_search\_raw 

_Pin the embedded Acquisition's search grid directly. Forwards to_ `acq_configure_search_raw()` _— the escape hatch under this object's own_`symbol_rate` _-driven auto-sizing, for a power user who wants a specific_`(doppler_bins, n_noncoh)` _instead. Only meaningful while searching (a no-op has already happened once tracking has begun; the acquisition search doesn't run again until the next_`reset()` _)._
```C++
int dsss_receiver_configure_search_raw (
    dsss_receiver_state_t * state,
    size_t doppler_bins,
    size_t n_noncoh
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `doppler_bins` Number of Doppler window tiles to search (&gt;= 1); capped by the create-time `doppler_uncertainty` span (one tile per code-epoch Doppler bin width). 
* `n_noncoh` Non-coherent looks accumulated per grid cell (1..256); more looks buys sensitivity at the cost of dwell, replacing the auto-sized count. 



**Returns:**

0 on success, -1 on invalid grid (see acq\_configure\_search\_raw). 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)
>>> rx.configure_search_raw(doppler_bins=1, n_noncoh=16)  # pin it
>>> rx.tracking                # still searching, on the pinned grid
0
```
 





        

<hr>



### function dsss\_receiver\_create 

_Create a DSSS receiver in the searching state._ 
```C++
dsss_receiver_state_t * dsss_receiver_create (
    const uint8_t * code,
    size_t code_len,
    double chip_rate,
    double symbol_rate,
    size_t spc,
    int m,
    double cn0_dbhz,
    double pfa,
    double pd,
    double doppler_uncertainty,
    size_t segments,
    size_t sps,
    int differential
) 
```



Only `code`/`chip_rate`/`symbol_rate` describe the signal itself — everything else is a physically-motivated default a caller can override, not a requirement. Internally: the embedded `Acquisition` is built via `acq_create_continuous()` (this receiver is inherently continuous/streaming)  always window-tiles, never coherently combines across epochs, sensitivity purely from an internally auto-sized non-coherent look count (see `acq_core.h`'s file doc comment); `Dll` always uses `bn=0.002` (this story's own validated stable loop bandwidth for a one-update-per-code-epoch geometry, not `dll_create()`'s own default of 0.01, which this story found unstable here) and `zeta=0.707`, `spacing=0.5`; `MpskReceiver` always uses `pulse=iandd`, `bn_carrier=bn_timing=0.01`, `zeta=0.707`, `acq_to_track=1`, `lock_thresh=0.3`, `warmup_syms=30` — this story's own validated values throughout. `lock_thresh=0.3` predates the lock statistic becoming a calibrated detector and is retained because it is validated here, but it now has a derivable reading: the carrier lock EMA's noise-only sd is 0.1132 at every M, so 0.3 is **2.65 noise sigmas**, a per-look Pfa of ~4e-3 — looser than `MpskReceiver`'s own 0.5 default (4.42 sigma, 5e-6) and still ~6 sigma clear of the +0.99 a locked BPSK constellation reads, which is why it holds. See `carrier_nda_core.h`. `n` (MpskReceiver's carrier-arm count) is derived from `sps`: the largest divisor of `sps` in `{4, 2, 1}`.




**Parameters:**


* `code` Spreading code, one 0/1 chip per element (0 -&gt; +1, 1 -&gt; -1 BPSK; only the low bit is used, so pass 0/1, not +/-1). 
* `code_len` Chips in `code` (the spreading factor). 
* `chip_rate` Chip rate, Hz. Required. 
* `symbol_rate` Data-symbol rate, Hz. Required — passed straight to the embedded Acquisition's own `symbol_rate` (diagnostic there; see `acq_create_continuous()`). 
* `spc` Samples/chip (front-end oversample); default 2 (fs = 2x chip\_rate). 
* `m` PSK order, 2/4/8; default 2 (BPSK). 
* `cn0_dbhz` Design C/N0 for acquisition sizing, dB-Hz; default 55.0. 
* `pfa` Acquisition false-alarm target; default 1e-3. 
* `pd` Acquisition detection-probability target; default 0.9. 
* `doppler_uncertainty` One-sided Doppler search half-range, Hz; default 100.0. 
* `segments` Dll's own non-coherent partial-correlation count per code epoch — its tracking- robustness parameter, independent of `sps` (see the module docstring); default 4, this story's own validated sweet spot. 
* `sps` MpskReceiver's samples/symbol, reached by an internal RateConverter bridging the despreader's own partial rate to this rate; default 8, MpskReceiver's own constructor default. 
* `differential` MpskReceiver's differential (rotation- invariant) demap; default 0 (coherent). 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssReceiver
>>> from doppler.wfm import Gold
>>> sf, chip, sym, spc = 1023, 3.0e6, 2100.0, 2
>>> fs, te, tsym = chip * spc, sf * spc, chip * spc / sym
>>> code = np.asarray(Gold().generate(sf)).astype(np.uint8)
>>> csign = np.where(code & 1, -1.0, 1.0)
>>> rng = np.random.default_rng(6)
>>> n = int(400 * tsym) + 2 * te            # 400 BPSK data symbols
>>> idx = np.arange(n)
>>> data = (rng.integers(0, 2, 404) * 2 - 1).astype(float)
>>> si = np.clip((idx / tsym).astype(int), 0, 403)
>>> spread = data[si] * csign[(idx // spc) % sf]        # DSSS chips
>>> sig = spread * np.exp(2j * np.pi * (50.0 / fs) * idx)  # +50 Hz
>>> pre = 3 * te                     # noise-only lead-in, pre-signal
>>> sigma = np.sqrt(fs / 10 ** (90.0 / 10))            # ~90 dB-Hz C/N0
>>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)
...          + 1j * rng.standard_normal(pre + n))
>>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)
...      + noise.astype(np.complex64))
>>> rx = DsssReceiver(code, chip_rate=chip, symbol_rate=sym, spc=spc,
...                   cn0_dbhz=55.0, doppler_uncertainty=100.0)
>>> syms = [rx.steps(x[p:p + te]) for p in range(0, len(x) - te, te)]
>>> syms = np.concatenate([s for s in syms if len(s)])
>>> rx.tracking                  # acquired, now demodulating
1
>>> len(syms) > 300              # a few hundred symbols recovered
True

Nearly all the energy lands on I, so the BPSK phase is resolved:

>>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))
True
```
 




        

<hr>



### function dsss\_receiver\_destroy 

_Destroy a receiver and release all four children._ 
```C++
void dsss_receiver_destroy (
    dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dsss\_receiver\_get\_chip\_phase 

_Dll's live tracked code phase (chips); 0.0 while searching._ 
```C++
double dsss_receiver_get_chip_phase (
    const dsss_receiver_state_t * state
) 
```




<hr>



### function dsss\_receiver\_get\_cn0\_dbhz\_est 

```C++
double dsss_receiver_get_cn0_dbhz_est (
    const dsss_receiver_state_t * state
) 
```




<hr>



### function dsss\_receiver\_get\_code\_rate 

_Dll's own tracking-quality indicator; 1.0 while searching._ 
```C++
double dsss_receiver_get_code_rate (
    const dsss_receiver_state_t * state
) 
```




<hr>



### function dsss\_receiver\_get\_doppler\_hz 

```C++
double dsss_receiver_get_doppler_hz (
    const dsss_receiver_state_t * state
) 
```




<hr>



### function dsss\_receiver\_get\_lock 

_MpskReceiver's carrier lock EMA; 0.0 while searching._ 
```C++
double dsss_receiver_get_lock (
    const dsss_receiver_state_t * state
) 
```




<hr>



### function dsss\_receiver\_get\_n 

```C++
int dsss_receiver_get_n (
    const dsss_receiver_state_t * state
) 
```




<hr>



### function dsss\_receiver\_get\_norm\_freq 

_MpskReceiver's tracked carrier frequency; 0.0 while searching._ 
```C++
double dsss_receiver_get_norm_freq (
    const dsss_receiver_state_t * state
) 
```




<hr>



### function dsss\_receiver\_get\_segments 

```C++
size_t dsss_receiver_get_segments (
    const dsss_receiver_state_t * state
) 
```




<hr>



### function dsss\_receiver\_get\_sps 

```C++
size_t dsss_receiver_get_sps (
    const dsss_receiver_state_t * state
) 
```




<hr>



### function dsss\_receiver\_get\_state 

```C++
void dsss_receiver_get_state (
    const dsss_receiver_state_t * state,
    void * blob
) 
```




<hr>



### function dsss\_receiver\_get\_tracking 

```C++
int dsss_receiver_get_tracking (
    const dsss_receiver_state_t * state
) 
```




<hr>



### function dsss\_receiver\_reset 

_Return to the searching state. Resets the embedded Acquisition and frees_ `dll` _/_`rc` _/_`rx` _(rebuilt from scratch on the next hit) — a receiver that has locked cannot be "reset back to tracking the same signal," only back to searching, matching every other object's reset() semantics in this codebase._
```C++
void dsss_receiver_reset (
    dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)
>>> rx.reset()                 # abort any lock, hunt from scratch
>>> (rx.tracking, rx.chip_phase)   # back to searching, all cleared
(0, 0.0)
```
 




        

<hr>



### function dsss\_receiver\_set\_state 

```C++
int dsss_receiver_set_state (
    dsss_receiver_state_t * state,
    const void * blob
) 
```




<hr>



### function dsss\_receiver\_state\_bytes 

```C++
size_t dsss_receiver_state_bytes (
    const dsss_receiver_state_t * state
) 
```




<hr>



### function dsss\_receiver\_steps 

_Stream raw cf32 samples; emit demodulated symbols once locked._ 
```C++
size_t dsss_receiver_steps (
    dsss_receiver_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```



While searching, samples feed the embedded Acquisition and nothing is emitted (0 return is normal, not an error). The moment a hit fires, `Dll`/`RateConverter`/`MpskReceiver` are built and seeded from it, and the unconsumed tail of THIS call — computed exactly from `acq->samples_consumed`, no samples dropped or double-fed — is handed straight to them in the same call. While tracking, samples feed `Dll -> RateConverter -> MpskReceiver` in sequence. Accepts any block size; state carries across calls (`Acquisition`/`Dll`/ `RateConverter`/`MpskReceiver` are all already block-size invariant, so this object needs no ring-buffering of its own).




**Parameters:**


* `state` Must be non-NULL. 
* `x` Input cf32 samples. 
* `x_len` Number of input samples. 
* `out` Output symbols; caller provides `max_out` capacity. 
* `max_out` Output capacity. 



**Returns:**

Number of symbols written (0 while searching, or while tracking with not yet a full symbol's worth of input). 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssReceiver
>>> from doppler.wfm import Gold
>>> sf, chip, sym, spc = 1023, 3.0e6, 2100.0, 2
>>> fs, te, tsym = chip * spc, sf * spc, chip * spc / sym
>>> code = np.asarray(Gold().generate(sf)).astype(np.uint8)
>>> csign = np.where(code & 1, -1.0, 1.0)
>>> rng = np.random.default_rng(6)
>>> n = int(400 * tsym) + 2 * te            # 400 BPSK data symbols
>>> idx = np.arange(n)
>>> data = (rng.integers(0, 2, 404) * 2 - 1).astype(float)
>>> si = np.clip((idx / tsym).astype(int), 0, 403)
>>> spread = data[si] * csign[(idx // spc) % sf]        # DSSS chips
>>> sig = spread * np.exp(2j * np.pi * (50.0 / fs) * idx)  # +50 Hz
>>> pre = 3 * te                     # noise-only lead-in, pre-signal
>>> sigma = np.sqrt(fs / 10 ** (90.0 / 10))            # ~90 dB-Hz C/N0
>>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)
...          + 1j * rng.standard_normal(pre + n))
>>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)
...      + noise.astype(np.complex64))
>>> rx = DsssReceiver(code, chip_rate=chip, symbol_rate=sym, spc=spc,
...                   cn0_dbhz=55.0, doppler_uncertainty=100.0)
>>> syms = [rx.steps(x[p:p + te]) for p in range(0, len(x) - te, te)]
>>> syms = np.concatenate([s for s in syms if len(s)])
>>> rx.tracking                  # acquired and now demodulating
1
>>> len(syms) > 300              # a few hundred symbols recovered
True

Nearly all the energy lands on I, so the BPSK phase is resolved:

>>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))
True
```
 





        

<hr>



### function dsss\_receiver\_steps\_max\_out 

```C++
size_t dsss_receiver_steps_max_out (
    dsss_receiver_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define DSSS\_RECEIVER\_STATE\_MAGIC 

```C++
#define DSSS_RECEIVER_STATE_MAGIC `DP_FOURCC ('D', 'S', 'R', 'X')`
```




<hr>



### define DSSS\_RECEIVER\_STATE\_VERSION 

```C++
#define DSSS_RECEIVER_STATE_VERSION `2u /* v2: pre-despread costas_state_t car + carry buffer added */`
```




<hr>



### define DSSS\_RX\_BN\_CARRIER 

```C++
#define DSSS_RX_BN_CARRIER `0.01`
```




<hr>



### define DSSS\_RX\_BN\_FLL 

```C++
#define DSSS_RX_BN_FLL `0.03`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dsss_receiver/dsss_receiver_core.h`

