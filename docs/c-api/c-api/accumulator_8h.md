

# File accumulator.h



[**FileList**](files.md) **>** [**accumulator**](dir_06136a2119985c3c219633f937232576.md) **>** [**accumulator.h**](accumulator_8h.md)

[Go to the source code of this file](accumulator_8h_source.md)

_General-purpose scalar accumulator for f32 and cf64 signals._ [More...](#detailed-description)

* `#include <complex.h>`
* `#include <stddef.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_acc\_cf64 | [**dp\_acc\_cf64\_t**](#typedef-dp_acc_cf64_t)  <br> |
| typedef struct dp\_acc\_f32 | [**dp\_acc\_f32\_t**](#typedef-dp_acc_f32_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_acc\_cf64\_add**](#function-dp_acc_cf64_add) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc, const double \_Complex \* x, size\_t n) <br> |
|  void | [**dp\_acc\_cf64\_add2d**](#function-dp_acc_cf64_add2d) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc, const double \_Complex \* x, size\_t rows, size\_t cols) <br> |
|  [**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* | [**dp\_acc\_cf64\_create**](#function-dp_acc_cf64_create) (void) <br> |
|  void | [**dp\_acc\_cf64\_destroy**](#function-dp_acc_cf64_destroy) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc) <br> |
|  double \_Complex | [**dp\_acc\_cf64\_dump**](#function-dp_acc_cf64_dump) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc) <br> |
|  double \_Complex | [**dp\_acc\_cf64\_get**](#function-dp_acc_cf64_get) (const [**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc) <br> |
|  void | [**dp\_acc\_cf64\_madd**](#function-dp_acc_cf64_madd) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc, const double \_Complex \*restrict x, const float \*restrict h, size\_t n) <br> |
|  void | [**dp\_acc\_cf64\_madd2d**](#function-dp_acc_cf64_madd2d) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc, const double \_Complex \*restrict x, const float \*restrict h, size\_t rows, size\_t cols) <br> |
|  void | [**dp\_acc\_cf64\_push**](#function-dp_acc_cf64_push) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc, double \_Complex x) <br> |
|  void | [**dp\_acc\_cf64\_reset**](#function-dp_acc_cf64_reset) ([**dp\_acc\_cf64\_t**](accumulator_8h.md#typedef-dp_acc_cf64_t) \* acc) <br> |
|  void | [**dp\_acc\_f32\_add**](#function-dp_acc_f32_add) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc, const float \* x, size\_t n) <br> |
|  void | [**dp\_acc\_f32\_add2d**](#function-dp_acc_f32_add2d) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc, const float \* x, size\_t rows, size\_t cols) <br> |
|  [**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* | [**dp\_acc\_f32\_create**](#function-dp_acc_f32_create) (void) <br> |
|  void | [**dp\_acc\_f32\_destroy**](#function-dp_acc_f32_destroy) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc) <br> |
|  float | [**dp\_acc\_f32\_dump**](#function-dp_acc_f32_dump) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc) <br> |
|  float | [**dp\_acc\_f32\_get**](#function-dp_acc_f32_get) (const [**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc) <br> |
|  void | [**dp\_acc\_f32\_madd**](#function-dp_acc_f32_madd) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc, const float \*restrict x, const float \*restrict h, size\_t n) <br> |
|  void | [**dp\_acc\_f32\_madd2d**](#function-dp_acc_f32_madd2d) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc, const float \*restrict x, const float \*restrict h, size\_t rows, size\_t cols) <br> |
|  void | [**dp\_acc\_f32\_push**](#function-dp_acc_f32_push) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc, float x) <br> |
|  void | [**dp\_acc\_f32\_reset**](#function-dp_acc_f32_reset) ([**dp\_acc\_f32\_t**](accumulator_8h.md#typedef-dp_acc_f32_t) \* acc) <br> |




























## Detailed Description


## Public Types Documentation




### typedef dp\_acc\_cf64\_t

```C++
typedef struct dp_acc_cf64 dp_acc_cf64_t;
```




<hr>



### typedef dp\_acc\_f32\_t

```C++
typedef struct dp_acc_f32 dp_acc_f32_t;
```




<hr>
## Public Functions Documentation




### function dp\_acc\_cf64\_add

```C++
void dp_acc_cf64_add (
    dp_acc_cf64_t * acc,
    const double _Complex * x,
    size_t n
)
```




<hr>



### function dp\_acc\_cf64\_add2d

```C++
void dp_acc_cf64_add2d (
    dp_acc_cf64_t * acc,
    const double _Complex * x,
    size_t rows,
    size_t cols
)
```




<hr>



### function dp\_acc\_cf64\_create

```C++
dp_acc_cf64_t * dp_acc_cf64_create (
    void
)
```




<hr>



### function dp\_acc\_cf64\_destroy

```C++
void dp_acc_cf64_destroy (
    dp_acc_cf64_t * acc
)
```




<hr>



### function dp\_acc\_cf64\_dump

```C++
double _Complex dp_acc_cf64_dump (
    dp_acc_cf64_t * acc
)
```




<hr>



### function dp\_acc\_cf64\_get

```C++
double _Complex dp_acc_cf64_get (
    const dp_acc_cf64_t * acc
)
```




<hr>



### function dp\_acc\_cf64\_madd

```C++
void dp_acc_cf64_madd (
    dp_acc_cf64_t * acc,
    const double _Complex *restrict x,
    const float *restrict h,
    size_t n
)
```




<hr>



### function dp\_acc\_cf64\_madd2d

```C++
void dp_acc_cf64_madd2d (
    dp_acc_cf64_t * acc,
    const double _Complex *restrict x,
    const float *restrict h,
    size_t rows,
    size_t cols
)
```




<hr>



### function dp\_acc\_cf64\_push

```C++
void dp_acc_cf64_push (
    dp_acc_cf64_t * acc,
    double _Complex x
)
```




<hr>



### function dp\_acc\_cf64\_reset

```C++
void dp_acc_cf64_reset (
    dp_acc_cf64_t * acc
)
```




<hr>



### function dp\_acc\_f32\_add

```C++
void dp_acc_f32_add (
    dp_acc_f32_t * acc,
    const float * x,
    size_t n
)
```




<hr>



### function dp\_acc\_f32\_add2d

```C++
void dp_acc_f32_add2d (
    dp_acc_f32_t * acc,
    const float * x,
    size_t rows,
    size_t cols
)
```




<hr>



### function dp\_acc\_f32\_create

```C++
dp_acc_f32_t * dp_acc_f32_create (
    void
)
```




<hr>



### function dp\_acc\_f32\_destroy

```C++
void dp_acc_f32_destroy (
    dp_acc_f32_t * acc
)
```




<hr>



### function dp\_acc\_f32\_dump

```C++
float dp_acc_f32_dump (
    dp_acc_f32_t * acc
)
```




<hr>



### function dp\_acc\_f32\_get

```C++
float dp_acc_f32_get (
    const dp_acc_f32_t * acc
)
```




<hr>



### function dp\_acc\_f32\_madd

```C++
void dp_acc_f32_madd (
    dp_acc_f32_t * acc,
    const float *restrict x,
    const float *restrict h,
    size_t n
)
```




<hr>



### function dp\_acc\_f32\_madd2d

```C++
void dp_acc_f32_madd2d (
    dp_acc_f32_t * acc,
    const float *restrict x,
    const float *restrict h,
    size_t rows,
    size_t cols
)
```




<hr>



### function dp\_acc\_f32\_push

```C++
void dp_acc_f32_push (
    dp_acc_f32_t * acc,
    float x
)
```




<hr>



### function dp\_acc\_f32\_reset

```C++
void dp_acc_f32_reset (
    dp_acc_f32_t * acc
)
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/accumulator/accumulator.h`
