

# File conv\_core.h

[**File List**](files.md) **>** [**conv**](dir_dadcb1d47e07452fac6fef665f736671.md) **>** [**conv\_core.h**](conv__core_8h.md)

[Go to the documentation of this file](conv__core_8h.md)


```C++

#ifndef CONV_CORE_H
#define CONV_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"

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
    unsigned punct_period;       
    uint32_t punct_mask;         
  } conv_code_t;

  int dp_conv_code_valid (const conv_code_t *c);

  JM_FORCEINLINE uint32_t
  conv_states (const conv_code_t *c)
  {
    return 1u << (c->k - 1u);
  }

  unsigned dp_conv_outputs (const conv_code_t *c, uint32_t state, unsigned bit);

  JM_FORCEINLINE uint32_t
  conv_next_state (const conv_code_t *c, uint32_t state, unsigned bit)
  {
    return (((bit & 1u) << (c->k - 1u)) | state) >> 1;
  }

  /* ── puncturing ──────────────────────────────────────────────────────── */

  int dp_conv_punctured (const conv_code_t *c);

  unsigned dp_conv_kept (const conv_code_t *c, unsigned phase);

  size_t dp_conv_coded_len (const conv_code_t *c, unsigned phase, size_t n_in);

  int dp_conv_steps_for (const conv_code_t *c, unsigned phase, size_t n_syms,
                         size_t *steps);

  unsigned dp_conv_phase_after (const conv_code_t *c, unsigned phase,
                                size_t n_in);

  /* ── the encoder ─────────────────────────────────────────────────────── */

  typedef struct
  {
    uint32_t reg;   
    uint32_t phase; 
  } conv_enc_t;

  void dp_conv_enc_init (conv_enc_t *s);

  size_t dp_conv_encode (conv_enc_t *s, const conv_code_t *c, const uint8_t *in,
                      size_t n_in, uint8_t *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* CONV_CORE_H */
```


