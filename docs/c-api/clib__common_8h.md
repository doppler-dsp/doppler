

# File clib\_common.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**clib\_common.h**](clib__common_8h.md)

[Go to the source code of this file](clib__common_8h_source.md)

_Common C99 types and performance macros for doppler._ [More...](#detailed-description)

* `#include "doppler/dp_complex.h"`
* `#include <stddef.h>`
* `#include <stdint.h>`
* `#include <math.h>`
* `#include <stdlib.h>`
* `#include <string.h>`
* `#include "doppler/jm_perf.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**dp\_fmod\_pos**](#function-dp_fmod_pos) (double x, double m) <br>`x` _folded into_`[0, m)` _for either sign of_`x` _._ |
|  double | [**dp\_lgamma**](#function-dp_lgamma) (double x) <br>_log\|Gamma(x)\|, reentrant_  _the one spelling of_`lgamma` _in this library._ |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  double | [**dp\_fftfreq**](#function-dp_fftfreq) (size\_t bin, size\_t n, double fs) <br>_The frequency of an FFT bin, in the units of_ `fs` _._ |
|  long | [**dp\_fftfreq\_index**](#function-dp_fftfreq_index) (size\_t bin, size\_t n) <br>`numpy.fft.fftfreq(n)[bin] * n` _— the SIGNED index of an FFT bin._ |
|  void \* | [**dp\_mallocn**](#function-dp_mallocn) (size\_t n, size\_t size) <br>_malloc for_ `n` _elements of_`size` _bytes; NULL when the byte count would overflow, or on OOM._ |
|  int | [**dp\_positive\_finite**](#function-dp_positive_finite) (double x) <br>_Whether_ `x` _is a positive, finite number._ |
|  void \* | [**dp\_reallocn**](#function-dp_reallocn) (void \* p, size\_t n, size\_t size) <br>_realloc to_ `n` _elements of_`size` _bytes; NULL when the byte count would overflow, or on OOM, with_`p` _untouched._ |
|  int | [**dp\_size\_add**](#function-dp_size_add) (size\_t a, size\_t b, size\_t \* out) <br>`a + b` _as a_`size_t` _, or a refusal when the sum does not fit._ |
|  int | [**dp\_size\_from\_double**](#function-dp_size_from_double) (double d, size\_t \* out) <br>`d` _as a_`size_t` _, truncated, or a refusal when it does not fit._ |
|  int | [**dp\_size\_mul**](#function-dp_size_mul) (size\_t a, size\_t b, size\_t \* out) <br>`a * b` _as a_`size_t` _, or a refusal when the product does not fit._ |
|  size\_t | [**dp\_size\_mul\_sat**](#function-dp_size_mul_sat) (size\_t a, size\_t b) <br>`a * b` _, or_`SIZE_MAX` _when the product does not fit._ |
|  void \* | [**dp\_xcalloc**](#function-dp_xcalloc) (size\_t nmemb, size\_t size) <br> |
|  void \* | [**dp\_xmalloc**](#function-dp_xmalloc) (size\_t n) <br> |
|  void \* | [**dp\_xmallocn**](#function-dp_xmallocn) (size\_t n, size\_t size) <br>_dp\_xmalloc for_ `n` _elements of_`size` _bytes: aborts on overflow or OOM._ |
|  void \* | [**dp\_xnn**](#function-dp_xnn) (void \* p) <br> |
|  void \* | [**dp\_xrealloc**](#function-dp_xrealloc) (void \* p, size\_t n) <br> |
|  void \* | [**dp\_xreallocn**](#function-dp_xreallocn) (void \* p, size\_t n, size\_t size) <br>_dp\_xrealloc to_ `n` _elements of_`size` _bytes: aborts on overflow or OOM._`n * size == 0` _is a 1-byte request:_`dp_xreallocn (p, 0, s)` _does NOT free_`p` _._ |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CMPLXF**](clib__common_8h.md#define-cmplxf) (r, i) `\_\_builtin\_complex ((float)(r), (float)(i))`<br> |
| define  | [**DP\_ERR\_CLOSED**](clib__common_8h.md#define-dp_err_closed)  `(-9)`<br> |
| define  | [**DP\_ERR\_EOF**](clib__common_8h.md#define-dp_err_eof)  `(-10)`<br> |
| define  | [**DP\_ERR\_INIT**](clib__common_8h.md#define-dp_err_init)  `(-1)`<br> |
| define  | [**DP\_ERR\_INTERRUPTED**](clib__common_8h.md#define-dp_err_interrupted)  `(-8)`<br> |
| define  | [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid)  `(-4)`<br> |
| define  | [**DP\_ERR\_MEMORY**](clib__common_8h.md#define-dp_err_memory)  `(-6)`<br> |
| define  | [**DP\_ERR\_RECV**](clib__common_8h.md#define-dp_err_recv)  `(-3)`<br> |
| define  | [**DP\_ERR\_SEND**](clib__common_8h.md#define-dp_err_send)  `(-2)`<br> |
| define  | [**DP\_ERR\_TIMEOUT**](clib__common_8h.md#define-dp_err_timeout)  `(-5)`<br> |
| define  | [**DP\_ERR\_TOO\_LARGE**](clib__common_8h.md#define-dp_err_too_large)  `(-7)`<br> |
| define  | [**DP\_NODISCARD**](clib__common_8h.md#define-dp_nodiscard)  <br>_Marks a function whose status must not be dropped._  |
| define  | [**DP\_OK**](clib__common_8h.md#define-dp_ok)  `0`<br> |

## Detailed Description


The header every component includes first: the C99 standard headers and the complex-math surface ([**dp\_complex.h**](dp__complex_8h.md)), the doppler-wide DP\_OK / DP\_ERR\_\* return codes, DP\_NODISCARD, the jm performance annotations ([**jm\_perf.h**](jm__perf_8h.md)), the abort-on-OOM allocators for trusted internal allocations, and a few inline numeric helpers shared library-wide (FFT bin frequencies, a sign-safe fmod, a reentrant lgamma). 


    
## Public Functions Documentation




### function dp\_fmod\_pos 

`x` _folded into_`[0, m)` _for either sign of_`x` _._
```C++
JM_FORCEINLINE double dp_fmod_pos (
    double x,
    double m
) 
```



C's `fmod()` keeps the dividend's sign, so a negative `x` comes back in `(-m, 0]` and every caller that wants a phase, a frequency or an index on a periodic axis has to add `m` back. That fix-up had been written by hand five times in this library  a code phase in chips, a harmonic into the analysed band, a table index, the DLL's replica tap and a hand-over phase  before this became its one home (doppler#1249); the gate `scripts/check_fmod_fold_sites.py` fails on the sixth.


External inline (JM\_FORCEINLINE), not `static inline` like its neighbours: the DLL's replica tap is an external inline in a header, and C99 6.7.4 forbids such a function from referencing one with internal linkage  the compiler said so.


A negative remainder within half an ulp of zero rounds `r + m` up to exactly `m`, outside the range; it folds to 0, its equal on the circle. The DLL's replica tap indexes the code with this result, and read `code[sf]`, one past the copy, at a spacing just off a phase-grid point (doppler#2110).


The test is `y >= m`, not `y < m`: doppler builds with -ffast-math, and gcc 14 at -O1 and above rewrote `r + m < m` as `r < 0`, always true here, so for a constant period that spelling returned `m` at run time. Written this way a NaN `x` also stays NaN rather than folding to a plausible phase 0. test\_dll\_core pins both with run-time inputs, over a constant and a run-time period.




**Parameters:**


* `x` Any real value. 
* `m` The period (&gt; 0). 



**Returns:**

`x` modulo `m`, in `[0, m)`; NaN for a NaN `x`. 





        

<hr>



### function dp\_lgamma 

_log\|Gamma(x)\|, reentrant_  _the one spelling of_`lgamma` _in this library._
```C++
double dp_lgamma (
    double x
) 
```



C's `lgamma()` writes the sign of Gamma into the global `signgam`, so two threads calling it race on a variable neither of them reads. That is exactly what happens when a pool of receivers rebuilds their tracking chains on different threads at once: every chain sizes a detector threshold through `dp_marcum_q()`, and ThreadSanitizer stopped the first multi-threaded test on it (doppler#1260). `lgamma_r()` takes the sign by pointer and touches no global. Declared here and defined in `native/src/detection/marcum_q.c` (detection\_core, which every threshold links) rather than inline, because an installed header may not name libm's `lgamma_r`  the shipped libraries do not define it, and `scripts/check_installed_headers.py` says so  and a strict C99 dialect does not declare it. `scripts/check_lgamma_sites.py` fails on a bare `lgamma` in `native/inc` or `native/src`.




**Parameters:**


* `x` Any real value. 



**Returns:**

`log|Gamma(x)|`, as `lgamma()` returns it. 





        

<hr>
## Public Static Functions Documentation




### function dp\_fftfreq 

_The frequency of an FFT bin, in the units of_ `fs` _._
```C++
static inline double dp_fftfreq (
    size_t bin,
    size_t n,
    double fs
) 
```



`dp_fftfreq_index(bin, n) * fs / n` — numpy's `fftfreq(n, d)[bin]` with the sample RATE where numpy takes the sample SPACING. That is the one deliberate difference from the numpy signature, and it is the right way round for this library: every caller here has `fs` in hand and would otherwise write `1.0 / fs` at the call site, which is a reciprocal to get wrong for no benefit. Pass `fs = 1.0` for normalised cycles/sample, which is numpy's default.




**Parameters:**


* `bin` Bin index in `[0, n)`. 
* `n` Grid size (&gt; 0). 
* `fs` Sample rate; the result is in these units. 



**Returns:**

Bin frequency in `[-fs/2, +fs/2)`. 





        

<hr>



### function dp\_fftfreq\_index 

`numpy.fft.fftfreq(n)[bin] * n` _— the SIGNED index of an FFT bin._
```C++
static inline long dp_fftfreq_index (
    size_t bin,
    size_t n
) 
```



`0 = DC`, ascending positive to `(n-1)/2`, then wrapping negative, so an even-length grid puts its Nyquist bin at `-n/2`. Multiply by the grid's bin spacing for Hz, or use dp\_fftfreq() for the normalised frequency.


Named for what it is. It arrived as an acquisition-specific helper called `dp_fftfreq_index`, which is how it came to disagree with numpy at exactly one index: it reported `+n/2` at the Nyquist bin. That is not wrong on its own  `+n/2` and `-n/2` are the same frequency, and a search on this grid cannot tell them apart  but every formula ported in from numpy then disagreed with the engine at the one bin the engine was most careful about. Following the universal convention deletes that class of surprise rather than documenting it.


What must not vary is the READER: a consumer seeded on one side of the fold while the search meant the other is off by the full span. That happened here once  an acquisition's wideband search and its hand-off spelled the fold differently  and it surfaced as a receiver reporting `tracking == 1` while decoding noise. So this lives in the COMMON header, inline, and `doppler.acquire.bin_to_signed` is a thin wrapper over it, so C and Python call the same code instead of restating the arithmetic.




**Parameters:**


* `bin` Bin index in `[0, n)`. 
* `n` Grid size. 



**Returns:**

Signed index in `[-(n/2), +((n-1)/2)]`. 





        

<hr>



### function dp\_mallocn 

_malloc for_ `n` _elements of_`size` _bytes; NULL when the byte count would overflow, or on OOM._
```C++
static inline void * dp_mallocn (
    size_t n,
    size_t size
) 
```



For a count a caller chose, on a path that refuses: check the result and refuse, exactly as for malloc.




**Note:**

When `n * size == 0` this asks for ONE byte, not zero: `malloc (0)` may return NULL, which a caller would read as a refusal. So NULL from dp\_mallocn means overflow or OOM and nothing else, and a zero-count result is a live pointer the caller frees as usual and never dereferences.



```C++
#include "doppler/clib_common.h"
#include <stdint.h>
#include <stdio.h>

int
main (void)
{
  float *x = dp_mallocn (256, sizeof *x);
  if (!x)
    return 1;
  x[255] = 1.0f;
  free (x);
  // 2^(w-1) elements of 2 bytes is 2^w bytes: refused, not wrapped to 0
  printf ("%d\n", dp_mallocn (SIZE_MAX / 2 + 1, 2) == NULL); // 1
  return 0;
}
```





**Parameters:**


* `n` Element count. 
* `size` Bytes per element. 



**Returns:**

The allocation, or NULL on overflow or OOM. 





        

<hr>



### function dp\_positive\_finite 

_Whether_ `x` _is a positive, finite number._
```C++
static inline int dp_positive_finite (
    double x
) 
```



The domain of a scale or gain that a constructor stores and later divides by or multiplies with: zero, a negative, NaN and either infinity are all refused. Spelled once here so every converter's create() refuses the same set; `x <= 0` alone lets NaN through (every comparison with NaN is false) and lets +inf through, which then maps every input to a full scale or to zero.




**Parameters:**


* `x` The value to test. 



**Returns:**

1 when `0 < x < inf`, else 0. 





        

<hr>



### function dp\_reallocn 

_realloc to_ `n` _elements of_`size` _bytes; NULL when the byte count would overflow, or on OOM, with_`p` _untouched._
```C++
static inline void * dp_reallocn (
    void * p,
    size_t n,
    size_t size
) 
```



The refusing twin of dp\_xreallocn(), for a buffer that grows on a path that can refuse (a resize, a decode, a create()): on NULL, `p` is neither freed nor moved and is still the caller's, exactly as after a failed realloc, so the usual pattern keeps working: `tmp = dp_reallocn (p, n, s); if (!tmp) refuse; p = tmp;`. Pick dp\_xreallocn() instead when the count is already bounded and only a genuine OOM could fail it. Passing NULL for `p` is a fresh allocation.




**Note:**

When `n * size == 0` this resizes `p` to ONE byte rather than calling `realloc (p, 0)`: glibc's frees `p` and returns NULL, which a caller would read as a refusal while still owning a freed pointer, and C23 makes it undefined. So dp\_reallocn never frees, NULL means overflow or OOM and nothing else, and a zero-count result is a live pointer the caller frees as usual.



```C++
#include "doppler/clib_common.h"
#include <stdint.h>
#include <stdio.h>

int
main (void)
{
  float *buf = dp_reallocn (NULL, 64, sizeof *buf);
  if (!buf)
    return 1;
  float *tmp = dp_reallocn (buf, 4096, sizeof *buf);
  if (!tmp)
    {
      free (buf); // a refusal leaves buf the caller's to free
      return 1;
    }
  buf       = tmp;
  buf[4095] = 0.0f;
  // 2^(w-1) elements of 2 bytes is 2^w bytes: refused, buf untouched
  printf ("%d\n", dp_reallocn (buf, SIZE_MAX / 2 + 1, 2) == NULL); // 1
  free (buf);
  return 0;
}
```





**Parameters:**


* `p` The block to resize, or NULL. 
* `n` Element count. 
* `size` Bytes per element. 



**Returns:**

The resized allocation, or NULL on overflow or OOM (`p` then unchanged and still owned by the caller). 





        

<hr>



### function dp\_size\_add 

`a + b` _as a_`size_t` _, or a refusal when the sum does not fit._
```C++
static inline int dp_size_add (
    size_t a,
    size_t b,
    size_t * out
) 
```



The other half of a multiply-add guard, `k * hop + frame_n` fits: a product checked by dp\_size\_mul() and then a sum checked here, rather than the one-line `k <= (SIZE_MAX - frame_n) / hop` that folds both into a division nobody can read twice the same way. The same contract as dp\_size\_mul(): 1 and the sum when it fits; 0 and `out` UNTOUCHED when it wraps; `out` may alias a summand, so a product chains into a sum.



```C++
#include "doppler/clib_common.h"
#include <stdint.h>
#include <stdio.h>

int
main (void)
{
  size_t end = 0;
  // 3 hops of 256 past a 1024-sample frame
  if (!dp_size_mul (3, 256, &end) || !dp_size_add (end, 1024, &end))
    return 1;
  printf ("%zu\n", end); // 1792
  printf ("%d\n", dp_size_add (SIZE_MAX, 1, &end)); // 0
  printf ("%zu\n", end); // 1792: a refusal leaves it alone
  return 0;
}
```





**Parameters:**


* `a` First summand. 
* `b` Second summand. 
* `out` Receives `a + b` when it fits; untouched otherwise. 



**Returns:**

1 when the sum fits in a `size_t`, 0 when it would wrap. 





        

<hr>



### function dp\_size\_from\_double 

`d` _as a_`size_t` _, truncated, or a refusal when it does not fit._
```C++
static inline int dp_size_from_double (
    double d,
    size_t * out
) 
```



The "does this double fit a size\_t" check, which every site wrote as `d <= (double)SIZE_MAX`  and that is wrong where size\_t is 64 bits: SIZE\_MAX is 2^64 - 1, which a double cannot hold, so the cast rounds UP to 2^64, the bound admits 2^64 itself, and the conversion of 2^64 to `size_t` is undefined behaviour (in practice 0 or SIZE\_MAX, by target). The right bound is a strict `<` against 2^w, w the width of size\_t: a power of two, so exactly representable whatever w is. It is formed as `(SIZE_MAX / 2 + 1) * 2.0`: SIZE\_MAX / 2 + 1 is 2^(w-1), exact as a double, and doubling it is exact. Where size\_t is 32 bits this is the same as `<= SIZE_MAX` for a whole number, and every double below 2^32 truncates into range. `scripts/check_size_mul_sites.py` fails on a `(double)SIZE_MAX` written anywhere else in library C.


The contract:



* it returns 1 and writes `(size_t)d`, truncated toward zero, when `0 <= d < 2^w`. -0.0 compares equal to 0 and converts to 0;
* it returns 0 and leaves `out` UNTOUCHED for a negative `d`, for 2^w and above, for +inf and for NaN (every comparison with NaN is false, and doppler builds with -fno-finite-math-only so that holds).




This is a static inline, so it is compiled under the INCLUDING translation unit's flags. A consumer that builds with -ffast-math (or -ffinite-math-only) tells its compiler no value is NaN or infinite, and the compiler may then fold the refusal of NaN and +inf away; such a caller must rule those out itself before calling this.


Whether `d` must also be whole is the caller's rule, not this one's: test `d == floor (d)` beside it.



```C++
#include "doppler/clib_common.h"
#include <stdio.h>

int
main (void)
{
  size_t n = 7;
  if (!dp_size_from_double (4096.75, &n))
    return 1;
  printf ("%zu\n", n); // 4096

  if (dp_size_from_double (-1.0, &n) || dp_size_from_double (NAN, &n))
    return 1;
  printf ("%zu\n", n); // 4096: a refusal leaves it alone
  return 0;
}
```





**Parameters:**


* `d` The value to convert. 
* `out` Receives `(size_t)d` when it fits; untouched otherwise. 



**Returns:**

1 when `0 <= d < 2^w`, else 0. 





        

<hr>



### function dp\_size\_mul 

`a * b` _as a_`size_t` _, or a refusal when the product does not fit._
```C++
static inline int dp_size_mul (
    size_t a,
    size_t b,
    size_t * out
) 
```



A size product that wraps is the worst way an allocation can go wrong: the buffer comes back SMALL, every check against it passes, and the writes that follow run past its end. The guard is one line, and that is the trouble with it  written by hand at each site it was spelled a different way every time (`n > SIZE_MAX / k`, `SIZE_MAX / a / b`, a strict `<` against the quotient), and one file grew three wrapping products in two review rounds. Spelled once here so every product that sizes memory is checked the same way; `scripts/check_size_mul_sites.py` fails on a `SIZE_MAX /` written anywhere else in library C.


Divide before multiplying, so nothing overflows on the way to the answer: `a * b` fits exactly when `b == 0` or `a <= SIZE_MAX / b`. It is plain C99 on purpose, not `__builtin_mul_overflow`: clang-cl on Windows builds this header too.


The contract a caller can lean on:



* it returns 1 and writes `a * b` to `out` when the product fits, including a zero factor (the product is then 0);
* it returns 0 and leaves `out` UNTOUCHED when it overflows, so a refusal never hands back a wrapped count;
* `out` may alias the storage a factor was read from: `dp_size_mul (n, k, &n)` is safe, because both factors are taken by value before anything is written. Chain a three-way product that way.





```C++
#include "doppler/clib_common.h"
#include <stdint.h>
#include <stdio.h>

int
main (void)
{
  size_t bytes = 0;
  if (!dp_size_mul (1024, sizeof (float), &bytes))
    return 1;
  printf ("%zu\n", bytes); // 4096

  size_t n = 7;
  if (!dp_size_mul (n, 3, &n) || !dp_size_mul (n, 2, &n))
    return 1;
  printf ("%zu\n", n); // 42

  size_t kept = 5;
  if (dp_size_mul (SIZE_MAX / 2 + 1, 2, &kept))
    return 1;
  printf ("%zu\n", kept); // 5: a refusal leaves it alone
  return 0;
}
```





**Parameters:**


* `a` First factor. 
* `b` Second factor. 
* `out` Receives `a * b` when it fits; untouched otherwise. 



**Returns:**

1 when the product fits in a `size_t`, 0 when it would wrap. 





        

<hr>



### function dp\_size\_mul\_sat 

`a * b` _, or_`SIZE_MAX` _when the product does not fit._
```C++
static inline size_t dp_size_mul_sat (
    size_t a,
    size_t b
) 
```



The saturating face of dp\_size\_mul(), for a function whose job is an UPPER BOUND  the most output a call can produce, the records a ring must hold. A wrapped bound sizes a buffer small, which is the one failure such a function exists to prevent; `SIZE_MAX` instead is a size no allocation satisfies, so a caller that trusts it is refused at the allocation rather than overrun after it. Built on dp\_size\_mul() so the overflow test has one spelling.



```C++
#include "doppler/clib_common.h"
#include <stdint.h>
#include <stdio.h>

int
main (void)
{
  printf ("%zu\n", dp_size_mul_sat (6, 7));                   // 42
  printf ("%d\n", dp_size_mul_sat (SIZE_MAX, 2) == SIZE_MAX); // 1
  return 0;
}
```





**Parameters:**


* `a` First factor. 
* `b` Second factor. 



**Returns:**

`a * b` when it fits in a `size_t`, else `SIZE_MAX`. 





        

<hr>



### function dp\_xcalloc 

```C++
static inline void * dp_xcalloc (
    size_t nmemb,
    size_t size
) 
```



calloc that aborts on OOM (zero-initialised trusted allocation). 


        

<hr>



### function dp\_xmalloc 

```C++
static inline void * dp_xmalloc (
    size_t n
) 
```



malloc that aborts on OOM (for a trusted internal allocation). 


        

<hr>



### function dp\_xmallocn 

_dp\_xmalloc for_ `n` _elements of_`size` _bytes: aborts on overflow or OOM._
```C++
static inline void * dp_xmallocn (
    size_t n,
    size_t size
) 
```



For a trusted internal allocation whose count is already validated.




**Note:**

When `n * size == 0` this asks for ONE byte, not zero: `malloc (0)` may return NULL, and dp\_xmalloc would abort on it as if out of memory. The result is a live pointer the caller frees as usual and never dereferences.



```C++
#include "doppler/clib_common.h"

int
main (void)
{
  double *acc = dp_xmallocn (1024, sizeof *acc);
  acc[1023]   = 0.0;
  free (acc);
  return 0;
}
```





**Parameters:**


* `n` Element count. 
* `size` Bytes per element. 



**Returns:**

The allocation; never NULL. 





        

<hr>



### function dp\_xnn 

```C++
static inline void * dp_xnn (
    void * p
) 
```



Assert a just-constructed object / allocation is non-NULL, aborting with a diagnostic on the impossible OOM. The single check point: a sub-object create() returns NULL only on OOM once its arguments are validated, so wrap the call — `x = dp_xnn (foo_create (...))` — instead of checking-and-unwinding at every call site. (Classic GNU `xmalloc`.) 


        

<hr>



### function dp\_xrealloc 

```C++
static inline void * dp_xrealloc (
    void * p,
    size_t n
) 
```



realloc that aborts on OOM — the third member of the family.


A grow-on-demand scratch buffer is the shape that wants it: the new size scales with the caller's block, but the only way the call fails is still genuine exhaustion, so the unwind path is as uncoverable as malloc's. Passing NULL for `p` is a fresh allocation, exactly as realloc defines. 


        

<hr>



### function dp\_xreallocn 

_dp\_xrealloc to_ `n` _elements of_`size` _bytes: aborts on overflow or OOM._`n * size == 0` _is a 1-byte request:_`dp_xreallocn (p, 0, s)` _does NOT free_`p` _._
```C++
static inline void * dp_xreallocn (
    void * p,
    size_t n,
    size_t size
) 
```



The grow-on-demand scratch buffer's helper, for a count the caller has already bounded. Passing NULL for `p` is a fresh allocation.




**Note:**

A zero count resizes `p` to ONE byte rather than calling `realloc (p, 0)`: glibc's frees `p` and returns NULL, which dp\_xrealloc would abort on as if out of memory, and C23 makes it undefined. The result is a live pointer the caller frees as usual.



```C++
#include "doppler/clib_common.h"

int
main (void)
{
  float *buf = dp_xreallocn (NULL, 64, sizeof *buf);
  buf        = dp_xreallocn (buf, 4096, sizeof *buf);
  buf[4095]  = 0.0f;
  free (buf);
  return 0;
}
```





**Parameters:**


* `p` The block to resize, or NULL. 
* `n` Element count. 
* `size` Bytes per element. 



**Returns:**

The resized allocation; never NULL. 





        

<hr>
## Macro Definition Documentation





### define CMPLXF 

```C++
#define CMPLXF (
    r,
    i
) `__builtin_complex ((float)(r), (float)(i))`
```



CMPLXF is C11 but missing on MinGW/ucrt64. \_\_builtin\_complex is the GCC-portable way to construct a complex value without relying on \_Complex\_I (which can propagate NaN in some implementations). Defined only where the C library does not already provide it.




**Parameters:**


* `r` Real part, converted to float. 
* `i` Imaginary part, converted to float. 




        

<hr>



### define DP\_ERR\_CLOSED 

```C++
#define DP_ERR_CLOSED `(-9)`
```



The context is draining or closed: it accepts no more sends, \ and a message it handed out can no longer be acked. A state, \ not a transport failure. 


        

<hr>



### define DP\_ERR\_EOF 

```C++
#define DP_ERR_EOF `(-10)`
```



The producer has finished: no more data is coming, ever. A \ state, not a failure, and distinct from DP\_ERR\_TIMEOUT, which \ means "not yet". Every transport spells it the same way  see \ docs/design/io-termination.md. 


        

<hr>



### define DP\_ERR\_INIT 

```C++
#define DP_ERR_INIT `(-1)`
```



Initialisation failed (context/socket). 


        

<hr>



### define DP\_ERR\_INTERRUPTED 

```C++
#define DP_ERR_INTERRUPTED `(-8)`
```



A blocking call returned because [**dp\_stream\_interrupt()**](group__interrupt.md#function-dp_stream_interrupt) was \ called  a request to stop, not a failure. 


        

<hr>



### define DP\_ERR\_INVALID 

```C++
#define DP_ERR_INVALID `(-4)`
```



Invalid argument. 


        

<hr>



### define DP\_ERR\_MEMORY 

```C++
#define DP_ERR_MEMORY `(-6)`
```



Memory allocation failure. 


        

<hr>



### define DP\_ERR\_RECV 

```C++
#define DP_ERR_RECV `(-3)`
```



Receive failed or timed out (EAGAIN). 


        

<hr>



### define DP\_ERR\_SEND 

```C++
#define DP_ERR_SEND `(-2)`
```



Send failed. 


        

<hr>



### define DP\_ERR\_TIMEOUT 

```C++
#define DP_ERR_TIMEOUT `(-5)`
```



Operation timed out. 


        

<hr>



### define DP\_ERR\_TOO\_LARGE 

```C++
#define DP_ERR_TOO_LARGE `(-7)`
```



Frame exceeds transport max payload. 


        

<hr>



### define DP\_NODISCARD 

_Marks a function whose status must not be dropped._ 
```C++
#define DP_NODISCARD 
```



A refusal nobody reads is invisible: the call returns DP\_ERR\_\*, the caller carries on as if it had worked, and no flag can see it, because -Wunused-result only checks a function that asks for it (#1663). Put this in front of the return type of any function that returns a refusal, and gcc and clang warn at every call site that discards it; the library's -Werror builds fail there. A `(void)` cast does NOT silence gcc: handle the status. Empty where the compiler has no such attribute. 


        

<hr>



### define DP\_OK 

```C++
#define DP_OK `0`
```



Success. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/clib_common.h`

