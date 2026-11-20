

# File ofdm\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**ofdm**](dir_78c6add8c81d41bee73cbf3d222b7ad8.md) **>** [**ofdm\_core.h**](ofdm__core_8h.md)

[Go to the source code of this file](ofdm__core_8h_source.md)

_A general OFDM modulator: constellation points and a layout in, baseband samples out._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/fft/fft_core.h"`
* `#include "doppler/jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) <br>_Ofdm state: the layout (configuration) and the position in it (state)._  |
| struct | [**ofdm\_class\_t**](structofdm__class__t.md) <br>_One kind of OFDM symbol. The arrays are owned by the modulator, copied in by_ [_**dp\_ofdm\_add\_class()**_](ofdm__core_8h.md#function-dp_ofdm_add_class) _and_[_**dp\_ofdm\_add\_fixed()**_](ofdm__core_8h.md#function-dp_ofdm_add_fixed) _._ |
| struct | [**ofdm\_fixed\_t**](structofdm__fixed__t.md) <br>_One entry of a class's fixed pattern._  |
| struct | [**ofdm\_run\_t**](structofdm__run__t.md) <br>_A run of OFDM symbols of one class._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_ofdm\_add\_class**](#function-dp_ofdm_add_class) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state, uint32\_t n\_fft, const uint8\_t \* subcarrier\_map, size\_t subcarrier\_map\_len, uint32\_t useful, uint32\_t cyclic\_prefix, uint32\_t window, double scale, uint32\_t n\_phase) <br>_Append one kind of OFDM symbol and return its index._  |
|  int | [**dp\_ofdm\_add\_fixed**](#function-dp_ofdm_add_fixed) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state, uint32\_t class\_index, uint32\_t phase, int32\_t k, double \_Complex value) <br>_Append one fixed entry to a class: a pilot or a training value._  |
|  int | [**dp\_ofdm\_add\_polarity**](#function-dp_ofdm_add_polarity) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state, uint32\_t class\_index, const float \_Complex \* polarity, size\_t polarity\_len) <br>_Give a class a polarity sequence._  |
|  int | [**dp\_ofdm\_add\_run**](#function-dp_ofdm_add_run) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state, uint32\_t class\_index, uint32\_t count, uint32\_t cyclic\_prefix, uint32\_t polarity\_offset, uint32\_t first\_phase) <br>_Append a run of OFDM symbols of one class and return its index._  |
|  int | [**dp\_ofdm\_add\_subcarrier\_gain**](#function-dp_ofdm_add_subcarrier_gain) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state, uint32\_t class\_index, const float \_Complex \* gain, size\_t gain\_len) <br>_Give a class a complex multiplier per subcarrier._  |
|  int | [**dp\_ofdm\_check**](#function-dp_ofdm_check) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state) <br>_The plain form of_ [_**dp\_ofdm\_check\_why()**_](ofdm__core_8h.md#function-dp_ofdm_check_why) _: 0 if the layout is usable, a negative value if not, with no sentence. What a C caller that does not print the reason calls._ |
|  int | [**dp\_ofdm\_check\_why**](#function-dp_ofdm_check_why) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state, const char \*\* why) <br>_Check the whole layout and derive what the render uses; say why when it is not usable. This is the binding's_ `check` _._ |
|  [**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* | [**dp\_ofdm\_create**](#function-dp_ofdm_create) (void) <br>_Create an empty modulator (no classes, no runs)._  |
|  void | [**dp\_ofdm\_destroy**](#function-dp_ofdm_destroy) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state) <br>_Destroy a modulator and release all memory._  |
|  size\_t | [**dp\_ofdm\_finish**](#function-dp_ofdm_finish) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state, size\_t n, float \_Complex \* out, size\_t max\_out) <br>_Return the window tail still held and clear it._  |
|  size\_t | [**dp\_ofdm\_finish\_max\_out**](#function-dp_ofdm_finish_max_out) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state, size\_t n) <br>_Capacity_ [_**dp\_ofdm\_finish()**_](ofdm__core_8h.md#function-dp_ofdm_finish) _needs for_`n` _samples._ |
|  void | [**dp\_ofdm\_get\_state**](#function-dp_ofdm_get_state) (const [**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state, void \* blob) <br>_Write the state into_ `blob` _._ |
|  size\_t | [**dp\_ofdm\_modulate**](#function-dp_ofdm_modulate) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out) <br>_The plain form of_ [_**dp\_ofdm\_modulate\_why()**_](ofdm__core_8h.md#function-dp_ofdm_modulate_why) _._ |
|  size\_t | [**dp\_ofdm\_modulate\_max\_out**](#function-dp_ofdm_modulate_max_out) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state, size\_t n\_in) <br>_Samples a render of_ `n_in` _points writes._ |
|  int64\_t | [**dp\_ofdm\_modulate\_why**](#function-dp_ofdm_modulate_why) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out, const char \*\* why) <br>_Render OFDM symbols from constellation points, or say why not._  |
|  size\_t | [**dp\_ofdm\_points\_remaining**](#function-dp_ofdm_points_remaining) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state) <br>_Constellation points the rest of the layout consumes._  |
|  void | [**dp\_ofdm\_reset**](#function-dp_ofdm_reset) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state) <br>_Return to the first OFDM symbol, keeping the layout._  |
|  int | [**dp\_ofdm\_set\_state**](#function-dp_ofdm_set_state) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state, const void \* blob) <br>_Restore the state; nonzero rejects_ `blob` _._ |
|  size\_t | [**dp\_ofdm\_state\_bytes**](#function-dp_ofdm_state_bytes) (const [**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* state) <br>_Bytes in the blob_ [_**dp\_ofdm\_get\_state()**_](ofdm__core_8h.md#function-dp_ofdm_get_state) _writes._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**OFDM\_ALL\_PHASES**](ofdm__core_8h.md#define-ofdm_all_phases)  `0xFFFFFFFFu`<br>_A fixed entry that applies in every pattern phase._  |
| define  | [**OFDM\_COUNT\_MAX**](ofdm__core_8h.md#define-ofdm_count_max)  `65536u`<br>_Most classes, and most fixed entries per class, a layout holds._  |
| define  | [**OFDM\_CP\_DEFAULT**](ofdm__core_8h.md#define-ofdm_cp_default)  `0xFFFFFFFFu`<br>_A run's cyclic prefix that keeps its class's._  |
| define  | [**OFDM\_DATA**](ofdm__core_8h.md#define-ofdm_data)  `1u`<br>_A subcarrier map value: the subcarrier carries a data point._  |
| define  | [**OFDM\_NULL**](ofdm__core_8h.md#define-ofdm_null)  `0u`<br>_A subcarrier map value: the subcarrier carries nothing._  |
| define  | [**OFDM\_N\_FFT\_MAX**](ofdm__core_8h.md#define-ofdm_n_fft_max)  `65536u`<br>_Largest transform length of a class._  |
| define  | [**OFDM\_N\_PHASE\_MAX**](ofdm__core_8h.md#define-ofdm_n_phase_max)  `4096u`<br>_Largest pattern period of a class, in OFDM symbols._  |
| define  | [**OFDM\_STATE\_MAGIC**](ofdm__core_8h.md#define-ofdm_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('O', 'F', 'D', 'M')`<br>_State blob magic ('O','F','D','M')._  |
| define  | [**OFDM\_STATE\_VERSION**](ofdm__core_8h.md#define-ofdm_state_version)  `1u`<br>_State blob layout version._  |

## Detailed Description


The modulator names no standard and includes nothing of wfmgen. What it renders is described by a **layout**, a list of **classes** (one kind of OFDM symbol: a transform length, a subcarrier map, a fixed pattern of pilots or training values, a scale, a cyclic prefix and a window) and a list of **runs** (a count of OFDM symbols of one class). A standard's preset is a filled-in layout; the design page, docs/design/ofdm.md, owns the shape and what forced each field.


### One OFDM symbol



Per OFDM symbol the modulator takes the data count of the symbol's pattern phase from the points, places them on the data subcarriers that no fixed entry displaces (in increasing subcarrier order), sets the fixed entries of the phase times the class's polarity element, multiplies the grid by the subcarrier gains, inverse transforms, multiplies by `scale`, reads `useful` samples cyclically from the transform output, prefixes the last `cyclic_prefix` of them, and overlap-adds the window.


With `m` the sample index into the transform output and `k` the subcarrier, the unnormalized inverse transform is `x[m] = scale * sum_k grid[k] * exp(+2j*pi*k*m/n_fft)`. The body is `x[m mod n_fft]` for `m < useful`; the symbol is the last `cyclic_prefix` body samples followed by the body.



### The window



The boundary between two OFDM symbols is a raised-cosine cross-fade of `window` samples, `rise[j] = sin^2 (pi (j + 1/2) / (2 window))` and `fall[j] = 1 - rise[j]`. The later symbol's first `window` samples are weighted by `rise`; the earlier symbol is continued past its end by `window` samples of the transform output (`x[(useful + j) mod n_fft]`), weighted by `fall`, and overlap-added. The length used at a boundary is the later symbol's class `window`; the first symbol rises from silence over its own, and the last continues over its own. A window of 0 is no window: the output is exactly the symbols, back to back. The continued tail is what crosses a call, so it is state; [**dp\_ofdm\_finish**](ofdm__core_8h.md#function-dp_ofdm_finish) returns the last one.



### The calls



Build the layout with [**dp\_ofdm\_add\_class**](ofdm__core_8h.md#function-dp_ofdm_add_class), [**dp\_ofdm\_add\_fixed**](ofdm__core_8h.md#function-dp_ofdm_add_fixed) and [**dp\_ofdm\_add\_run**](ofdm__core_8h.md#function-dp_ofdm_add_run); the first [**dp\_ofdm\_modulate**](ofdm__core_8h.md#function-dp_ofdm_modulate) (or [**dp\_ofdm\_check**](ofdm__core_8h.md#function-dp_ofdm_check)) checks it whole. Adding to a checked layout re-opens it (the position and the held window tail stay), so a signal can be extended while it is rendered. Then [**dp\_ofdm\_modulate**](ofdm__core_8h.md#function-dp_ofdm_modulate) renders whole OFDM symbols from the points it is given.



```C++
dp_ofdm_state_t *o = dp_ofdm_create ();
const uint8_t map[8] = { 0, 1, 1, 1, 0, 1, 1, 1 };   // DC and edge null
dp_ofdm_add_class (o, 8u, map, 8u, 0u, 0u, 0u, 1.0, 1u);
dp_ofdm_add_run (o, 0u, 2u, 0xFFFFFFFFu, 0u, 0u);
float _Complex pts[12], out[16];       // 6 data subcarriers x 2 symbols
for (int i = 0; i < 12; i++)
  pts[i] = 1.0f;
size_t n = dp_ofdm_modulate (o, pts, 12, out, 16);   // n == 16
dp_ofdm_destroy (o);
```
 



    
## Public Functions Documentation




### function dp\_ofdm\_add\_class 

_Append one kind of OFDM symbol and return its index._ 
```C++
int dp_ofdm_add_class (
    dp_ofdm_state_t * state,
    uint32_t n_fft,
    const uint8_t * subcarrier_map,
    size_t subcarrier_map_len,
    uint32_t useful,
    uint32_t cyclic_prefix,
    uint32_t window,
    double scale,
    uint32_t n_phase
) 
```



Copies `subcarrier_map`. The optional per-subcarrier gains and the polarity sequence are added to the class by [**dp\_ofdm\_add\_subcarrier\_gain()**](ofdm__core_8h.md#function-dp_ofdm_add_subcarrier_gain) and [**dp\_ofdm\_add\_polarity()**](ofdm__core_8h.md#function-dp_ofdm_add_polarity). Refuses (a negative return, nothing changed) what cannot be honoured; a sealed layout is re-opened.




**Parameters:**


* `state` The modulator. 
* `n_fft` Transform length, 2 to OFDM\_N\_FFT\_MAX. 
* `subcarrier_map` `n_fft` entries, index `k + n_fft/2`, each OFDM\_DATA or OFDM\_NULL. 
* `subcarrier_map_len` Must equal `n_fft`. 
* `useful` Body samples read cyclically from the transform; 0 reads as `n_fft`. 
* `cyclic_prefix` Default prefix, samples; at most `useful`. 
* `window` Window transition, samples; at most the symbol length. 
* `scale` Multiplies the transform output. 
* `n_phase` Period of the fixed pattern, at least 1. 



**Returns:**

The class index, or a negative value if refused.



```C++
>>> import numpy as np
>>> from doppler.ofdm import Ofdm
>>> o = Ofdm()
>>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
>>> o.add_class(8, m, cyclic_prefix=2)
0
>>> o.add_class(8, m, cyclic_prefix=2, window=1)
1
```
 


        

<hr>



### function dp\_ofdm\_add\_fixed 

_Append one fixed entry to a class: a pilot or a training value._ 
```C++
int dp_ofdm_add_fixed (
    dp_ofdm_state_t * state,
    uint32_t class_index,
    uint32_t phase,
    int32_t k,
    double _Complex value
) 
```



A fixed entry displaces data on its subcarrier in its phase (when the map has data there) and is multiplied by the class's polarity element.




**Parameters:**


* `state` The modulator. 
* `class_index` An existing class. 
* `phase` The pattern phase it belongs to, below the class's `n_phase`, or OFDM\_ALL\_PHASES. 
* `k` Subcarrier, `-n_fft/2 .. n_fft/2 - 1`. 
* `value` The value placed. 



**Returns:**

0, or a negative value if refused.



```C++
>>> import numpy as np
>>> from doppler.ofdm import Ofdm
>>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
>>> o = Ofdm()
>>> o.add_class(8, m, cyclic_prefix=2)
0
>>> o.add_run(0, 1)
0
>>> o.points_remaining()      # six data subcarriers
6
>>> o.add_fixed(0, 2**32 - 1, 3, 1j)   # a pilot on a data subcarrier
>>> o.points_remaining()      # it displaces one
5
```
 


        

<hr>



### function dp\_ofdm\_add\_polarity 

_Give a class a polarity sequence._ 
```C++
int dp_ofdm_add_polarity (
    dp_ofdm_state_t * state,
    uint32_t class_index,
    const float _Complex * polarity,
    size_t polarity_len
) 
```



OFDM symbol `n` of a run multiplies its fixed entries (never its data) by element `(polarity_offset + n) mod polarity_len`. Without it the scalar is 1.




**Parameters:**


* `state` The modulator. 
* `class_index` An existing class without a sequence yet. 
* `polarity` The sequence. 
* `polarity_len` At least 1. 



**Returns:**

0, or a negative value if refused.



```C++
>>> import numpy as np
>>> from doppler.ofdm import Ofdm
>>> z = np.zeros(8, np.uint8)               # no data subcarriers at all
>>> p = Ofdm(); p.add_class(8, z, cyclic_prefix=2)
0
>>> p.add_fixed(0, 2**32 - 1, 2, 1.0)      # one pilot, every phase
>>> p.add_polarity(0, np.array([1, -1], np.complex64))
>>> p.add_run(0, 2)
0
>>> x = p.modulate(np.empty(0, np.complex64))
>>> bool(np.allclose(x[2:10], -x[12:20]))   # the pilot flips sign
True
```
 


        

<hr>



### function dp\_ofdm\_add\_run 

_Append a run of OFDM symbols of one class and return its index._ 
```C++
int dp_ofdm_add_run (
    dp_ofdm_state_t * state,
    uint32_t class_index,
    uint32_t count,
    uint32_t cyclic_prefix,
    uint32_t polarity_offset,
    uint32_t first_phase
) 
```





**Parameters:**


* `state` The modulator. 
* `class_index` An existing class. 
* `count` OFDM symbols, at least 1. 
* `cyclic_prefix` Override of the class's prefix, or OFDM\_CP\_DEFAULT; at most the class's `useful`. 
* `polarity_offset` Polarity element of the run's first symbol. 
* `first_phase` Pattern phase of the run's first symbol. 



**Returns:**

The run index, or a negative value if refused.



```C++
>>> import numpy as np
>>> from doppler.ofdm import Ofdm
>>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
>>> o = Ofdm()
>>> o.add_class(8, m, cyclic_prefix=2)
0
>>> o.add_run(0, 2)
0
>>> o.add_run(0, 3)
1
```
 


        

<hr>



### function dp\_ofdm\_add\_subcarrier\_gain 

_Give a class a complex multiplier per subcarrier._ 
```C++
int dp_ofdm_add_subcarrier_gain (
    dp_ofdm_state_t * state,
    uint32_t class_index,
    const float _Complex * gain,
    size_t gain_len
) 
```



Applied to the whole grid (data and fixed entries alike): a per-subband phase rotation, or any per-subcarrier amplitude. Without it the gain is 1 everywhere.




**Parameters:**


* `state` The modulator. 
* `class_index` An existing class without gains yet. 
* `gain` `n_fft` multipliers, index `k + n_fft/2`. 
* `gain_len` Must equal the class's `n_fft`. 



**Returns:**

0, or a negative value if refused.



```C++
>>> import numpy as np
>>> from doppler.ofdm import Ofdm
>>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
>>> o = Ofdm()
>>> o.add_class(8, m, cyclic_prefix=2)
0
>>> o.add_run(0, 1)
0
>>> pts = np.ones(6, np.complex64)
>>> one = o.modulate(pts)
>>> o.reset()
>>> o.add_subcarrier_gain(0, 2 * np.ones(8, np.complex64))
>>> bool(np.allclose(o.modulate(pts), 2 * one))
True
```
 


        

<hr>



### function dp\_ofdm\_check 

_The plain form of_ [_**dp\_ofdm\_check\_why()**_](ofdm__core_8h.md#function-dp_ofdm_check_why) _: 0 if the layout is usable, a negative value if not, with no sentence. What a C caller that does not print the reason calls._
```C++
int dp_ofdm_check (
    dp_ofdm_state_t * state
) 
```





**Parameters:**


* `state` The modulator. 



**Returns:**

0 if the layout is usable, a negative value if not. 





        

<hr>



### function dp\_ofdm\_check\_why 

_Check the whole layout and derive what the render uses; say why when it is not usable. This is the binding's_ `check` _._
```C++
int dp_ofdm_check_why (
    dp_ofdm_state_t * state,
    const char ** why
) 
```



Refuses a layout with no run, a window longer than an OFDM symbol of its class, or two fixed entries on one subcarrier in one phase. Derives what the render uses (the data count per phase, the window weights, one inverse transform per distinct length). Idempotent once it has passed.




**Parameters:**


* `state` The modulator. 
* `why` Where the refusal's sentence goes: a static string naming the condition that failed. Left as the caller set it when the layout is usable. May be NULL. 



**Returns:**

0 if the layout is usable, a negative value if not.



```C++
>>> import numpy as np
>>> from doppler.ofdm import Ofdm
>>> o = Ofdm()
>>> m = np.zeros(8, np.uint8); m[[1, 2, 3]] = 1
>>> o.add_class(8, m)
0
>>> o.check()                # no run yet
Traceback (most recent call last):
    ...
ValueError: the layout has no run
>>> o.add_run(0, 1)
0
>>> o.check()                # usable: returns None
```
 


        

<hr>



### function dp\_ofdm\_create 

_Create an empty modulator (no classes, no runs)._ 
```C++
dp_ofdm_state_t * dp_ofdm_create (
    void
) 
```





**Returns:**

Heap-allocated state, never NULL: running out of memory aborts. 




**Note:**

Caller must call [**dp\_ofdm\_destroy()**](ofdm__core_8h.md#function-dp_ofdm_destroy) when done. 





        

<hr>



### function dp\_ofdm\_destroy 

_Destroy a modulator and release all memory._ 
```C++
void dp_ofdm_destroy (
    dp_ofdm_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_ofdm\_finish 

_Return the window tail still held and clear it._ 
```C++
size_t dp_ofdm_finish (
    dp_ofdm_state_t * state,
    size_t n,
    float _Complex * out,
    size_t max_out
) 
```



After the last OFDM symbol this is the final fall of the window; with a window of 0, or before anything is rendered, there is none.




**Parameters:**


* `state` The modulator. 
* `n` Samples asked for; the held tail is returned up to this. 
* `out` Receives the tail. 
* `max_out` Capacity of `out`. 



**Returns:**

Samples written.



```C++
>>> import numpy as np
>>> from doppler.ofdm import Ofdm
>>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
>>> o = Ofdm()
>>> o.add_class(8, m, cyclic_prefix=2, window=2)
0
>>> o.add_run(0, 2)
0
>>> x = o.modulate(np.ones(12, np.complex64))
>>> o.finish().size          # the window's last fall
2
>>> o.finish().size          # and it is cleared
0
```
 


        

<hr>



### function dp\_ofdm\_finish\_max\_out 

_Capacity_ [_**dp\_ofdm\_finish()**_](ofdm__core_8h.md#function-dp_ofdm_finish) _needs for_`n` _samples._
```C++
size_t dp_ofdm_finish_max_out (
    dp_ofdm_state_t * state,
    size_t n
) 
```





**Parameters:**


* `state` The modulator. 
* `n` Samples asked for. 



**Returns:**

The held tail's length, at most `n`. 





        

<hr>



### function dp\_ofdm\_get\_state 

_Write the state into_ `blob` _._
```C++
void dp_ofdm_get_state (
    const dp_ofdm_state_t * state,
    void * blob
) 
```



The blob holds the position (run index, OFDM symbols rendered of the run, the held window tail) and a hash of the layout, so it never restores into a different one.




**Parameters:**


* `state` The modulator. 
* `blob` Receives [**dp\_ofdm\_state\_bytes()**](ofdm__core_8h.md#function-dp_ofdm_state_bytes) bytes. 




        

<hr>



### function dp\_ofdm\_modulate 

_The plain form of_ [_**dp\_ofdm\_modulate\_why()**_](ofdm__core_8h.md#function-dp_ofdm_modulate_why) _._
```C++
size_t dp_ofdm_modulate (
    dp_ofdm_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `state` The modulator. 
* `in` Constellation points. 
* `n_in` Number of points. 
* `out` Receives the samples. 
* `max_out` Capacity of `out`. 



**Returns:**

Samples written, or 0 if the call is refused or writes none; a caller who must tell the two apart uses [**dp\_ofdm\_modulate\_why()**](ofdm__core_8h.md#function-dp_ofdm_modulate_why). 





        

<hr>



### function dp\_ofdm\_modulate\_max\_out 

_Samples a render of_ `n_in` _points writes._
```C++
size_t dp_ofdm_modulate_max_out (
    dp_ofdm_state_t * state,
    size_t n_in
) 
```



Simulates the render from the current position: OFDM symbols are taken in layout order until the next one needs more points than remain. The result is zero if `n_in` ends inside an OFDM symbol or runs past the layout (that call is refused), for a layout that does not seal, and for no points when the next OFDM symbol needs some. Use it to size the output before the call.




**Parameters:**


* `state` The modulator. 
* `n_in` Constellation points. 



**Returns:**

Samples that call will write.



```C++
>>> import numpy as np
>>> from doppler.ofdm import Ofdm
>>> o = Ofdm()
>>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
>>> o.add_class(8, m, cyclic_prefix=2)
0
>>> o.add_run(0, 3)
0
>>> o.modulate_max_out(12)   # two OFDM symbols of 10 samples
20
>>> o.modulate_max_out(5)    # ends inside a symbol
0
```
 


        

<hr>



### function dp\_ofdm\_modulate\_why 

_Render OFDM symbols from constellation points, or say why not._ 
```C++
int64_t dp_ofdm_modulate_why (
    dp_ofdm_state_t * state,
    const float _Complex * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out,
    const char ** why
) 
```





**Parameters:**


* `state` The modulator; sealed by this call if it is not. 
* `in` Constellation points, consumed in layout order. 
* `n_in` Number of points; must end on an OFDM symbol boundary. 
* `out` Receives the samples. 
* `max_out` Capacity of `out`. Short is a refusal, not a truncation. 
* `why` NULL, or receives a static sentence for a refusal and is left as the caller set it otherwise: an unusable layout (the sentence [**dp\_ofdm\_check\_why()**](ofdm__core_8h.md#function-dp_ofdm_check_why) gives), points that end inside an OFDM symbol, more points than the layout has left, or an output buffer shorter than [**dp\_ofdm\_modulate\_max\_out()**](ofdm__core_8h.md#function-dp_ofdm_modulate_max_out). 



**Returns:**

Samples written, which may be 0: no points at the end of the layout, or zero points mid-layout when the next OFDM symbol needs some. A refused call changes nothing and reports it by face: C returns a negative value and sets `why`; the Python binding raises ValueError with that sentence.



```C++
>>> import numpy as np
>>> from doppler.ofdm import Ofdm
>>> o = Ofdm()
>>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
>>> o.add_class(8, m, cyclic_prefix=2)
0
>>> o.add_run(0, 3)
0
>>> o.points_remaining()
18
>>> x = o.modulate(np.ones(12, np.complex64))   # two of the three
>>> x.size
20
>>> o.points_remaining()
6
>>> o.modulate(np.ones(5, np.complex64))        # inside a symbol
Traceback (most recent call last):
    ...
ValueError: the points end inside an OFDM symbol: pass a count that ends where one does (see points_remaining and modulate_max_out)
```
 


        

<hr>



### function dp\_ofdm\_points\_remaining 

_Constellation points the rest of the layout consumes._ 
```C++
size_t dp_ofdm_points_remaining (
    dp_ofdm_state_t * state
) 
```



Zero for a layout that does not seal.




**Parameters:**


* `state` The modulator. 



**Returns:**

Points from the current position to the end of the layout.



```C++
>>> import numpy as np
>>> from doppler.ofdm import Ofdm
>>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
>>> o = Ofdm()
>>> o.add_class(8, m, cyclic_prefix=2)
0
>>> o.add_run(0, 3)
0
>>> o.points_remaining()
18
```
 


        

<hr>



### function dp\_ofdm\_reset 

_Return to the first OFDM symbol, keeping the layout._ 
```C++
void dp_ofdm_reset (
    dp_ofdm_state_t * state
) 
```



The next render starts from the post-seal state: the first run, no held tail. The layout, sealed or not, is unchanged.




**Parameters:**


* `state` Must be non-NULL.


```C++
>>> import numpy as np
>>> from doppler.ofdm import Ofdm
>>> m = np.zeros(8, np.uint8); m[[1, 2, 3, 5, 6, 7]] = 1
>>> o = Ofdm()
>>> o.add_class(8, m, cyclic_prefix=2)
0
>>> o.add_run(0, 2)
0
>>> x = o.modulate(np.ones(12, np.complex64))
>>> o.reset()
>>> o.points_remaining()
12
```
 


        

<hr>



### function dp\_ofdm\_set\_state 

_Restore the state; nonzero rejects_ `blob` _._
```C++
int dp_ofdm_set_state (
    dp_ofdm_state_t * state,
    const void * blob
) 
```



Rejects a blob whose layout hash differs from this modulator's, whose position names no OFDM symbol of the layout, or whose held tail is not the length the position implies. The modulator is untouched on rejection.




**Parameters:**


* `state` A modulator built with the same layout. 
* `blob` A blob from [**dp\_ofdm\_get\_state()**](ofdm__core_8h.md#function-dp_ofdm_get_state), [**dp\_ofdm\_state\_bytes()**](ofdm__core_8h.md#function-dp_ofdm_state_bytes) long. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if rejected. 





        

<hr>



### function dp\_ofdm\_state\_bytes 

_Bytes in the blob_ [_**dp\_ofdm\_get\_state()**_](ofdm__core_8h.md#function-dp_ofdm_get_state) _writes._
```C++
size_t dp_ofdm_state_bytes (
    const dp_ofdm_state_t * state
) 
```



A function of the layout alone (the envelope, the position and one window-tail slot per sample of the largest window), so it does not change as the render advances.




**Parameters:**


* `state` The modulator. 



**Returns:**

Blob size in bytes. 





        

<hr>
## Macro Definition Documentation





### define OFDM\_ALL\_PHASES 

_A fixed entry that applies in every pattern phase._ 
```C++
#define OFDM_ALL_PHASES `0xFFFFFFFFu`
```




<hr>



### define OFDM\_COUNT\_MAX 

_Most classes, and most fixed entries per class, a layout holds._ 
```C++
#define OFDM_COUNT_MAX `65536u`
```




<hr>



### define OFDM\_CP\_DEFAULT 

_A run's cyclic prefix that keeps its class's._ 
```C++
#define OFDM_CP_DEFAULT `0xFFFFFFFFu`
```




<hr>



### define OFDM\_DATA 

_A subcarrier map value: the subcarrier carries a data point._ 
```C++
#define OFDM_DATA `1u`
```




<hr>



### define OFDM\_NULL 

_A subcarrier map value: the subcarrier carries nothing._ 
```C++
#define OFDM_NULL `0u`
```




<hr>



### define OFDM\_N\_FFT\_MAX 

_Largest transform length of a class._ 
```C++
#define OFDM_N_FFT_MAX `65536u`
```




<hr>



### define OFDM\_N\_PHASE\_MAX 

_Largest pattern period of a class, in OFDM symbols._ 
```C++
#define OFDM_N_PHASE_MAX `4096u`
```




<hr>



### define OFDM\_STATE\_MAGIC 

_State blob magic ('O','F','D','M')._ 
```C++
#define OFDM_STATE_MAGIC `DP_FOURCC ('O', 'F', 'D', 'M')`
```




<hr>



### define OFDM\_STATE\_VERSION 

_State blob layout version._ 
```C++
#define OFDM_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ofdm/ofdm_core.h`

