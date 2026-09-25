

# File rs\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**rs**](dir_947c1c4c0fb4d4a4d0adf0ec99900cc5.md) **>** [**rs\_core.h**](rs__core_8h.md)

[Go to the documentation of this file](rs__core_8h.md)


```C++

#ifndef RS_CORE_H
#define RS_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define RS_SYMBOL_BITS_MAX 8

#define RS_NROOTS_MAX 64

#define RS_N_MAX 255

  typedef struct
  {
    unsigned symbol_bits; 
    uint16_t field_poly;  
    unsigned nroots;      
    unsigned first_root;  
    unsigned root_stride; 
  } rs_code_t;

  typedef struct
  {
    rs_code_t code;               
    unsigned  n;                  
    unsigned  k;                  
    unsigned  e;                  
    uint8_t   exp[2 * RS_N_MAX];  
    uint8_t   log[RS_N_MAX + 1];  
    uint8_t   gen[RS_NROOTS_MAX + 1]; 
  } rs_t;

  int rs_code_valid (const rs_code_t *c);

  int rs_init (rs_t *rs, const rs_code_t *c);

  const uint8_t *rs_generator (const rs_t *rs);

  void rs_encode (const rs_t *rs, const uint8_t *info, uint8_t *parity);

  void rs_syndromes (const rs_t *rs, const uint8_t *codeword, uint8_t *syn);

  int rs_codeword_ok (const rs_t *rs, const uint8_t *codeword);

  int rs_decode (const rs_t *rs, uint8_t *codeword);

#ifdef __cplusplus
}
#endif

#endif /* RS_CORE_H */
```


