

# File mpsk\_rx\_loops.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**mpsk\_receiver**](dir_a1dc26622ebd32726f4fc723db7ccb3b.md) **>** [**mpsk\_rx\_loops.h**](mpsk__rx__loops_8h.md)

[Go to the source code of this file](mpsk__rx__loops_8h_source.md)

_The two loops an M-PSK receiver closes, independent of its front end._ [More...](#detailed-description)

* `#include "agc/agc_core.h"`
* `#include "carrier_nda/carrier_nda_core.h"`
* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "lockdet/lockdet_core.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include "mpsk/mpsk_core.h"`
* `#include "ratesync/ratesync_core.h"`
* `#include "dp_tlm/dp_tlm_core.h"`
* `#include <complex.h>`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) <br>_The receiver's loops: timing, carrier, handover, demapper._  |
| struct | [**mpsk\_rx\_tlm\_t**](structmpsk__rx__tlm__t.md) <br>_Telemetry attachment for the receiver's own two probes; the timing and carrier probes ride their own sub-attachments._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**mpsk\_\_rx\_\_loops\_8h\_1a99fb83031ce9923c84392b4e92f956b5**](#enum-mpsk__rx__loops_8h_1a99fb83031ce9923c84392b4e92f956b5)  <br>_Matched-filter pulse shape. Aliases of the cascade's own vocabulary so one set of names covers the whole family._  |
| enum  | [**mpsk\_\_rx\_\_loops\_8h\_1abc6126af1d45847bc59afa0aa3216b04**](#enum-mpsk__rx__loops_8h_1abc6126af1d45847bc59afa0aa3216b04)  <br>_Where the NDA carrier discriminator reads from._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**mpsk\_rx\_agc\_bn**](#function-mpsk_rx_agc_bn) (double bn\_carrier, double bn\_timing, double ratio) <br> |
|  void | [**mpsk\_rx\_config\_carrier**](#function-mpsk_rx_config_carrier) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l) <br>_(Re-)size the carrier loop filter for the tap's update rate._  |
|  void | [**mpsk\_rx\_configure\_lock**](#function-mpsk_rx_configure_lock) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune the handover detector; see_ [_**mpsk\_receiver\_configure\_lock()**_](mpsk__receiver__core_8h.md#function-mpsk_receiver_configure_lock) _, which forwards here. A live handover survives; the verify run restarts._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) size\_t | [**mpsk\_rx\_derive\_m\_out**](#function-mpsk_rx_derive_m_out) (double cap, int strict) <br>_Terminal outputs per symbol, derived: the largest even count in 2..8 the caller's own rate constraint allows._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**mpsk\_rx\_disc**](#function-mpsk_rx_disc) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, float complex z) <br>_Run the NDA discriminator on one tapped sample._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**mpsk\_rx\_fold**](#function-mpsk_rx_fold) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, const float complex \* ys, size\_t n, float complex zpre, int n\_pre, float complex \* y\_out, int ted) <br>_Fold one front end's burst of outputs into both loops._  |
|  double | [**mpsk\_rx\_freq\_est**](#function-mpsk_rx_freq_est) (const [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l) <br>_Tracked carrier offset in cycles/sample at the LO's rate — the loop's own estimate, excluding the front end's configured centre._  |
|  void | [**mpsk\_rx\_loops\_get\_state**](#function-mpsk_rx_loops_get_state) (const [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, void \* blob) <br>_Serialize the loops' mutable state into_ `blob` _._ |
|  void | [**mpsk\_rx\_loops\_init**](#function-mpsk_rx_loops_init) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, int m, double sps, double lo\_sps, size\_t m\_out, double bn\_carrier, double zeta, double bn\_timing, double bn\_agc\_ratio, int ted, int acq\_to\_track, double lock\_thresh, int differential, int nda\_tap) <br>_Initialise the loops in place (no allocation)._  |
|  void | [**mpsk\_rx\_loops\_reset**](#function-mpsk_rx_loops_reset) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l) <br>_Re-seed both loops to their post-init state; keep configuration._  |
|  int | [**mpsk\_rx\_loops\_set\_state**](#function-mpsk_rx_loops_set_state) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, const void \* blob) <br>_Restore the loops' mutable state from_ `blob` _._ |
|  size\_t | [**mpsk\_rx\_loops\_state\_bytes**](#function-mpsk_rx_loops_state_bytes) (const [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l) <br>_Bytes_ [_**mpsk\_rx\_loops\_get\_state()**_](mpsk__rx__loops_8h.md#function-mpsk_rx_loops_get_state) _writes._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**mpsk\_rx\_push\_mf\_in**](#function-mpsk_rx_push_mf_in) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, float complex z) <br>_Push one MFR-INPUT sample into the NDA discriminator._  |
|  void | [**mpsk\_rx\_set\_freq\_est**](#function-mpsk_rx_set_freq_est) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, double val) <br>_Overwrite the tracked carrier offset (cycles/sample at the LO's rate) so the next output de-rotates by exactly_ `val` _._ |
|  int | [**mpsk\_rx\_set\_telemetry**](#function-mpsk_rx_set_telemetry) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) telemetry across both loops; see_ [_**mpsk\_receiver\_set\_telemetry()**_](mpsk__receiver__core_8h.md#function-mpsk_receiver_set_telemetry) _, which forwards here._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**mpsk\_rx\_steer**](#function-mpsk_rx_steer) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, double pe) <br>_Filter a carrier phase error and update_ `freq_ctrl` _._ |
|  int | [**mpsk\_rx\_symbol\_to\_bits**](#function-mpsk_rx_symbol_to_bits) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, float complex y, uint8\_t \* bits) <br>_Slice one recovered symbol to its log2(M) hard bits (LSB-first)._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int | [**mpsk\_rx\_take\_output**](#function-mpsk_rx_take_output) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, float complex y, float complex \* sym, int ted) <br>_Fold one terminal-stage output into both loops._  |
|  void | [**mpsk\_rx\_tlm\_flush**](#function-mpsk_rx_tlm_flush) (const [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l) <br>_Emit the receiver's own probes plus the timing loop's. Out-of-line on purpose; callers gate on_ `l->tlm.ctx` _._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**mpsk\_rx\_updates\_per\_symbol**](#function-mpsk_rx_updates_per_symbol) (const [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l) <br>_How many times per symbol the chosen tap updates the carrier loop._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**MPSK\_RX\_AGC\_ALPHA**](mpsk__rx__loops_8h.md#define-mpsk_rx_agc_alpha)  `0.01`<br> |
| define  | [**MPSK\_RX\_AGC\_BW\_RATIO**](mpsk__rx__loops_8h.md#define-mpsk_rx_agc_bw_ratio)  `0.05`<br> |
| define  | [**MPSK\_RX\_AGC\_RATIO\_DEFAULT**](mpsk__rx__loops_8h.md#define-mpsk_rx_agc_ratio_default)  `[**MPSK\_RX\_AGC\_BW\_RATIO**](mpsk__rx__loops_8h.md#define-mpsk_rx_agc_bw_ratio)`<br>_AGC bandwidth ratio, derived: 20x slower than the slowest loop it feeds. The RATIO is the part that is not negotiable (see the block above); the value is_ `MPSK_RX_AGC_BW_RATIO` _, and it is a parameter only because the right separation depends on how fast the channel's LEVEL moves against its phase and timing. Zero asks for the default rather than being rejected._ |
| define  | [**MPSK\_RX\_EPS**](mpsk__rx__loops_8h.md#define-mpsk_rx_eps)  `1e-12`<br> |
| define  | [**MPSK\_RX\_HANDOVER\_DOWN**](mpsk__rx__loops_8h.md#define-mpsk_rx_handover_down)  `0.8`<br> |
| define  | [**MPSK\_RX\_HANDOVER\_N\_DOWN**](mpsk__rx__loops_8h.md#define-mpsk_rx_handover_n_down)  `32u`<br> |
| define  | [**MPSK\_RX\_HANDOVER\_N\_UP**](mpsk__rx__loops_8h.md#define-mpsk_rx_handover_n_up)  `8u`<br> |
| define  | [**MPSK\_RX\_LOCK\_THRESH\_DEFAULT**](mpsk__rx__loops_8h.md#define-mpsk_rx_lock_thresh_default)  `0.4999`<br>_Lock threshold, derived:_ `sigma_H0 * eta(Pfa)` _at_`Pfa = 5e-6` _._ |
| define  | [**MPSK\_RX\_LOOPS\_STATE\_MAGIC**](mpsk__rx__loops_8h.md#define-mpsk_rx_loops_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('M', 'R', 'X', 'L')`<br> |
| define  | [**MPSK\_RX\_LOOPS\_STATE\_VERSION**](mpsk__rx__loops_8h.md#define-mpsk_rx_loops_state_version)  `6u`<br> |
| define  | [**MPSK\_RX\_M\_OUT\_DEFAULT**](mpsk__rx__loops_8h.md#define-mpsk_rx_m_out_default)  `8`<br> |
| define  | [**MPSK\_RX\_NUM\_PHASES**](mpsk__rx__loops_8h.md#define-mpsk_rx_num_phases)  `1024u`<br> |
| define  | [**MPSK\_RX\_NUM\_PHASES\_DEFAULT**](mpsk__rx__loops_8h.md#define-mpsk_rx_num_phases_default)  `64u`<br>_Matched-filter bank arms, derived: the measured saturation point._  |
| define  | [**MPSK\_RX\_ZETA\_DEFAULT**](mpsk__rx__loops_8h.md#define-mpsk_rx_zeta_default)  `0.70710678118654752`<br>_Loop damping, derived:_ `1/sqrt(2)` _, critically damped._ |

## Detailed Description


Everything a receiver does _after_ its down-converter emits a terminal-stage output: the symbol-timing loop, the carrier loop, the acquisition/tracking handover, and the slicer/demapper state. It owns no filter, no NCO and no cascade — it consumes matched-filtered outputs and produces the two control values that steer whatever produced them.



```C++
front end (Ddc / Ddcr)                    mpsk_rx_loops_t
─────────────────────                     ───────────────
LO mix ─ cascade ─ matched filter ──y──>  carrier disc ──> freq_ctrl ─┐
   ^                    ^                 timing TED  ──> rate_ctrl ─┤
   └────────────────────┴──────────────────────────────────────────── ┘
```



That split is the whole reason two receiver types cost barely more than one. MpskReceiver drives a `Ddc` (complex input) and MpskReceiverR drives a `Ddcr` (real input, halfband R2C front end); both call the same [**mpsk\_rx\_take\_output()**](mpsk__rx__loops_8h.md#function-mpsk_rx_take_output) on every output their front end emits, so the loops are one implementation rather than two peers that can drift apart. The timing half is literally RateSync's — [**ratesync\_loop\_t**](structratesync__loop__t.md), embedded here — so a fix to the TED or its normaliser reaches RateSync and both receivers at once.


### Both discriminators live on the symbol strobe



**Acquisition** uses the M-th-power NDA error ([**carrier\_nda\_disc**](carrier__nda__core_8h.md#function-carrier_nda_disc)) and **tracking** uses a decision-directed one, but both read the same sample: the on-time strobe. Only that sample is a constellation point — the other terminal outputs fall between symbols, where the matched filter is averaging two of them, so their M-th power carries ISI rather than carrier phase.


This costs less than it appears to. The strobe fires every `m_out`-th output whatever the timing loop currently believes, so the NDA loop still pulls in before timing lock — it just does so on one consistent phase of the pulse instead of all of them. And because both discriminators then share one update rate, the handover is a pure discriminator swap: no update period changes, so the loop filter's integrator — which holds a phase command per update — carries the frequency estimate across untouched, in both directions.



### Units



`bn_carrier` and the timing loop's `bn` are both normalised to the **symbol rate**, so one setting means the same thing at every input rate — the same argument RateSync makes for referencing its control to the terminal stage. The discriminators produce a phase error in radians; `freq_ctrl` must be cycles per sample **at the LO's own rate**, which is the input rate for a complex front end and half it for a real one (the halfband decimates before the LO). Each receiver reports its own `lo_sps` for that reason, and the conversion lives in `freq_scale`. 



    
## Public Types Documentation




### enum mpsk\_\_rx\_\_loops\_8h\_1a99fb83031ce9923c84392b4e92f956b5 

_Matched-filter pulse shape. Aliases of the cascade's own vocabulary so one set of names covers the whole family._ 
```C++
enum mpsk__rx__loops_8h_1a99fb83031ce9923c84392b4e92f956b5 {
    MPSK_RX_PULSE_IANDD = RC_PULSE_IANDD,
    MPSK_RX_PULSE_RRC = RC_PULSE_RRC
};
```




<hr>



### enum mpsk\_\_rx\_\_loops\_8h\_1abc6126af1d45847bc59afa0aa3216b04 

_Where the NDA carrier discriminator reads from._ 
```C++
enum mpsk__rx__loops_8h_1abc6126af1d45847bc59afa0aa3216b04 {
    MPSK_RX_NDA_TAP_STROBE = 0,
    MPSK_RX_NDA_TAP_MF_OUT = 1,
    MPSK_RX_NDA_TAP_MF_IN = 2
};
```



An M-th-power discriminator updating at rate `F` can only observe a frequency error of `|df| < F/(2M)` — above that its M-th-power phase advances more than pi per update and the error folds. So the tap point IS the pull-in range, and it trades directly against signal quality:



|tap   |update rate   |unambiguous \|df\|   |cost    |
|-----|-----|-----|-----|
|`STROBE`   |`Rs`   |`Rs/(2M)`   |needs symbol timing    |
|`MF_OUT`   |`m_out*Rs`   |`m_out*Rs/(2M)`   |inter-symbol ISI bias    |
|`MF_IN`   |`bank_sps`   |`bank_sps*Rs/(2M)`   |~`10*log10(bank_sps)` dB of EXCESS NOISE BANDWIDTH   |






That third row read "none -- see below" until it was measured, and the omission was load-bearing: it is what made `MF_IN` look free and got it pinned as the continuous flavor's tap.


**The cost is not lost signal energy, and it is not intrinsic to reading ahead of the matched filter.** A Nyquist-sampled band-limited signal loses nothing by being sampled fast, so the obvious "it forgoes the
matched filter's processing gain" story is wrong  an earlier revision of this comment told it, with a `10*log10(sps)` law that grows without bound. Measured at the node with the AGC off so the path is linear (`native/validation/rx_dynamics.c` documents the run): the `MF_IN` node sits **6.01 dB** below Es/N0 at `bank_sps = 4` while the terminal node sits 1.7 dB below it, and `10*log10(4) = 6.02 dB`. The deficit is IDENTICAL at 6.79, 12 and 20 dB Es/N0  a pure bandwidth ratio, not an SNR-dependent effect.


The mechanism: DEC band-limits to ITS OWN Nyquist, `+-bank_sps*Rs/2`, while the signal occupies ~`+-Rs`. Nothing between them removes the difference, and the terminal filter  the first thing in the cascade matched to the signal  is downstream of this tap. So the tap reads a node carrying several times the noise bandwidth it needs.


It is **bounded by the plan**, not by the input rate: `bank_sps` is a planner outcome, and at `sps = 64` it is still 8, so the cost is 9.0 dB there and not 18.


**This is the tap's price, not a defect awaiting a fix.** Band-limiting the node to the signal  an arm filter, or the 2 sps decimation S3.3 considers  would recover most of it and is deliberately NOT planned: both cost serialized state on every object that carries this tap, and `STROBE` already reads the node that IS matched to the signal, for free. A caller choosing `MF_IN` is buying `bank_sps/(2M)` of pull-in range and paying `10*log10(bank_sps)` dB of lock sensitivity for it. What degrades is the M-th-power LOCK statistic, because that is an SNR measure and not a phase measure; the loop itself acquires at every operating point measured.


There is a second axis, and it is the one the cascade rebuild lost. `STROBE` is the only tap that depends on **symbol timing**: it reads the one output the timing loop nominates, so before timing lock it is reading an arbitrary phase of the pulse. `MF_OUT` consumes every terminal output and so does not care which one is on-time; `MF_IN` reads the MFR's input entirely ahead of it. Both therefore restore the property the NDA path exists for — acquiring with no data _and no symbol timing_ — which is why they are not merely "wider".


No tap waits. `STROBE` steers from its first strobe whether or not the timing loop has declared, so its dependency is a reason to CHOOSE another tap when the carrier must acquire before timing does — not something the receiver resolves behind the caller's back. Gating it was tried and measured: across a 24-cell sweep it moved one cell, because what it really bought was a carrier transient that started at a known instant and was therefore easier to measure.


Fixed at construction: the caller picks the trade once, and nothing switches underneath it. 


        

<hr>
## Public Functions Documentation




### function mpsk\_rx\_agc\_bn 

```C++
JM_FORCEINLINE double mpsk_rx_agc_bn (
    double bn_carrier,
    double bn_timing,
    double ratio
) 
```




<hr>



### function mpsk\_rx\_config\_carrier 

_(Re-)size the carrier loop filter for the tap's update rate._ 
```C++
void mpsk_rx_config_carrier (
    mpsk_rx_loops_t * l
) 
```



Called by [**mpsk\_rx\_loops\_init()**](mpsk__rx__loops_8h.md#function-mpsk_rx_loops_init), and AGAIN by each receiver's create() once the cascade has published its `bank_sps` — which arrives too late for init, so the MF\_IN tap would otherwise keep gains designed for the `lo_sps` placeholder. `integ` survives [**loop\_filter\_init()**](loop__filter__core_8h.md#function-loop_filter_init) by contract, and every other tap re-derives the gains it already had. 


        

<hr>



### function mpsk\_rx\_configure\_lock 

_Re-tune the handover detector; see_ [_**mpsk\_receiver\_configure\_lock()**_](mpsk__receiver__core_8h.md#function-mpsk_receiver_configure_lock) _, which forwards here. A live handover survives; the verify run restarts._
```C++
void mpsk_rx_configure_lock (
    mpsk_rx_loops_t * l,
    double up_thresh,
    double down_thresh,
    uint32_t n_up,
    uint32_t n_down
) 
```




<hr>



### function mpsk\_rx\_derive\_m\_out 

_Terminal outputs per symbol, derived: the largest even count in 2..8 the caller's own rate constraint allows._ 
```C++
JM_FORCEINLINE size_t mpsk_rx_derive_m_out (
    double cap,
    int strict
) 
```



Even by construction (the Gardner detector needs an on-time strobe and a transition gate `m_out/2` back), capped at 8 because that is where an I&D matched filter reaches the coherent bound — past it the extra outputs buy nothing. The floor matters more than the cap: at low oversampling the shipped constant 8 is simply not available, and `m_out = 2` with `pulse="iandd"` degenerates the matched filter to a two-tap sum that barely opens the eye (measured lock statistic −0.34, acquisition failing about half the time). Deriving it is what stops a caller pairing a rate and an `m_out` that cannot work together.


**It is parameterised by the CONSTRAINT, not by the rate**, because the two twins do not share one. The complex path requires `sps >= m_out`; the real path requires `sps > 2*m_out`, strictly, because Ddcr needs a decimation ratio below 0.5. design/mpsk.md §8 states the real rule as `min(8, 2*floor(sps/4))` and that rule contradicts the constructor it feeds: at `sps = 8` it yields 4 (needs `8 > 8`) and at `sps = 16` it yields 8 (needs `16 > 16`) — both REJECTED by `mpsk_receiver_create_real()`. A derivation whose answer cannot be built is worse than a default, so the bound is passed in and honoured here.




**Parameters:**


* `cap` Upper bound on `m_out` from the caller's constraint: `sps` for the complex twin, `sps/2` for the real one. 
* `strict` Non-zero when the bound is strict (`m_out < cap`) rather than inclusive (`m_out <= cap`) — the real twin's case. 



**Returns:**

An even count in 2..8, or 0 when the bound cannot carry even 2 — a refusal, not a clamp, because a receiver that cannot hand the detector two outputs per symbol has nothing to detect with. 





        

<hr>



### function mpsk\_rx\_disc 

_Run the NDA discriminator on one tapped sample._ 
```C++
JM_FORCEINLINE  JM_HOT void mpsk_rx_disc (
    mpsk_rx_loops_t * l,
    float complex z
) 
```



Shared by all three tap points so the discriminator, its AGC and its lock statistic exist exactly once however the caller chose to feed them.


The discriminator and its lock EMA run on every sample it is handed — the drop-back rule needs them, and `lock` must stay observable throughout acquisition. The steer runs whenever the loop is not already tracking.




**Parameters:**


* `l` Loops. 
* `z` The tapped sample. 




        

<hr>



### function mpsk\_rx\_fold 

_Fold one front end's burst of outputs into both loops._ 
```C++
JM_FORCEINLINE  JM_HOT int mpsk_rx_fold (
    mpsk_rx_loops_t * l,
    const float complex * ys,
    size_t n,
    float complex zpre,
    int n_pre,
    float complex * y_out,
    int ted
) 
```



The whole per-sample body below the front end, and the reason the receiver is one object with two `step` entry points rather than two types: a complex DDC and a real DDCR differ in what they hand over, and in nothing they hand it to. Both entry points reduce to this call, so "the loops
behave identically regardless of front end" is a property of one function rather than a claim about two copies of one.




**Parameters:**


* `l` Loops. Must be non-NULL. 
* `ys` The terminal-stage outputs the front end just produced. 
* `n` How many. 
* `zpre` The MFR-input sample, when the front end produced one. 
* `n_pre` Non-zero when `zpre` is live. 
* `y_out` Receives the recovered symbol when the return is 1. 
* `ted` RATESYNC\_TED\_GARDNER or RATESYNC\_TED\_DTTL — pass a literal for a specialised (branch-free) instantiation. 



**Returns:**

1 if a symbol was emitted (into `y_out`), 0 otherwise. 





        

<hr>



### function mpsk\_rx\_freq\_est 

_Tracked carrier offset in cycles/sample at the LO's rate — the loop's own estimate, excluding the front end's configured centre._ 
```C++
double mpsk_rx_freq_est (
    const mpsk_rx_loops_t * l
) 
```




<hr>



### function mpsk\_rx\_loops\_get\_state 

_Serialize the loops' mutable state into_ `blob` _._
```C++
void mpsk_rx_loops_get_state (
    const mpsk_rx_loops_t * l,
    void * blob
) 
```




<hr>



### function mpsk\_rx\_loops\_init 

_Initialise the loops in place (no allocation)._ 
```C++
void mpsk_rx_loops_init (
    mpsk_rx_loops_t * l,
    int m,
    double sps,
    double lo_sps,
    size_t m_out,
    double bn_carrier,
    double zeta,
    double bn_timing,
    double bn_agc_ratio,
    int ted,
    int acq_to_track,
    double lock_thresh,
    int differential,
    int nda_tap
) 
```





**Parameters:**


* `l` Loops to initialise. Must be non-NULL. 
* `m` Constellation order M (2, 4, 8). 
* `sps` Samples per symbol at the receiver's input. 
* `lo_sps` Samples per symbol at the LO's own rate: `sps` for a complex front end, `sps/2` for a real one, whose halfband decimates before the LO. 
* `m_out` Terminal outputs per symbol (even, &gt;= 2). 
* `bn_carrier` Carrier loop noise bandwidth, per symbol. 
* `zeta` Damping factor for both loops. 
* `bn_timing` Timing loop noise bandwidth, per symbol. 
* `ted` RATESYNC\_TED\_GARDNER or RATESYNC\_TED\_DTTL. 
* `acq_to_track` Enable the two-way NDA&lt;-&gt;decision handover. 
* `lock_thresh` Handover declare threshold on the carrier lock EMA; the drop threshold sits at MPSK\_RX\_HANDOVER\_DOWN x it, and both directions are verify-counted. The EMA's H0 sd is CARRIER\_NDA\_LOCK\_NORM\_SD (0.1132) for every M, so this divided by that is the threshold in noise sigmas and its per-look Pfa is Q(that) — 0.5 is 4.42 sigma, Pfa 5e-6. See [**carrier\_nda\_core.h**](carrier__nda__core_8h.md). 
* `differential` bits(): differential (rotation-invariant) demap. 
* `nda_tap` MPSK\_RX\_NDA\_TAP\_\* — where the NDA discriminator reads, which sets its pull-in range and whether it depends on symbol timing at all. 
* `bn_agc_ratio` Scales the front end's AGC off the SLOWEST of the two loop bandwidths; must be in (0, 1). See [**mpsk\_rx\_agc\_bn()**](mpsk__rx__loops_8h.md#function-mpsk_rx_agc_bn). 




        

<hr>



### function mpsk\_rx\_loops\_reset 

_Re-seed both loops to their post-init state; keep configuration._ 
```C++
void mpsk_rx_loops_reset (
    mpsk_rx_loops_t * l
) 
```





**Parameters:**


* `l` Must be non-NULL. 




        

<hr>



### function mpsk\_rx\_loops\_set\_state 

_Restore the loops' mutable state from_ `blob` _._
```C++
int mpsk_rx_loops_set_state (
    mpsk_rx_loops_t * l,
    const void * blob
) 
```





**Returns:**

DP\_OK, or DP\_ERR\_INVALID if any envelope rejects. 





        

<hr>



### function mpsk\_rx\_loops\_state\_bytes 

_Bytes_ [_**mpsk\_rx\_loops\_get\_state()**_](mpsk__rx__loops_8h.md#function-mpsk_rx_loops_get_state) _writes._
```C++
size_t mpsk_rx_loops_state_bytes (
    const mpsk_rx_loops_t * l
) 
```




<hr>



### function mpsk\_rx\_push\_mf\_in 

_Push one MFR-INPUT sample into the NDA discriminator._ 
```C++
JM_FORCEINLINE  JM_HOT void mpsk_rx_push_mf_in (
    mpsk_rx_loops_t * l,
    float complex z
) 
```



The MPSK\_RX\_NDA\_TAP\_MF\_IN path, and a no-op for every other tap. There is no arm filter here and none is wanted: the cascade has already band-limited this node and the AGC has already levelled it, which is the whole reason the tap exists. 


        

<hr>



### function mpsk\_rx\_set\_freq\_est 

_Overwrite the tracked carrier offset (cycles/sample at the LO's rate) so the next output de-rotates by exactly_ `val` _._
```C++
void mpsk_rx_set_freq_est (
    mpsk_rx_loops_t * l,
    double val
) 
```




<hr>



### function mpsk\_rx\_set\_telemetry 

_Attach (or detach) telemetry across both loops; see_ [_**mpsk\_receiver\_set\_telemetry()**_](mpsk__receiver__core_8h.md#function-mpsk_receiver_set_telemetry) _, which forwards here._
```C++
int mpsk_rx_set_telemetry (
    mpsk_rx_loops_t * l,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```




<hr>



### function mpsk\_rx\_steer 

_Filter a carrier phase error and update_ `freq_ctrl` _._
```C++
JM_FORCEINLINE  JM_HOT void mpsk_rx_steer (
    mpsk_rx_loops_t * l,
    double pe
) 
```



**The negation is load-bearing.** A DDC mixes with its LO directly (`x * lo_step_ctrl(...)`), where carrier\_nda's older loop mixed with the conjugate (`x * conjf(lo_step_ctrl(...))`), so the same physical de-rotation is the opposite sign on this port. Without it the loop is positive feedback and the M-th-power S-curve's stable and unstable equilibria swap: the receiver locks _hard_ onto the half-way grid, timing and symbol count look perfect, and the only tell is the carrier lock metric sitting at a steady **negative** value (-0.48 for QPSK, where +0.62 is a real lock) while every symbol lands on a decision boundary. 


        

<hr>



### function mpsk\_rx\_symbol\_to\_bits 

_Slice one recovered symbol to its log2(M) hard bits (LSB-first)._ 
```C++
int mpsk_rx_symbol_to_bits (
    mpsk_rx_loops_t * l,
    float complex y,
    uint8_t * bits
) 
```





**Returns:**

The bit count written to `bits`. 





        

<hr>



### function mpsk\_rx\_take\_output 

_Fold one terminal-stage output into both loops._ 
```C++
JM_FORCEINLINE  JM_HOT int mpsk_rx_take_output (
    mpsk_rx_loops_t * l,
    float complex y,
    float complex * sym,
    int ted
) 
```



The receiver's whole per-output body, shared verbatim by the complex- and real-input types. On an on-time strobe it writes the recovered symbol and returns 1.




**Parameters:**


* `l` Loops. Must be non-NULL. 
* `y` One matched-filtered output from the front end. 
* `sym` Receives the recovered symbol when the return is 1. 
* `ted` RATESYNC\_TED\_GARDNER or RATESYNC\_TED\_DTTL — pass a literal for a specialised (branch-free) instantiation. 



**Returns:**

1 if this output was an on-time strobe, 0 otherwise. 





        

<hr>



### function mpsk\_rx\_tlm\_flush 

_Emit the receiver's own probes plus the timing loop's. Out-of-line on purpose; callers gate on_ `l->tlm.ctx` _._
```C++
void mpsk_rx_tlm_flush (
    const mpsk_rx_loops_t * l
) 
```




<hr>



### function mpsk\_rx\_updates\_per\_symbol 

_How many times per symbol the chosen tap updates the carrier loop._ 
```C++
JM_FORCEINLINE double mpsk_rx_updates_per_symbol (
    const mpsk_rx_loops_t * l
) 
```



The tap's whole point, as one number: it is both the factor by which the discriminator's unambiguous frequency range grows over the symbol-rate case, and the loop filter's update rate (see config\_carrier). 


        

<hr>
## Macro Definition Documentation





### define MPSK\_RX\_AGC\_ALPHA 

```C++
#define MPSK_RX_AGC_ALPHA `0.01`
```




<hr>



### define MPSK\_RX\_AGC\_BW\_RATIO 

```C++
#define MPSK_RX_AGC_BW_RATIO `0.05`
```




<hr>



### define MPSK\_RX\_AGC\_RATIO\_DEFAULT 

_AGC bandwidth ratio, derived: 20x slower than the slowest loop it feeds. The RATIO is the part that is not negotiable (see the block above); the value is_ `MPSK_RX_AGC_BW_RATIO` _, and it is a parameter only because the right separation depends on how fast the channel's LEVEL moves against its phase and timing. Zero asks for the default rather than being rejected._
```C++
#define MPSK_RX_AGC_RATIO_DEFAULT `MPSK_RX_AGC_BW_RATIO`
```




<hr>



### define MPSK\_RX\_EPS 

```C++
#define MPSK_RX_EPS `1e-12`
```




<hr>



### define MPSK\_RX\_HANDOVER\_DOWN 

```C++
#define MPSK_RX_HANDOVER_DOWN `0.8`
```




<hr>



### define MPSK\_RX\_HANDOVER\_N\_DOWN 

```C++
#define MPSK_RX_HANDOVER_N_DOWN `32u`
```




<hr>



### define MPSK\_RX\_HANDOVER\_N\_UP 

```C++
#define MPSK_RX_HANDOVER_N_UP `8u`
```




<hr>



### define MPSK\_RX\_LOCK\_THRESH\_DEFAULT 

_Lock threshold, derived:_ `sigma_H0 * eta(Pfa)` _at_`Pfa = 5e-6` _._
```C++
#define MPSK_RX_LOCK_THRESH_DEFAULT `0.4999`
```



`0.1132 * 4.4159 = 0.4999`, which is the 0.5 that shipped — so this row changes no behaviour and is here because a number that was picked and a number that was derived look identical until one of them has to move. The limited statistic reads ~1.0 at lock for EVERY M (§4), so no per-M correction is carried.


### It is sized against H0 alone, and that bounds where it means anything



`sigma_H0 * eta(Pfa)` is a FALSE-ALARM threshold: it answers "how high must
the statistic be before noise alone rarely reaches it". The other half of a detector's sizing — how often the statistic clears it when the receiver IS locked — depends on Es/N0, and "reads ~1.0 at lock" carried no Es/N0 with it until this block. Measured over the scored window, at the geometry the standard battery uses (`docs/design/rx-test.md`; the duty cycles are in the standard record, `dp_rx_result_t::lock_duty`):



|Es/N0 (BPSK)   ||`locked` duty   |statistic &gt; 0    |
|-----|-----|-----|-----|
|6.79 dB   |SER = 1e-3   |**100 %**   |100 %    |
|+1 dB   ||69 %   |100 %    |
|0 dB   ||**24 %**   |100 %    |
|−3 dB   ||0.2 %   |95 %   |






At 0 dB the loops are tracking — the statistic is positive throughout, and a concatenated link over that same record delivers **error-free frames** (`docs/design/fec-receive.md` §8). What refuses is the threshold.


**So this default is an UNCODED-link indicator.** A caller running below its own SER = 1e-3 anchor — which is where forward error correction exists to put you — must not gate on `mpsk_receiver_get_locked()`. Pass a threshold sized for the link, or gate on something that works there: frame synchronization, or the node-sync statistic (`node_sync_score`), which in lock reads the channel symbol error rate directly.


doppler#835 carries the measurement and the options; nothing here has changed behaviour, because a threshold that moves silently is worse than one whose scope is written down. 



        

<hr>



### define MPSK\_RX\_LOOPS\_STATE\_MAGIC 

```C++
#define MPSK_RX_LOOPS_STATE_MAGIC `DP_FOURCC ('M', 'R', 'X', 'L')`
```




<hr>



### define MPSK\_RX\_LOOPS\_STATE\_VERSION 

```C++
#define MPSK_RX_LOOPS_STATE_VERSION `6u`
```




<hr>



### define MPSK\_RX\_M\_OUT\_DEFAULT 

```C++
#define MPSK_RX_M_OUT_DEFAULT `8`
```




<hr>



### define MPSK\_RX\_NUM\_PHASES 

```C++
#define MPSK_RX_NUM_PHASES `1024u`
```




<hr>



### define MPSK\_RX\_NUM\_PHASES\_DEFAULT 

_Matched-filter bank arms, derived: the measured saturation point._ 
```C++
#define MPSK_RX_NUM_PHASES_DEFAULT `64u`
```



64 against a shipped 1024 — a 16x bank for no measurable gain. The arms set the fractional-timing resolution to 1/N of an output period, and the measurement (design/mpsk.md §8) finds it saturating at 64 on RRC and inert at every value on I&D. 


        

<hr>



### define MPSK\_RX\_ZETA\_DEFAULT 

_Loop damping, derived:_ `1/sqrt(2)` _, critically damped._
```C++
#define MPSK_RX_ZETA_DEFAULT `0.70710678118654752`
```



A constant, not a computation — nothing in this receiver moves the optimal damping, and both loops already share one value. It is a parameter only because it was once thought to be one. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/mpsk_receiver/mpsk_rx_loops.h`

