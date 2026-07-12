

# File mpsk\_receiver\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**mpsk\_receiver**](dir_a1dc26622ebd32726f4fc723db7ccb3b.md) **>** [**mpsk\_receiver\_core.h**](mpsk__receiver__core_8h.md)

[Go to the source code of this file](mpsk__receiver__core_8h_source.md)

_Pulse-shaped M-PSK receiver: NDA-acquired carrier + Gardner timing._ [More...](#detailed-description)

* `#include "agc/agc_core.h"`
* `#include "carrier_nda/carrier_nda_core.h"`
* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "farrow/farrow_core.h"`
* `#include "fir/fir_core.h"`
* `#include "jm_perf.h"`
* `#include "lo/lo_core.h"`
* `#include "lockdet/lockdet_core.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include "mpsk/mpsk_core.h"`
* `#include "symsync/symsync_core.h"`
* `#include "telemetry/telemetry.h"`
* `#include <complex.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) <br>_M-PSK receiver state._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**mpsk\_\_receiver\_\_core\_8h\_1a4198b91c1537d92fd53077eb3ac4ba36**](#enum-mpsk__receiver__core_8h_1a4198b91c1537d92fd53077eb3ac4ba36)  <br>_Matched-filter pulse shape selector._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**mpsk\_receiver\_bits**](#function-mpsk_receiver_bits) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, const float complex \* x, size\_t x\_len, uint8\_t \* out, size\_t max\_out) <br>_Demodulate a cf32 block and emit hard Gray-coded bits._  |
|  size\_t | [**mpsk\_receiver\_bits\_max\_out**](#function-mpsk_receiver_bits_max_out) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  void | [**mpsk\_receiver\_configure\_lock**](#function-mpsk_receiver_configure_lock) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune the acquisition&lt;-&gt;tracking handover detector directly._  |
|  [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* | [**mpsk\_receiver\_create**](#function-mpsk_receiver_create) (int m, size\_t sps, int n, int pulse, double rrc\_beta, int rrc\_span, double bn\_carrier, double zeta, double bn\_timing, int acq\_to\_track, double lock\_thresh, double init\_norm\_freq, size\_t warmup\_syms, int differential) <br>_Create an M-PSK receiver._  |
|  void | [**mpsk\_receiver\_destroy**](#function-mpsk_receiver_destroy) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Destroy an M-PSK receiver and release all memory._  |
|  double | [**mpsk\_receiver\_get\_lock**](#function-mpsk_receiver_get_lock) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  int | [**mpsk\_receiver\_get\_m**](#function-mpsk_receiver_get_m) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  int | [**mpsk\_receiver\_get\_n**](#function-mpsk_receiver_get_n) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  double | [**mpsk\_receiver\_get\_norm\_freq**](#function-mpsk_receiver_get_norm_freq) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  size\_t | [**mpsk\_receiver\_get\_sps**](#function-mpsk_receiver_get_sps) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  void | [**mpsk\_receiver\_get\_state**](#function-mpsk_receiver_get_state) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, void \* blob) <br> |
|  double | [**mpsk\_receiver\_get\_timing\_rate**](#function-mpsk_receiver_get_timing_rate) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  int | [**mpsk\_receiver\_get\_tracking**](#function-mpsk_receiver_get_tracking) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  void | [**mpsk\_receiver\_reset**](#function-mpsk_receiver_reset) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br>_Re-seed the carrier/timing loops to their create-time state._  |
|  void | [**mpsk\_receiver\_set\_norm\_freq**](#function-mpsk_receiver_set_norm_freq) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, double val) <br> |
|  int | [**mpsk\_receiver\_set\_state**](#function-mpsk_receiver_set_state) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, const void \* blob) <br> |
|  int | [**mpsk\_receiver\_set\_telemetry**](#function-mpsk_receiver_set_telemetry) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context across the receiver. Registers the receiver's own "&lt;prefix&gt;.lock" probe (the carrier lock EMA) and "&lt;prefix&gt;.tracking" (the two-way handover decision, 0/1 — the lockdet output, so a consumer sees exactly when the carrier was handed to the decision-directed discriminator or dropped back to NDA), then forwards the attach to both embedded loops: the carrier loop registers "&lt;prefix&gt;.car.lock" / ".e" / ".freq" / ".locked" (plus its arm AGC's "&lt;prefix&gt;.car.agc.gain\_db") and the symbol-timing loop registers "&lt;prefix&gt;.sync.e" / ".freq" / ".rate" / ".lock" / ".locked"_  _twelve probes total, all thinned by_`decim` _. Every probe except the AGC's emits once per recovered symbol (the receiver flushes both loops at the symbol strobe, not at the carrier loop's sample rate); the AGC's emits at its own amortized gain-update rate. Passing NULL detaches the receiver and both loops. Setup path, never hot; the context is borrowed and must outlive the attachment (SPSC rules in_[_**telemetry/telemetry.h**_](telemetry_8h.md) _)._ |
|  size\_t | [**mpsk\_receiver\_state\_bytes**](#function-mpsk_receiver_state_bytes) (const [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |
|  size\_t | [**mpsk\_receiver\_steps**](#function-mpsk_receiver_steps) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br>_Demodulate a cf32 block and emit the recovered symbols._  |
|  size\_t | [**mpsk\_receiver\_steps\_max\_out**](#function-mpsk_receiver_steps_max_out) ([**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**MPSK\_RECEIVER\_STATE\_MAGIC**](mpsk__receiver__core_8h.md#define-mpsk_receiver_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('M', 'P', 'S', 'K')`<br> |
| define  | [**MPSK\_RECEIVER\_STATE\_VERSION**](mpsk__receiver__core_8h.md#define-mpsk_receiver_state_version)  `4u /\* v4: handover lockdet counters \*/`<br> |

## Detailed Description


A complete per-sample inline modem for a continuous (unspread) M-PSK signal. It composes the project's tracking primitives on one shared sample loop:



* [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) — per-sample carrier wipe-off with the integer NCO, plus a non-data-aided M-th-power discriminator on an I/Q arm integrate-and-dump (n dumps/symbol). This **acquires the carrier with no symbol timing and no data present** (cold start), which is what lets the receiver pull in before the matched filter / timing loop have settled.
* a matched filter ([**fir\_state\_t**](structfir__state__t.md), owned) on the de-rotated stream: either an **integrate-and-dump boxcar** (`MPSK_RX_PULSE_IANDD`, default) or a **root-raised-cosine** (`MPSK_RX_PULSE_RRC`) for band-limited links.
* [**symsync\_state\_t**](structsymsync__state__t.md) — a carrier-blind Gardner symbol-timing loop on the matched-filter output, emitting one symbol per recovered symbol period.




Carrier recovery follows the project rule: **predetection de-rotation** (per-sample wipe, always) and **postdetection discrimination**. Two discriminators steer one shared NCO:
* **acquisition** — the NDA M-th-power error, n times/symbol, with no data or timing required (`tracking == 0`).
* **tracking** — once `acq_to_track` is enabled and the loop has locked, a decision-directed error `e = Im(y·conj(â))/|y|` on the recovered symbols (lower jitter, naturally lower loop bandwidth at symbol rate). The same NCO/loop filter carries the frequency estimate across the switch. The handover is **opt-in** (`acq_to_track`, default off — the receiver then stays in robust NDA tracking the whole time) and **two-way**: an embedded lock detector ([**lockdet\_core.h**](lockdet__core_8h.md)) steps on the carrier lock metric once per recovered symbol, declaring tracking after a verify count of consecutive above-threshold symbols and — on a sustained lock loss — dropping back to the NDA acquisition steer, which re-pulls the carrier with no data/timing assumptions. The shared NCO carries the frequency estimate through both directions, so a drop-back is a discriminator swap, not a re-acquisition from cold.




The loop locks to one of M phases — an **M-fold ambiguity** on absolute phase. Resolve it with differential demapping (`bits(..., differential=1)`) or a sync word downstream. A DSSS-MPSK receiver is `Dll(segments) -> MpskReceiver`: despread to symbol-rate soft chips, then this modem.


Lifecycle: `mpsk_receiver_create -> (steps / bits / reset)* -> _destroy`.



```C++
// QPSK, 8 samples/symbol, I&D matched filter, NDA acquisition
mpsk_receiver_state_t *rx = mpsk_receiver_create(
    4, 8, 4, MPSK_RX_PULSE_IANDD, 0.35, 8,
    0.01, 0.707, 0.01, 0, 0.5, 0.0, 100, 0);
float complex sym[256];
size_t k = mpsk_receiver_steps(rx, rx_in, rx_len, sym, 256);
double f = mpsk_receiver_get_norm_freq(rx);  // tracked residual carrier
mpsk_receiver_destroy(rx);
```
 


    
## Public Types Documentation




### enum mpsk\_\_receiver\_\_core\_8h\_1a4198b91c1537d92fd53077eb3ac4ba36 

_Matched-filter pulse shape selector._ 
```C++
enum mpsk__receiver__core_8h_1a4198b91c1537d92fd53077eb3ac4ba36 {
    MPSK_RX_PULSE_IANDD = 0,
    MPSK_RX_PULSE_RRC = 1
};
```




<hr>
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





        

<hr>



### function mpsk\_receiver\_bits\_max\_out 

```C++
size_t mpsk_receiver_bits_max_out (
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



Full lockdet control over `handover`, mirroring [**costas\_configure\_lock()**](costas__core_8h.md#function-costas_configure_lock): a split declare/drop threshold pair on the carrier lock EMA (level hysteresis) and both verify counts (time hysteresis). Previously only settable at construction (`lock_thresh`, with `MPSK_RX_HANDOVER_DOWN`/ `_N_UP`/`_N_DOWN` fixed compile-time constants)  this is the post-construction re-tune Dll and Costas both already have. A live handover survives the re-tune; the in-flight verify run restarts.




**Parameters:**


* `state` Must be non-NULL. 
* `up_thresh` Declare threshold on the carrier lock EMA. 
* `down_thresh` Drop threshold; choose &lt;= up\_thresh for level hysteresis. 
* `n_up` Consecutive above-threshold symbols to hand over to the decision-directed discriminator; clamped &gt;= 1. 
* `n_down` Consecutive below-threshold symbols to fall back to NDA acquisition; clamped &gt;= 1. 
```C++
>>> from doppler.track import MpskReceiver
>>> rx = MpskReceiver(m=4, sps=4, acq_to_track=1)
>>> rx.tracking
0
>>> rx.configure_lock(0.9, 0.72, 4, 16)   # tighter declare, faster drop
```
 




        

<hr>



### function mpsk\_receiver\_create 

_Create an M-PSK receiver._ 
```C++
mpsk_receiver_state_t * mpsk_receiver_create (
    int m,
    size_t sps,
    int n,
    int pulse,
    double rrc_beta,
    int rrc_span,
    double bn_carrier,
    double zeta,
    double bn_timing,
    int acq_to_track,
    double lock_thresh,
    double init_norm_freq,
    size_t warmup_syms,
    int differential
) 
```





**Parameters:**


* `m` Constellation order M, 2/4/8 (default 4 = QPSK). 
* `sps` Samples per symbol (default 8). 
* `n` Carrier arm dumps per symbol (default 4; sps % n == 0). 
* `pulse` Matched-filter shape (default MPSK\_RX\_PULSE\_IANDD). 
* `rrc_beta` RRC roll-off in `[0, 1]` (default 0.35; RRC only). 
* `rrc_span` RRC one-sided span in symbols (default 8; RRC only). 
* `bn_carrier` Carrier loop noise bandwidth (default 0.01). 
* `zeta` Damping factor for both loops (default 0.707). 
* `bn_timing` Symbol-timing loop noise bandwidth (default 0.01). 
* `acq_to_track` Enable the two-way NDA&lt;-&gt;decision-directed handover (default 0). 
* `lock_thresh` Handover declare threshold on the carrier lock metric (default 0.5); the drop threshold sits at 0.8x for level hysteresis, and both directions are verify-counted (8 symbols up / 32 down). 
* `init_norm_freq` Seed carrier frequency, cycles/sample (default 0.0). 
* `warmup_syms` Symbols before the acq-to-track switch is allowed (default 100). 
* `differential` bits(): differential (rotation-invariant) demap (default 0 = coherent). 



**Returns:**

Heap-allocated state, or NULL on invalid args / allocation failure. 




**Note:**

Caller must call [**mpsk\_receiver\_destroy()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_destroy) when done. 





        

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



### function mpsk\_receiver\_get\_lock 

```C++
double mpsk_receiver_get_lock (
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



### function mpsk\_receiver\_get\_n 

```C++
int mpsk_receiver_get_n (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_norm\_freq 

```C++
double mpsk_receiver_get_norm_freq (
    const mpsk_receiver_state_t * state
) 
```




<hr>



### function mpsk\_receiver\_get\_sps 

```C++
size_t mpsk_receiver_get_sps (
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



### function mpsk\_receiver\_get\_timing\_rate 

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



### function mpsk\_receiver\_reset 

_Re-seed the carrier/timing loops to their create-time state._ 
```C++
void mpsk_receiver_reset (
    mpsk_receiver_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function mpsk\_receiver\_set\_norm\_freq 

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

_Attach (or detach) a telemetry context across the receiver. Registers the receiver's own "&lt;prefix&gt;.lock" probe (the carrier lock EMA) and "&lt;prefix&gt;.tracking" (the two-way handover decision, 0/1 — the lockdet output, so a consumer sees exactly when the carrier was handed to the decision-directed discriminator or dropped back to NDA), then forwards the attach to both embedded loops: the carrier loop registers "&lt;prefix&gt;.car.lock" / ".e" / ".freq" / ".locked" (plus its arm AGC's "&lt;prefix&gt;.car.agc.gain\_db") and the symbol-timing loop registers "&lt;prefix&gt;.sync.e" / ".freq" / ".rate" / ".lock" / ".locked"_  _twelve probes total, all thinned by_`decim` _. Every probe except the AGC's emits once per recovered symbol (the receiver flushes both loops at the symbol strobe, not at the carrier loop's sample rate); the AGC's emits at its own amortized gain-update rate. Passing NULL detaches the receiver and both loops. Setup path, never hot; the context is borrowed and must outlive the attachment (SPSC rules in_[_**telemetry/telemetry.h**_](telemetry_8h.md) _)._
```C++
int mpsk_receiver_set_telemetry (
    mpsk_receiver_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "rx". 
* `decim` Emit every decim-th symbol; &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take the twelve probes (the attach fails whole; everything detached). 
```C++
>>> import numpy as np
>>> from doppler.track import MpskReceiver
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> rx = MpskReceiver(m=4, sps=4)
>>> rx.set_telemetry(tlm, "rx")
>>> len(tlm.probe_names())
12
>>> rng = np.random.default_rng(7)
>>> syms = (1 - 2 * rng.integers(0, 2, 512)).astype(np.complex64)
>>> x = np.repeat(syms, 4)
>>> _ = rx.steps(x)
>>> recs = tlm.read()   # eleven records per emitted symbol + AGC
>>> n_sync = len(recs[recs["probe"] == tlm.probe_id("rx.sync.e")])
>>> n_car = len(recs[recs["probe"] == tlm.probe_id("rx.car.e")])
>>> n_sync > 0 and n_sync == n_car
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



Runs the per-sample loop (carrier wipe-off + NDA arm + matched filter + Gardner timing) over `x` and writes one cf32 symbol per recovered symbol period. Fewer outputs than inputs (~ x\_len / sps). Read norm\_freq for the tracked carrier and lock for the carrier lock metric.




**Parameters:**


* `state` Receiver state. Must be non-NULL. 
* `x` Input cf32 samples. 
* `x_len` Number of input samples. 
* `out` Output symbols; caller provides `max_out` capacity. 
* `max_out` Output capacity. 



**Returns:**

Number of symbols written. 





        

<hr>



### function mpsk\_receiver\_steps\_max\_out 

```C++
size_t mpsk_receiver_steps_max_out (
    mpsk_receiver_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define MPSK\_RECEIVER\_STATE\_MAGIC 

```C++
#define MPSK_RECEIVER_STATE_MAGIC `DP_FOURCC ('M', 'P', 'S', 'K')`
```




<hr>



### define MPSK\_RECEIVER\_STATE\_VERSION 

```C++
#define MPSK_RECEIVER_STATE_VERSION `4u /* v4: handover lockdet counters */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/mpsk_receiver/mpsk_receiver_core.h`

