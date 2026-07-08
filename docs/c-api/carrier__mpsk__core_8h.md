

# File carrier\_mpsk\_core.h



[**FileList**](files.md) **>** [**carrier\_mpsk**](dir_aac9a6642a6538588e08cd0551821cb3.md) **>** [**carrier\_mpsk\_core.h**](carrier__mpsk__core_8h.md)

[Go to the source code of this file](carrier__mpsk__core_8h_source.md)

_M-PSK carrier-tracking loop (integer-NCO de-rotation + decision PLL)._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "lo/lo_core.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include "mpsk/mpsk_core.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) <br>_M-PSK carrier loop state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**carrier\_mpsk\_configure**](#function-carrier_mpsk_configure) ([**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state, double bn, double zeta) <br> |
|  [**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* | [**carrier\_mpsk\_create**](#function-carrier_mpsk_create) (double bn, double zeta, double init\_norm\_freq, size\_t tsamps, double bn\_fll, int m) <br>_Create an M-PSK carrier loop instance._  |
|  void | [**carrier\_mpsk\_destroy**](#function-carrier_mpsk_destroy) ([**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state) <br>_Destroy an M-PSK carrier loop instance and release all memory._  |
|  double | [**carrier\_mpsk\_get\_bn**](#function-carrier_mpsk_get_bn) (const [**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state) <br> |
|  double | [**carrier\_mpsk\_get\_bn\_fll**](#function-carrier_mpsk_get_bn_fll) (const [**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state) <br> |
|  double | [**carrier\_mpsk\_get\_last\_error**](#function-carrier_mpsk_get_last_error) (const [**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state) <br> |
|  double | [**carrier\_mpsk\_get\_lock\_metric**](#function-carrier_mpsk_get_lock_metric) (const [**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state) <br> |
|  int | [**carrier\_mpsk\_get\_m**](#function-carrier_mpsk_get_m) (const [**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state) <br> |
|  double | [**carrier\_mpsk\_get\_norm\_freq**](#function-carrier_mpsk_get_norm_freq) (const [**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state) <br> |
|  void | [**carrier\_mpsk\_get\_state**](#function-carrier_mpsk_get_state) (const [**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state, void \* blob) <br>_Serialize the full loop state into_ `blob` _._ |
|  void | [**carrier\_mpsk\_init**](#function-carrier_mpsk_init) ([**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* s, double bn, double zeta, double init\_norm\_freq, size\_t tsamps, double bn\_fll, int m) <br>_Initialise an M-PSK carrier loop in place (no allocation)._  |
|  void | [**carrier\_mpsk\_reset**](#function-carrier_mpsk_reset) ([**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state) <br>_Re-seed the loop to its create-time frequency/phase; keep config._  |
|  void | [**carrier\_mpsk\_set\_bn**](#function-carrier_mpsk_set_bn) ([**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state, double val) <br> |
|  void | [**carrier\_mpsk\_set\_bn\_fll**](#function-carrier_mpsk_set_bn_fll) ([**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state, double val) <br> |
|  void | [**carrier\_mpsk\_set\_norm\_freq**](#function-carrier_mpsk_set_norm_freq) ([**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state, double val) <br> |
|  int | [**carrier\_mpsk\_set\_state**](#function-carrier_mpsk_set_state) ([**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state, const void \* blob) <br>_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._  |
|  size\_t | [**carrier\_mpsk\_state\_bytes**](#function-carrier_mpsk_state_bytes) (const [**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  size\_t | [**carrier\_mpsk\_steps**](#function-carrier_mpsk_steps) ([**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br> |
|  size\_t | [**carrier\_mpsk\_steps\_max\_out**](#function-carrier_mpsk_steps_max_out) ([**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* state) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**carrier\_mpsk\_update**](#function-carrier_mpsk_update) ([**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* s, float complex P) <br>_Per-symbol carrier update: decision discriminator -&gt; loop -&gt; NCO._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float complex | [**carrier\_mpsk\_wipeoff**](#function-carrier_mpsk_wipeoff) ([**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) \* s, float complex x) <br>_Per-sample carrier wipe-off: de-rotate_ `x` _by the NCO, advance it._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CARRIER\_MPSK\_EPS**](carrier__mpsk__core_8h.md#define-carrier_mpsk_eps)  `1e-12f`<br> |
| define  | [**CARRIER\_MPSK\_LOCK\_ALPHA**](carrier__mpsk__core_8h.md#define-carrier_mpsk_lock_alpha)  `0.1`<br> |
| define  | [**CARRIER\_MPSK\_STATE\_MAGIC**](carrier__mpsk__core_8h.md#define-carrier_mpsk_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc)('C', 'M', 'P', 'K')`<br> |
| define  | [**CARRIER\_MPSK\_STATE\_VERSION**](carrier__mpsk__core_8h.md#define-carrier_mpsk_state_version)  `1u`<br> |

## Detailed Description


The M-ary generalization of the Costas loop ([**costas\_core.h**](costas__core_8h.md)): per sample it de-rotates the input with the integer-phase `lo` NCO (carrier wipe-off); every `tsamps` samples it dumps the coherent integrate-and-dump prompt, runs a **decision-directed M-PSK** phase discriminator, filters the error through an embedded 2nd-order [**loop\_filter\_state\_t**](structloop__filter__state__t.md), and steers the NCO frequency + phase. It tracks a small _residual_ carrier (bulk Doppler is removed upstream by acquisition); the steering NCO is `lo`, so the phase is bounded and exactly reproducible.


The discriminator slices the prompt to the nearest constellation point `ahat = mpsk_slice(P, m)` and uses `e = Im(P * conj(ahat)) / |P|` (= sin of the phase error near lock). At `m` = 2 this reduces _exactly_ to the BPSK Costas discriminator. An optional decision-directed cross-product **FLL assist** (`bn_fll > 0`) widens the frequency pull-in.


The loop locks to one of `m` phases — an **M-fold ambiguity** on absolute phase. Resolve it downstream with differential demapping (mpsk\_diff\_demap) or a sync word; this loop only recovers the carrier and emits the prompts.


The block API (carrier\_mpsk\_steps) is the Python face; the JM\_FORCEINLINE [**carrier\_mpsk\_wipeoff()**](carrier__mpsk__core_8h.md#function-carrier_mpsk_wipeoff)/carrier\_mpsk\_update() are the C composition API a receiver inlines into its own sample loop.



```C++
// QPSK carrier loop, 64 samples/symbol, FLL-assisted
carrier_mpsk_state_t *c = carrier_mpsk_create(0.05, 0.707, 0.0, 64, 0.01, 4);
float complex sym[16];
size_t k = carrier_mpsk_steps(c, rx, rx_len, sym, 16);
double f = c->nco.norm_freq;                 // tracked residual carrier
carrier_mpsk_destroy(c);
```
 


    
## Public Functions Documentation




### function carrier\_mpsk\_configure 

```C++
void carrier_mpsk_configure (
    carrier_mpsk_state_t * state,
    double bn,
    double zeta
) 
```




<hr>



### function carrier\_mpsk\_create 

_Create an M-PSK carrier loop instance._ 
```C++
carrier_mpsk_state_t * carrier_mpsk_create (
    double bn,
    double zeta,
    double init_norm_freq,
    size_t tsamps,
    double bn_fll,
    int m
) 
```





**Parameters:**


* `bn` Loop noise bandwidth (default 0.05). 
* `zeta` Damping factor (default 0.707). 
* `init_norm_freq` Seed carrier frequency, cycles/sample (default 0.0). 
* `tsamps` Samples per symbol (default 64). 
* `bn_fll` FLL-assist bandwidth (default 0.0 = pure PLL). 
* `m` Constellation order M, 2/4/8 (default 4 = QPSK). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**carrier\_mpsk\_destroy()**](carrier__mpsk__core_8h.md#function-carrier_mpsk_destroy) when done. 





        

<hr>



### function carrier\_mpsk\_destroy 

_Destroy an M-PSK carrier loop instance and release all memory._ 
```C++
void carrier_mpsk_destroy (
    carrier_mpsk_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function carrier\_mpsk\_get\_bn 

```C++
double carrier_mpsk_get_bn (
    const carrier_mpsk_state_t * state
) 
```




<hr>



### function carrier\_mpsk\_get\_bn\_fll 

```C++
double carrier_mpsk_get_bn_fll (
    const carrier_mpsk_state_t * state
) 
```




<hr>



### function carrier\_mpsk\_get\_last\_error 

```C++
double carrier_mpsk_get_last_error (
    const carrier_mpsk_state_t * state
) 
```




<hr>



### function carrier\_mpsk\_get\_lock\_metric 

```C++
double carrier_mpsk_get_lock_metric (
    const carrier_mpsk_state_t * state
) 
```




<hr>



### function carrier\_mpsk\_get\_m 

```C++
int carrier_mpsk_get_m (
    const carrier_mpsk_state_t * state
) 
```




<hr>



### function carrier\_mpsk\_get\_norm\_freq 

```C++
double carrier_mpsk_get_norm_freq (
    const carrier_mpsk_state_t * state
) 
```




<hr>



### function carrier\_mpsk\_get\_state 

_Serialize the full loop state into_ `blob` _._
```C++
void carrier_mpsk_get_state (
    const carrier_mpsk_state_t * state,
    void * blob
) 
```




<hr>



### function carrier\_mpsk\_init 

_Initialise an M-PSK carrier loop in place (no allocation)._ 
```C++
void carrier_mpsk_init (
    carrier_mpsk_state_t * s,
    double bn,
    double zeta,
    double init_norm_freq,
    size_t tsamps,
    double bn_fll,
    int m
) 
```



Seeds the NCO at `init_norm_freq` and the loop integrator to the matching per-symbol frequency so de-rotation is correct from the first sample.




**Parameters:**


* `s` State to initialise. Must be non-NULL. 
* `bn` Loop noise bandwidth, normalised to the symbol rate. 
* `zeta` Damping factor (0.707 = critically damped). 
* `init_norm_freq` Seed carrier frequency, cycles/sample. 
* `tsamps` Samples per symbol (the integrate-and-dump period). 
* `bn_fll` FLL-assist bandwidth (0 = pure PLL). 
* `m` Constellation order M (2, 4, 8). 




        

<hr>



### function carrier\_mpsk\_reset 

_Re-seed the loop to its create-time frequency/phase; keep config._ 
```C++
void carrier_mpsk_reset (
    carrier_mpsk_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function carrier\_mpsk\_set\_bn 

```C++
void carrier_mpsk_set_bn (
    carrier_mpsk_state_t * state,
    double val
) 
```




<hr>



### function carrier\_mpsk\_set\_bn\_fll 

```C++
void carrier_mpsk_set_bn_fll (
    carrier_mpsk_state_t * state,
    double val
) 
```




<hr>



### function carrier\_mpsk\_set\_norm\_freq 

```C++
void carrier_mpsk_set_norm_freq (
    carrier_mpsk_state_t * state,
    double val
) 
```




<hr>



### function carrier\_mpsk\_set\_state 

_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._ 
```C++
int carrier_mpsk_set_state (
    carrier_mpsk_state_t * state,
    const void * blob
) 
```




<hr>



### function carrier\_mpsk\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t carrier_mpsk_state_bytes (
    const carrier_mpsk_state_t * state
) 
```




<hr>



### function carrier\_mpsk\_steps 

```C++
size_t carrier_mpsk_steps (
    carrier_mpsk_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```




<hr>



### function carrier\_mpsk\_steps\_max\_out 

```C++
size_t carrier_mpsk_steps_max_out (
    carrier_mpsk_state_t * state
) 
```




<hr>



### function carrier\_mpsk\_update 

_Per-symbol carrier update: decision discriminator -&gt; loop -&gt; NCO._ 
```C++
JM_FORCEINLINE  JM_HOT void carrier_mpsk_update (
    carrier_mpsk_state_t * s,
    float complex P
) 
```



Slices the prompt `P` to the nearest M-PSK point `ahat`, forms the decision-directed phase error `e = Im(P conj(ahat)) / |P|`, optionally runs a decision-directed cross-product FLL on the data-wiped prompts, filters, and steers the NCO frequency + a proportional phase nudge. Updates the lock metric (decision-aligned `Re(P conj(ahat))/|P|`) and last\_error. Inline.




**Parameters:**


* `s` Carrier loop state. Must be non-NULL. 
* `P` The dumped integrate-and-dump prompt for this symbol. 




        

<hr>



### function carrier\_mpsk\_wipeoff 

_Per-sample carrier wipe-off: de-rotate_ `x` _by the NCO, advance it._
```C++
JM_FORCEINLINE  JM_HOT float complex carrier_mpsk_wipeoff (
    carrier_mpsk_state_t * s,
    float complex x
) 
```



`x * conj(lo_step(nco))` — strips the (tracked) carrier ahead of the matched-filter integrate-and-dump. Inline, zero call overhead.




**Parameters:**


* `s` Carrier loop state. Must be non-NULL. 
* `x` One input sample. 



**Returns:**

The de-rotated sample to feed the integrator. 





        

<hr>
## Macro Definition Documentation





### define CARRIER\_MPSK\_EPS 

```C++
#define CARRIER_MPSK_EPS `1e-12f`
```




<hr>



### define CARRIER\_MPSK\_LOCK\_ALPHA 

```C++
#define CARRIER_MPSK_LOCK_ALPHA `0.1`
```




<hr>



### define CARRIER\_MPSK\_STATE\_MAGIC 

```C++
#define CARRIER_MPSK_STATE_MAGIC `DP_FOURCC ('C', 'M', 'P', 'K')`
```




<hr>



### define CARRIER\_MPSK\_STATE\_VERSION 

```C++
#define CARRIER_MPSK_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/carrier_mpsk/carrier_mpsk_core.h`

