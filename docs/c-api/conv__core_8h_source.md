

# File conv\_core.h

[**File List**](files.md) **>** [**conv**](dir_779d3467bbcde033259ac71c6a9863bb.md) **>** [**conv\_core.h**](conv__core_8h.md)

[Go to the documentation of this file](conv__core_8h.md)


```C++

#ifndef CONV_CORE_H
#define CONV_CORE_H

#include "clib_common.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define CONV_K_MAX 9

#define CONV_N_MAX 6

  typedef struct
  {
    unsigned k;                  
    unsigned n;                  
    uint32_t poly[CONV_N_MAX];   
    uint32_t invert;             
  } conv_code_t;

  int conv_code_valid (const conv_code_t *c);

  JM_FORCEINLINE uint32_t
  conv_states (const conv_code_t *c)
  {
    return 1u << (c->k - 1u);
  }

  unsigned conv_outputs (const conv_code_t *c, uint32_t state, unsigned bit);

  JM_FORCEINLINE uint32_t
  conv_next_state (const conv_code_t *c, uint32_t state, unsigned bit)
  {
    return (((bit & 1u) << (c->k - 1u)) | state) >> 1;
  }

  /* ── the encoder ─────────────────────────────────────────────────────── */

  typedef struct
  {
    uint32_t reg; 
  } conv_enc_t;

  void conv_enc_init (conv_enc_t *s);

  size_t conv_encode (conv_enc_t *s, const conv_code_t *c, const uint8_t *in,
                      size_t n_in, uint8_t *out, size_t max_out);

  /* ── the decoder ─────────────────────────────────────────────────────── */

  typedef struct viterbi_state_t viterbi_state_t;

  viterbi_state_t *viterbi_create (const conv_code_t *c, size_t depth);

  void viterbi_destroy (viterbi_state_t *s);

  void viterbi_reset (viterbi_state_t *s);

  size_t viterbi_decode (viterbi_state_t *s, const float *llr, size_t n_llr,
                         uint8_t *out, size_t max_out);

  size_t viterbi_decode_max_out (const viterbi_state_t *s, size_t n_llr);

  const conv_code_t *viterbi_code (const viterbi_state_t *s);

  size_t viterbi_depth (const viterbi_state_t *s);

#ifdef __cplusplus
}
#endif

#endif /* CONV_CORE_H */
```


