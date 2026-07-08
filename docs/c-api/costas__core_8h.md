

# File costas\_core.h



[**FileList**](files.md) **>** [**costas**](dir_9b517cb2745356d7938c9e100210a101.md) **>** [**costas\_core.h**](costas__core_8h.md)

[Go to the source code of this file](costas__core_8h_source.md)

_Costas carrier-tracking loop (integer-NCO de-rotation + PI loop)._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "lo/lo_core.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**costas\_state\_t**](structcostas__state__t.md) <br>_Costas loop state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**costas\_configure**](#function-costas_configure) ([**costas\_state\_t**](structcostas__state__t.md) \* state, double bn, double zeta) <br> |
|  [**costas\_state\_t**](structcostas__state__t.md) \* | [**costas\_create**](#function-costas_create) (double bn, double zeta, double init\_norm\_freq, size\_t tsamps, double bn\_fll) <br>_Create a Costas instance._  |
|  void | [**costas\_destroy**](#function-costas_destroy) ([**costas\_state\_t**](structcostas__state__t.md) \* state) <br>_Destroy a Costas instance and release all memory._  |
|  double | [**costas\_get\_bn**](#function-costas_get_bn) (const [**costas\_state\_t**](structcostas__state__t.md) \* state) <br> |
|  double | [**costas\_get\_bn\_fll**](#function-costas_get_bn_fll) (const [**costas\_state\_t**](structcostas__state__t.md) \* state) <br> |
|  double | [**costas\_get\_last\_error**](#function-costas_get_last_error) (const [**costas\_state\_t**](structcostas__state__t.md) \* state) <br> |
|  double | [**costas\_get\_lock\_metric**](#function-costas_get_lock_metric) (const [**costas\_state\_t**](structcostas__state__t.md) \* state) <br> |
|  double | [**costas\_get\_norm\_freq**](#function-costas_get_norm_freq) (const [**costas\_state\_t**](structcostas__state__t.md) \* state) <br> |
|  void | [**costas\_get\_state**](#function-costas_get_state) (const [**costas\_state\_t**](structcostas__state__t.md) \* state, void \* blob) <br>_Serialize the full loop state into_ `blob` _._ |
|  void | [**costas\_init**](#function-costas_init) ([**costas\_state\_t**](structcostas__state__t.md) \* s, double bn, double zeta, double init\_norm\_freq, size\_t tsamps, double bn\_fll) <br>_Initialise a Costas loop in place (no allocation)._  |
|  void | [**costas\_reset**](#function-costas_reset) ([**costas\_state\_t**](structcostas__state__t.md) \* state) <br>_Re-seed the loop to its create-time frequency/phase; keep config._  |
|  void | [**costas\_set\_bn**](#function-costas_set_bn) ([**costas\_state\_t**](structcostas__state__t.md) \* state, double val) <br> |
|  void | [**costas\_set\_bn\_fll**](#function-costas_set_bn_fll) ([**costas\_state\_t**](structcostas__state__t.md) \* state, double val) <br> |
|  void | [**costas\_set\_norm\_freq**](#function-costas_set_norm_freq) ([**costas\_state\_t**](structcostas__state__t.md) \* state, double val) <br> |
|  int | [**costas\_set\_state**](#function-costas_set_state) ([**costas\_state\_t**](structcostas__state__t.md) \* state, const void \* blob) <br>_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._  |
|  size\_t | [**costas\_state\_bytes**](#function-costas_state_bytes) (const [**costas\_state\_t**](structcostas__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  size\_t | [**costas\_steps**](#function-costas_steps) ([**costas\_state\_t**](structcostas__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br> |
|  size\_t | [**costas\_steps\_max\_out**](#function-costas_steps_max_out) ([**costas\_state\_t**](structcostas__state__t.md) \* state) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**costas\_update**](#function-costas_update) ([**costas\_state\_t**](structcostas__state__t.md) \* s, float complex P) <br>_Per-symbol carrier update: discriminator -&gt; loop filter -&gt; steer NCO._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float complex | [**costas\_wipeoff**](#function-costas_wipeoff) ([**costas\_state\_t**](structcostas__state__t.md) \* s, float complex x) <br>_Per-sample carrier wipe-off: de-rotate_ `x` _by the NCO, advance it._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**COSTAS\_EPS**](costas__core_8h.md#define-costas_eps)  `1e-12f`<br> |
| define  | [**COSTAS\_LOCK\_ALPHA**](costas__core_8h.md#define-costas_lock_alpha)  `0.1`<br> |
| define  | [**COSTAS\_STATE\_MAGIC**](costas__core_8h.md#define-costas_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc)('C', 'S', 'T', 'S')`<br> |
| define  | [**COSTAS\_STATE\_VERSION**](costas__core_8h.md#define-costas_state_version)  `1u`<br> |

## Detailed Description


A continuous BPSK carrier-recovery loop: per sample it de-rotates the input with the integer-phase `lo` NCO (carrier wipe-off); every `tsamps` samples it dumps the coherent integrate-and-dump accumulator, runs a decision-directed Costas phase discriminator, filters the error through an embedded 2nd-order [**loop\_filter\_state\_t**](structloop__filter__state__t.md), and steers the NCO frequency + phase. It tracks a small _residual_ carrier offset (the bulk Doppler is removed upstream by FFT acquisition); the steering NCO is `lo`, so the phase is bounded and exactly reproducible (no double-accumulator drift).


The block API (costas\_steps) is the Python face; the JM\_FORCEINLINE [**costas\_wipeoff()**](costas__core_8h.md#function-costas_wipeoff)/costas\_update() are the C composition API a despreader / tracking channel inlines into its own sample loop.


Lifecycle: costas\_create -&gt; [steps / configure / reset]\* -&gt; costas\_destroy, or embed by value with [**costas\_init()**](costas__core_8h.md#function-costas_init).


Set `bn_fll > 0` to enable FLL assist (a wide-pull-in frequency-lock loop aiding the PLL) for large or fast-moving residuals; `bn_fll = 0` is a pure Costas PLL.



```C++
costas_state_t *c = costas_create(0.05, 0.707, 0.01, 64, 0.0);
float complex sym[16];
size_t k = costas_steps(c, rx, rx_len, sym, 16);  // one prompt per symbol
double f = c->nco.norm_freq;                       // tracked residual
costas_destroy(c);
```
 


    
## Public Functions Documentation




### function costas\_configure 

```C++
void costas_configure (
    costas_state_t * state,
    double bn,
    double zeta
) 
```




<hr>



### function costas\_create 

_Create a Costas instance._ 
```C++
costas_state_t * costas_create (
    double bn,
    double zeta,
    double init_norm_freq,
    size_t tsamps,
    double bn_fll
) 
```





**Parameters:**


* `bn` Loop noise bandwidth (default 0.05). 
* `zeta` Damping factor (default 0.707). 
* `init_norm_freq` Seed carrier frequency, cycles/sample (default 0.0). 
* `tsamps` Samples per symbol (default 64). 
* `bn_fll` FLL-assist bandwidth (default 0.0 = pure PLL). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**costas\_destroy()**](costas__core_8h.md#function-costas_destroy) when done. 





        

<hr>



### function costas\_destroy 

_Destroy a Costas instance and release all memory._ 
```C++
void costas_destroy (
    costas_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function costas\_get\_bn 

```C++
double costas_get_bn (
    const costas_state_t * state
) 
```




<hr>



### function costas\_get\_bn\_fll 

```C++
double costas_get_bn_fll (
    const costas_state_t * state
) 
```




<hr>



### function costas\_get\_last\_error 

```C++
double costas_get_last_error (
    const costas_state_t * state
) 
```




<hr>



### function costas\_get\_lock\_metric 

```C++
double costas_get_lock_metric (
    const costas_state_t * state
) 
```




<hr>



### function costas\_get\_norm\_freq 

```C++
double costas_get_norm_freq (
    const costas_state_t * state
) 
```




<hr>



### function costas\_get\_state 

_Serialize the full loop state into_ `blob` _._
```C++
void costas_get_state (
    const costas_state_t * state,
    void * blob
) 
```




<hr>



### function costas\_init 

_Initialise a Costas loop in place (no allocation)._ 
```C++
void costas_init (
    costas_state_t * s,
    double bn,
    double zeta,
    double init_norm_freq,
    size_t tsamps,
    double bn_fll
) 
```



The by-value counterpart to [**costas\_create()**](costas__core_8h.md#function-costas_create): a tracking channel that embeds a [**costas\_state\_t**](structcostas__state__t.md) initialises it here. Seeds the NCO at `init_norm_freq` and the loop integrator to the matching per-symbol frequency so de-rotation is correct from the first sample.




**Parameters:**


* `s` State to initialise. Must be non-NULL. 
* `bn` Loop noise bandwidth, normalised to the symbol rate. 
* `zeta` Damping factor (0.707 = critically damped). 
* `init_norm_freq` Seed carrier frequency, cycles/sample. 
* `tsamps` Samples per symbol (the integrate-and-dump period). 
* `bn_fll` FLL-assist bandwidth (0 = pure PLL). 




        

<hr>



### function costas\_reset 

_Re-seed the loop to its create-time frequency/phase; keep config._ 
```C++
void costas_reset (
    costas_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function costas\_set\_bn 

```C++
void costas_set_bn (
    costas_state_t * state,
    double val
) 
```




<hr>



### function costas\_set\_bn\_fll 

```C++
void costas_set_bn_fll (
    costas_state_t * state,
    double val
) 
```




<hr>



### function costas\_set\_norm\_freq 

```C++
void costas_set_norm_freq (
    costas_state_t * state,
    double val
) 
```




<hr>



### function costas\_set\_state 

_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._ 
```C++
int costas_set_state (
    costas_state_t * state,
    const void * blob
) 
```




<hr>



### function costas\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t costas_state_bytes (
    const costas_state_t * state
) 
```




<hr>



### function costas\_steps 

```C++
size_t costas_steps (
    costas_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```




<hr>



### function costas\_steps\_max\_out 

```C++
size_t costas_steps_max_out (
    costas_state_t * state
) 
```




<hr>



### function costas\_update 

_Per-symbol carrier update: discriminator -&gt; loop filter -&gt; steer NCO._ 
```C++
JM_FORCEINLINE  JM_HOT void costas_update (
    costas_state_t * s,
    float complex P
) 
```



Runs the decision-directed BPSK Costas discriminator on the prompt `P`, filters it, and writes the new frequency (lo\_set\_norm\_freq) plus a proportional phase nudge into the NCO. Updates the lock metric and last\_error (the instantaneous loop stress). Inline for composition.




**Parameters:**


* `s` Costas state. Must be non-NULL. 
* `P` The dumped integrate-and-dump prompt for this symbol. 




        

<hr>



### function costas\_wipeoff 

_Per-sample carrier wipe-off: de-rotate_ `x` _by the NCO, advance it._
```C++
JM_FORCEINLINE  JM_HOT float complex costas_wipeoff (
    costas_state_t * s,
    float complex x
) 
```



`x * conj(lo_step(nco))` — strips the (tracked) carrier ahead of the matched-filter integrate-and-dump. Inline, zero call overhead.




**Parameters:**


* `s` Costas state. Must be non-NULL. 
* `x` One input sample. 



**Returns:**

The de-rotated sample to feed the integrator. 





        

<hr>
## Macro Definition Documentation





### define COSTAS\_EPS 

```C++
#define COSTAS_EPS `1e-12f`
```




<hr>



### define COSTAS\_LOCK\_ALPHA 

```C++
#define COSTAS_LOCK_ALPHA `0.1`
```




<hr>



### define COSTAS\_STATE\_MAGIC 

```C++
#define COSTAS_STATE_MAGIC `DP_FOURCC ('C', 'S', 'T', 'S')`
```




<hr>



### define COSTAS\_STATE\_VERSION 

```C++
#define COSTAS_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/costas/costas_core.h`

