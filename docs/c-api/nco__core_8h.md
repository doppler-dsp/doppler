

# File nco\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**nco**](dir_2f9ed967bc16fefd26d0244d883adb58.md) **>** [**nco\_core.h**](nco__core_8h.md)

[Go to the source code of this file](nco__core_8h_source.md)

_Pure phase-accumulator NCO._ [More...](#detailed-description)

* `#include "clib_common.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**nco\_state\_t**](structnco__state__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**nco\_state\_t**](structnco__state__t.md) \* | [**nco\_create**](#function-nco_create) (float norm\_freq, uint32\_t nmax) <br>_Create an NCO._  |
|  void | [**nco\_destroy**](#function-nco_destroy) ([**nco\_state\_t**](structnco__state__t.md) \* nco) <br> |
|  void | [**nco\_execute\_u32**](#function-nco_execute_u32) ([**nco\_state\_t**](structnco__state__t.md) \* nco, uint32\_t \* out, size\_t n) <br>_Output n raw 32-bit accumulator values._  |
|  void | [**nco\_execute\_u32\_ovf**](#function-nco_execute_u32_ovf) ([**nco\_state\_t**](structnco__state__t.md) \* nco, uint32\_t \* out, uint8\_t \* carry, size\_t n) <br>_Output n raw accumulator values and per-sample overflow flags._  |
|  void | [**nco\_execute\_u32\_scaled**](#function-nco_execute_u32_scaled) ([**nco\_state\_t**](structnco__state__t.md) \* nco, uint32\_t \* out, size\_t n) <br>_Output n accumulator values scaled to [0, nmax)._  |
|  float | [**nco\_get\_freq**](#function-nco_get_freq) (const [**nco\_state\_t**](structnco__state__t.md) \* nco) <br> |
|  uint32\_t | [**nco\_get\_phase**](#function-nco_get_phase) (const [**nco\_state\_t**](structnco__state__t.md) \* nco) <br> |
|  uint32\_t | [**nco\_get\_phase\_inc**](#function-nco_get_phase_inc) (const [**nco\_state\_t**](structnco__state__t.md) \* nco) <br> |
|  void | [**nco\_reset**](#function-nco_reset) ([**nco\_state\_t**](structnco__state__t.md) \* nco) <br> |
|  void | [**nco\_set\_freq**](#function-nco_set_freq) ([**nco\_state\_t**](structnco__state__t.md) \* nco, float norm\_freq) <br> |
|  void | [**nco\_set\_phase**](#function-nco_set_phase) ([**nco\_state\_t**](structnco__state__t.md) \* nco, uint32\_t phase) <br> |




























## Detailed Description


Lifted from [**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) (c/src/nco.c) — phase-accumulator half only. The sine-LUT / complex-phasor output lives in lo\_core (LO object).


The 32-bit unsigned accumulator advances by phase\_inc each sample and wraps naturally at 2^32. Three output mappings are provided:


nco\_execute\_u32 raw accumulator value [0, 2^32) nco\_execute\_u32\_scaled (uint64)phase \* nmax &gt;&gt; 32 → [0, nmax) nco\_execute\_u32\_ovf raw + per-sample carry/overflow flag


nmax=0 in nco\_execute\_u32\_scaled is treated identically to nco\_execute\_u32 (returns raw accumulator unchanged).


Normalised-frequency → phase\_inc conversion:


phase\_inc = floor((norm\_freq mod 1.0) × 2^32)


Negative frequencies fold correctly: −0.25 → phase\_inc = 3×2^30.



```C++
nco_state_t *nco = nco_create(0.25f, 0);
uint32_t out[256];
nco_execute_u32(nco, out, 256);
nco_destroy(nco);
```



reset() zeroes phase only; norm\_freq and nmax are unchanged.



## Public Functions Documentation




### function nco\_create

_Create an NCO._
```C++
nco_state_t * nco_create (
    float norm_freq,
    uint32_t nmax
)
```





**Parameters:**


* `norm_freq` Normalised frequency (cycles per sample). Any float; folded into [0, 1) internally.
* `nmax` Wrap target for nco\_execute\_u32\_scaled. Pass 0 to always return the raw accumulator.



**Returns:**

Heap-allocated state, or NULL on OOM.







<hr>



### function nco\_destroy

```C++
void nco_destroy (
    nco_state_t * nco
)
```



Free all resources. NULL is a no-op.




<hr>



### function nco\_execute\_u32

_Output n raw 32-bit accumulator values._
```C++
void nco_execute_u32 (
    nco_state_t * nco,
    uint32_t * out,
    size_t n
)
```





**Parameters:**


* `nco` Must be non-NULL.
* `out` Output buffer, length &gt;= n (uint32\_t).
* `n` Number of samples.






<hr>



### function nco\_execute\_u32\_ovf

_Output n raw accumulator values and per-sample overflow flags._
```C++
void nco_execute_u32_ovf (
    nco_state_t * nco,
    uint32_t * out,
    uint8_t * carry,
    size_t n
)
```



carry[i] == 1 when the accumulator wrapped on sample i (one input period elapsed), 0 otherwise. Useful for polyphase sample-clock generation — the carry marks when to consume the next input sample.




**Parameters:**


* `nco` Must be non-NULL.
* `out` Raw phase values, length &gt;= n (uint32\_t).
* `carry` Overflow flags, length &gt;= n (uint8\_t).
* `n` Number of samples.






<hr>



### function nco\_execute\_u32\_scaled

_Output n accumulator values scaled to [0, nmax)._
```C++
void nco_execute_u32_scaled (
    nco_state_t * nco,
    uint32_t * out,
    size_t n
)
```



Uses the branchless fixed-point identity: out[i] = (uint64\_t)phase \* nmax &gt;&gt; 32


Works for any nmax without division. When nco-&gt;nmax == 0, falls back to the raw accumulator (same as nco\_execute\_u32).




**Parameters:**


* `nco` Must be non-NULL.
* `out` Output buffer, length &gt;= n (uint32\_t).
* `n` Number of samples.






<hr>



### function nco\_get\_freq

```C++
float nco_get_freq (
    const nco_state_t * nco
)
```




<hr>



### function nco\_get\_phase

```C++
uint32_t nco_get_phase (
    const nco_state_t * nco
)
```




<hr>



### function nco\_get\_phase\_inc

```C++
uint32_t nco_get_phase_inc (
    const nco_state_t * nco
)
```




<hr>



### function nco\_reset

```C++
void nco_reset (
    nco_state_t * nco
)
```



Zero phase accumulator. norm\_freq and nmax are unchanged.




<hr>



### function nco\_set\_freq

```C++
void nco_set_freq (
    nco_state_t * nco,
    float norm_freq
)
```



Update normalised frequency without disturbing the phase.




<hr>



### function nco\_set\_phase

```C++
void nco_set_phase (
    nco_state_t * nco,
    uint32_t phase
)
```



Write phase accumulator (phase seek / sync).




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/nco/nco_core.h`
