

# File burst\_acq\_core.h



[**FileList**](files.md) **>** [**burst\_acq**](dir_d3ec06985dce876581dd948705a4d1da.md) **>** [**burst\_acq\_core.h**](burst__acq__core_8h.md)

[Go to the source code of this file](burst__acq__core_8h_source.md)

_BurstAcquisition — thin forwarder onto acq\_core.c's shared engine._ [More...](#detailed-description)

* `#include "acq/acq_core.h"`
* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**burst\_acq\_state\_t**](structburst__acq__state__t.md) <br>_BurstAcquisition state: a pure wrapper around one shared_ [_**acq\_state\_t**_](structacq__state__t.md) _engine._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**burst\_acq\_configure\_search\_raw**](#function-burst_acq_configure_search_raw) ([**burst\_acq\_state\_t**](structburst__acq__state__t.md) \* state, size\_t doppler\_bins, size\_t n\_noncoh) <br>_Pin the search grid directly, bypassing the auto-sizing search._  |
|  [**burst\_acq\_state\_t**](structburst__acq__state__t.md) \* | [**burst\_acq\_create**](#function-burst_acq_create) (const uint8\_t \* code, size\_t code\_len, size\_t reps, size\_t spc, double chip\_rate, double cn0\_dbhz, double doppler\_uncertainty, double pfa, double pd, int noise\_mode) <br>_Create a burst-mode acquisition engine (forwards to_ [_**acq\_create\_burst()**_](acq__core_8h.md#function-acq_create_burst) __ _see its doc comment in_[_**acq\_core.h**_](acq__core_8h.md) _for the full physics)._ |
|  void | [**burst\_acq\_destroy**](#function-burst_acq_destroy) ([**burst\_acq\_state\_t**](structburst__acq__state__t.md) \* state) <br>_Destroy and free an instance._  |
|  void | [**burst\_acq\_get\_state**](#function-burst_acq_get_state) (const [**burst\_acq\_state\_t**](structburst__acq__state__t.md) \* state, void \* blob) <br> |
|  size\_t | [**burst\_acq\_push**](#function-burst_acq_push) ([**burst\_acq\_state\_t**](structburst__acq__state__t.md) \* state, const float complex \* x, size\_t n\_in, [**acq\_result\_t**](structacq__result__t.md) \* result, size\_t max\_results) <br>_Stream raw samples; emit one event per CFAR dump above threshold._  |
|  void | [**burst\_acq\_reset**](#function-burst_acq_reset) ([**burst\_acq\_state\_t**](structburst__acq__state__t.md) \* state) <br>_Drain the input ring and reset the coherent accumulator._  |
|  int | [**burst\_acq\_set\_state**](#function-burst_acq_set_state) ([**burst\_acq\_state\_t**](structburst__acq__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**burst\_acq\_state\_bytes**](#function-burst_acq_state_bytes) (const [**burst\_acq\_state\_t**](structburst__acq__state__t.md) \* state) <br> |




























## Detailed Description


Composes [**acq\_state\_t**](structacq__state__t.md) ([**native/inc/acq/acq\_core.h**](acq__core_8h.md)) as an embedded pointer, built via [**acq\_create\_burst()**](acq__core_8h.md#function-acq_create_burst)  the BURST front door onto the SAME shared engine `Acquisition` ([**acq\_core.h**](acq__core_8h.md)) composes via [**acq\_create\_continuous()**](acq__core_8h.md#function-acq_create_continuous). Every function here is a direct forward to the corresponding acq\_\* call; the entire algorithm lives in acq\_core.c exactly once (see docs/design/async-dsss-receiver.md's Acquisition/BurstAcquisition split and CLAUDE.md's "every algorithm lives in C exactly once" rule).



```C++
uint8_t code[7] = { 1, 1, 1, 0, 1, 0, 0 };
burst_acq_state_t *obj = burst_acq_create(code, 7, 8, 4, 1000000.0, 50.0,
                                          0.0, 1e-3, 0.9, 0);
acq_result_t hits[64];
size_t nh = burst_acq_push(obj, samples, n_samples, hits, 64);
burst_acq_destroy(obj);
```
 


    
## Public Functions Documentation




### function burst\_acq\_configure\_search\_raw 

_Pin the search grid directly, bypassing the auto-sizing search._ 
```C++
int burst_acq_configure_search_raw (
    burst_acq_state_t * state,
    size_t doppler_bins,
    size_t n_noncoh
) 
```



Forwards to [**acq\_configure\_search\_raw()**](acq__core_8h.md#function-acq_configure_search_raw) on the embedded engine (see its doc comment in [**acq\_core.h**](acq__core_8h.md)): resizes every grid-dependent buffer/plan, re-derives the threshold ladder for the pinned grid, and clears in-flight accumulation — call between push() calls, never a substitute for one.




**Parameters:**


* `state` Allocated engine (non-NULL). 
* `doppler_bins` Coherent depth to pin, in `[1, reps]`. 
* `n_noncoh` Non-coherent look count to pin, in `[1, ACQ_N_NONCOH_SAFETY_CEILING]`. 



**Returns:**

0 on success, -1 if either argument is out of range or an allocation fails (the engine keeps its prior grid on failure). 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstAcquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,
...                      cn0_dbhz=50.0)
>>> b.configure_search_raw(doppler_bins=4, n_noncoh=2)  # pin the grid
>>> b.doppler_bins, b.n_noncoh
(4, 2)
>>> burst = np.tile(np.roll(s0, 17), 8).astype(np.complex64)
>>> b.push(burst)[0][:2]      # detects at the pinned grid
(0, 17)
```
 





        

<hr>



### function burst\_acq\_create 

_Create a burst-mode acquisition engine (forwards to_ [_**acq\_create\_burst()**_](acq__core_8h.md#function-acq_create_burst) __ _see its doc comment in_[_**acq\_core.h**_](acq__core_8h.md) _for the full physics)._
```C++
burst_acq_state_t * burst_acq_create (
    const uint8_t * code,
    size_t code_len,
    size_t reps,
    size_t spc,
    double chip_rate,
    double cn0_dbhz,
    double doppler_uncertainty,
    double pfa,
    double pd,
    int noise_mode
) 
```





**Parameters:**


* `code` PN chips (0/1), length `code_len`. 
* `code_len` Number of chips supplied (= sf). 
* `reps` Max coherent code repetitions (&gt;= 1). 
* `spc` Samples per chip (&gt;= 1). 
* `chip_rate` Chip rate in Hz (&gt; 0). 
* `cn0_dbhz` Carrier-to-noise density in dB-Hz (&gt; 0). 
* `doppler_uncertainty` One-sided Doppler search half-range in Hz. 
* `pfa` Target system false-alarm probability (0,1). 
* `pd` Target detection probability (0,1). 
* `noise_mode` CFAR mode index: 0=mean, 1=median, 2=min, 3=max. 



**Returns:**

Heap-allocated state, or NULL on bad arguments / allocation failure. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstAcquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> burst = np.tile(np.roll(s0, 17), 24).astype(np.complex64)
>>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,
...                      cn0_dbhz=50.0)
>>> b.push(burst)[0][:2]      # detects (Doppler bin, code phase)
(0, 17)
```
 





        

<hr>



### function burst\_acq\_destroy 

_Destroy and free an instance._ 
```C++
void burst_acq_destroy (
    burst_acq_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function burst\_acq\_get\_state 

```C++
void burst_acq_get_state (
    const burst_acq_state_t * state,
    void * blob
) 
```




<hr>



### function burst\_acq\_push 

_Stream raw samples; emit one event per CFAR dump above threshold._ 
```C++
size_t burst_acq_push (
    burst_acq_state_t * state,
    const float complex * x,
    size_t n_in,
    acq_result_t * result,
    size_t max_results
) 
```



Forwards to [**acq\_push()**](acq__core_8h.md#function-acq_push) on the embedded engine (see its doc comment in [**acq\_core.h**](acq__core_8h.md) for the framing/CFAR mechanics). Each event carries the peak's Doppler bin and code phase (the two search axes), its CFAR statistic, and an estimated C/N0 — see [**acq\_result\_t**](structacq__result__t.md).




**Parameters:**


* `state` Allocated engine (non-NULL). 
* `x` Raw input, interleaved CF32, `n_in` complex samples. 
* `n_in` Number of complex input samples. 
* `result` Output array for detection events. 
* `max_results` Capacity of `result`. 



**Returns:**

Number of events written (0 … max\_results). 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstAcquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> burst = np.tile(np.roll(s0, 17), 24).astype(np.complex64)
>>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,
...                      cn0_dbhz=50.0)
>>> b.push(burst)[0][:2]      # (Doppler bin, code phase)
(0, 17)
```
 





        

<hr>



### function burst\_acq\_reset 

_Drain the input ring and reset the coherent accumulator._ 
```C++
void burst_acq_reset (
    burst_acq_state_t * state
) 
```



Forwards to [**acq\_reset()**](acq__core_8h.md#function-acq_reset) on the embedded engine: discards any buffered samples that have not yet completed a frame and clears the non-coherent power accumulator and dwell bookkeeping, so the next push() begins a fresh search from an empty ring. Construction parameters are untouched.




**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstAcquisition
>>> from doppler.wfm import PN, mls_poly
>>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
...                      length=5).generate(31)).astype(np.uint8)
>>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
...     np.complex64)
>>> burst = np.tile(np.roll(s0, 17), 24).astype(np.complex64)
>>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,
...                      cn0_dbhz=50.0)
>>> _ = b.push(burst[:100])   # a partial frame, buffered mid-stream
>>> b.reset()                 # drop it before it can bias a detection
>>> b.push(burst)[0][:2]      # (Doppler bin, code phase)
(0, 17)
```
 




        

<hr>



### function burst\_acq\_set\_state 

```C++
int burst_acq_set_state (
    burst_acq_state_t * state,
    const void * blob
) 
```




<hr>



### function burst\_acq\_state\_bytes 

```C++
size_t burst_acq_state_bytes (
    const burst_acq_state_t * state
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/burst_acq/burst_acq_core.h`

