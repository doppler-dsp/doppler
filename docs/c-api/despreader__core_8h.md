

# File despreader\_core.h



[**FileList**](files.md) **>** [**despreader**](dir_9949992fff5aebed427f83f9eaa478ca.md) **>** [**despreader\_core.h**](despreader__core_8h.md)

[Go to the source code of this file](despreader__core_8h_source.md)

_Continuous DSSS despreader — Costas carrier loop + DLL code loop._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "costas/costas_core.h"`
* `#include "detection/detection_core.h"`
* `#include "dll/dll_core.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`
* `#include "lo/lo_core.h"`
* `#include "lockdet/lockdet_core.h"`
* `#include "loop_filter/loop_filter_core.h"`
* `#include "telemetry/telemetry.h"`
* `#include <complex.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**despreader\_state\_t**](structdespreader__state__t.md) <br>_Despreader state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**despreader\_bits**](#function-despreader_bits) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, const float complex \* x, size\_t x\_len, uint8\_t \* out, size\_t max\_out) <br> |
|  size\_t | [**despreader\_bits\_max\_out**](#function-despreader_bits_max_out) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br> |
|  void | [**despreader\_configure\_carrier\_lock**](#function-despreader_configure_carrier_lock) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, double up\_thresh, double down\_thresh, uint32\_t n\_up, uint32\_t n\_down) <br>_Re-tune the embedded carrier loop's lock detector directly._  |
|  int | [**despreader\_configure\_code\_lock**](#function-despreader_configure_code_lock) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, double pfa, size\_t n\_looks, double ref\_snr\_db) <br>_Re-tune the embedded code loop's lock detector._  |
|  [**despreader\_state\_t**](structdespreader__state__t.md) \* | [**despreader\_create**](#function-despreader_create) (const uint8\_t \* code, size\_t code\_len, size\_t sps, double init\_norm\_freq, double init\_chip, double bn\_carrier, double bn\_code, double bn\_fll, double zeta, double spacing, size\_t periods\_per\_bit) <br>_Create a despreader (COPIES_ `code` _)._ |
|  void | [**despreader\_destroy**](#function-despreader_destroy) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br>_Destroy a despreader and release all memory._  |
|  size\_t | [**despreader\_get\_bit\_phase**](#function-despreader_get_bit_phase) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br> |
|  double | [**despreader\_get\_bn\_carrier**](#function-despreader_get_bn_carrier) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br> |
|  double | [**despreader\_get\_bn\_code**](#function-despreader_get_bn_code) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br> |
|  int | [**despreader\_get\_carrier\_locked**](#function-despreader_get_carrier_locked) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br>_Carrier lock decision (1 = locked): the embedded Costas loop's verify-counted detector on its lock-metric EMA (see costas\_configure\_lock)._  |
|  int | [**despreader\_get\_code\_locked**](#function-despreader_get_code_locked) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br>_Code lock decision (1 = locked): the embedded DLL's verify-counted CFAR detector (see dll\_configure\_lock); live in composition — the despreader runs the same always-on detector dll\_steps does._  |
|  double | [**despreader\_get\_code\_phase**](#function-despreader_get_code_phase) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br> |
|  double | [**despreader\_get\_code\_rate**](#function-despreader_get_code_rate) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br> |
|  double | [**despreader\_get\_lock\_metric**](#function-despreader_get_lock_metric) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br> |
|  double | [**despreader\_get\_norm\_freq**](#function-despreader_get_norm_freq) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br> |
|  void | [**despreader\_get\_state**](#function-despreader_get_state) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state, void \* blob) <br> |
|  void | [**despreader\_init**](#function-despreader_init) ([**despreader\_state\_t**](structdespreader__state__t.md) \* ch, const uint8\_t \* code, size\_t code\_len, size\_t sps, double init\_norm\_freq, double init\_chip, double bn\_carrier, double bn\_code, double bn\_fll, double zeta, double spacing, size\_t periods\_per\_bit) <br>_Initialise a despreader in place; BORROWS_ `code` _._ |
|  void | [**despreader\_reset**](#function-despreader_reset) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br>_Re-seed both loops to the create-time frequency/phase; keep config._  |
|  void | [**despreader\_set\_bn\_carrier**](#function-despreader_set_bn_carrier) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, double val) <br> |
|  void | [**despreader\_set\_bn\_code**](#function-despreader_set_bn_code) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, double val) <br> |
|  void | [**despreader\_set\_norm\_freq**](#function-despreader_set_norm_freq) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, double val) <br> |
|  int | [**despreader\_set\_state**](#function-despreader_set_state) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, const void \* blob) <br> |
|  int | [**despreader\_set\_telemetry**](#function-despreader_set_telemetry) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* tlm, const char \* prefix, uint32\_t decim) <br>_Attach (or detach) a telemetry context across the despreader. Pure forwarder — the despreader registers no probes of its own: the carrier loop registers "&lt;prefix&gt;.car.lock" / ".e" / ".freq" / ".locked" and the code loop registers "&lt;prefix&gt;.code.e" / ".rate" / ".lock" / ".locked" (the ".locked" pair are the loops' verify-counted lockdet decisions, 0/1) — eight probes, all thinned by_ `decim` _and emitted once per code period (the despreader flushes both loops at its per-period update). Passing NULL detaches both loops. Setup path, never hot; the context is borrowed and must outlive the attachment (SPSC rules in_[_**telemetry/telemetry.h**_](telemetry_8h.md) _)._ |
|  size\_t | [**despreader\_state\_bytes**](#function-despreader_state_bytes) (const [**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br> |
|  size\_t | [**despreader\_steps**](#function-despreader_steps) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br> |
|  size\_t | [**despreader\_steps\_max\_out**](#function-despreader_steps_max_out) ([**despreader\_state\_t**](structdespreader__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DESPREADER\_STATE\_MAGIC**](despreader__core_8h.md#define-despreader_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('D', 'S', 'P', 'R')`<br> |
| define  | [**DESPREADER\_STATE\_VERSION**](despreader__core_8h.md#define-despreader_state_version)  `/* multi line expression */`<br> |

## Detailed Description


A complete continuous despreader for a DSSS-BPSK signal: it composes a [**costas\_state\_t**](structcostas__state__t.md) carrier loop and a [**dll\_state\_t**](structdll__state__t.md) code loop on a single shared per-sample integrate-and-dump. Per sample it wipes the carrier (costas\_wipeoff, integer NCO) and feeds the de-rotated sample to the DLL's early/prompt/late correlators (dll\_accumulate); per code period it dumps the prompt and updates both loops — the code loop on the early/late envelopes, the carrier loop on the same prompt symbol. `steps()` emits one prompt per period; `bits()` bit-syncs the prompts into hard data bits (a data bit spans `periods_per_bit` code periods).


It is seeded by acquisition (the FFT search supplies the coarse carrier frequency + code phase); the loops then track the residual. Set `bn_fll > 0` for FLL-assisted carrier pull-in.


Lifecycle: `despreader_create -> (steps / bits / reset)* -> despreader_destroy`.



```C++
uint8_t code[127] = { ... };  // one code period, 0/1 chips
despreader_state_t *ch = despreader_create(code, 127, 8, 0.0, 0.0,
                                      0.05, 0.005, 0.0, 0.707, 0.5, 1);
float complex sym[64];
size_t k = despreader_steps(ch, rx, rx_len, sym, 64);  // prompt per period
despreader_destroy(ch);
```
 


    
## Public Functions Documentation




### function despreader\_bits 

```C++
size_t despreader_bits (
    despreader_state_t * state,
    const float complex * x,
    size_t x_len,
    uint8_t * out,
    size_t max_out
) 
```




<hr>



### function despreader\_bits\_max\_out 

```C++
size_t despreader_bits_max_out (
    despreader_state_t * state
) 
```




<hr>



### function despreader\_configure\_carrier\_lock 

_Re-tune the embedded carrier loop's lock detector directly._ 
```C++
void despreader_configure_carrier_lock (
    despreader_state_t * state,
    double up_thresh,
    double down_thresh,
    uint32_t n_up,
    uint32_t n_down
) 
```



Thin forwarder to [**costas\_configure\_lock()**](costas__core_8h.md#function-costas_configure_lock) on the embedded Costas loop — symmetric with [**despreader\_get\_carrier\_locked()**](despreader__core_8h.md#function-despreader_get_carrier_locked) exposing its state: state is readable, so config should be writable too, rather than forcing a caller who needs this control to drop to raw Dll+Costas composition instead of Despreader. See [**costas\_configure\_lock()**](costas__core_8h.md#function-costas_configure_lock) for the parameter semantics. 

**Parameters:**


* `state` Must be non-NULL. 
* `up_thresh` Declare threshold on the lock-metric EMA. 
* `down_thresh` Drop threshold (&lt;= up\_thresh for level hysteresis). 
* `n_up` Consecutive above-threshold symbols to declare. 
* `n_down` Consecutive below-threshold symbols to drop. 
```C++
>>> import numpy as np
>>> from doppler.dsss import Despreader
>>> d = Despreader(code=np.zeros(31, dtype=np.uint8), sps=2)
>>> d.configure_carrier_lock(0.9, 0.8, 4, 16)  # tighter declare/drop
```
 




        

<hr>



### function despreader\_configure\_code\_lock 

_Re-tune the embedded code loop's lock detector._ 
```C++
int despreader_configure_code_lock (
    despreader_state_t * state,
    double pfa,
    size_t n_looks,
    double ref_snr_db
) 
```



Thin forwarder to [**dll\_configure\_lock()**](dll__core_8h.md#function-dll_configure_lock) on the embedded DLL — the derived (pfa-style) entry point, matching Despreader's role as the "easy" composed API (Dll's raw escape hatch, [**dll\_configure\_lock\_raw()**](dll__core_8h.md#function-dll_configure_lock_raw), stays a Dll-only control for a caller that composes Dll+Costas directly). See [**dll\_configure\_lock()**](dll__core_8h.md#function-dll_configure_lock) for the parameter semantics. 

**Parameters:**


* `state` Must be non-NULL. 
* `pfa` Per-decision false-alarm probability, in (0, 1). 
* `n_looks` Non-coherent integration depth N (looks); clamped &gt;= 1. 
* `ref_snr_db` Noise-reference estimator SNR in dB (&gt; 0), or 0 to derive from `n_looks` (see [**dll\_configure\_lock()**](dll__core_8h.md#function-dll_configure_lock)). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when `pfa` is outside (0, 1). 
```C++
>>> import numpy as np
>>> from doppler.dsss import Despreader
>>> d = Despreader(code=np.zeros(31, dtype=np.uint8), sps=2)
>>> d.configure_code_lock(1e-3, 20)
>>> d.code_locked
False
>>> d.configure_code_lock(2.0, 20)
Traceback (most recent call last):
    ...
ValueError: configure_code_lock failed (rc=-4)
```
 





        

<hr>



### function despreader\_create 

_Create a despreader (COPIES_ `code` _)._
```C++
despreader_state_t * despreader_create (
    const uint8_t * code,
    size_t code_len,
    size_t sps,
    double init_norm_freq,
    double init_chip,
    double bn_carrier,
    double bn_code,
    double bn_fll,
    double zeta,
    double spacing,
    size_t periods_per_bit
) 
```





**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**despreader\_destroy()**](despreader__core_8h.md#function-despreader_destroy) when done. 





        

<hr>



### function despreader\_destroy 

_Destroy a despreader and release all memory._ 
```C++
void despreader_destroy (
    despreader_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function despreader\_get\_bit\_phase 

```C++
size_t despreader_get_bit_phase (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_bn\_carrier 

```C++
double despreader_get_bn_carrier (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_bn\_code 

```C++
double despreader_get_bn_code (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_carrier\_locked 

_Carrier lock decision (1 = locked): the embedded Costas loop's verify-counted detector on its lock-metric EMA (see costas\_configure\_lock)._ 
```C++
int despreader_get_carrier_locked (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_code\_locked 

_Code lock decision (1 = locked): the embedded DLL's verify-counted CFAR detector (see dll\_configure\_lock); live in composition — the despreader runs the same always-on detector dll\_steps does._ 
```C++
int despreader_get_code_locked (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_code\_phase 

```C++
double despreader_get_code_phase (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_code\_rate 

```C++
double despreader_get_code_rate (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_lock\_metric 

```C++
double despreader_get_lock_metric (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_norm\_freq 

```C++
double despreader_get_norm_freq (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_get\_state 

```C++
void despreader_get_state (
    const despreader_state_t * state,
    void * blob
) 
```




<hr>



### function despreader\_init 

_Initialise a despreader in place; BORROWS_ `code` _._
```C++
void despreader_init (
    despreader_state_t * ch,
    const uint8_t * code,
    size_t code_len,
    size_t sps,
    double init_norm_freq,
    double init_chip,
    double bn_carrier,
    double bn_code,
    double bn_fll,
    double zeta,
    double spacing,
    size_t periods_per_bit
) 
```



The by-value counterpart to [**despreader\_create()**](despreader__core_8h.md#function-despreader_create): the caller retains ownership of `code` (it is not copied or freed). Seeds the carrier NCO at `init_norm_freq` and the code phase at `init_chip` (the acquisition estimate). The carrier loop's update period is one code period (`code_len * sps` samples).




**Parameters:**


* `ch` State to initialise. Must be non-NULL. 
* `code` Spreading code (0/1 chips), one period; borrowed. 
* `code_len` Code length (chips per period); &gt;= 1. 
* `sps` Samples per chip. 
* `init_norm_freq` Seed carrier frequency, cycles/sample. 
* `init_chip` Seed code phase, chips. 
* `bn_carrier` Carrier loop noise bandwidth. 
* `bn_code` Code loop noise bandwidth. 
* `bn_fll` Carrier FLL-assist bandwidth (0 = pure PLL). 
* `zeta` Damping factor for both loops. 
* `spacing` DLL early/late tap offset, chips. 
* `periods_per_bit` Code periods per data bit (1 = one bit per period). 




        

<hr>



### function despreader\_reset 

_Re-seed both loops to the create-time frequency/phase; keep config._ 
```C++
void despreader_reset (
    despreader_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function despreader\_set\_bn\_carrier 

```C++
void despreader_set_bn_carrier (
    despreader_state_t * state,
    double val
) 
```




<hr>



### function despreader\_set\_bn\_code 

```C++
void despreader_set_bn_code (
    despreader_state_t * state,
    double val
) 
```




<hr>



### function despreader\_set\_norm\_freq 

```C++
void despreader_set_norm_freq (
    despreader_state_t * state,
    double val
) 
```




<hr>



### function despreader\_set\_state 

```C++
int despreader_set_state (
    despreader_state_t * state,
    const void * blob
) 
```




<hr>



### function despreader\_set\_telemetry 

_Attach (or detach) a telemetry context across the despreader. Pure forwarder — the despreader registers no probes of its own: the carrier loop registers "&lt;prefix&gt;.car.lock" / ".e" / ".freq" / ".locked" and the code loop registers "&lt;prefix&gt;.code.e" / ".rate" / ".lock" / ".locked" (the ".locked" pair are the loops' verify-counted lockdet decisions, 0/1) — eight probes, all thinned by_ `decim` _and emitted once per code period (the despreader flushes both loops at its per-period update). Passing NULL detaches both loops. Setup path, never hot; the context is borrowed and must outlive the attachment (SPSC rules in_[_**telemetry/telemetry.h**_](telemetry_8h.md) _)._
```C++
int despreader_set_telemetry (
    despreader_state_t * state,
    dp_tlm_t * tlm,
    const char * prefix,
    uint32_t decim
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `tlm` Telemetry context to attach, or NULL to detach. 
* `prefix` Probe-name prefix, e.g. "ch0". 
* `decim` Emit every decim-th code period; &gt;= 1. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID when the probe table cannot take all eight probes (the attach fails whole; everything detached). 
```C++
>>> import numpy as np
>>> from doppler.dsss import Despreader
>>> from doppler.telemetry import Telemetry
>>> tlm = Telemetry(1 << 12)
>>> code = (np.arange(31) % 2).astype(np.uint8)
>>> ch = Despreader(code=code, sps=4)
>>> ch.set_telemetry(tlm, "ch0")
>>> names = sorted(tlm.probe_names())
>>> names[:4]
['ch0.car.e', 'ch0.car.freq', 'ch0.car.lock', 'ch0.car.locked']
>>> names[4:]
['ch0.code.e', 'ch0.code.lock', 'ch0.code.locked', 'ch0.code.rate']
>>> chips = 1.0 - 2.0 * (np.arange(31) % 2)
>>> x = np.tile(np.repeat(chips, 4), 40).astype(np.complex64)
>>> _ = ch.steps(x)
>>> recs = tlm.read()   # eight records per code period
>>> len(recs) > 0 and len(recs) % 8 == 0
True
```
 





        

<hr>



### function despreader\_state\_bytes 

```C++
size_t despreader_state_bytes (
    const despreader_state_t * state
) 
```




<hr>



### function despreader\_steps 

```C++
size_t despreader_steps (
    despreader_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```




<hr>



### function despreader\_steps\_max\_out 

```C++
size_t despreader_steps_max_out (
    despreader_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define DESPREADER\_STATE\_MAGIC 

```C++
#define DESPREADER_STATE_MAGIC `DP_FOURCC ('D', 'S', 'P', 'R')`
```




<hr>



### define DESPREADER\_STATE\_VERSION 

```C++
#define DESPREADER_STATE_VERSION `/* multi line expression */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/despreader/despreader_core.h`

