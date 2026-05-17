

# File accumulator.h

[**File List**](files.md) **>** [**accumulator**](dir_06136a2119985c3c219633f937232576.md) **>** [**accumulator.h**](accumulator_8h.md)

[Go to the documentation of this file](accumulator_8h.md)


```C++


#ifndef NATIVE_ACCUMULATOR_H
#define NATIVE_ACCUMULATOR_H

#include <complex.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct dp_acc_f32 dp_acc_f32_t;
  typedef struct dp_acc_cf64 dp_acc_cf64_t;

  dp_acc_f32_t *dp_acc_f32_create (void);
  dp_acc_cf64_t *dp_acc_cf64_create (void);
  void dp_acc_f32_destroy (dp_acc_f32_t *acc);
  void dp_acc_cf64_destroy (dp_acc_cf64_t *acc);

  void dp_acc_f32_reset (dp_acc_f32_t *acc);
  void dp_acc_cf64_reset (dp_acc_cf64_t *acc);
  float dp_acc_f32_dump (dp_acc_f32_t *acc);
  double _Complex dp_acc_cf64_dump (dp_acc_cf64_t *acc);

  float dp_acc_f32_get (const dp_acc_f32_t *acc);
  double _Complex dp_acc_cf64_get (const dp_acc_cf64_t *acc);

  void dp_acc_f32_push (dp_acc_f32_t *acc, float x);
  void dp_acc_cf64_push (dp_acc_cf64_t *acc, double _Complex x);

  void dp_acc_f32_add (dp_acc_f32_t *acc, const float *x, size_t n);
  void dp_acc_cf64_add (dp_acc_cf64_t *acc, const double _Complex *x,
                        size_t n);

  void dp_acc_f32_madd (dp_acc_f32_t *acc, const float *restrict x,
                        const float *restrict h, size_t n);
  void dp_acc_cf64_madd (dp_acc_cf64_t *acc, const double _Complex *restrict x,
                         const float *restrict h, size_t n);

  void dp_acc_f32_add2d (dp_acc_f32_t *acc, const float *x, size_t rows,
                         size_t cols);
  void dp_acc_cf64_add2d (dp_acc_cf64_t *acc, const double _Complex *x,
                          size_t rows, size_t cols);

  void dp_acc_f32_madd2d (dp_acc_f32_t *acc, const float *restrict x,
                          const float *restrict h, size_t rows, size_t cols);
  void dp_acc_cf64_madd2d (dp_acc_cf64_t *acc,
                           const double _Complex *restrict x,
                           const float *restrict h, size_t rows, size_t cols);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_ACCUMULATOR_H */
```
