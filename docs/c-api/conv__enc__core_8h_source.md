

# File conv\_enc\_core.h

[**File List**](files.md) **>** [**conv\_enc**](dir_b689baf1ac742b6ceba235289d5a286b.md) **>** [**conv\_enc\_core.h**](conv__enc__core_8h.md)

[Go to the documentation of this file](conv__enc__core_8h.md)


```C++

#ifndef CONV_ENC_CORE_H
#define CONV_ENC_CORE_H

#include "clib_common.h"
#include "conv/conv_core.h"
#include "dp_state.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    conv_code_t code; 
    conv_enc_t  enc;  
    /*<<property_struct_fields>>*/
  } conv_enc_state_t;

  conv_enc_state_t *conv_enc_create (const uint32_t *poly, size_t poly_len,
                                     uint32_t k, uint32_t invert);

  conv_enc_state_t *conv_enc_create_code (const conv_code_t *c);

  void conv_enc_destroy (conv_enc_state_t *state);

  void conv_enc_reset (conv_enc_state_t *state);

  size_t conv_enc_encode_max_out (const conv_enc_state_t *state, size_t n_in);

  size_t conv_enc_encode (conv_enc_state_t *state, const uint8_t *in,
                          size_t n_in, uint8_t *out, size_t max_out);

  const conv_code_t *conv_enc_code (const conv_enc_state_t *s);

  /* ── the state bytes interface ─────────────────────────────────────────
   *
   * The register is running state that survives between calls, so this
   * speaks the standard bytes interface like every other stateful object in
   * the tree. An encoder is a link in a chain, and one link that cannot be
   * checkpointed is enough to make the chain un-resumable. See
   * docs/design/state-serialization.md.
   */

#define CONV_ENC_STATE_MAGIC DP_FOURCC ('C', 'V', 'E', 'N')
#define CONV_ENC_STATE_VERSION 1u

  size_t conv_enc_state_bytes (const conv_enc_state_t *s);

  void conv_enc_get_state (const conv_enc_state_t *s, void *blob);

  int conv_enc_set_state (conv_enc_state_t *s, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* CONV_ENC_CORE_H */
```


