

# File util.h



[**FileList**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**util.h**](util_8h.md)

[Go to the source code of this file](util_8h_source.md)

_SIMD-accelerated complex arithmetic._ [More...](#detailed-description)

* `#include <complex.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  double complex | [**dp\_c16\_mul**](#function-dp_c16_mul) (double complex a, double complex b) <br>_Multiply two complex doubles using the fastest available SIMD path._  |




























## Detailed Description


Dispatch is compile-time by architecture:
* x86 / x86-64: SSE2 128-bit intrinsics (baseline for all x86-64)
* AArch64 (ARM64): NEON 128-bit float64x2 intrinsics
* Other: C99 `a * b` scalar fallback





```C++
#include <dp/util.h>
#include <complex.h>

double complex a = 1.0 + 2.0*I;
double complex b = 3.0 + 4.0*I;
double complex c = dp_c16_mul(a, b);  // (1+2i)(3+4i) = -5+10i
```




## Public Functions Documentation




### function dp\_c16\_mul

_Multiply two complex doubles using the fastest available SIMD path._
```C++
double complex dp_c16_mul (
    double complex a,
    double complex b
)
```



Uses SSE2 on x86/x86-64, NEON float64x2 intrinsics on AArch64, and a plain C99 `a * b` scalar fallback on all other architectures.




**Parameters:**


* `a` First complex operand.
* `b` Second complex operand.



**Returns:**

`a` × `b`.







<hr>

------------------------------
The documentation for this class was generated from the following file `c/include/dp/util.h`
