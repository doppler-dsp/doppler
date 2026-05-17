

# File lo\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**lo**](dir_e3bbeba8c021d4d74db794db08bafd77.md) **>** [**lo\_core.h**](lo__core_8h.md)

[Go to the source code of this file](lo__core_8h_source.md)

_Local oscillator: NCO + 2^16 sin/cos LUT → CF32 phasors._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**lo\_state\_t**](structlo__state__t.md) <br>_LO state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**lo\_state\_t**](structlo__state__t.md) \* | [**lo\_create**](#function-lo_create) (double norm\_freq) <br>_Create an LO instance._  |
|  void | [**lo\_destroy**](#function-lo_destroy) ([**lo\_state\_t**](structlo__state__t.md) \* state) <br> |
|  double | [**lo\_get\_norm\_freq**](#function-lo_get_norm_freq) (const [**lo\_state\_t**](structlo__state__t.md) \* state) <br> |
|  uint32\_t | [**lo\_get\_phase**](#function-lo_get_phase) (const [**lo\_state\_t**](structlo__state__t.md) \* state) <br> |
|  uint32\_t | [**lo\_get\_phase\_inc**](#function-lo_get_phase_inc) (const [**lo\_state\_t**](structlo__state__t.md) \* state) <br> |
|  void | [**lo\_reset**](#function-lo_reset) ([**lo\_state\_t**](structlo__state__t.md) \* state) <br> |
|  void | [**lo\_set\_norm\_freq**](#function-lo_set_norm_freq) ([**lo\_state\_t**](structlo__state__t.md) \* state, double norm\_freq) <br> |
|  void | [**lo\_set\_phase**](#function-lo_set_phase) ([**lo\_state\_t**](structlo__state__t.md) \* state, uint32\_t phase) <br> |
|  size\_t | [**lo\_steps**](#function-lo_steps) ([**lo\_state\_t**](structlo__state__t.md) \* state, size\_t n, float complex \* out) <br>_Generate n CF32 phasors at the current norm\_freq._  |
|  size\_t | [**lo\_steps\_ctrl**](#function-lo_steps_ctrl) ([**lo\_state\_t**](structlo__state__t.md) \* state, const float \* ctrl, size\_t ctrl\_len, float complex \* out) <br>_Generate CF32 phasors with per-sample FM deviation._  |
|  size\_t | [**lo\_steps\_ctrl\_max\_out**](#function-lo_steps_ctrl_max_out) ([**lo\_state\_t**](structlo__state__t.md) \* state) <br> |
|  size\_t | [**lo\_steps\_max\_out**](#function-lo_steps_max_out) ([**lo\_state\_t**](structlo__state__t.md) \* state) <br>_Maximum samples per call (determines pre-allocated buffer size)._  |




























## Detailed Description


The 32-bit phase accumulator drives a static 65536-entry float sine LUT. The top 16 bits of the phase select the LUT index; a quarter-cycle offset (LUT\_QTR = 16384) converts sin to cos without extra storage:


idx = phase &gt;&gt; 16 out(i) = cos(θ) + j·sin(θ) = lut((idx + LUT\_QTR) & 0xFFFF) + j·lut(idx)


Output is emitted BEFORE the phase is incremented (same convention as NCO).


The 16-bit phase truncation gives ~96 dBc SFDR.


Lifecycle: lo\_create → (steps / reset)\* → lo\_destroy



```C++
lo_state_t *lo = lo_create(0.25);
float complex out[4];
lo_steps(lo, 4, out);
// out ≈ { 1+0j, 0+1j, -1+0j, 0-1j }
lo_destroy(lo);
```
 


    
## Public Functions Documentation




### function lo\_create 

_Create an LO instance._ 
```C++
lo_state_t * lo_create (
    double norm_freq
) 
```



Initialises the shared LUT on first call (thread-safe concern: use from a single thread or call [**lo\_create()**](lo__core_8h.md#function-lo_create) before spawning threads).




**Parameters:**


* `norm_freq` Normalised frequency (cycles per sample). Any value; fractional part used internally. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 





        

<hr>



### function lo\_destroy 

```C++
void lo_destroy (
    lo_state_t * state
) 
```



Free all resources. May be NULL (no-op). 


        

<hr>



### function lo\_get\_norm\_freq 

```C++
double lo_get_norm_freq (
    const lo_state_t * state
) 
```




<hr>



### function lo\_get\_phase 

```C++
uint32_t lo_get_phase (
    const lo_state_t * state
) 
```




<hr>



### function lo\_get\_phase\_inc 

```C++
uint32_t lo_get_phase_inc (
    const lo_state_t * state
) 
```




<hr>



### function lo\_reset 

```C++
void lo_reset (
    lo_state_t * state
) 
```



Zero the phase accumulator. norm\_freq is unchanged. 


        

<hr>



### function lo\_set\_norm\_freq 

```C++
void lo_set_norm_freq (
    lo_state_t * state,
    double norm_freq
) 
```




<hr>



### function lo\_set\_phase 

```C++
void lo_set_phase (
    lo_state_t * state,
    uint32_t phase
) 
```




<hr>



### function lo\_steps 

_Generate n CF32 phasors at the current norm\_freq._ 
```C++
size_t lo_steps (
    lo_state_t * state,
    size_t n,
    float complex * out
) 
```



Output is emitted before increment: out(0) corresponds to the phase at entry, out(1) to phase + phase\_inc, etc. Returns n. 


        

<hr>



### function lo\_steps\_ctrl 

_Generate CF32 phasors with per-sample FM deviation._ 
```C++
size_t lo_steps_ctrl (
    lo_state_t * state,
    const float * ctrl,
    size_t ctrl_len,
    float complex * out
) 
```



ctrl(i) (real float, fractional part used) is converted to a per-sample phase-increment delta added on top of the base phase\_inc. The base norm\_freq is not modified.


Output length equals ctrl\_len. Returns ctrl\_len. 


        

<hr>



### function lo\_steps\_ctrl\_max\_out 

```C++
size_t lo_steps_ctrl_max_out (
    lo_state_t * state
) 
```




<hr>



### function lo\_steps\_max\_out 

_Maximum samples per call (determines pre-allocated buffer size)._ 
```C++
size_t lo_steps_max_out (
    lo_state_t * state
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/lo/lo_core.h`

