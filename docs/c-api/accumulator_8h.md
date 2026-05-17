

# File accumulator.h



[**FileList**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**accumulator.h**](accumulator_8h.md)

[Go to the source code of this file](accumulator_8h_source.md)

_General-purpose scalar accumulator for f32 and cf64 signals._ [More...](#detailed-description)

* `#include <dp/stream.h>`
* `#include <stddef.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_acc\_cf64 | [**dp\_acc\_cf64\_t**](#typedef-dp_acc_cf64_t)  <br>_Stateful complex cf64 accumulator._  |
| typedef struct dp\_acc\_f32 | [**dp\_acc\_f32\_t**](#typedef-dp_acc_f32_t)  <br>_Stateful real f32 accumulator._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_acc\_cf64\_add**](#function-dp_acc_cf64_add) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc, const double \_Complex \* x, size\_t n) <br>_Accumulate a 1-D cf64 array: acc += Σ x[k], k=0..n-1._  |
|  void | [**dp\_acc\_cf64\_add2d**](#function-dp_acc_cf64_add2d) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc, const double \_Complex \* x, size\_t rows, size\_t cols) <br>_Accumulate a row-major 2-D cf64 array: acc += Σᵢ Σⱼ x[i][j]._  |
|  [**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* | [**dp\_acc\_cf64\_create**](#function-dp_acc_cf64_create) (void) <br>_Allocate and zero a complex cf64 accumulator._  |
|  void | [**dp\_acc\_cf64\_destroy**](#function-dp_acc_cf64_destroy) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc) <br>_Free a complex cf64 accumulator._  |
|  double \_Complex | [**dp\_acc\_cf64\_dump**](#function-dp_acc_cf64_dump) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc) <br>_Return the current cf64 value then reset to zero._  |
|  double \_Complex | [**dp\_acc\_cf64\_get**](#function-dp_acc_cf64_get) (const [**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc) <br>_Return the current cf64 accumulated value (non-destructive)._  |
|  void | [**dp\_acc\_cf64\_madd**](#function-dp_acc_cf64_madd) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc, const double \_Complex \*restrict x, const float \*restrict h, size\_t n) <br>_1-D multiply-accumulate (MAC) for cf64 × real h: acc += Σ x[k]·h[k], k=0..n-1._  |
|  void | [**dp\_acc\_cf64\_madd2d**](#function-dp_acc_cf64_madd2d) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc, const double \_Complex \*restrict x, const float \*restrict h, size\_t rows, size\_t cols) <br>_2-D multiply-accumulate for cf64 × real h: acc += Σᵢ Σⱼ x[i][j]·h[i][j]._  |
|  void | [**dp\_acc\_cf64\_push**](#function-dp_acc_cf64_push) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc, double \_Complex x) <br>_Add a single cf64 sample: acc += x._  |
|  void | [**dp\_acc\_cf64\_reset**](#function-dp_acc_cf64_reset) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc) <br>_Zero the cf64 accumulator without freeing it._  |
|  void | [**dp\_acc\_f32\_add**](#function-dp_acc_f32_add) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc, const float \* x, size\_t n) <br>_Accumulate a 1-D f32 array: acc += Σ x[k], k=0..n-1._  |
|  void | [**dp\_acc\_f32\_add2d**](#function-dp_acc_f32_add2d) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc, const float \* x, size\_t rows, size\_t cols) <br>_Accumulate a row-major 2-D f32 array: acc += Σᵢ Σⱼ x[i][j]._  |
|  [**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* | [**dp\_acc\_f32\_create**](#function-dp_acc_f32_create) (void) <br>_Allocate and zero a real f32 accumulator._  |
|  void | [**dp\_acc\_f32\_destroy**](#function-dp_acc_f32_destroy) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc) <br>_Free a real f32 accumulator._  |
|  float | [**dp\_acc\_f32\_dump**](#function-dp_acc_f32_dump) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc) <br>_Return the current f32 value then reset to zero._  |
|  float | [**dp\_acc\_f32\_get**](#function-dp_acc_f32_get) (const [**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc) <br>_Return the current f32 accumulated value (non-destructive)._  |
|  void | [**dp\_acc\_f32\_madd**](#function-dp_acc_f32_madd) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc, const float \*restrict x, const float \*restrict h, size\_t n) <br>_1-D multiply-accumulate (MAC) for f32: acc += Σ x[k]·h[k], k=0..n-1._  |
|  void | [**dp\_acc\_f32\_madd2d**](#function-dp_acc_f32_madd2d) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc, const float \*restrict x, const float \*restrict h, size\_t rows, size\_t cols) <br>_2-D multiply-accumulate for f32: acc += Σᵢ Σⱼ x[i][j]·h[i][j]._  |
|  void | [**dp\_acc\_f32\_push**](#function-dp_acc_f32_push) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc, float x) <br>_Add a single f32 sample: acc += x._  |
|  void | [**dp\_acc\_f32\_reset**](#function-dp_acc_f32_reset) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc) <br>_Zero the f32 accumulator without freeing it._  |




























## Detailed Description


## Public Types Documentation




### typedef dp\_acc\_cf64\_t

_Stateful complex cf64 accumulator._
```C++
typedef struct dp_acc_cf64 dp_acc_cf64_t;
```




<hr>



### typedef dp\_acc\_f32\_t

_Stateful real f32 accumulator._
```C++
typedef struct dp_acc_f32 dp_acc_f32_t;
```




<hr>
## Public Functions Documentation




### function dp\_acc\_cf64\_add

_Accumulate a 1-D cf64 array: acc += Σ x[k], k=0..n-1._
```C++
void dp_acc_cf64_add (
    dp_acc_cf64_t * acc,
    const double _Complex * x,
    size_t n
)
```





**Parameters:**


* `acc` Must be non-NULL.
* `x` Input array of `n` double \_Complex samples.
* `n` Number of complex samples.






<hr>



### function dp\_acc\_cf64\_add2d

_Accumulate a row-major 2-D cf64 array: acc += Σᵢ Σⱼ x[i][j]._
```C++
void dp_acc_cf64_add2d (
    dp_acc_cf64_t * acc,
    const double _Complex * x,
    size_t rows,
    size_t cols
)
```





**Parameters:**


* `acc` Must be non-NULL.
* `x` Row-major array of `rows` × `cols` double \_Complex samples.
* `rows` Number of rows.
* `cols` Number of columns.






<hr>



### function dp\_acc\_cf64\_create

_Allocate and zero a complex cf64 accumulator._
```C++
dp_acc_cf64_t * dp_acc_cf64_create (
    void
)
```





**Returns:**

Heap-allocated accumulator, or NULL on failure.







<hr>



### function dp\_acc\_cf64\_destroy

_Free a complex cf64 accumulator._
```C++
void dp_acc_cf64_destroy (
    dp_acc_cf64_t * acc
)
```





**Parameters:**


* `acc` May be NULL (no-op).






<hr>



### function dp\_acc\_cf64\_dump

_Return the current cf64 value then reset to zero._
```C++
double _Complex dp_acc_cf64_dump (
    dp_acc_cf64_t * acc
)
```





**Parameters:**


* `acc` Must be non-NULL.



**Returns:**

Accumulated value before the reset.







<hr>



### function dp\_acc\_cf64\_get

_Return the current cf64 accumulated value (non-destructive)._
```C++
double _Complex dp_acc_cf64_get (
    const dp_acc_cf64_t * acc
)
```





**Parameters:**


* `acc` Must be non-NULL.






<hr>



### function dp\_acc\_cf64\_madd

_1-D multiply-accumulate (MAC) for cf64 × real h: acc += Σ x[k]·h[k], k=0..n-1._
```C++
void dp_acc_cf64_madd (
    dp_acc_cf64_t * acc,
    const double _Complex *restrict x,
    const float *restrict h,
    size_t n
)
```



Complex samples are multiplied by real coefficients, matching the typical polyphase FIR structure where taps are real-valued.




**Parameters:**


* `acc` Must be non-NULL.
* `x` Complex signal array (`n` double \_Complex samples).
* `h` Real coefficient array (`n` floats).
* `n` Number of samples / taps.






<hr>



### function dp\_acc\_cf64\_madd2d

_2-D multiply-accumulate for cf64 × real h: acc += Σᵢ Σⱼ x[i][j]·h[i][j]._
```C++
void dp_acc_cf64_madd2d (
    dp_acc_cf64_t * acc,
    const double _Complex *restrict x,
    const float *restrict h,
    size_t rows,
    size_t cols
)
```





**Parameters:**


* `acc` Must be non-NULL.
* `x` Complex signal matrix (`rows` × `cols` double \_Complex).
* `h` Real coefficient matrix (`rows` × `cols` floats).
* `rows` Number of rows.
* `cols` Number of columns.






<hr>



### function dp\_acc\_cf64\_push

_Add a single cf64 sample: acc += x._
```C++
void dp_acc_cf64_push (
    dp_acc_cf64_t * acc,
    double _Complex x
)
```





**Parameters:**


* `acc` Must be non-NULL.
* `x` Complex value to add.






<hr>



### function dp\_acc\_cf64\_reset

_Zero the cf64 accumulator without freeing it._
```C++
void dp_acc_cf64_reset (
    dp_acc_cf64_t * acc
)
```





**Parameters:**


* `acc` Must be non-NULL.






<hr>



### function dp\_acc\_f32\_add

_Accumulate a 1-D f32 array: acc += Σ x[k], k=0..n-1._
```C++
void dp_acc_f32_add (
    dp_acc_f32_t * acc,
    const float * x,
    size_t n
)
```



Inner loop is written to allow auto-vectorisation.




**Parameters:**


* `acc` Must be non-NULL.
* `x` Input array of `n` floats.
* `n` Number of elements.






<hr>



### function dp\_acc\_f32\_add2d

_Accumulate a row-major 2-D f32 array: acc += Σᵢ Σⱼ x[i][j]._
```C++
void dp_acc_f32_add2d (
    dp_acc_f32_t * acc,
    const float * x,
    size_t rows,
    size_t cols
)
```



Equivalent to dp\_acc\_f32\_add(acc, x, rows\*cols) for contiguous arrays; provided for clarity at call sites using 2-D data.




**Parameters:**


* `acc` Must be non-NULL.
* `x` Row-major array of `rows` × `cols` floats.
* `rows` Number of rows.
* `cols` Number of columns.






<hr>



### function dp\_acc\_f32\_create

_Allocate and zero a real f32 accumulator._
```C++
dp_acc_f32_t * dp_acc_f32_create (
    void
)
```





**Returns:**

Heap-allocated accumulator, or NULL on failure.







<hr>



### function dp\_acc\_f32\_destroy

_Free a real f32 accumulator._
```C++
void dp_acc_f32_destroy (
    dp_acc_f32_t * acc
)
```





**Parameters:**


* `acc` May be NULL (no-op).






<hr>



### function dp\_acc\_f32\_dump

_Return the current f32 value then reset to zero._
```C++
float dp_acc_f32_dump (
    dp_acc_f32_t * acc
)
```



Equivalent to get() followed by reset(). The canonical "dump" operation for the polyphase decimation overflow event.




**Parameters:**


* `acc` Must be non-NULL.



**Returns:**

Accumulated value before the reset.







<hr>



### function dp\_acc\_f32\_get

_Return the current f32 accumulated value (non-destructive)._
```C++
float dp_acc_f32_get (
    const dp_acc_f32_t * acc
)
```





**Parameters:**


* `acc` Must be non-NULL.






<hr>



### function dp\_acc\_f32\_madd

_1-D multiply-accumulate (MAC) for f32: acc += Σ x[k]·h[k], k=0..n-1._
```C++
void dp_acc_f32_madd (
    dp_acc_f32_t * acc,
    const float *restrict x,
    const float *restrict h,
    size_t n
)
```





**Parameters:**


* `acc` Must be non-NULL.
* `x` Signal array (`n` floats).
* `h` Coefficient array (`n` floats).
* `n` Number of elements.






<hr>



### function dp\_acc\_f32\_madd2d

_2-D multiply-accumulate for f32: acc += Σᵢ Σⱼ x[i][j]·h[i][j]._
```C++
void dp_acc_f32_madd2d (
    dp_acc_f32_t * acc,
    const float *restrict x,
    const float *restrict h,
    size_t rows,
    size_t cols
)
```



Both arrays must be row-major with identical dimensions.




**Parameters:**


* `acc` Must be non-NULL.
* `x` Signal matrix (`rows` × `cols` floats).
* `h` Coefficient matrix (`rows` × `cols` floats).
* `rows` Number of rows.
* `cols` Number of columns.






<hr>



### function dp\_acc\_f32\_push

_Add a single f32 sample: acc += x._
```C++
void dp_acc_f32_push (
    dp_acc_f32_t * acc,
    float x
)
```





**Parameters:**


* `acc` Must be non-NULL.
* `x` Value to add.






<hr>



### function dp\_acc\_f32\_reset

_Zero the f32 accumulator without freeing it._
```C++
void dp_acc_f32_reset (
    dp_acc_f32_t * acc
)
```





**Parameters:**


* `acc` Must be non-NULL.






<hr>

------------------------------
The documentation for this class was generated from the following file `c/include/dp/accumulator.h`
