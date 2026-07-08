

# File channel\_core.h



[**FileList**](files.md) **>** [**channel**](dir_7cd82dec1dfa46f6b0156d9a972e4575.md) **>** [**channel\_core.h**](channel__core_8h.md)

[Go to the source code of this file](channel__core_8h_source.md)

_GPS-style tracking channel — Costas carrier loop + DLL code loop._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "costas/costas_core.h"`
* `#include "dll/dll_core.h"`
* `#include "jm_perf.h"`
* `#include <complex.h>`
* `#include "lo/lo_core.h"`
* `#include "loop_filter/loop_filter_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**channel\_state\_t**](structchannel__state__t.md) <br>_Tracking-channel state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**channel\_bits**](#function-channel_bits) ([**channel\_state\_t**](structchannel__state__t.md) \* state, const float complex \* x, size\_t x\_len, uint8\_t \* out, size\_t max\_out) <br> |
|  size\_t | [**channel\_bits\_max\_out**](#function-channel_bits_max_out) ([**channel\_state\_t**](structchannel__state__t.md) \* state) <br> |
|  [**channel\_state\_t**](structchannel__state__t.md) \* | [**channel\_create**](#function-channel_create) (const uint8\_t \* code, size\_t code\_len, size\_t sps, double init\_norm\_freq, double init\_chip, double bn\_carrier, double bn\_code, double bn\_fll, double zeta, double spacing, size\_t nav\_period) <br>_Create a tracking channel (COPIES_ `code` _)._ |
|  void | [**channel\_destroy**](#function-channel_destroy) ([**channel\_state\_t**](structchannel__state__t.md) \* state) <br>_Destroy a tracking channel and release all memory._  |
|  size\_t | [**channel\_get\_bit\_phase**](#function-channel_get_bit_phase) (const [**channel\_state\_t**](structchannel__state__t.md) \* state) <br> |
|  double | [**channel\_get\_bn\_carrier**](#function-channel_get_bn_carrier) (const [**channel\_state\_t**](structchannel__state__t.md) \* state) <br> |
|  double | [**channel\_get\_bn\_code**](#function-channel_get_bn_code) (const [**channel\_state\_t**](structchannel__state__t.md) \* state) <br> |
|  double | [**channel\_get\_code\_phase**](#function-channel_get_code_phase) (const [**channel\_state\_t**](structchannel__state__t.md) \* state) <br> |
|  double | [**channel\_get\_code\_rate**](#function-channel_get_code_rate) (const [**channel\_state\_t**](structchannel__state__t.md) \* state) <br> |
|  double | [**channel\_get\_lock\_metric**](#function-channel_get_lock_metric) (const [**channel\_state\_t**](structchannel__state__t.md) \* state) <br> |
|  double | [**channel\_get\_norm\_freq**](#function-channel_get_norm_freq) (const [**channel\_state\_t**](structchannel__state__t.md) \* state) <br> |
|  void | [**channel\_get\_state**](#function-channel_get_state) (const [**channel\_state\_t**](structchannel__state__t.md) \* state, void \* blob) <br> |
|  void | [**channel\_init**](#function-channel_init) ([**channel\_state\_t**](structchannel__state__t.md) \* ch, const uint8\_t \* code, size\_t code\_len, size\_t sps, double init\_norm\_freq, double init\_chip, double bn\_carrier, double bn\_code, double bn\_fll, double zeta, double spacing, size\_t nav\_period) <br>_Initialise a tracking channel in place; BORROWS_ `code` _._ |
|  void | [**channel\_reset**](#function-channel_reset) ([**channel\_state\_t**](structchannel__state__t.md) \* state) <br>_Re-seed both loops to the create-time frequency/phase; keep config._  |
|  void | [**channel\_set\_bn\_carrier**](#function-channel_set_bn_carrier) ([**channel\_state\_t**](structchannel__state__t.md) \* state, double val) <br> |
|  void | [**channel\_set\_bn\_code**](#function-channel_set_bn_code) ([**channel\_state\_t**](structchannel__state__t.md) \* state, double val) <br> |
|  void | [**channel\_set\_norm\_freq**](#function-channel_set_norm_freq) ([**channel\_state\_t**](structchannel__state__t.md) \* state, double val) <br> |
|  int | [**channel\_set\_state**](#function-channel_set_state) ([**channel\_state\_t**](structchannel__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**channel\_state\_bytes**](#function-channel_state_bytes) (const [**channel\_state\_t**](structchannel__state__t.md) \* state) <br> |
|  size\_t | [**channel\_steps**](#function-channel_steps) ([**channel\_state\_t**](structchannel__state__t.md) \* state, const float complex \* x, size\_t x\_len, float complex \* out, size\_t max\_out) <br> |
|  size\_t | [**channel\_steps\_max\_out**](#function-channel_steps_max_out) ([**channel\_state\_t**](structchannel__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CHANNEL\_STATE\_MAGIC**](channel__core_8h.md#define-channel_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('C','H','A','N')`<br> |
| define  | [**CHANNEL\_STATE\_VERSION**](channel__core_8h.md#define-channel_state_version)  `1u`<br> |

## Detailed Description


A complete tracking channel for a continuous DSSS-BPSK signal: it composes a [**costas\_state\_t**](structcostas__state__t.md) carrier loop and a [**dll\_state\_t**](structdll__state__t.md) code loop on a single shared per-sample integrate-and-dump. Per sample it wipes the carrier (costas\_wipeoff, integer NCO) and feeds the de-rotated sample to the DLL's early/prompt/late correlators (dll\_accumulate); per code period it dumps the prompt and updates both loops — the code loop on the early/late envelopes, the carrier loop on the same prompt symbol. `steps()` emits one prompt per period; `bits()` bit-syncs the prompts into hard data bits (a data bit spans `nav_period` code periods).


It is seeded by acquisition (the FFT search supplies the coarse carrier frequency + code phase); the loops then track the residual. Set `bn_fll > 0` for FLL-assisted carrier pull-in.


Lifecycle: channel\_create -&gt; [steps / bits / reset]\* -&gt; channel\_destroy.



```C++
uint8_t code[127] = { ... };  // one code period, 0/1 chips
channel_state_t *ch = channel_create(code, 127, 8, 0.0, 0.0,
                                      0.05, 0.005, 0.0, 0.707, 0.5, 1);
float complex sym[64];
size_t k = channel_steps(ch, rx, rx_len, sym, 64);  // prompt per period
channel_destroy(ch);
```
 


    
## Public Functions Documentation




### function channel\_bits 

```C++
size_t channel_bits (
    channel_state_t * state,
    const float complex * x,
    size_t x_len,
    uint8_t * out,
    size_t max_out
) 
```




<hr>



### function channel\_bits\_max\_out 

```C++
size_t channel_bits_max_out (
    channel_state_t * state
) 
```




<hr>



### function channel\_create 

_Create a tracking channel (COPIES_ `code` _)._
```C++
channel_state_t * channel_create (
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
    size_t nav_period
) 
```





**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**channel\_destroy()**](channel__core_8h.md#function-channel_destroy) when done. 





        

<hr>



### function channel\_destroy 

_Destroy a tracking channel and release all memory._ 
```C++
void channel_destroy (
    channel_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function channel\_get\_bit\_phase 

```C++
size_t channel_get_bit_phase (
    const channel_state_t * state
) 
```




<hr>



### function channel\_get\_bn\_carrier 

```C++
double channel_get_bn_carrier (
    const channel_state_t * state
) 
```




<hr>



### function channel\_get\_bn\_code 

```C++
double channel_get_bn_code (
    const channel_state_t * state
) 
```




<hr>



### function channel\_get\_code\_phase 

```C++
double channel_get_code_phase (
    const channel_state_t * state
) 
```




<hr>



### function channel\_get\_code\_rate 

```C++
double channel_get_code_rate (
    const channel_state_t * state
) 
```




<hr>



### function channel\_get\_lock\_metric 

```C++
double channel_get_lock_metric (
    const channel_state_t * state
) 
```




<hr>



### function channel\_get\_norm\_freq 

```C++
double channel_get_norm_freq (
    const channel_state_t * state
) 
```




<hr>



### function channel\_get\_state 

```C++
void channel_get_state (
    const channel_state_t * state,
    void * blob
) 
```




<hr>



### function channel\_init 

_Initialise a tracking channel in place; BORROWS_ `code` _._
```C++
void channel_init (
    channel_state_t * ch,
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
    size_t nav_period
) 
```



The by-value counterpart to [**channel\_create()**](channel__core_8h.md#function-channel_create): the caller retains ownership of `code` (it is not copied or freed). Seeds the carrier NCO at `init_norm_freq` and the code phase at `init_chip` (the acquisition estimate). The carrier loop's update period is one code period (`code_len * sps` samples).




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
* `nav_period` Code periods per data bit (1 = one bit per period). 




        

<hr>



### function channel\_reset 

_Re-seed both loops to the create-time frequency/phase; keep config._ 
```C++
void channel_reset (
    channel_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function channel\_set\_bn\_carrier 

```C++
void channel_set_bn_carrier (
    channel_state_t * state,
    double val
) 
```




<hr>



### function channel\_set\_bn\_code 

```C++
void channel_set_bn_code (
    channel_state_t * state,
    double val
) 
```




<hr>



### function channel\_set\_norm\_freq 

```C++
void channel_set_norm_freq (
    channel_state_t * state,
    double val
) 
```




<hr>



### function channel\_set\_state 

```C++
int channel_set_state (
    channel_state_t * state,
    const void * blob
) 
```




<hr>



### function channel\_state\_bytes 

```C++
size_t channel_state_bytes (
    const channel_state_t * state
) 
```




<hr>



### function channel\_steps 

```C++
size_t channel_steps (
    channel_state_t * state,
    const float complex * x,
    size_t x_len,
    float complex * out,
    size_t max_out
) 
```




<hr>



### function channel\_steps\_max\_out 

```C++
size_t channel_steps_max_out (
    channel_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define CHANNEL\_STATE\_MAGIC 

```C++
#define CHANNEL_STATE_MAGIC `DP_FOURCC ('C','H','A','N')`
```




<hr>



### define CHANNEL\_STATE\_VERSION 

```C++
#define CHANNEL_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/channel/channel_core.h`

