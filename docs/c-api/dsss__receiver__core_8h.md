

# File dsss\_receiver\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dsss\_receiver**](dir_6f60e61f873068f58aa42ec05bfb1a20.md) **>** [**dsss\_receiver\_core.h**](dsss__receiver__core_8h.md)

[Go to the source code of this file](dsss__receiver__core_8h_source.md)

_Composed continuous DSSS receiver: Acquisition -&gt; Costas(bn\_fll) pre-despread carrier wipeoff -&gt; Dll(segments) -&gt; RateConverter -&gt; MpskReceiver, one object._ [More...](#detailed-description)

* `#include "doppler/RateConverter/RateConverter_core.h"`
* `#include "doppler/acq/acq_core.h"`
* `#include "doppler/cic/cic_core.h"`
* `#include "doppler/dll/dll_core.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/hbdecim/hbdecim_core.h"`
* `#include "doppler/mpsk_receiver/mpsk_receiver_core.h"`
* `#include "doppler/resamp/resamp_core.h"`
* `#include "doppler/resample/resample_core.h"`
* `#include "doppler/dp_complex.h"`
* `#include <stddef.h>`
* `#include "doppler/costas/costas_core.h"`
* `#include "doppler/snr/snr_core.h"`
* `#include "doppler/ber/ber_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) <br>_Composed receiver state._  |
| struct | [**dsss\_receiver\_extra\_t**](structdsss__receiver__extra__t.md) <br>_Receiver-level payload that follows the state envelope: the routing flag, cached hit estimates, the grid (layout key) and the carrier-carry depth._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_dsss\_receiver\_configure\_chain\_raw**](#function-dp_dsss_receiver_configure_chain_raw) ([**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state, size\_t segments, size\_t sps, int n) <br>_Pin the despread/resample/demod grid directly, bypassing the create-time_ `segments` _/_`sps` _defaults._ |
|  void | [**dp\_dsss\_receiver\_configure\_lock\_raw**](#function-dp_dsss_receiver_configure_lock_raw) ([**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state, double up\_thresh, double down\_thresh, size\_t n\_looks, double alpha, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune the embedded Dll's code-lock detector directly. Forwards to_ `dp_dll_configure_lock_raw()` _, which also clears the in-flight lock statistic and drops lock. Only meaningful once tracking has begun: while searching it tunes the placeholder Dll, which the next hit replaces, so the setting is lost (as it is on reset() and configure\_chain\_raw()). The detector is the hysteretic lockdet over the DLL's per-N-look CFAR statistic —_`up_thresh` _/_`down_thresh` _set the declare/drop levels and_`n_up` _/_`n_down` _the consecutive-look verify counts, trading declare latency against false-alarm rate._ |
|  int | [**dp\_dsss\_receiver\_configure\_search\_raw**](#function-dp_dsss_receiver_configure_search_raw) ([**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state, size\_t doppler\_bins, size\_t n\_noncoh) <br>_Pin the embedded Acquisition's search grid directly. Forwards to_ `dp_acq_configure_search_raw()` _— the escape hatch under this object's own_`symbol_rate` _-driven auto-sizing, for a power user who wants a specific_`(doppler_bins, n_noncoh)` _instead. Pinning exits the continuous engine's wideband window-tiling mode, re-derives its thresholds and resets it, discarding any search in flight. While tracking the engine is idle, so the pinned grid governs the search that resumes after the next_`reset()` _._ |
|  [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* | [**dp\_dsss\_receiver\_create**](#function-dp_dsss_receiver_create) (const uint8\_t \* code, size\_t code\_len, double chip\_rate, double symbol\_rate, double sample\_rate, int m, double cn0\_dbhz, double pfa, double pd, double doppler\_uncertainty, size\_t segments, size\_t sps, int differential) <br>_Create a DSSS receiver in the searching state._  |
|  void | [**dp\_dsss\_receiver\_destroy**](#function-dp_dsss_receiver_destroy) ([**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state) <br>_Destroy a receiver and release all four children._  |
|  double | [**dp\_dsss\_receiver\_get\_chip\_phase**](#function-dp_dsss_receiver_get_chip_phase) (const [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state) <br>_Dll's live tracked code phase (chips); 0.0 while searching._  |
|  double | [**dp\_dsss\_receiver\_get\_cn0\_dbhz\_est**](#function-dp_dsss_receiver_get_cn0_dbhz_est) (const [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state) <br>_C/N0 estimate from the winning acquisition hit._  |
|  double | [**dp\_dsss\_receiver\_get\_code\_rate**](#function-dp_dsss_receiver_get_code_rate) (const [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state) <br>_Dll's tracked code rate; 1.0 while searching._  |
|  double | [**dp\_dsss\_receiver\_get\_doppler\_hz**](#function-dp_dsss_receiver_get_doppler_hz) (const [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state) <br>_Doppler estimate from the winning acquisition hit._  |
|  double | [**dp\_dsss\_receiver\_get\_lock**](#function-dp_dsss_receiver_get_lock) (const [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state) <br>_MpskReceiver's carrier lock EMA; 0.0 while searching._  |
|  int | [**dp\_dsss\_receiver\_get\_n**](#function-dp_dsss_receiver_get_n) (const [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state) <br>_MpskReceiver terminal outputs per symbol (_ `m_out` _) in use._ |
|  double | [**dp\_dsss\_receiver\_get\_norm\_freq**](#function-dp_dsss_receiver_get_norm_freq) (const [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state) <br>_MpskReceiver's tracked carrier frequency; 0.0 while searching._  |
|  size\_t | [**dp\_dsss\_receiver\_get\_segments**](#function-dp_dsss_receiver_get_segments) (const [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state) <br>_Dll partial correlations per code period currently in use._  |
|  size\_t | [**dp\_dsss\_receiver\_get\_sps**](#function-dp_dsss_receiver_get_sps) (const [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state) <br>_MpskReceiver samples per symbol currently in use._  |
|  void | [**dp\_dsss\_receiver\_get\_state**](#function-dp_dsss_receiver_get_state) (const [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state, void \* blob) <br>_Serialize the whole receiver (searching or tracking) into_ `blob` _._ |
|  int | [**dp\_dsss\_receiver\_get\_tracking**](#function-dp_dsss_receiver_get_tracking) (const [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state) <br>_Routing state: searching or tracking._  |
|  void | [**dp\_dsss\_receiver\_reset**](#function-dp_dsss_receiver_reset) ([**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state) <br>_Return to the searching state. Resets the embedded Acquisition and rebuilds_ `dll` _/_`rc` _/_`rx` _and the carrier loop at the placeholder seed (phase 0, no Doppler) on the current grid (a configure\_chain\_raw() pin survives), clearing the carrier carry and the cached Doppler/C/N0 estimates. Any configure\_lock\_raw() tuning is lost with the old Dll; a grid pinned by configure\_search\_raw() is kept. A receiver that has locked cannot be "reset back to tracking the same signal," only back to searching, matching every other object's reset() semantics in this codebase._ |
|  int | [**dp\_dsss\_receiver\_set\_state**](#function-dp_dsss_receiver_set_state) ([**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state, const void \* blob) <br>_Restore a blob from_ [_**dp\_dsss\_receiver\_get\_state()**_](dsss__receiver__core_8h.md#function-dp_dsss_receiver_get_state) _into_`state` _._ |
|  size\_t | [**dp\_dsss\_receiver\_state\_bytes**](#function-dp_dsss_receiver_state_bytes) (const [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state) <br>_Byte size of_ `state's` _blob._ |
|  size\_t | [**dp\_dsss\_receiver\_steps**](#function-dp_dsss_receiver_steps) ([**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, float \_Complex \* out, size\_t max\_out) <br>_Stream raw cf32 samples; emit demodulated symbols once locked._  |
|  size\_t | [**dp\_dsss\_receiver\_steps\_max\_out**](#function-dp_dsss_receiver_steps_max_out) ([**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md) \* state) <br>_Output capacity hint for_ [_**dp\_dsss\_receiver\_steps()**_](dsss__receiver__core_8h.md#function-dp_dsss_receiver_steps) _._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DSSS\_RECEIVER\_STATE\_MAGIC**](dsss__receiver__core_8h.md#define-dsss_receiver_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('D', 'S', 'R', 'X')`<br>_Magic tag ("DSRX") stamped in a receiver state blob._  |
| define  | [**DSSS\_RECEIVER\_STATE\_VERSION**](dsss__receiver__core_8h.md#define-dsss_receiver_state_version)  `/* multi line expression */`<br>_Layout version of the receiver state blob._  |
| define  | [**DSSS\_RX\_BN\_CARRIER**](dsss__receiver__core_8h.md#define-dsss_rx_bn_carrier)  `0.01`<br>_Pre-despread carrier loop's PLL noise bandwidth, normalized to its once-per-code-period update rate._  |
| define  | [**DSSS\_RX\_BN\_FLL**](dsss__receiver__core_8h.md#define-dsss_rx_bn_fll)  `0.03`<br>_Pre-despread carrier loop's FLL-assist bandwidth, normalized to its once-per-code-period update rate._  |

## Detailed Description


The single-object form of the chain validated across this repo's "continuous async-DSSS receiver" story (`docs/gallery/async-dsss-receiver-spec.md`, `docs/gallery/dsss-acq-characterization.md`, `docs/gallery/dsss-receiver.md`): a continuous, non-bursty spreading code whose data-symbol clock need not be synchronous to the code-epoch clock. `steps()` streams raw samples through whichever child is currently active:



* **searching** (`tracking() == 0`): samples feed the embedded `Acquisition`. Nothing is emitted. On a hit, the carrier loop/ `Dll`/`RateConverter`/`MpskReceiver` are built from the hit's code phase and Doppler estimate (the exact `acq_result_t::chip_phase` hand-off phase and `RateConverter`-bridged sample-rate hand-off this repo's gallery pages validated by hand), and the **unconsumed tail** of the same `steps()` call is handed straight to them — no samples are dropped at the transition.
* **tracking** (`tracking() == 1`): samples are first derotated by a pre-despread carrier loop (`costas_wipeoff`/`costas_update`, one update per code period, `bn_fll`-assisted  removes BULK Doppler and its RATE OF CHANGE before the code loop ever sees it; a fixed/ bounded residual alone is fine downstream-only, per `docs/design/async-dsss-receiver.md` §3.4, but an unbounded Doppler RATE is not), one code period at a time (a small internal carry buffer holds any leftover partial-period tail across calls  `steps()` still accepts any block size), then feed `Dll -> RateConverter -> MpskReceiver` in sequence  the C-level equivalent of hand-composing those four objects (plus the new carrier stage)  and demodulated symbols are emitted. This is a NEW composition living entirely in this object  deliberately NOT a swap to the existing `Despreader` object (which fuses Costas+Dll per-sample), because `Despreader` embeds `Dll` via `dp_dll_init()`, hardcoded to `segments==1`; it cannot carry this object's own `segments>1` async-lookback tracking. `dp_dll_steps()` itself is called completely unmodified.




Per `[[feedback_despread_resample_demod_separation]]` (this story's own hard-won lesson): `segments` (the despreader's own tracking parameter) and `sps` (the demodulator's own sample-rate need) are independently configurable and bridged by an explicit `RateConverter`, never coupled to each other.



```C++
// "Just works": only the signal's own physical parameters are required.
dp_dsss_receiver_state_t *rx = dp_dsss_receiver_create(
    code, code_len, 3.0e6, 2100.0,   // chip_rate, symbol_rate
    6.0e6, 2,                        // sample_rate, m (BPSK)
    55.0, 1e-3, 0.9, 100.0,          // cn0_dbhz, pfa, pd,
                                     // doppler_uncertainty
    4, 8,                            // segments, sps
    0);                              // differential
float _Complex syms[4096];
size_t n = dp_dsss_receiver_steps(rx, x, x_len, syms, 4096);
dp_dsss_receiver_destroy(rx);
```
 


    
## Public Functions Documentation




### function dp\_dsss\_receiver\_configure\_chain\_raw 

_Pin the despread/resample/demod grid directly, bypassing the create-time_ `segments` _/_`sps` _defaults._
```C++
int dp_dsss_receiver_configure_chain_raw (
    dp_dsss_receiver_state_t * state,
    size_t segments,
    size_t sps,
    int n
) 
```



The escape hatch for the one composition-specific knob this object adds beyond its children's own: `segments` (Dll's tracking parameter) and `sps`/`n` (MpskReceiver's samples and terminal outputs per symbol) are independently overridable here, still bridged by a freshly-sized `RateConverter`  never coupled to each other (see the module docstring). It rebuilds `dll`/`rc`/`rx` at once, allocating every replacement before freeing the old ones (`Acquisition`'s `acq_regrid()` discipline), and the pinned grid is the one every later chain is built on: the next hit's hand-off and reset() both rebuild at it.


The new grid is checked before anything is rebuilt, and refused with the receiver left on its prior grid: `segments` &gt;= 1, `sps` &gt;= 2, a symbol no faster than a chip and a resampling ratio that is a normal double (doppler#2103), and an `n` MpskReceiver would accept (doppler#2426). Still an abort: an allocation failure in the rebuild, which runs under dp\_xnn (doppler#2112).




**Parameters:**


* `state` The receiver. 
* `segments` Dll tracking segments per code period, &gt;= 1. 
* `sps` MpskReceiver samples per symbol (the resample target), &gt;= 2. 
* `n` MpskReceiver's terminal outputs per symbol (`m_out`): inside [**dp\_ratesync\_m\_ok()**](ratesync__core_8h.md#function-dp_ratesync_m_ok) (even, 2..8) and at most `sps`  [**mpsk\_rx\_m\_out\_ok()**](mpsk__rx__loops_8h.md#function-mpsk_rx_m_out_ok), the rule create()'s own derived `n` satisfies, so get\_n() always re-applies (doppler#2650). It need not divide `sps`. 



**Returns:**

0 on success, -1 on a grid refused as above (the receiver keeps its prior grid). 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0,
...                   sample_rate=6.0e6)
>>> rx.configure_chain_raw(segments=6, sps=8, n=8)  # re-pin the chain
>>> rx.segments                       # tracking grid updated in place
6
```
 





        

<hr>



### function dp\_dsss\_receiver\_configure\_lock\_raw 

_Re-tune the embedded Dll's code-lock detector directly. Forwards to_ `dp_dll_configure_lock_raw()` _, which also clears the in-flight lock statistic and drops lock. Only meaningful once tracking has begun: while searching it tunes the placeholder Dll, which the next hit replaces, so the setting is lost (as it is on reset() and configure\_chain\_raw()). The detector is the hysteretic lockdet over the DLL's per-N-look CFAR statistic —_`up_thresh` _/_`down_thresh` _set the declare/drop levels and_`n_up` _/_`n_down` _the consecutive-look verify counts, trading declare latency against false-alarm rate._
```C++
void dp_dsss_receiver_configure_lock_raw (
    dp_dsss_receiver_state_t * state,
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
>>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0,
...                   sample_rate=6.0e6)
>>> rx.configure_lock_raw(up_thresh=0.4, down_thresh=0.2, n_looks=20,
...                       alpha=0.1, n_up=5, n_down=3)
>>> rx.tracking                # a no-op until a hit builds the Dll
0
```
 




        

<hr>



### function dp\_dsss\_receiver\_configure\_search\_raw 

_Pin the embedded Acquisition's search grid directly. Forwards to_ `dp_acq_configure_search_raw()` _— the escape hatch under this object's own_`symbol_rate` _-driven auto-sizing, for a power user who wants a specific_`(doppler_bins, n_noncoh)` _instead. Pinning exits the continuous engine's wideband window-tiling mode, re-derives its thresholds and resets it, discarding any search in flight. While tracking the engine is idle, so the pinned grid governs the search that resumes after the next_`reset()` _._
```C++
int dp_dsss_receiver_configure_search_raw (
    dp_dsss_receiver_state_t * state,
    size_t doppler_bins,
    size_t n_noncoh
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `doppler_bins` Coherent depth to pin, in `[1, reps]` as [**dp\_acq\_configure\_search\_raw()**](acq__core_8h.md#function-dp_acq_configure_search_raw) defines it. 
* `n_noncoh` Non-coherent looks accumulated per grid cell (1..256); more looks buys sensitivity at the cost of dwell, replacing the auto-sized count. 



**Returns:**

0 on success, -1 on an invalid grid (including `n_noncoh` &gt; 1 on an engine with rate hypotheses) or a failed allocation (the engine keeps its prior grid; see [**dp\_acq\_configure\_search\_raw()**](acq__core_8h.md#function-dp_acq_configure_search_raw)). 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0,
...                   sample_rate=6.0e6)
>>> rx.configure_search_raw(doppler_bins=1, n_noncoh=16)  # pin it
>>> rx.tracking                # still searching, on the pinned grid
0
```
 





        

<hr>



### function dp\_dsss\_receiver\_create 

_Create a DSSS receiver in the searching state._ 
```C++
dp_dsss_receiver_state_t * dp_dsss_receiver_create (
    const uint8_t * code,
    size_t code_len,
    double chip_rate,
    double symbol_rate,
    double sample_rate,
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



Only `code`/`chip_rate`/`symbol_rate` describe the signal itself — everything else is a physically-motivated default a caller can override, not a requirement. Internally: the embedded `Acquisition` is built via `dp_acq_create_continuous()` (this receiver is inherently continuous/streaming)  always window-tiles, never coherently combines across epochs, sensitivity purely from an internally auto-sized non-coherent look count (see `acq_core.h`'s file doc comment); `Dll` always uses `bn=0.002` (this story's own validated stable loop bandwidth for a one-update-per-code-epoch geometry, not `dp_dll_create()`'s own default of 0.01, which this story found unstable here) and `zeta=0.707`, `spacing=0.5`; `MpskReceiver` always uses `pulse=iandd`, `bn_carrier=bn_timing=0.01`, `zeta=0.707` and `lock_thresh=0.3` — this story's own validated values throughout. It also passed `acq_to_track=1` and `warmup_syms=30` until those were deleted (doppler#877, `1f417e97`); the composed receiver now runs its one NDA discriminator here as everywhere else. `lock_thresh=0.3` predates the lock statistic becoming a calibrated detector and is retained because it is validated here, but it now has a derivable reading: the carrier lock EMA's noise-only sd is 0.1132 at every M, so 0.3 is **2.65 noise sigmas**, a per-look Pfa of ~4e-3 — looser than `MpskReceiver`'s own 0.5 default (4.42 sigma, 5e-6) and still ~6 sigma clear of the +0.99 a locked BPSK constellation reads, which is why it holds. See `carrier_nda_core.h`. `n` (MpskReceiver's terminal outputs per symbol, `m_out`) is derived from `sps` by [**mpsk\_rx\_derive\_m\_out()**](mpsk__rx__loops_8h.md#function-mpsk_rx_derive_m_out): the even count `2*floor(sps/2)`, capped at 8.




**Parameters:**


* `code` Spreading code, one 0/1 chip per element (0 -&gt; +1, 1 -&gt; -1 BPSK; only the low bit is used, so pass 0/1, not +/-1). 
* `code_len` Chips in `code` (the spreading factor). 
* `chip_rate` Chip rate, Hz. Required. 
* `symbol_rate` Data-symbol rate, Hz. Required — passed straight to the embedded Acquisition's own `symbol_rate` (diagnostic there; see `dp_acq_create_continuous()`). 
* `sample_rate` Input sample rate, Hz: a whole number of samples a chip (to a part in 1e9), at least one, or 0 for two a chip (the default). A fractional rate is refused: this receiver's code loop and carrier stage run whole-sample code periods (AsyncDsssReceiver takes any). 
* `m` PSK order, 2/4/8; default 2 (BPSK). 
* `cn0_dbhz` Design C/N0 for acquisition sizing, dB-Hz; default 55.0. 
* `pfa` Acquisition false-alarm target; default 1e-3. 
* `pd` Acquisition detection-probability target; default 0.9. 
* `doppler_uncertainty` One-sided Doppler search half-range, Hz; default 100.0. 
* `segments` Dll's partial correlations per code epoch: its despread output rate and the grain of its look-back window, independent of `sps` (see the module docstring); default 4. 
* `sps` MpskReceiver's samples/symbol, reached by an internal RateConverter bridging the despreader's own partial rate to this rate; default 8, MpskReceiver's own constructor default. 
* `differential` MpskReceiver's differential (rotation- invariant) demap; default 0 (coherent). 



**Returns:**

A new receiver in the searching state, owned by the caller (free with [**dp\_dsss\_receiver\_destroy()**](dsss__receiver__core_8h.md#function-dp_dsss_receiver_destroy)); NULL when `code` is NULL, a parameter is outside its domain (`m` not 2/4/8, `sample_rate` not a whole number of samples a chip of at least one, `sps` &lt; 2, a non-positive or non-finite rate, a symbol rate above the chip rate, a code/segments pair the Dll refuses), or the embedded Acquisition refuses its own parameters (`pfa`, `pd`, `cn0_dbhz`, ...). An allocation failure aborts the process rather than returning NULL. 
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
>>> rx = DsssReceiver(code, chip_rate=chip, symbol_rate=sym, sample_rate=fs,
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



### function dp\_dsss\_receiver\_destroy 

_Destroy a receiver and release all four children._ 
```C++
void dp_dsss_receiver_destroy (
    dp_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_dsss\_receiver\_get\_chip\_phase 

_Dll's live tracked code phase (chips); 0.0 while searching._ 
```C++
double dp_dsss_receiver_get_chip_phase (
    const dp_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Code phase in chips, from [**dp\_dll\_get\_code\_phase()**](dll__core_8h.md#function-dp_dll_get_code_phase). 





        

<hr>



### function dp\_dsss\_receiver\_get\_cn0\_dbhz\_est 

_C/N0 estimate from the winning acquisition hit._ 
```C++
double dp_dsss_receiver_get_cn0_dbhz_est (
    const dp_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

dB-Hz; 0.0 before the first hit and after reset(). 





        

<hr>



### function dp\_dsss\_receiver\_get\_code\_rate 

_Dll's tracked code rate; 1.0 while searching._ 
```C++
double dp_dsss_receiver_get_code_rate (
    const dp_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Chips advanced per nominal chip (~1.0 at lock), from [**dp\_dll\_get\_code\_rate()**](dll__core_8h.md#function-dp_dll_get_code_rate). 





        

<hr>



### function dp\_dsss\_receiver\_get\_doppler\_hz 

_Doppler estimate from the winning acquisition hit._ 
```C++
double dp_dsss_receiver_get_doppler_hz (
    const dp_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Hz; 0.0 before the first hit and after reset(). Not updated while tracking. 





        

<hr>



### function dp\_dsss\_receiver\_get\_lock 

_MpskReceiver's carrier lock EMA; 0.0 while searching._ 
```C++
double dp_dsss_receiver_get_lock (
    const dp_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The NDA lock statistic, near 1 when locked and near 0 on noise, from [**dp\_mpsk\_receiver\_get\_lock()**](mpsk__receiver__core_8h.md#function-dp_mpsk_receiver_get_lock). 





        

<hr>



### function dp\_dsss\_receiver\_get\_n 

_MpskReceiver terminal outputs per symbol (_ `m_out` _) in use._
```C++
int dp_dsss_receiver_get_n (
    const dp_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

`2*floor(sps/2)` capped at 8 from create, or the last configure\_chain\_raw() pin. 





        

<hr>



### function dp\_dsss\_receiver\_get\_norm\_freq 

_MpskReceiver's tracked carrier frequency; 0.0 while searching._ 
```C++
double dp_dsss_receiver_get_norm_freq (
    const dp_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Cycles/sample at the demodulator's rate (sps \* symbol\_rate): the residual left after the pre-despread carrier loop, not the full Doppler. 





        

<hr>



### function dp\_dsss\_receiver\_get\_segments 

_Dll partial correlations per code period currently in use._ 
```C++
size_t dp_dsss_receiver_get_segments (
    const dp_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The create-time value, or the last configure\_chain\_raw() pin. 





        

<hr>



### function dp\_dsss\_receiver\_get\_sps 

_MpskReceiver samples per symbol currently in use._ 
```C++
size_t dp_dsss_receiver_get_sps (
    const dp_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

The create-time value, or the last configure\_chain\_raw() pin. 





        

<hr>



### function dp\_dsss\_receiver\_get\_state 

_Serialize the whole receiver (searching or tracking) into_ `blob` _._
```C++
void dp_dsss_receiver_get_state (
    const dp_dsss_receiver_state_t * state,
    void * blob
) 
```





**Parameters:**


* `state` Must be non-NULL; not modified. 
* `blob` Caller-owned buffer of [**dp\_dsss\_receiver\_state\_bytes()**](dsss__receiver__core_8h.md#function-dp_dsss_receiver_state_bytes) bytes. 




        

<hr>



### function dp\_dsss\_receiver\_get\_tracking 

_Routing state: searching or tracking._ 
```C++
int dp_dsss_receiver_get_tracking (
    const dp_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

0 while searching (after create/reset), 1 once a hit has built the chain and symbols are being demodulated. 





        

<hr>



### function dp\_dsss\_receiver\_reset 

_Return to the searching state. Resets the embedded Acquisition and rebuilds_ `dll` _/_`rc` _/_`rx` _and the carrier loop at the placeholder seed (phase 0, no Doppler) on the current grid (a configure\_chain\_raw() pin survives), clearing the carrier carry and the cached Doppler/C/N0 estimates. Any configure\_lock\_raw() tuning is lost with the old Dll; a grid pinned by configure\_search\_raw() is kept. A receiver that has locked cannot be "reset back to tracking the same signal," only back to searching, matching every other object's reset() semantics in this codebase._
```C++
void dp_dsss_receiver_reset (
    dp_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.dsss import DsssReceiver
>>> from doppler.wfm import Gold
>>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
>>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0,
...                   sample_rate=6.0e6)
>>> rx.reset()                 # abort any lock, hunt from scratch
>>> (rx.tracking, rx.chip_phase)   # back to searching, all cleared
(0, 0.0)
```
 




        

<hr>



### function dp\_dsss\_receiver\_set\_state 

_Restore a blob from_ [_**dp\_dsss\_receiver\_get\_state()**_](dsss__receiver__core_8h.md#function-dp_dsss_receiver_get_state) _into_`state` _._
```C++
int dp_dsss_receiver_set_state (
    dp_dsss_receiver_state_t * state,
    const void * blob
) 
```



A searching engine may restore a tracking blob and vice versa. Children are restored in order (acq, carrier, dll, rc, rx); a child that rejects aborts with DP\_ERR\_INVALID after the earlier children were already restored.




**Parameters:**


* `state` Must be non-NULL, built with the same code/spc and set to the same `segments`/`sps`/`n` grid as the blob's source. 
* `blob` Blob of dp\_dsss\_receiver\_state\_bytes(state) bytes. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID on an envelope mismatch, a grid that differs from the live engine's (pin it with configure\_chain\_raw() first), a carry length &gt;= tsamps, or a rejected child. 





        

<hr>



### function dp\_dsss\_receiver\_state\_bytes 

_Byte size of_ `state's` _blob._
```C++
size_t dp_dsss_receiver_state_bytes (
    const dp_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Envelope + extra + the five children + a tsamps-sample carry region; fixed for a given grid, independent of `tracking`. 





        

<hr>



### function dp\_dsss\_receiver\_steps 

_Stream raw cf32 samples; emit demodulated symbols once locked._ 
```C++
size_t dp_dsss_receiver_steps (
    dp_dsss_receiver_state_t * state,
    const float _Complex * x,
    size_t x_len,
    float _Complex * out,
    size_t max_out
) 
```



While searching, samples feed the embedded Acquisition and nothing is emitted (0 return is normal, not an error). The moment a hit fires, `Dll`/`RateConverter`/`MpskReceiver` are built and seeded from it, and the rest of THIS call from where the hit's frame ended — the hit's own `samples_consumed`, so no samples are dropped or double-fed — is handed straight to them in the same call. While tracking, samples feed `Dll -> RateConverter -> MpskReceiver` in sequence. Accepts any block size; state carries across calls (`Acquisition`/`Dll`/ `RateConverter`/`MpskReceiver` are all already block-size invariant; the only buffering this object adds is the carrier loop's partial-code-period carry). While tracking, each call allocates two scratch buffers (abort on OOM).




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
>>> rx = DsssReceiver(code, chip_rate=chip, symbol_rate=sym, sample_rate=fs,
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



### function dp\_dsss\_receiver\_steps\_max\_out 

_Output capacity hint for_ [_**dp\_dsss\_receiver\_steps()**_](dsss__receiver__core_8h.md#function-dp_dsss_receiver_steps) _._
```C++
size_t dp_dsss_receiver_steps_max_out (
    dp_dsss_receiver_state_t * state
) 
```





**Parameters:**


* `state` Unused. 



**Returns:**

Always 0: size `out` from the input length `x_len`, which is always enough (at most one symbol per input sample). 





        

<hr>
## Macro Definition Documentation





### define DSSS\_RECEIVER\_STATE\_MAGIC 

_Magic tag ("DSRX") stamped in a receiver state blob._ 
```C++
#define DSSS_RECEIVER_STATE_MAGIC `DP_FOURCC ('D', 'S', 'R', 'X')`
```




<hr>



### define DSSS\_RECEIVER\_STATE\_VERSION 

_Layout version of the receiver state blob._ 
```C++
#define DSSS_RECEIVER_STATE_VERSION `/* multi line expression */`
```




<hr>



### define DSSS\_RX\_BN\_CARRIER 

_Pre-despread carrier loop's PLL noise bandwidth, normalized to its once-per-code-period update rate._ 
```C++
#define DSSS_RX_BN_CARRIER `0.01`
```




<hr>



### define DSSS\_RX\_BN\_FLL 

_Pre-despread carrier loop's FLL-assist bandwidth, normalized to its once-per-code-period update rate._ 
```C++
#define DSSS_RX_BN_FLL `0.03`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dsss_receiver/dsss_receiver_core.h`

