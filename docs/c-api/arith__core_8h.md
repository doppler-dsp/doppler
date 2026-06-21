

# File arith\_core.h



[**FileList**](files.md) **>** [**arith**](dir_51d42af7a43550d997314136379d62d2.md) **>** [**arith\_core.h**](arith__core_8h.md)

[Go to the source code of this file](arith__core_8h_source.md)

_Arith module — public C API for fixed-point arithmetic on Q15 (int16\_t) and Q8 (int8\_t) arrays. All elementwise operations write into a caller-supplied output buffer of the same length as the shorter input. Saturation clamps results to the representable range rather than wrapping, matching the two's-complement DSP convention._ 

* `#include "clib_common.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**add\_q15**](#function-add_q15) (const int16\_t \* a, size\_t a\_len, const int16\_t \* b, size\_t b\_len, int16\_t \* out) <br>_Elementwise saturating add of two Q15 arrays. Each pair of samples is added as int32\_t and then clamped to_ `[-32768, 32767]` _before storing, so overflow wraps to the saturation boundary rather than producing garbage bits._ |
|  void | [**add\_q8**](#function-add_q8) (const int8\_t \* a, size\_t a\_len, const int8\_t \* b, size\_t b\_len, int8\_t \* out) <br>_Elementwise saturating add of two Q8 arrays. Each pair of samples is added as int16\_t and then clamped to_ `[-128, 127]` _before storing, preventing overflow wrap on near-boundary values._ |
|  int64\_t | [**dot\_q15**](#function-dot_q15) (const int16\_t \* a, size\_t a\_len, const int16\_t \* b, size\_t b\_len) <br>_Inner product of two Q15 arrays, returning the raw Q30 sum. Accumulates products as int64\_t — no scaling or saturation is applied here. The caller shifts the result right 15 bits (e.g. via shr\_i64) to recover a Q15 scalar, or keeps the Q30 value for further arithmetic._  |
|  int32\_t | [**dot\_q8**](#function-dot_q8) (const int8\_t \* a, size\_t a\_len, const int8\_t \* b, size\_t b\_len) <br>_Inner product of two Q8 arrays, returning the raw Q14 sum. Accumulates products as int32\_t — no scaling or saturation is applied. The result lives in Q14 space (each product is Q7\*Q7 = Q14); the caller shifts right 7 to recover a Q7 scalar if needed._  |
|  void | [**mul\_q15**](#function-mul_q15) (const int16\_t \* a, size\_t a\_len, const int16\_t \* b, size\_t b\_len, int16\_t \* out) <br>_Elementwise Q15 multiply with round-half-up and saturation. Computes_ `out[i]` _= sat16((_`a[i]` _\*_`b[i]` _+ 16384) &gt;&gt; 15). The bias 16384 (= 1 &lt;&lt; 14) implements round-half-up before the 15-bit right shift, so 0.5 \* 0.5 = 0.25 in Q15 arithmetic (16384 \* 16384 -&gt; 8192) rather than truncating toward zero._ |
|  void | [**mul\_q8**](#function-mul_q8) (const int8\_t \* a, size\_t a\_len, const int8\_t \* b, size\_t b\_len, int8\_t \* out) <br>_Elementwise Q8 multiply with round-half-up and saturation. Computes_ `out[i]` _= sat8((_`a[i]` _\*_`b[i]` _+ 64) &gt;&gt; 7). The bias 64 (= 1 &lt;&lt; 6) rounds at the half-LSB position before the 7-bit shift, mirroring the rounding convention of mul\_q15 but for 8-bit fixed-point. In Q8 terms, 0.5 \* 0.5 = 0.25 (64 \* 64 -&gt; 32)._ |
|  void | [**shl\_i64**](#function-shl_i64) (const int64\_t \* a, size\_t a\_len, int64\_t \* out, int n) <br>_Elementwise logical left shift of an int64\_t array. No saturation is applied — the caller is responsible for ensuring that no element will overflow 64 bits. Shifts &gt;= 63 produce zero. Designed to scale int64\_t accumulators (e.g. after a chain of multiply-accumulate operations) before final truncation._  |
|  void | [**shl\_q15**](#function-shl_q15) (const int16\_t \* a, size\_t a\_len, int16\_t \* out, int n) <br>_Elementwise arithmetic left shift of a Q15 array with saturation. Computes_ `out[i]` _= sat16(_`a[i]` _&lt;&lt; n), equivalent to multiplying by 2^n in fixed-point. Any shifted value that exceeds the int16\_t range is clamped, preventing silent wraparound._ |
|  void | [**shl\_q8**](#function-shl_q8) (const int8\_t \* a, size\_t a\_len, int8\_t \* out, int n) <br>_Elementwise arithmetic left shift of a Q8 array with saturation. Computes_ `out[i]` _= sat8(_`a[i]` _&lt;&lt; n). Shifts that would exceed the int8\_t range are clamped, preventing silent wraparound into wrong-sign results._ |
|  void | [**shr\_i64**](#function-shr_i64) (const int64\_t \* a, size\_t a\_len, int64\_t \* out, int n) <br>_Elementwise arithmetic right shift of an int64\_t array with round-half-up. Adds 1 &lt;&lt; (n-1) as a bias before shifting, so values at the half-LSB boundary round up. Primarily used to normalise the raw Q30 output of dot\_q15 back to Q15 by shifting right 15 bits._  |
|  void | [**shr\_q15**](#function-shr_q15) (const int16\_t \* a, size\_t a\_len, int16\_t \* out, int n) <br>_Elementwise arithmetic right shift of a Q15 array with round-half-up. Adds 1 &lt;&lt; (n-1) as a rounding bias before shifting, so values exactly at the half-LSB boundary round up rather than truncating. Equivalent to dividing by 2^n in Q15 fixed-point with correct rounding._  |
|  void | [**shr\_q8**](#function-shr_q8) (const int8\_t \* a, size\_t a\_len, int8\_t \* out, int n) <br>_Elementwise arithmetic right shift of a Q8 array with round-half-up. Adds 1 &lt;&lt; (n-1) as a rounding bias before shifting, matching the rounding convention of shr\_q15 at 8-bit precision._  |
|  void | [**sub\_q15**](#function-sub_q15) (const int16\_t \* a, size\_t a\_len, const int16\_t \* b, size\_t b\_len, int16\_t \* out) <br>_Elementwise saturating subtract of two Q15 arrays. Computes_ `out[i]` _= sat16(_`a[i]` _-_`b[i]` _) for i in_`[0, min(len(a), len(b)))` _. The intermediate difference is computed as int32\_t to detect overflow before clamping, preserving the correct sign at the saturation boundary._ |
|  void | [**sub\_q8**](#function-sub_q8) (const int8\_t \* a, size\_t a\_len, const int8\_t \* b, size\_t b\_len, int8\_t \* out) <br>_Elementwise saturating subtract of two Q8 arrays. Computes_ `out[i]` _= sat8(_`a[i]` _-_`b[i]` _) for i in_`[0, min(len(a), len(b)))` _. The intermediate difference is computed as int16\_t so overflow is detected before clamping._ |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  int16\_t | [**sat16**](#function-sat16) (int32\_t x) <br> |
|  int8\_t | [**sat8**](#function-sat8) (int16\_t x) <br> |


























## Public Functions Documentation




### function add\_q15 

_Elementwise saturating add of two Q15 arrays. Each pair of samples is added as int32\_t and then clamped to_ `[-32768, 32767]` _before storing, so overflow wraps to the saturation boundary rather than producing garbage bits._
```C++
void add_q15 (
    const int16_t * a,
    size_t a_len,
    const int16_t * b,
    size_t b_len,
    int16_t * out
) 
```





**Parameters:**


* `a` First input array (int16\_t). 
* `a_len` Number of elements in a. 
* `b` Second input array (int16\_t), same length as a. 
* `b_len` Number of elements in b. 
* `out` Output array (int16\_t), length min(a\_len, b\_len).


```C++
>>> from doppler.arith import add_q15
>>> import numpy as np
>>> a = np.array([100, 20000, -20000], dtype=np.int16)
>>> b = np.array([50,  20000, -20000], dtype=np.int16)
>>> add_q15(a, b).tolist()
[150, 32767, -32768]
```
 


        

<hr>



### function add\_q8 

_Elementwise saturating add of two Q8 arrays. Each pair of samples is added as int16\_t and then clamped to_ `[-128, 127]` _before storing, preventing overflow wrap on near-boundary values._
```C++
void add_q8 (
    const int8_t * a,
    size_t a_len,
    const int8_t * b,
    size_t b_len,
    int8_t * out
) 
```





**Parameters:**


* `a` First input array (int8\_t). 
* `a_len` Number of elements in a. 
* `b` Second input array (int8\_t), same length as a. 
* `b_len` Number of elements in b. 
* `out` Output array (int8\_t), length min(a\_len, b\_len).


```C++
>>> from doppler.arith import add_q8
>>> import numpy as np
>>> a = np.array([50, 100, -100], dtype=np.int8)
>>> b = np.array([50,  30,  -50], dtype=np.int8)
>>> add_q8(a, b).tolist()
[100, 127, -128]
```
 


        

<hr>



### function dot\_q15 

_Inner product of two Q15 arrays, returning the raw Q30 sum. Accumulates products as int64\_t — no scaling or saturation is applied here. The caller shifts the result right 15 bits (e.g. via shr\_i64) to recover a Q15 scalar, or keeps the Q30 value for further arithmetic._ 
```C++
int64_t dot_q15 (
    const int16_t * a,
    size_t a_len,
    const int16_t * b,
    size_t b_len
) 
```





**Parameters:**


* `a` First input array (int16\_t). 
* `a_len` Number of elements in a. 
* `b` Second input array (int16\_t), same length as a. 
* `b_len` Number of elements in b. 



**Returns:**

Raw Q30 accumulation (int64\_t).



```C++
>>> from doppler.arith import dot_q15
>>> import numpy as np
>>> a = np.array([100, 200, 300], dtype=np.int16)
>>> b = np.array([1, 2, 3], dtype=np.int16)
>>> dot_q15(a, b)
1400
```
 


        

<hr>



### function dot\_q8 

_Inner product of two Q8 arrays, returning the raw Q14 sum. Accumulates products as int32\_t — no scaling or saturation is applied. The result lives in Q14 space (each product is Q7\*Q7 = Q14); the caller shifts right 7 to recover a Q7 scalar if needed._ 
```C++
int32_t dot_q8 (
    const int8_t * a,
    size_t a_len,
    const int8_t * b,
    size_t b_len
) 
```





**Parameters:**


* `a` First input array (int8\_t). 
* `a_len` Number of elements in a. 
* `b` Second input array (int8\_t), same length as a. 
* `b_len` Number of elements in b. 



**Returns:**

Raw Q14 accumulation (int32\_t).



```C++
>>> from doppler.arith import dot_q8
>>> import numpy as np
>>> a = np.array([10, 20, 30], dtype=np.int8)
>>> b = np.array([1, 2, 3], dtype=np.int8)
>>> dot_q8(a, b)
140
```
 


        

<hr>



### function mul\_q15 

_Elementwise Q15 multiply with round-half-up and saturation. Computes_ `out[i]` _= sat16((_`a[i]` _\*_`b[i]` _+ 16384) &gt;&gt; 15). The bias 16384 (= 1 &lt;&lt; 14) implements round-half-up before the 15-bit right shift, so 0.5 \* 0.5 = 0.25 in Q15 arithmetic (16384 \* 16384 -&gt; 8192) rather than truncating toward zero._
```C++
void mul_q15 (
    const int16_t * a,
    size_t a_len,
    const int16_t * b,
    size_t b_len,
    int16_t * out
) 
```





**Parameters:**


* `a` First input array (int16\_t). 
* `a_len` Number of elements in a. 
* `b` Second input array (int16\_t), same length as a. 
* `b_len` Number of elements in b. 
* `out` Output array (int16\_t), length min(a\_len, b\_len).


```C++
>>> from doppler.arith import mul_q15
>>> import numpy as np
>>> a = np.array([16384, 16384, 32767], dtype=np.int16)
>>> b = np.array([16384, -16384, 32767], dtype=np.int16)
>>> mul_q15(a, b).tolist()
[8192, -8192, 32766]
```
 


        

<hr>



### function mul\_q8 

_Elementwise Q8 multiply with round-half-up and saturation. Computes_ `out[i]` _= sat8((_`a[i]` _\*_`b[i]` _+ 64) &gt;&gt; 7). The bias 64 (= 1 &lt;&lt; 6) rounds at the half-LSB position before the 7-bit shift, mirroring the rounding convention of mul\_q15 but for 8-bit fixed-point. In Q8 terms, 0.5 \* 0.5 = 0.25 (64 \* 64 -&gt; 32)._
```C++
void mul_q8 (
    const int8_t * a,
    size_t a_len,
    const int8_t * b,
    size_t b_len,
    int8_t * out
) 
```





**Parameters:**


* `a` First input array (int8\_t). 
* `a_len` Number of elements in a. 
* `b` Second input array (int8\_t), same length as a. 
* `b_len` Number of elements in b. 
* `out` Output array (int8\_t), length min(a\_len, b\_len).


```C++
>>> from doppler.arith import mul_q8
>>> import numpy as np
>>> a = np.array([64,  64, -64], dtype=np.int8)
>>> b = np.array([64, -64,  64], dtype=np.int8)
>>> mul_q8(a, b).tolist()
[32, -32, -32]
```
 


        

<hr>



### function shl\_i64 

_Elementwise logical left shift of an int64\_t array. No saturation is applied — the caller is responsible for ensuring that no element will overflow 64 bits. Shifts &gt;= 63 produce zero. Designed to scale int64\_t accumulators (e.g. after a chain of multiply-accumulate operations) before final truncation._ 
```C++
void shl_i64 (
    const int64_t * a,
    size_t a_len,
    int64_t * out,
    int n
) 
```





**Parameters:**


* `a` Input array (int64\_t). 
* `a_len` Number of elements in a. 
* `out` Output array (int64\_t), same length as a. 
* `n` Shift count (non-negative integer; &gt;= 63 yields 0).


```C++
>>> from doppler.arith import shl_i64
>>> import numpy as np
>>> a = np.array([100, 200, -200], dtype=np.int64)
>>> shl_i64(a, 3).tolist()
[800, 1600, -1600]
```
 


        

<hr>



### function shl\_q15 

_Elementwise arithmetic left shift of a Q15 array with saturation. Computes_ `out[i]` _= sat16(_`a[i]` _&lt;&lt; n), equivalent to multiplying by 2^n in fixed-point. Any shifted value that exceeds the int16\_t range is clamped, preventing silent wraparound._
```C++
void shl_q15 (
    const int16_t * a,
    size_t a_len,
    int16_t * out,
    int n
) 
```





**Parameters:**


* `a` Input array (int16\_t). 
* `a_len` Number of elements in a. 
* `out` Output array (int16\_t), same length as a. 
* `n` Shift count (non-negative integer).


```C++
>>> from doppler.arith import shl_q15
>>> import numpy as np
>>> a = np.array([8192, 16384, 20000], dtype=np.int16)
>>> shl_q15(a, 1).tolist()
[16384, 32767, 32767]
```
 


        

<hr>



### function shl\_q8 

_Elementwise arithmetic left shift of a Q8 array with saturation. Computes_ `out[i]` _= sat8(_`a[i]` _&lt;&lt; n). Shifts that would exceed the int8\_t range are clamped, preventing silent wraparound into wrong-sign results._
```C++
void shl_q8 (
    const int8_t * a,
    size_t a_len,
    int8_t * out,
    int n
) 
```





**Parameters:**


* `a` Input array (int8\_t). 
* `a_len` Number of elements in a. 
* `out` Output array (int8\_t), same length as a. 
* `n` Shift count (non-negative integer).


```C++
>>> from doppler.arith import shl_q8
>>> import numpy as np
>>> a = np.array([10, 50, 64], dtype=np.int8)
>>> shl_q8(a, 1).tolist()
[20, 100, 127]
```
 


        

<hr>



### function shr\_i64 

_Elementwise arithmetic right shift of an int64\_t array with round-half-up. Adds 1 &lt;&lt; (n-1) as a bias before shifting, so values at the half-LSB boundary round up. Primarily used to normalise the raw Q30 output of dot\_q15 back to Q15 by shifting right 15 bits._ 
```C++
void shr_i64 (
    const int64_t * a,
    size_t a_len,
    int64_t * out,
    int n
) 
```





**Parameters:**


* `a` Input array (int64\_t). 
* `a_len` Number of elements in a. 
* `out` Output array (int64\_t), same length as a. 
* `n` Shift count (non-negative integer; &gt;= 63 is clamped to 63).


```C++
>>> from doppler.arith import dot_q15, shr_i64
>>> import numpy as np
>>> raw = dot_q15(
...     np.array([16384, 16384], dtype=np.int16),
...     np.array([16384, 16384], dtype=np.int16),
... )
>>> shr_i64(np.array([raw], dtype=np.int64), 15).tolist()
[16384]
```
 


        

<hr>



### function shr\_q15 

_Elementwise arithmetic right shift of a Q15 array with round-half-up. Adds 1 &lt;&lt; (n-1) as a rounding bias before shifting, so values exactly at the half-LSB boundary round up rather than truncating. Equivalent to dividing by 2^n in Q15 fixed-point with correct rounding._ 
```C++
void shr_q15 (
    const int16_t * a,
    size_t a_len,
    int16_t * out,
    int n
) 
```





**Parameters:**


* `a` Input array (int16\_t). 
* `a_len` Number of elements in a. 
* `out` Output array (int16\_t), same length as a. 
* `n` Shift count (non-negative integer).


```C++
>>> from doppler.arith import shr_q15
>>> import numpy as np
>>> a = np.array([100, 101, 102, -100], dtype=np.int16)
>>> shr_q15(a, 2).tolist()
[25, 25, 26, -25]
```
 


        

<hr>



### function shr\_q8 

_Elementwise arithmetic right shift of a Q8 array with round-half-up. Adds 1 &lt;&lt; (n-1) as a rounding bias before shifting, matching the rounding convention of shr\_q15 at 8-bit precision._ 
```C++
void shr_q8 (
    const int8_t * a,
    size_t a_len,
    int8_t * out,
    int n
) 
```





**Parameters:**


* `a` Input array (int8\_t). 
* `a_len` Number of elements in a. 
* `out` Output array (int8\_t), same length as a. 
* `n` Shift count (non-negative integer).


```C++
>>> from doppler.arith import shr_q8
>>> import numpy as np
>>> a = np.array([10, 11, 12, -10], dtype=np.int8)
>>> shr_q8(a, 2).tolist()
[3, 3, 3, -2]
```
 


        

<hr>



### function sub\_q15 

_Elementwise saturating subtract of two Q15 arrays. Computes_ `out[i]` _= sat16(_`a[i]` _-_`b[i]` _) for i in_`[0, min(len(a), len(b)))` _. The intermediate difference is computed as int32\_t to detect overflow before clamping, preserving the correct sign at the saturation boundary._
```C++
void sub_q15 (
    const int16_t * a,
    size_t a_len,
    const int16_t * b,
    size_t b_len,
    int16_t * out
) 
```





**Parameters:**


* `a` Minuend array (int16\_t). 
* `a_len` Number of elements in a. 
* `b` Subtrahend array (int16\_t), same length as a. 
* `b_len` Number of elements in b. 
* `out` Output array (int16\_t), length min(a\_len, b\_len).


```C++
>>> from doppler.arith import sub_q15
>>> import numpy as np
>>> a = np.array([100,  0, -32768], dtype=np.int16)
>>> b = np.array([50,   0,     10], dtype=np.int16)
>>> sub_q15(a, b).tolist()
[50, 0, -32768]
```
 


        

<hr>



### function sub\_q8 

_Elementwise saturating subtract of two Q8 arrays. Computes_ `out[i]` _= sat8(_`a[i]` _-_`b[i]` _) for i in_`[0, min(len(a), len(b)))` _. The intermediate difference is computed as int16\_t so overflow is detected before clamping._
```C++
void sub_q8 (
    const int8_t * a,
    size_t a_len,
    const int8_t * b,
    size_t b_len,
    int8_t * out
) 
```





**Parameters:**


* `a` Minuend array (int8\_t). 
* `a_len` Number of elements in a. 
* `b` Subtrahend array (int8\_t), same length as a. 
* `b_len` Number of elements in b. 
* `out` Output array (int8\_t), length min(a\_len, b\_len).


```C++
>>> from doppler.arith import sub_q8
>>> import numpy as np
>>> a = np.array([50,   0, -128], dtype=np.int8)
>>> b = np.array([30,   0,   10], dtype=np.int8)
>>> sub_q8(a, b).tolist()
[20, 0, -128]
```
 


        

<hr>
## Public Static Functions Documentation




### function sat16 

```C++
static inline int16_t sat16 (
    int32_t x
) 
```




<hr>



### function sat8 

```C++
static inline int8_t sat8 (
    int16_t x
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/arith/arith_core.h`

