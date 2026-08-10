

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
| enum  | [**mpsk\_\_rx\_\_loops\_8h\_1a99fb83031ce9923c84392b4e92f956b5**](#enum-mpsk__rx__loops_8h_1a99fb83031ce9923c84392b4e92f956b5)  <br>_Where the NDA carrier discriminator reads from._  |
| enum  | [**mpsk\_\_rx\_\_loops\_8h\_1adf764cbdea00d65edcd07bb9953ad2b7**](#enum-mpsk__rx__loops_8h_1adf764cbdea00d65edcd07bb9953ad2b7)  <br>_Matched-filter pulse shape. Aliases of the cascade's own vocabulary so one set of names covers the whole family._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**mpsk\_rx\_agc\_bn**](#function-mpsk_rx_agc_bn) (double bn\_carrier, double bn\_timing, double ratio) <br> |
|  void | [**mpsk\_rx\_configure\_lock**](#function-mpsk_rx_configure_lock) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune the handover detector; see_ [_**mpsk\_receiver\_configure\_lock()**_](mpsk__receiver__core_8h.md#function-mpsk_receiver_configure_lock) _, which forwards here. A live handover survives; the verify run restarts._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**mpsk\_rx\_disc**](#function-mpsk_rx_disc) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, float complex z) <br>_Run the NDA discriminator on one tapped sample._  |
|  double | [**mpsk\_rx\_freq\_est**](#function-mpsk_rx_freq_est) (const [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l) <br>_Tracked carrier offset in cycles/sample at the LO's rate — the loop's own estimate, excluding the front end's configured centre._  |
|  void | [**mpsk\_rx\_loops\_get\_state**](#function-mpsk_rx_loops_get_state) (const [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, void \* blob) <br>_Serialize the loops' mutable state into_ `blob` _._ |
|  void | [**mpsk\_rx\_loops\_init**](#function-mpsk_rx_loops_init) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, int m, double sps, double lo\_sps, size\_t m\_out, double bn\_carrier, double zeta, double bn\_timing, double bn\_agc\_ratio, int ted, int acq\_to\_track, double lock\_thresh, size\_t warmup\_syms, int differential, int nda\_tap) <br>_Initialise the loops in place (no allocation)._  |
|  void | [**mpsk\_rx\_loops\_reset**](#function-mpsk_rx_loops_reset) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l) <br>_Re-seed both loops to their post-init state; keep configuration._  |
|  int | [**mpsk\_rx\_loops\_set\_state**](#function-mpsk_rx_loops_set_state) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, const void \* blob) <br>_Restore the loops' mutable state from_ `blob` _._ |
|  size\_t | [**mpsk\_rx\_loops\_state\_bytes**](#function-mpsk_rx_loops_state_bytes) (const [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l) <br>_Bytes_ [_**mpsk\_rx\_loops\_get\_state()**_](mpsk__rx__loops_8h.md#function-mpsk_rx_loops_get_state) _writes._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**mpsk\_rx\_push\_lo**](#function-mpsk_rx_push_lo) ([**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) \* l, float complex z) <br>_Feed one post-LO, pre-cascade sample to the free-running arm._  |
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
| define  | [**MPSK\_RX\_ARM\_DIV**](mpsk__rx__loops_8h.md#define-mpsk_rx_arm_div)  `2u`<br> |
| define  | [**MPSK\_RX\_EPS**](mpsk__rx__loops_8h.md#define-mpsk_rx_eps)  `1e-12`<br> |
| define  | [**MPSK\_RX\_HANDOVER\_DOWN**](mpsk__rx__loops_8h.md#define-mpsk_rx_handover_down)  `0.8`<br> |
| define  | [**MPSK\_RX\_HANDOVER\_N\_DOWN**](mpsk__rx__loops_8h.md#define-mpsk_rx_handover_n_down)  `32u`<br> |
| define  | [**MPSK\_RX\_HANDOVER\_N\_UP**](mpsk__rx__loops_8h.md#define-mpsk_rx_handover_n_up)  `8u`<br> |
| define  | [**MPSK\_RX\_LOOPS\_STATE\_MAGIC**](mpsk__rx__loops_8h.md#define-mpsk_rx_loops_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('M', 'R', 'X', 'L')`<br> |
| define  | [**MPSK\_RX\_LOOPS\_STATE\_VERSION**](mpsk__rx__loops_8h.md#define-mpsk_rx_loops_state_version)  `5u`<br> |
| define  | [**MPSK\_RX\_M\_OUT\_DEFAULT**](mpsk__rx__loops_8h.md#define-mpsk_rx_m_out_default)  `8`<br> |
| define  | [**MPSK\_RX\_NUM\_PHASES**](mpsk__rx__loops_8h.md#define-mpsk_rx_num_phases)  `1024u`<br> |

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

_Where the NDA carrier discriminator reads from._ 
```C++
enum mpsk__rx__loops_8h_1a99fb83031ce9923c84392b4e92f956b5 {
    MPSK_RX_NDA_TAP_STROBE = 0,
    MPSK_RX_NDA_TAP_MF_ALL = 1,
    MPSK_RX_NDA_TAP_LO_ARM = 2
};
```



An M-th-power discriminator updating at rate `F` can only observe a frequency error of `|df| < F/(2M)` — above that its M-th-power phase advances more than pi per update and the error folds. So the tap point IS the pull-in range, and it trades directly against signal quality:



|tap   |update rate   |unambiguous \|df\|   |cost    |
|-----|-----|-----|-----|
|`STROBE`   |`Rs`   |`Rs/(2M)`   |needs symbol timing    |
|`MF_ALL`   |`m_out*Rs`   |`m_out*Rs/(2M)`   |inter-symbol ISI bias    |
|`LO_ARM`   |LO rate   |`f_lo/(2M)`   |no matched filtering   |






There is a second axis, and it is the one the cascade rebuild lost. `STROBE` is the only tap that depends on **symbol timing**: it reads the one output the timing loop nominates, so before timing lock it is reading an arbitrary phase of the pulse. `MF_ALL` consumes every terminal output and so does not care which one is on-time; `LO_ARM` runs a free-running arm filter ahead of the cascade entirely. Both therefore restore the property the NDA path exists for — acquiring with no data _and no symbol timing_ — which is why they are not merely "wider".


No tap waits. `STROBE` steers from its first strobe whether or not the timing loop has declared, so its dependency is a reason to CHOOSE another tap when the carrier must acquire before timing does — not something the receiver resolves behind the caller's back. Gating it was tried and measured: across a 24-cell sweep it moved one cell, because what it really bought was a carrier transient that started at a known instant and was therefore easier to measure.


Fixed at construction: the caller picks the trade once, and nothing switches underneath it. 


        

<hr>



### enum mpsk\_\_rx\_\_loops\_8h\_1adf764cbdea00d65edcd07bb9953ad2b7 

_Matched-filter pulse shape. Aliases of the cascade's own vocabulary so one set of names covers the whole family._ 
```C++
enum mpsk__rx__loops_8h_1adf764cbdea00d65edcd07bb9953ad2b7 {
    MPSK_RX_PULSE_IANDD = RC_PULSE_IANDD,
    MPSK_RX_PULSE_RRC = RC_PULSE_RRC
};
```




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
    size_t warmup_syms,
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
* `warmup_syms` Symbols before the handover is allowed. 
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



### function mpsk\_rx\_push\_lo 

_Feed one post-LO, pre-cascade sample to the free-running arm._ 
```C++
JM_FORCEINLINE  JM_HOT void mpsk_rx_push_lo (
    mpsk_rx_loops_t * l,
    float complex z
) 
```



The MPSK\_RX\_NDA\_TAP\_LO\_ARM path, and a no-op for every other tap. Called once per LO step by the owning receiver, ahead of the cascade — which is exactly why this tap needs no symbol timing and reaches the widest frequency range the front end can offer. 


        

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



### define MPSK\_RX\_ARM\_DIV 

```C++
#define MPSK_RX_ARM_DIV `2u`
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



### define MPSK\_RX\_LOOPS\_STATE\_MAGIC 

```C++
#define MPSK_RX_LOOPS_STATE_MAGIC `DP_FOURCC ('M', 'R', 'X', 'L')`
```




<hr>



### define MPSK\_RX\_LOOPS\_STATE\_VERSION 

```C++
#define MPSK_RX_LOOPS_STATE_VERSION `5u`
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

------------------------------
The documentation for this class was generated from the following file `native/inc/mpsk_receiver/mpsk_rx_loops.h`

