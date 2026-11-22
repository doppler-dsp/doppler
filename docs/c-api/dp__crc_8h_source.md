

# File dp\_crc.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_crc.h**](dp__crc_8h.md)

[Go to the documentation of this file](dp__crc_8h.md)


```C++

#ifndef DP_CRC_H
#define DP_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  unsigned width;  
  uint64_t poly;   
  uint64_t init;   
  int      refout; 
  uint64_t xorout; 
  int      refin;  
} dp_crc_t;

#define DP_CRC_32_ISO_HDLC \
  ((dp_crc_t){ .width = 32u, .poly = 0x04C11DB7u, .init = 0xFFFFFFFFu, \
               .refout = 1, .xorout = 0xFFFFFFFFu, .refin = 1 })
#define DP_CRC_32_BZIP2 \
  ((dp_crc_t){ .width = 32u, .poly = 0x04C11DB7u, .init = 0xFFFFFFFFu, \
               .refout = 0, .xorout = 0xFFFFFFFFu, .refin = 0 })
#define DP_CRC_32C \
  ((dp_crc_t){ .width = 32u, .poly = 0x1EDC6F41u, .init = 0xFFFFFFFFu, \
               .refout = 1, .xorout = 0xFFFFFFFFu, .refin = 1 })
#define DP_CRC_16_IBM_3740 \
  ((dp_crc_t){ .width = 16u, .poly = 0x1021u, .init = 0xFFFFu, \
               .refout = 0, .xorout = 0u, .refin = 0 })
#define DP_CRC_16_XMODEM \
  ((dp_crc_t){ .width = 16u, .poly = 0x1021u, .init = 0u, \
               .refout = 0, .xorout = 0u, .refin = 0 })
#define DP_CRC_16_KERMIT \
  ((dp_crc_t){ .width = 16u, .poly = 0x1021u, .init = 0u, \
               .refout = 1, .xorout = 0u, .refin = 1 })
#define DP_CRC_8_SMBUS \
  ((dp_crc_t){ .width = 8u, .poly = 0x07u, .init = 0u, \
               .refout = 0, .xorout = 0u, .refin = 0 })
#define DP_CRC_8_ROHC \
  ((dp_crc_t){ .width = 8u, .poly = 0x07u, .init = 0xFFu, \
               .refout = 1, .xorout = 0u, .refin = 1 })
#define DP_CRC_64_ECMA_182 \
  ((dp_crc_t){ .width = 64u, .poly = 0x42F0E1EBA9EA3693ull, .init = 0u, \
               .refout = 0, .xorout = 0u, .refin = 0 })

/* The register's mask: `width` low bits set. */
static inline uint64_t
dp_crc_mask (unsigned width)
{
  return width == 64u ? ~(uint64_t)0 : (((uint64_t)1 << width) - 1u);
}

static inline int
dp_crc_valid (const dp_crc_t *p)
{
  if (p == NULL || p->width < 1u || p->width > 64u)
    return 0;
  const uint64_t mask = dp_crc_mask (p->width);
  return (p->poly & ~mask) == 0u && (p->init & ~mask) == 0u
         && (p->xorout & ~mask) == 0u;
}

static inline uint64_t
dp_crc_reflect (uint64_t v, unsigned width)
{
  uint64_t r = 0;
  for (unsigned i = 0; i < width; i++)
    r |= ((v >> i) & 1u) << (width - 1u - i);
  return r;
}

static inline uint64_t
dp_crc_step (const dp_crc_t *p, uint64_t reg, unsigned bit)
{
  const uint64_t top = (reg >> (p->width - 1u)) & 1u;
  reg                = (reg << 1) & dp_crc_mask (p->width);
  return (top ^ (uint64_t)(bit & 1u)) ? reg ^ p->poly : reg;
}

static inline uint64_t
dp_crc_finish (const dp_crc_t *p, uint64_t reg)
{
  if (p->refout)
    reg = dp_crc_reflect (reg, p->width);
  return reg ^ p->xorout;
}

static inline uint64_t
dp_crc_bits (const dp_crc_t *p, const uint8_t *bits, size_t n)
{
  uint64_t reg = p->init;
  for (size_t i = 0; i < n; i++)
    reg = dp_crc_step (p, reg, bits[i]);
  return dp_crc_finish (p, reg);
}

static inline uint64_t
dp_crc_bytes (const dp_crc_t *p, const uint8_t *bytes, size_t n)
{
  uint64_t reg = p->init;
  for (size_t i = 0; i < n; i++)
    for (unsigned k = 0; k < 8u; k++)
      reg = dp_crc_step (p, reg,
                         p->refin ? (bytes[i] >> k) & 1u
                                  : (bytes[i] >> (7u - k)) & 1u);
  return dp_crc_finish (p, reg);
}

#ifdef __cplusplus
}
#endif

#endif /* DP_CRC_H */
```


