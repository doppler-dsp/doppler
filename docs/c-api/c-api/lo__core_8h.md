

# File lo\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**lo**](dir_e3bbeba8c021d4d74db794db08bafd77.md) **>** [**lo\_core.h**](lo__core_8h.md)

[Go to the source code of this file](lo__core_8h_source.md)

_Local oscillator: NCO phase accumulator + 2^16 sin/cos LUT._ [More...](#detailed-description)

* `#include "clib_common.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**lo\_state\_t**](structlo__state__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**lo\_state\_t**](structlo__state__t.md) \* | [**lo\_create**](#function-lo_create) (float norm\_freq) <br>_Create a local oscillator._  |
|  void | [**lo\_destroy**](#function-lo_destroy) ([**lo\_state\_t**](structlo__state__t.md) \* lo) <br> |
|  void | [**lo\_execute\_cf32**](#function-lo_execute_cf32) ([**lo\_state\_t**](structlo__state__t.md) \* lo, float \_Complex \* out, size\_t n) <br>_Generate n complex phasors (CF32)._  |
|  void | [**lo\_execute\_cf32\_ctrl**](#function-lo_execute_cf32_ctrl) ([**lo\_state\_t**](structlo__state__t.md) \* lo, const float \* ctrl, float \_Complex \* out, size\_t n) <br>_Generate n complex phasors with per-sample FM control._  |
|  float | [**lo\_get\_freq**](#function-lo_get_freq) (const [**lo\_state\_t**](structlo__state__t.md) \* lo) <br> |
|  uint32\_t | [**lo\_get\_phase**](#function-lo_get_phase) (const [**lo\_state\_t**](structlo__state__t.md) \* lo) <br> |
|  uint32\_t | [**lo\_get\_phase\_inc**](#function-lo_get_phase_inc) (const [**lo\_state\_t**](structlo__state__t.md) \* lo) <br> |
|  void | [**lo\_reset**](#function-lo_reset) ([**lo\_state\_t**](structlo__state__t.md) \* lo) <br> |
|  void | [**lo\_set\_freq**](#function-lo_set_freq) ([**lo\_state\_t**](structlo__state__t.md) \* lo, float norm\_freq) <br> |
|  void | [**lo\_set\_phase**](#function-lo_set_phase) ([**lo\_state\_t**](structlo__state__t.md) \* lo, uint32\_t phase) <br> |




























## Detailed Description


Lifted from [**dp\_nco\_t**](nco_8h.md#typedef-dp_nco_t) (c/src/nco.c) — LUT and complex-phasor section. The pure phase-accumulator output (u32/ovf) lives in nco\_core.


A 2^16-entry float32 sine LUT is initialised once (lazy, guarded by a static flag) and shared across all [**lo\_state\_t**](structlo__state__t.md) instances. Memory: 256 KB (fits comfortably in L2 on all modern CPUs). SFDR: ~96 dBc (16-bit phase truncation → 6 × 16 dB rule).


LUT addressing: sin(θ) = lut[phase &gt;&gt; 16] cos(θ) = lut[(uint16\_t)((phase &gt;&gt; 16) + LUT\_QTR)] (LUT\_QTR = 16384)


On x86 targets with AVX-512F, lo\_execute\_cf32 uses gather loads to compute 16 phasors per iteration. All other targets use the scalar path.


reset() zeroes phase only; norm\_freq is unchanged.



```C++
lo_state_t *lo = lo_create(0.1f);
float _Complex out[256];
lo_execute_cf32(lo, out, 256);
lo_destroy(lo);
```




## Public Functions Documentation




### function lo\_create

_Create a local oscillator._
```C++
lo_state_t * lo_create (
    float norm_freq
)
```



Triggers LUT initialisation on first call (one-time cost).




**Parameters:**


* `norm_freq` Normalised frequency (cycles per sample). Any float; folded into [0, 1) internally.



**Returns:**

Heap-allocated state, or NULL on OOM.







<hr>



### function lo\_destroy

```C++
void lo_destroy (
    lo_state_t * lo
)
```



Free all resources. NULL is a no-op.




<hr>



### function lo\_execute\_cf32

_Generate n complex phasors (CF32)._
```C++
void lo_execute_cf32 (
    lo_state_t * lo,
    float _Complex * out,
    size_t n
)
```



Each sample is cos(θ) + j·sin(θ) where θ advances by phase\_inc per sample. On AVX-512F builds, processes 16 samples per loop iteration using i32gather from the float LUT.


Output buffer sizing: allocate at least n samples.




**Parameters:**


* `lo` Must be non-NULL.
* `out` CF32 output, length &gt;= n.
* `n` Number of samples.






<hr>



### function lo\_execute\_cf32\_ctrl

_Generate n complex phasors with per-sample FM control._
```C++
void lo_execute_cf32_ctrl (
    lo_state_t * lo,
    const float * ctrl,
    float _Complex * out,
    size_t n
)
```



ctrl[i] is a normalised-frequency deviation added to the base phase\_inc before each sample: inc\_eff = phase\_inc + (uint32\_t)floor(ctrl[i] × 2^32)


Mirrors dp\_nco\_execute\_cf32\_ctrl. Used by the DDC chain for closed-loop frequency tracking.




**Parameters:**


* `lo` Must be non-NULL.
* `ctrl` Per-sample frequency deviations, float32, length &gt;= n.
* `out` CF32 output, length &gt;= n.
* `n` Number of samples.






<hr>



### function lo\_get\_freq

```C++
float lo_get_freq (
    const lo_state_t * lo
)
```




<hr>



### function lo\_get\_phase

```C++
uint32_t lo_get_phase (
    const lo_state_t * lo
)
```




<hr>



### function lo\_get\_phase\_inc

```C++
uint32_t lo_get_phase_inc (
    const lo_state_t * lo
)
```




<hr>



### function lo\_reset

```C++
void lo_reset (
    lo_state_t * lo
)
```



Zero phase accumulator. norm\_freq is unchanged.




<hr>



### function lo\_set\_freq

```C++
void lo_set_freq (
    lo_state_t * lo,
    float norm_freq
)
```



Update normalised frequency without disturbing the phase.




<hr>



### function lo\_set\_phase

```C++
void lo_set_phase (
    lo_state_t * lo,
    uint32_t phase
)
```



Write phase accumulator (phase seek / sync).




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/lo/lo_core.h`
