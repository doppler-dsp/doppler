

# File wfm\_private.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_private.h**](wfm__private_8h.md)

[Go to the source code of this file](wfm__private_8h_source.md)

_Shared internals of the wfm composer, the Plan and the synth._ [More...](#detailed-description)

* `#include <stddef.h>`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  [**WFM\_NOINLINE**](wfm__private_8h.md#define-wfm_noinline) void | [**wfm\_add\_scaled**](#function-wfm_add_scaled) (float \_Complex \*restrict out, const float \_Complex \*restrict in, size\_t n, float gain) <br>`out[i] += gain * in[i]` __ _the ONE real-gain accumulate every wfm path uses to add a scaled stream (a noise stream, a source) into a sum. (A phase-rotated source, whose gain is complex, is the Plan's own override and has no compose twin to agree with.)_ |
|  [**WFM\_NOINLINE**](wfm__private_8h.md#define-wfm_noinline) void | [**wfm\_mix\_block**](#function-wfm_mix_block) (float \_Complex \*restrict out, const float \_Complex \*restrict c) <br>_One block of the carrier mix:_ `out[i] *= c[i]` _for exactly WFM\_MIX\_BLOCK complex samples._ |
|  void | [**wfm\_mix\_carrier**](#function-wfm_mix_carrier) (float \_Complex \*restrict out, const float \_Complex \*restrict carrier, size\_t n) <br>`out[i] *= carrier[i]` _(complex)_ _the carrier mix, in whole blocks._ |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**WFM\_MIX\_BLOCK**](wfm__private_8h.md#define-wfm_mix_block)  `32`<br>_Samples a mix block holds; every carrier mix runs in blocks of this many, the last one padded with zeros._  |
| define  | [**WFM\_NOINLINE**](wfm__private_8h.md#define-wfm_noinline)  <br>_Keep a function out of line: a loop of its own no caller can fuse._  |

## Detailed Description


Not part of the public API: nothing here is exported, and nothing here is promised to a caller (the [**acq\_private.h**](acq__private_8h.md) / [**correlator\_private.h**](correlator__private_8h.md) pattern). 


    
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



### function wfm\_mix\_block 

_One block of the carrier mix:_ `out[i] *= c[i]` _for exactly WFM\_MIX\_BLOCK complex samples._
```C++
static WFM_NOINLINE void wfm_mix_block (
    float _Complex *restrict out,
    const float _Complex *restrict c
) 
```



The one place a complex multiply of the synth's block path is compiled. A vectorised loop has a body and a scalar tail, and a compiler is free to round them differently: a fused product-sum (-ffp-contract), or, on clang for ARMv8.3+ (Apple silicon), FCMLA in the body, which -ffp-contract=off does not stop because -ffast-math marks every operation fusable. So a sample's last bit depended on where the caller's block ended (a Plan builds in one run; a composer pulls 257 at a time), doppler#2805.


Every sample now goes through THIS function, and it is out of line with a fixed trip count, so its machine code is one body: a sample is multiplied the same way at any position in any pull, on every compiler.


`noinline` stops inlining; it is not `noipa`. A compiler may still analyse the call across the translation unit (constant arguments, cloning), so the guarantee rests on the fixed trip count, which every call site shares. 


        

<hr>



### function wfm\_mix\_carrier 

`out[i] *= carrier[i]` _(complex)_ _the carrier mix, in whole blocks._
```C++
static inline void wfm_mix_carrier (
    float _Complex *restrict out,
    const float _Complex *restrict carrier,
    size_t n
) 
```



The final partial block is run through the same wfm\_mix\_block() on a zero-padded copy and only the samples that exist are kept, as the FIR does (fir\_core.c, #1893).




**Parameters:**


* `out` Signal, mixed in place; length `n`. 
* `carrier` The carrier, length `n`; must not overlap `out`. 
* `n` Samples. 




        

<hr>
## Macro Definition Documentation





### define WFM\_MIX\_BLOCK 

_Samples a mix block holds; every carrier mix runs in blocks of this many, the last one padded with zeros._ 
```C++
#define WFM_MIX_BLOCK `32`
```




<hr>



### define WFM\_NOINLINE 

_Keep a function out of line: a loop of its own no caller can fuse._ 
```C++
#define WFM_NOINLINE 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_private.h`

