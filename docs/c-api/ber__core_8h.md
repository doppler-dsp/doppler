

# File ber\_core.h



[**FileList**](files.md) **>** [**ber**](dir_b6e9705448f5ec813187161d6664687c.md) **>** [**ber\_core.h**](ber__core_8h.md)

[Go to the source code of this file](ber__core_8h_source.md)

_Error-rate measurement: settled windows, detected alignment, and an exact confidence interval._ [More...](#detailed-description)

* `#include "dp_state.h"`
* `#include <complex.h>`
* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**ber\_align\_t**](structber__align__t.md) <br>_Where the recovered stream sits against truth, and how sure._  |
| struct | [**ber\_interval\_t**](structber__interval__t.md) <br>_A rate with its exact interval. Assert on_ `lo` _, never on_`p_hat` _._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**ber\_interval\_t**](structber__interval__t.md) | [**ber\_confidence**](#function-ber_confidence) (size\_t errors, size\_t symbols, double conf) <br>_Exact confidence interval for a run stopped on an ERROR count._  |
|  double | [**ber\_esn0\_db\_for\_ser**](#function-ber_esn0_db_for_ser) (int m, double ser) <br>_Es/N0 (dB) at which the coherent bound equals_ `ser` _._ |
|  double | [**ber\_evm\_db**](#function-ber_evm_db) (const float complex \* rx, size\_t rx\_len, size\_t lo, size\_t hi, int m) <br>_Self-referenced EVM (dB) over an EXPLICIT window_ `[lo, hi)` _._ |
|  double | [**ber\_evm\_scatter\_floor\_db**](#function-ber_evm_scatter_floor_db) (int m) <br>_EVM (dB) of an M-PSK constellation at a UNIFORMLY RANDOM rotation._  |
|  int | [**ber\_lock\_symbol**](#function-ber_lock_symbol) (const uint8\_t \* flags, size\_t flags\_len, size\_t sustain, double min\_frac) <br>_First symbol from which a verify-counted flag is SUSTAINED._  |
|  double | [**ber\_qfunc**](#function-ber_qfunc) (double x) <br>_Gaussian tail_ `Q(x) = P(N(0,1) > x)` _._ |
|  size\_t | [**ber\_settle\_from**](#function-ber_settle_from) (size\_t budget, int timing\_lock, int carrier\_lock) <br>_Combine an analytic settling budget with measured lock instants._  |
|  size\_t | [**ber\_settle\_syms**](#function-ber_settle_syms) (double bn\_timing, double bn\_carrier) <br>_Symbols to discard before a steady-state measurement means anything._  |
|  double | [**ber\_theory\_ber**](#function-ber_theory_ber) (int m, double esn0) <br>_Coherent GRAY-coded M-PSK bit error rate at Es/N0 (LINEAR). BPSK and Gray QPSK are exactly_ `Q(sqrt(2 Eb/N0))` _; 8PSK uses_`SER/log2 M` _, exact in the high-Es/N0 limit where an error lands on a neighbour._ |
|  double | [**ber\_theory\_ser**](#function-ber_theory_ser) (int m, double esn0) <br>_Coherent M-PSK symbol error rate at matched-filter Es/N0 (LINEAR)._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BER\_CONF**](ber__core_8h.md#define-ber_conf)  `0.99`<br>_Default two-sided confidence level._  |
| define  | [**BER\_LAG\_SPAN**](ber__core_8h.md#define-ber_lag_span)  `200`<br>_Default lag search half-width, symbols. Generous on purpose: group delay varies a lot with pulse shape and front end (RRC on the complex path needed -34), and a clipped search reported SER 0.48 on a perfect receiver._  |
| define  | [**BER\_MAX\_LAGS**](ber__core_8h.md#define-ber_max_lags)  `2048`<br>_Largest lag search the fixed-size scratch supports._  |
| define  | [**BER\_SYNC\_PFA**](ber__core_8h.md#define-ber_sync_pfa)  `1e-6`<br>_Default whole-search false-alarm probability for the align gate._  |
| define  | [**BER\_SYNC\_SYMS**](ber__core_8h.md#define-ber_sync_syms)  `256u`<br>_Default marker length when aligning on a stretch of truth._  |
| define  | [**BER\_TARGET\_ERRORS**](ber__core_8h.md#define-ber_target_errors)  `200u`<br>_Default error target: ~7% relative standard error (_ `1/sqrt(r)` _)._ |
| define  | [**BER\_TARGET\_SER**](ber__core_8h.md#define-ber_target_ser)  `1e-3`<br>_Default operating point: the SER a characterisation anchors at._  |

## Detailed Description


**An error rate is a measurement, and this module is its instrument.** Every confidently-wrong receiver number this project has produced came from one of three places, and each has a gate here. A measurement that has not passed all three is not a result:



* **Is it SETTLED?** A second-order loop needs ~5/Bn symbols; two cascaded loops ADD their budgets; joint tracking DOUBLES the sum. So the floor is `2*(5/bn_timing + 5/bn_carrier)` — [**ber\_settle\_syms()**](ber__core_8h.md#function-ber_settle_syms) — and the window must additionally clear every lock indicator the receiver publishes, plus the handover instant again when one is enabled. Measuring inside that window measures settling and reports it as steady state: measured cost, -9.0 dB EVM where the settled answer is -23.2 dB, and SER 5.9x the coherent bound where the settled answer is 1.7x.
* **Have we counted enough ERRORS?** Fix the ERROR count and let the symbol count fall out (inverse binomial sampling). The relative standard error is then `1/sqrt(r)` — a function of the error count ALONE — so the error target IS the precision. Stopping on a fixed symbol count makes precision depend on the very rate being measured: 20 000 symbols at SER 1e-3 gives ~20 errors and ~22% relative error, which reads as real seed-to-seed variation in the receiver and is not.
* **Does it MAKE SENSE?** Cross-check against measurements that cannot fail the same way: the truth-free EVM ([**ber\_evm\_scatter\_floor\_db()**](ber__core_8h.md#function-ber_evm_scatter_floor_db) bounds what it can prove) and the coherent theory curve ([**ber\_theory\_ser()**](ber__core_8h.md#function-ber_theory_ser)).




### The alignment is DETECTED, never searched



The historic footgun is scoring `min over (lag, rotation)` of the error count. That is not a measurement of the receiver, it is an optimisation over the answer, and it fails both ways: a wide search on a short window finds a lucky low-error alignment on garbage (false PASS), and a narrow one misses the true alignment on a healthy receiver and reports chance (false FLOOR). Both have shipped here — a committed "~12 dB floor" that was really ~5 dB, and an "SER 0.48" on a receiver running at 0.0000 that needed lag -34.


[**ber\_meter\_align()**](ber__meter__core_8h.md#function-ber_meter_align) _detects_ the alignment instead, correlating against a known marker (a sync word, a PN code period, or — in a simulation, where truth exists — a stretch of the truth sequence itself). It returns the lag and the absolute carrier phase from the correlation peak, gated by a false-alarm probability through the canonical detection primitives and Bonferroni-corrected over the lags searched. A marker too short to identify an alignment reports `ok = 0` rather than a plausible wrong lag, and the marker's own symbols are excluded from scoring so the symbols that fixed the alignment cannot also flatter the rate.



### Reuse, not re-derivation



Nothing numeric is invented here. The confidence interval is the exact Gamma/chi-square one and its quantiles come from `det_threshold()` / `det_threshold_noncoherent()` — doppler's own inverse regularized incomplete gamma, already validated in the detection module — rather than a second copy of a series/continued-fraction kernel. Verified bit-identical to SciPy's `chi2.ppf` at r = 1, 2, 20, 200 and 1000. Gray coding comes from `mpsk`, the blind SNR from `snr`. 



    
## Public Functions Documentation




### function ber\_confidence 

_Exact confidence interval for a run stopped on an ERROR count._ 
```C++
ber_interval_t ber_confidence (
    size_t errors,
    size_t symbols,
    double conf
) 
```



Both quantiles come from doppler's own inverse regularized incomplete gamma rather than a second copy of one: `det_threshold_noncoherent(q, r)` returns the `b` with `marcum_q(r, 0, b) = q`, and `marcum_q(r, 0, b) = Q(r, b^2/2)`, so `chi2_q(2r)/2 = 0.5 * det_threshold_noncoherent(1-q, r)^2`. At `r = 1` that reduces to the closed form and the interval is `[-ln(1-a/2)/N, -ln(a/2)/N]` — no normal approximation anywhere, so it stays honest at the small error counts where a Wald interval is worst.


With `r = 0` there is no point estimate, but the exact one-sided upper limit `-ln(alpha)/N` still holds and is returned — the honest way to report "no errors in N symbols". 


        

<hr>



### function ber\_esn0\_db\_for\_ser 

_Es/N0 (dB) at which the coherent bound equals_ `ser` _._
```C++
double ber_esn0_db_for_ser (
    int m,
    double ser
) 
```



How an implementation loss is quoted honestly: convert the MEASURED rate to the Es/N0 theory would need to produce it, and subtract. A loss in dB is comparable across M and across operating points; a ratio of rates is not. 


        

<hr>



### function ber\_evm\_db 

_Self-referenced EVM (dB) over an EXPLICIT window_ `[lo, hi)` _._
```C++
double ber_evm_db (
    const float complex * rx,
    size_t rx_len,
    size_t lo,
    size_t hi,
    int m
) 
```



Scores each symbol against the stream's OWN hard decision, with the constellation rotation estimated from the data — so it references neither the transmitted symbols nor a lag, and cannot be fooled by an alignment search. At a matched-filter output the error vector IS the complex noise, so a locked stream reads `EVM[dB] ~ -(Es/N0)[dB]`. EVM is an I/Q-plane quantity: there is no factor of two — that belongs to an I-only measurement, and quoting it flatters the result by 3 dB.


**Pass the real `m`**, and read the result against ber\_evm\_scatter\_floor\_db(m), never against 0 dB.


The window is EXPLICIT because BER and EVM must be measured on the SAME one. A convenience "back half" default silently scores a different window than the error rate did, and the two eventually disagree in a way that reads as a receiver defect rather than the harness bug it is.




**Returns:**

EVM in dB, or 0.0 ("no lock") for a window under 20 symbols. 





        

<hr>



### function ber\_evm\_scatter\_floor\_db 

_EVM (dB) of an M-PSK constellation at a UNIFORMLY RANDOM rotation._ 
```C++
double ber_evm_scatter_floor_db (
    int m
) 
```



The FLOOR of a self-referenced EVM: what a completely destroyed constant-modulus constellation reads. Slicing a unit-modulus point at a uniformly random phase to its nearest of M neighbours leaves `E|e|^2 = 2 - 2 sin(pi/M)/(pi/M)`: **-1.4 dB at BPSK, -7.0 at QPSK, -12.9 at 8PSK**.


**Any fixed EVM threshold must be stated against this, never against 0 dB.** "Scattered reads ~0 dB" is the BPSK limit only. At 8PSK a stream with no carrier recovery at all reads -12.9 dB — which is also what a perfectly healthy 13 dB link reads — so a `< -12.0` assertion is satisfied by pure noise. That was live in this repo's own receiver tests until 2026-07-27. The room between "on the bound at the SER=1e-3 anchor" and "completely broken" collapses as M grows: 5.4 dB at BPSK, 3.3 at QPSK, 2.8 at 8PSK, so at high M the EVM cannot carry a verdict by itself. 


        

<hr>



### function ber\_lock\_symbol 

_First symbol from which a verify-counted flag is SUSTAINED._ 
```C++
int ber_lock_symbol (
    const uint8_t * flags,
    size_t flags_len,
    size_t sustain,
    double min_frac
) 
```



"Sustained" is `sustain` consecutive symbols high AND at least `min_frac` of everything after that point high too. Both halves carry weight: the run rejects a single lucky decision, the fraction rejects a detector that declares early then flaps. Dating the lock by the FINAL contiguous run instead is right with no noise and badly wrong with it — one late dip once moved a reported lock from 415 to 2286 and left no measurement window.




**Returns:**

The symbol index, or -1 for "never locked" — the honest answer, which forces the caller to say so rather than measure a transient. 





        

<hr>



### function ber\_qfunc 

_Gaussian tail_ `Q(x) = P(N(0,1) > x)` _._
```C++
double ber_qfunc (
    double x
) 
```




<hr>



### function ber\_settle\_from 

_Combine an analytic settling budget with measured lock instants._ 
```C++
size_t ber_settle_from (
    size_t budget,
    int timing_lock,
    int carrier_lock
) 
```



The POLICY for where a steady-state window may start, in one place: `max(budget, timing lock, carrier lock)`. The analytic budget and the receiver's own indicators are both fallible in the SAME direction, so whichever settles last decides.


There was a fourth term until doppler#877. A receiver that handed the carrier from an NDA discriminator to a decision-directed one settled last of all — the handover fired after every lock indicator and the new loop then had its own transient, so it contributed `its instant + the budget again` (measured on 8PSK at its SER=1e-3 anchor: handover at symbol 2525 against a 2000-symbol budget, and 5.95x the coherent bound if the window started at 2000 rather than 4525). No receiver in this library hands over any more, so the term went with the handover rather than remaining as an argument that could only be passed -1.


Pass -1 for any indicator the receiver does not publish (which is what [**ber\_lock\_symbol()**](ber__core_8h.md#function-ber_lock_symbol) returns for "never locked"). **A -1 timing or carrier lock means there is NO valid steady-state window** — check that yourself before trusting the return.




**Parameters:**


* `budget` [**ber\_settle\_syms()**](ber__core_8h.md#function-ber_settle_syms) of the loops in use. 
* `timing_lock` [**ber\_lock\_symbol()**](ber__core_8h.md#function-ber_lock_symbol) of the timing flag, or -1. 
* `carrier_lock` [**ber\_lock\_symbol()**](ber__core_8h.md#function-ber_lock_symbol) of the carrier flag, or -1. 



**Returns:**

First symbol of the measurement window. 





        

<hr>



### function ber\_settle\_syms 

_Symbols to discard before a steady-state measurement means anything._ 
```C++
size_t ber_settle_syms (
    double bn_timing,
    double bn_carrier
) 
```



`2 * (5/bn_timing + 5/bn_carrier)`. Three factors, and skipping any of them produces a confident wrong number: 5/Bn per loop is the standard second-order settling time (in symbols, because both `bn` are normalised to the SYMBOL rate); the two budgets ADD because the loops are CASCADED (the carrier discriminator reads the on-time strobe, so it cannot converge until timing has); and the sum DOUBLES for joint tracking, where each loop sees the other's transient as a disturbance.


This is a floor, not the answer — take the max of it and every lock indicator the receiver publishes, plus the handover instant again if one is enabled. Pass a loop's `bn` as 0 if it is not running. 


        

<hr>



### function ber\_theory\_ber 

_Coherent GRAY-coded M-PSK bit error rate at Es/N0 (LINEAR). BPSK and Gray QPSK are exactly_ `Q(sqrt(2 Eb/N0))` _; 8PSK uses_`SER/log2 M` _, exact in the high-Es/N0 limit where an error lands on a neighbour._
```C++
double ber_theory_ber (
    int m,
    double esn0
) 
```




<hr>



### function ber\_theory\_ser 

_Coherent M-PSK symbol error rate at matched-filter Es/N0 (LINEAR)._ 
```C++
double ber_theory_ser (
    int m,
    double esn0
) 
```



`BPSK: Q(sqrt(2 Es/N0))`, `QPSK: 2 Q(sqrt(Es/N0))`, `8PSK: 2 Q(sqrt(2 Es/N0) sin(pi/8))` — the nearest-neighbour union bound, tight to well under a percent at any Es/N0 worth testing at.


**This is a COHERENT bound.** A differentially-decoded rate is ~2x it, because a differential decision fails when either of its two symbols is wrong (measured 1.88-2.11 across M and both receiver paths). Pairing a differential measurement with this curve invents a factor of two of "implementation loss". 


        

<hr>
## Macro Definition Documentation





### define BER\_CONF 

_Default two-sided confidence level._ 
```C++
#define BER_CONF `0.99`
```




<hr>



### define BER\_LAG\_SPAN 

_Default lag search half-width, symbols. Generous on purpose: group delay varies a lot with pulse shape and front end (RRC on the complex path needed -34), and a clipped search reported SER 0.48 on a perfect receiver._ 
```C++
#define BER_LAG_SPAN `200`
```




<hr>



### define BER\_MAX\_LAGS 

_Largest lag search the fixed-size scratch supports._ 
```C++
#define BER_MAX_LAGS `2048`
```




<hr>



### define BER\_SYNC\_PFA 

_Default whole-search false-alarm probability for the align gate._ 
```C++
#define BER_SYNC_PFA `1e-6`
```




<hr>



### define BER\_SYNC\_SYMS 

_Default marker length when aligning on a stretch of truth._ 
```C++
#define BER_SYNC_SYMS `256u`
```




<hr>



### define BER\_TARGET\_ERRORS 

_Default error target: ~7% relative standard error (_ `1/sqrt(r)` _)._
```C++
#define BER_TARGET_ERRORS `200u`
```




<hr>



### define BER\_TARGET\_SER 

_Default operating point: the SER a characterisation anchors at._ 
```C++
#define BER_TARGET_SER `1e-3`
```



Anchoring at a fixed error rate rather than a fixed Es/N0 asks "does the
receiver meet its bound" at the same place on the curve for every constellation (per-M, 6.8 / 10.3 / 15.7 dB for BPSK / QPSK / 8PSK). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ber/ber_core.h`

