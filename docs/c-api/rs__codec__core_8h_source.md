

# File rs\_codec\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**rs\_codec**](dir_3e7cbca72be4a95038c1797bf5803786.md) **>** [**rs\_codec\_core.h**](rs__codec__core_8h.md)

[Go to the documentation of this file](rs__codec__core_8h.md)


```C++

#ifndef RS_CODEC_CORE_H
#define RS_CODEC_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "rs/rs_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  rs_t rs;
  /*<<property_struct_fields>>*/
} rs_codec_state_t;

rs_codec_state_t *rs_codec_create (uint32_t nroots, uint32_t symbol_bits,
                                   uint32_t field_poly, uint32_t first_root,
                                   uint32_t root_stride);

void rs_codec_destroy (rs_codec_state_t *state);

size_t rs_codec_encode_max_out (rs_codec_state_t *state, size_t n_in);

size_t rs_codec_encode (rs_codec_state_t *state, const uint8_t *in,
                        size_t n_in, uint8_t *out, size_t max_out);

int rs_codec_decode (rs_codec_state_t *state, uint8_t *codeword,
                     size_t codeword_len);

size_t rs_codec_syndromes_max_out (rs_codec_state_t *state, size_t n_in);

size_t rs_codec_syndromes (rs_codec_state_t *state, const uint8_t *in,
                           size_t n_in, uint8_t *out, size_t max_out);

int rs_codec_codeword_ok (rs_codec_state_t *state, const uint8_t *codeword,
                          size_t codeword_len);

size_t rs_codec_generator (rs_codec_state_t *state, uint8_t *out,
                           size_t out_len);

size_t rs_codec_get_n (const rs_codec_state_t *state);

size_t rs_codec_get_k (const rs_codec_state_t *state);

size_t rs_codec_get_e (const rs_codec_state_t *state);

size_t rs_codec_get_nroots (const rs_codec_state_t *state);

size_t rs_codec_get_symbol_bits (const rs_codec_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* RS_CODEC_CORE_H */
```


