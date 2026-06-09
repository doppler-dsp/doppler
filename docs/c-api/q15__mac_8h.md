

# File q15\_mac.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**q15\_mac.h**](q15__mac_8h.md)

[Go to the source code of this file](q15__mac_8h_source.md)

_Static inline Q15 dot-product primitives: scalar fallback and AVX2._ [More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  int64\_t | [**dot\_q15\_scalar**](#function-dot_q15_scalar) (const int16\_t \* a, const int16\_t \* b, size\_t n) <br>_Scalar Q15 dot product._  |


























## Detailed Description


Provides two functions:


int64\_t dot\_q15\_scalar(a, b, n) int64\_t dot\_q15\_avx2 (a, b, n) — only when **AVX2** is defined


Both compute the exact integer inner product sum(a[i] \* b[i]) for i in [0, n), accumulating into int64\_t without saturation. The caller decides how to round and saturate the result.


Result format: Q30 (each Q15 × Q15 product is Q30; n products accumulate in int64\_t, which has sufficient headroom for n ≤ 2^33 before overflow).


The hsum\_epi32\_i64 helper is also exposed so callers that maintain their own AVX2 accumulator can reduce it to a scalar at the end.


Usage: 
```C++
#include "q15_mac.h"

int64_t acc;
#if defined(__AVX2__)
    acc = dot_q15_avx2(a, b, n);
#else
    acc = dot_q15_scalar(a, b, n);
#endif
// shift to Q15: int32_t out = (int32_t)((acc + (1 << 14)) >> 15);
```
 


    
## Public Static Functions Documentation




### function dot\_q15\_scalar 

_Scalar Q15 dot product._ 
```C++
static inline int64_t dot_q15_scalar (
    const int16_t * a,
    const int16_t * b,
    size_t n
) 
```



Each product is widened to int32\_t before accumulation into int64\_t to avoid overflow. Correct for all Q15 inputs and any n ≤ 2^33. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/q15_mac.h`

