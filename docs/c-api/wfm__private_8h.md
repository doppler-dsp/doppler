

# File wfm\_private.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_private.h**](wfm__private_8h.md)

[Go to the source code of this file](wfm__private_8h_source.md)

_Shared internals of the wfm composer, the Plan and the synth._ [More...](#detailed-description)

* `#include <stddef.h>`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  [**WFM\_NOINLINE**](wfm__private_8h.md#define-wfm_noinline) void | [**wfm\_add\_scaled**](#function-wfm_add_scaled) (float \_Complex \*restrict out, const float \_Complex \*restrict in, size\_t n, float gain) <br>`out[i] += gain * in[i]` __ _the ONE real-gain accumulate every wfm path uses to add a scaled stream (a noise stream, a source) into a sum. (A phase-rotated source, whose gain is complex, is the Plan's own override and has no compose twin to agree with.)_ |
|  void | [**wfm\_mix\_carrier**](#function-wfm_mix_carrier) (float \_Complex \*restrict out, const float \_Complex \*restrict carrier, size\_t n) <br>`out[i] *= carrier[i]` _(complex)_ _the carrier mix._ |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**WFM\_NOINLINE**](wfm__private_8h.md#define-wfm_noinline)  <br>_Keep a function out of line: a loop of its own no caller can fuse._  |

## Detailed Description


Not part of the public API: nothing here is exported, and nothing here is promised to a caller (the [**acq\_private.h**](acq__private_8h.md) / [**det\_private.h**](det__private_8h.md) pattern). 


    
## Public Static Functions Documentation




### function wfm\_add\_scaled 

`out[i] += gain * in[i]` __ _the ONE real-gain accumulate every wfm path uses to add a scaled stream (a noise stream, a source) into a sum. (A phase-rotated source, whose gain is complex, is the Plan's own override and has no compose twin to agree with.)_
```C++
static WFM_NOINLINE void wfm_add_scaled (
    float _Complex *restrict out,
    const float _Complex *restrict in,
    size_t n,
    float gain
) 
```



A compose and a Plan must produce the same bits for the same scene. Each adds the same streams, but written inline as `sig * carrier + noise` in one place and `cache + noise` in another they are the same number only where the compiler never fuses a multiply-add; under this tree's `-ffast-math` an FMA target (AArch64, or x86 with `-mfma`) rounds the first once and the second twice (doppler#2789). So the signal is STORED (rounded) first and every path adds through this function.


`noinline` keeps it a loop of its own that no caller can fuse with the pass before it. It is a `static` in each translation unit that includes this header (nothing is exported), compiled from this one source under the one set of flags, so every copy runs the same instructions.




**Parameters:**


* `out` Sum, updated in place; length `n`. 
* `in` The stream to add, length `n`; must not overlap `out`. 
* `n` Samples. 
* `gain` Multiplier on `in` (1.0f for a stream already at its amplitude). 




        

<hr>



### function wfm\_mix\_carrier 

`out[i] *= carrier[i]` _(complex)_ _the carrier mix._
```C++
static void wfm_mix_carrier (
    float _Complex *restrict out,
    const float _Complex *restrict carrier,
    size_t n
) 
```



A complex multiply is `a*c - b*d`, and a compiler with baseline FMA fuses one of the two products into the subtract. A vectorised loop has a body and a scalar tail of its own, which choose differently, so the same sample came out different bits depending on where the caller's block boundary fell (a Plan builds its cache in one run; a composer pulls 257 at a time)  doppler#2805. The cure is not in this function: it is that wfm\_synth\_core.c is compiled with `-ffp-contract=off` (native/src/wfm\_synth/ wfm\_synth\_extra.cmake), so every multiply-add of the file, here and at the sites that mix inline, rounds each product separately and is a property of the sample, not of its position in the block.




**Parameters:**


* `out` Signal, mixed in place; length `n`. 
* `carrier` The carrier, length `n`; must not overlap `out`. 
* `n` Samples. 




        

<hr>
## Macro Definition Documentation





### define WFM\_NOINLINE 

_Keep a function out of line: a loop of its own no caller can fuse._ 
```C++
#define WFM_NOINLINE 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_private.h`

