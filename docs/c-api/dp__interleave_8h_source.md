

# File dp\_interleave.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_interleave.h**](dp__interleave_8h.md)

[Go to the documentation of this file](dp__interleave_8h.md)


```C++

#ifndef DP_INTERLEAVE_H
#define DP_INTERLEAVE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline size_t
dp_interleave_index (size_t i, size_t rows, size_t cols)
{
  return (i % cols) * rows + (i / cols);
}

static inline size_t
dp_deinterleave_index (size_t o, size_t rows, size_t cols)
{
  return dp_interleave_index (o, cols, rows);
}

static inline size_t
dp_interleave_block_units (size_t rows, size_t cols)
{
  return rows * cols;
}

static inline void
dp_interleave_raw (const void *in, void *out, size_t rows, size_t cols,
                   size_t unit_bytes)
{
  const unsigned char *s = (const unsigned char *)in;
  unsigned char       *d = (unsigned char *)out;
  for (size_t r = 0; r < rows; r++)
    for (size_t c = 0; c < cols; c++)
      memcpy (d + (c * rows + r) * unit_bytes,
              s + (r * cols + c) * unit_bytes, unit_bytes);
}

static inline void
dp_interleave_u8 (const uint8_t *in, uint8_t *out, size_t rows, size_t cols,
                  size_t unit)
{
  dp_interleave_raw (in, out, rows, cols, unit);
}

static inline void
dp_deinterleave_u8 (const uint8_t *in, uint8_t *out, size_t rows, size_t cols,
                    size_t unit)
{
  dp_interleave_raw (in, out, cols, rows, unit);
}

static inline void
dp_interleave_f32 (const float *in, float *out, size_t rows, size_t cols,
                   size_t unit)
{
  dp_interleave_raw (in, out, rows, cols, unit * sizeof (float));
}

static inline void
dp_deinterleave_f32 (const float *in, float *out, size_t rows, size_t cols,
                     size_t unit)
{
  dp_interleave_raw (in, out, cols, rows, unit * sizeof (float));
}

#ifdef __cplusplus
}
#endif

#endif /* DP_INTERLEAVE_H */
```


