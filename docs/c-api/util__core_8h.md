

# File util\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**util**](dir_301ecbdb0604927cf0b3895ddfaba60f.md) **>** [**util\_core.h**](util__core_8h.md)

[Go to the source code of this file](util__core_8h_source.md)

_Util module — public C API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include <math.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**dp\_bitorder\_t**](#enum-dp_bitorder_t)  <br>_Bit order within a byte, for_ [_**hex\_to\_bin**_](util__core_8h.md#function-hex_to_bin) _and_[_**bin\_to\_hex**_](util__core_8h.md#function-bin_to_hex) _._ |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**bin\_to\_hex**](#function-bin_to_hex) (const uint8\_t \* bits, size\_t n\_bits, char \* out, size\_t max\_out, int bitorder) <br>_Render unpacked bits back to a hex string — the exact inverse._  |
|  int | [**bin\_to\_int**](#function-bin_to_int) (const uint8\_t \* bits, size\_t n\_bits, uint64\_t \* out, int bitorder) <br>_Read unpacked bits back into an integer — the exact inverse._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**ema\_alpha\_decim**](#function-ema_alpha_decim) (double alpha, size\_t d) <br>_The EMA coefficient that advances_ `d` _samples in one step:_`1 - (1 - alpha)^d` _._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**ema\_step**](#function-ema_step) (double state, double x, double alpha) <br>_One step of a first-order exponential moving average:_ `state <- state + alpha * (x - state)` _._ |
|  size\_t | [**hex\_to\_bin**](#function-hex_to_bin) (const char \* hex, uint8\_t \* out, size\_t max\_out, int bitorder) <br>_Expand a hex string to unpacked bits, one per byte._  |
|  size\_t | [**int\_to\_bin**](#function-int_to_bin) (uint64\_t v, unsigned n\_bits, uint8\_t \* out, size\_t max\_out, int bitorder) <br>_Expand the low_ `n_bits` _of an integer to unpacked bits._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**saturate**](#function-saturate) (double v, double lo, double hi, double nan\_to) <br>_Saturate a value into_ `[lo, hi]` _,_**total over every double** _— including NaN and both infinities._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float complex | [**square\_clip**](#function-square_clip) (float complex y, float lin) <br>_Square-clip a complex sample: clip the real and imaginary parts independently to_ `[-lin, lin]` _(a square region in the IQ plane, not a circular magnitude limit). Each component is passed through unchanged when its magnitude is within the threshold and clamped to the nearest boundary otherwise._ |




























## Detailed Description


The util functions are header-only and JM\_FORCEINLINE: any caller that includes this header inlines them with zero call overhead, and the util Python extension module exposes the very same definitions. There is one source of truth per function, here. 


    
## Public Types Documentation




### enum dp\_bitorder\_t 

_Bit order within a byte, for_ [_**hex\_to\_bin**_](util__core_8h.md#function-hex_to_bin) _and_[_**bin\_to\_hex**_](util__core_8h.md#function-bin_to_hex) _._
```C++
enum dp_bitorder_t {
    DP_BITORDER_BIG = 0,
    DP_BITORDER_LITTLE = 1
};
```



The name and the values follow numpy's `packbits`/`unpackbits` `bitorder=` argument, because that is the convention anyone writing this conversion has already met. It is a DIFFERENT axis from the `endian` (`le`/`be`) used by the BLUE writer, which selects a file's BYTE order — the `"EEEI"` / `"IEEE"` field of a type-1000 header. A hex literal's character order already fixes which byte comes first; what is left to choose is the order of bits inside one. Overloading one word for both would make a sync word and a sample stream disagree silently. 


        

<hr>
## Public Functions Documentation




### function bin\_to\_hex 

_Render unpacked bits back to a hex string — the exact inverse._ 
```C++
size_t bin_to_hex (
    const uint8_t * bits,
    size_t n_bits,
    char * out,
    size_t max_out,
    int bitorder
) 
```



Round-tripping is the property worth relying on and the one its test asserts: for any literal, `bin_to_hex(hex_to_bin(s))` is `s` (lower-case), in either bit order.




**Parameters:**


* `bits` `n_bits` unpacked bits; any non-zero byte reads as 1. 
* `n_bits` number of bits; must be a multiple of 4. 
* `out` receives the digits plus a NUL. 
* `max_out` capacity of `out` in chars, NUL included. 
* `bitorder` DP\_BITORDER\_BIG or DP\_BITORDER\_LITTLE. 



**Returns:**

digits written, NOT counting the NUL, or 0 if `n_bits` is not a multiple of 4, on NULL, an unknown `bitorder`, or `max_out` too small — in which case `out` is untouched. 





        

<hr>



### function bin\_to\_int 

_Read unpacked bits back into an integer — the exact inverse._ 
```C++
int bin_to_int (
    const uint8_t * bits,
    size_t n_bits,
    uint64_t * out,
    int bitorder
) 
```



Returns a status rather than the value because every `uint64_t` is a legitimate result, so there is no value left over to mean "refused".




**Parameters:**


* `bits` `n_bits` unpacked bits; any non-zero byte reads as 1. 
* `n_bits` 1..64. 
* `out` receives the value. 
* `bitorder` DP\_BITORDER\_BIG or DP\_BITORDER\_LITTLE. 



**Returns:**

0, or -1 if `n_bits` is 0 or over 64, on NULL, or an unknown `bitorder` — in which case `out` is untouched. 





        

<hr>



### function ema\_alpha\_decim 

_The EMA coefficient that advances_ `d` _samples in one step:_`1 - (1 - alpha)^d` _._
```C++
JM_FORCEINLINE double ema_alpha_decim (
    double alpha,
    size_t d
) 
```



A decimated loop updates its average once per chunk of `d` samples and must not thereby change its own time constant. Compounding the pole exactly is what makes `decim` a performance knob instead of a retune.


#### Why &lt;tt&gt;expm1&lt;/tt&gt;/&lt;tt&gt;log1p&lt;/tt&gt; rather than the direct expression



`1.0 - pow(1.0 - alpha, d)` cancels catastrophically for small `alpha`, and the damage is worst exactly where a narrow-band estimator lives. Measured at `d == 1`, where the answer must be `alpha` itself:



|`alpha`   |direct `1-(1-alpha)^1`   |this function    |
|-----|-----|-----|
|0.05   |6 ulps off   |exact    |
|1e-5   |26865 ulps off   |exact   |






`agc_steps` used the repeated-multiply form and had this defect; it now forms BOTH its per-chunk coefficients with this function. Being exact at `d == 1` is the property that lets a caller set `decim = 1` and get bit-for-bit the undecimated recursion, so the decimated and per-sample paths can be compared at all.




**Parameters:**


* `alpha` Per-sample coefficient in `[0, 1]`. 
* `d` Chunk length in samples, `>= 1`. 



**Returns:**

The per-chunk coefficient, in `[0, 1]`. 
```C++
>>> from doppler.util import ema_alpha_decim
>>> ema_alpha_decim(0.05, 1)         # d == 1 returns alpha exactly
0.05
>>> round(ema_alpha_decim(0.05, 8), 12)
0.336579568711
>>> ema_alpha_decim(1.0, 4)          # pass-through stays pass-through
1.0
>>> ema_alpha_decim(0.0, 8)          # frozen stays frozen
0.0
```
 






        

<hr>



### function ema\_step 

_One step of a first-order exponential moving average:_ `state <- state + alpha * (x - state)` _._
```C++
JM_FORCEINLINE double ema_step (
    double state,
    double x,
    double alpha
) 
```



The canonical EMA for the whole library. It was written out four times before this existed — `agc` (power detector), `async_dsss_receiver` (the lock\_num/lock\_den pair), `acc_trace` (ACC\_TRACE\_EXP) and the recursion `det_ema_alpha` sizes — in **two different algebraic forms**, which are identical on paper and not in floating point. Duplicated implementations drift; this is the one.


#### Why this form, and not &lt;tt&gt;alpha\*x + (1-alpha)\*state&lt;/tt&gt;



Both were measured against a 60-digit reference over 5000 steps. The incremental form written here is the more accurate one everywhere the library actually operates, by a margin that grows as the average gets longer — which is the direction a narrow-band estimator moves:



|`alpha`   |this form   |`alpha*x + (1-alpha)*state`    |
|-----|-----|-----|
|0.05   |9.0e-17   |6.5e-16    |
|1e-3   |3.1e-16   |1.6e-15    |
|1e-5   |2.7e-17   |5.4e-15   |






The other form wins exactly one case, and it is a boundary rather than a regime: at `alpha == 1` it returns `x` bit-exactly while the incremental form does not (measured inexact for 9.6% of random `(state, x)` pairs, because `state + 1*(x - state)` rounds twice). That case is real — `det_ema_alpha` returns exactly 1.0 for "no gain
requested, so no averaging" — so it is handled explicitly below rather than paid for at every alpha.




**Parameters:**


* `state` Current EMA state. 
* `x` New observation. 
* `alpha` Coefficient in `[0, 1]`. `1` is pass-through (no averaging) and is exact; `0` freezes the state and is exact. A value above 1 saturates to pass-through rather than overshooting. 



**Returns:**

The updated state.




**Note:**

NOT total in `x`: a non-finite observation poisons the state permanently, because an EMA remembers. That is deliberate — the guard belongs at the boundary where an untrusted value first becomes persistent state, which is this function's input. Use [**saturate**](util__core_8h.md#function-saturate) there, as `agc_steps` does. See `agc_core.h` for what one unguarded non-finite sample cost. 
```C++
>>> from doppler.util import ema_step
>>> ema_step(0.0, 1.0, 0.5)          # halfway to the observation
0.5
>>> ema_step(2.0, 2.0, 0.25)         # at its fixed point, no motion
2.0
>>> ema_step(1.0, 7.0, 1.0)          # alpha 1 is exact pass-through
7.0
>>> ema_step(1.0, 7.0, 0.0)          # alpha 0 freezes the state
1.0
```
 






        

<hr>



### function hex\_to\_bin 

_Expand a hex string to unpacked bits, one per byte._ 
```C++
size_t hex_to_bin (
    const char * hex,
    uint8_t * out,
    size_t max_out,
    int bitorder
) 
```



The general form of a transcription this library had exactly one hand-rolled instance of: `ccsds_tm_asm_bits` expands `0x1ACFFC1D` MSB-first, and its own comment says it exists so that the expansion is not written twice. A marker an assembler and a receiver expand differently syncs to nothing, so the expansion is worth owning once.


Each hex digit contributes 4 bits and digits are read left to right, so an ODD number of digits is accepted and yields a 4-bit tail. Under DP\_BITORDER\_BIG the bits come out in the order the literal is read; under DP\_BITORDER\_LITTLE the bits within each byte are reversed, and a trailing half-byte is reversed within its own four bits.




**Parameters:**


* `hex` NUL-terminated hex digits, `0-9a-fA-F`. No `0x`, no separators — a rejected character is a REFUSAL rather than a skipped one, because a typo'd marker that silently shortens is the failure this exists to avoid. 
* `out` receives `4 * strlen(hex)` bits, one per byte, 0 or 1. 
* `max_out` capacity of `out` in bits. 
* `bitorder` DP\_BITORDER\_BIG or DP\_BITORDER\_LITTLE. 



**Returns:**

bits written, or 0 on a bad digit, an empty string, a NULL, an unknown `bitorder`, or `max_out` too small — `out` untouched.



```C++
uint8_t b[32];
size_t  n = hex_to_bin ("1ACFFC1D", b, sizeof b, DP_BITORDER_BIG);
// n == 32, and b[0..7] is 0,0,0,1,1,0,1,0 — the CCSDS ASM, bit 0 first
```
 


        

<hr>



### function int\_to\_bin 

_Expand the low_ `n_bits` _of an integer to unpacked bits._
```C++
size_t int_to_bin (
    uint64_t v,
    unsigned n_bits,
    uint8_t * out,
    size_t max_out,
    int bitorder
) 
```



The form a field literal usually wants, and the one to reach for first: a sync word, a marker, a tag. Exact, compiler-checked and with no failure mode a typo can reach — `int_to_bin (0x1ACFFC1DULL, 32, ...)` cannot be misspelled the way `"1ACFFC1D"` can. [**hex\_to\_bin**](util__core_8h.md#function-hex_to_bin) is for the two cases this cannot serve: a literal wider than 64 bits, and text arriving from outside (a CLI flag, a JSON record) where the value is a string before it is anything else.


Bit order is the same rule [**hex\_to\_bin**](util__core_8h.md#function-hex_to_bin) follows, so the two agree bit-for-bit on any value both can express: units of 8 bits from the start, a final short unit reversed within itself.




**Parameters:**


* `v` the value; only the low `n_bits` are read. 
* `n_bits` 1..64. Bit 0 out is the MOST significant of those under DP\_BITORDER\_BIG, which is what makes `int_to_bin (0x1A, 8, ...)` read `0,0,0,1,1,0,1,0`. 
* `out` receives `n_bits` bytes, each 0 or 1. 
* `max_out` capacity of `out` in bits. 
* `bitorder` DP\_BITORDER\_BIG or DP\_BITORDER\_LITTLE. 



**Returns:**

`n_bits`, or 0 if `n_bits` is 0 or over 64, on NULL, an unknown `bitorder`, or `max_out` too small — `out` untouched. 





        

<hr>



### function saturate 

_Saturate a value into_ `[lo, hi]` _,_**total over every double** _— including NaN and both infinities._
```C++
JM_FORCEINLINE double saturate (
    double v,
    double lo,
    double hi,
    double nan_to
) 
```



`fmin`/`fmax` are not enough for this job. A plain `fmin(fmax(v, lo), hi)` propagates NaN on some platforms and silently returns a bound on others, and a hand-written `v > hi ? hi : v` leaves NaN untouched, because every comparison against NaN is false. This function has no fall-through: a value that is neither inside the interval, nor below it, nor above it can only be NaN.




**
**

Which end is _safe_ is domain knowledge, not arithmetic. A gain control guarding a measured power wants NaN at the **ceiling** — an unknown level must drive the gain down, because too little gain loses a signal while too much rails everything downstream. A lock statistic wants NaN at the **floor** — an unknown lock is not a lock. Baking either choice in would hand the wrong default to half its callers, so `nan_to` is a parameter and each call site states its own safe direction.




**
**

At the boundary where an untrusted value first becomes **persistent state** — the input of an EMA, an accumulator, or an integrator. Ahead of that boundary a bad value corrupts one output and is gone; past it, it is remembered and every quantity derived from it inherits the damage. One guard there makes the whole downstream chain total, where a clamp at each stage is several chances to miss one.




**Parameters:**


* `v` Value to saturate. Any double. 
* `lo` Lower bound, returned for any `v < lo`. 
* `hi` Upper bound, returned for any `v > hi`. 
* `nan_to` Returned when `v` is NaN. Pick the end that is safe in the caller's own terms; it is usually `lo` or `hi`. 



**Returns:**

`v` when `lo <= v <= hi`, otherwise `lo`, `hi` or `nan_to`. 
```C++
>>> from doppler.util import saturate
>>> saturate(0.5, 0.0, 1.0, 1.0)     # inside the interval
0.5
>>> saturate(2.0, 0.0, 1.0, 1.0)     # above the ceiling
1.0
>>> saturate(-3.0, 0.0, 1.0, 1.0)    # below the floor
0.0
>>> saturate(float("inf"), 0.0, 1.0, 1.0)   # infinity is just above
1.0
>>> saturate(float("nan"), 0.0, 1.0, 1.0)   # NaN takes the caller's end
1.0
>>> saturate(float("nan"), 0.0, 1.0, 0.0)   # ... which may be the other
0.0
```
 





        

<hr>



### function square\_clip 

_Square-clip a complex sample: clip the real and imaginary parts independently to_ `[-lin, lin]` _(a square region in the IQ plane, not a circular magnitude limit). Each component is passed through unchanged when its magnitude is within the threshold and clamped to the nearest boundary otherwise._
```C++
JM_FORCEINLINE float complex square_clip (
    float complex y,
    float lin
) 
```





**Parameters:**


* `y` Complex CF32 input sample. 
* `lin` Per-component clip threshold (linear amplitude, &gt;= 0). Values outside `[-lin, lin]` are clamped; values on the boundary are preserved exactly. 



**Returns:**

Sample with each component limited to `[-lin, lin]`. 
```C++
>>> from doppler.util import square_clip
>>> square_clip(0.5+0.25j, 1.0)   # within bounds, passed through
(0.5+0.25j)
>>> square_clip(2.0+0.5j, 1.0)    # real clipped, imag unchanged
(1+0.5j)
>>> square_clip(3.0-4.0j, 1.0)    # both components clipped
(1-1j)
>>> square_clip(0.5+0.5j, 0.25)   # smaller threshold clips both
(0.25+0.25j)
>>> square_clip(-2.0+0.0j, 1.0)   # negative real clipped
(-1+0j)
```
 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/util/util_core.h`

