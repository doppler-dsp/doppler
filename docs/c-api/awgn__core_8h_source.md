

# File awgn\_core.h

[**File List**](files.md) **>** [**awgn**](dir_6240b6c8e1c7fd073a984e370d89f937.md) **>** [**awgn\_core.h**](awgn__core_8h.md)

[Go to the documentation of this file](awgn__core_8h.md)


```C++

#ifndef DP_AWGN_CORE_H
#define DP_AWGN_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    uint64_t s[4];      /* xoshiro256++ scalar state             */
    uint64_t seed;      /* the seed dp_awgn_reset() replays: create's,
                           or the last reseed's or restored blob's     */
    float    amplitude;
  } dp_awgn_state_t;

  dp_awgn_state_t *dp_awgn_create (uint64_t seed, float amplitude);

  void dp_awgn_destroy (dp_awgn_state_t *state);

  void dp_awgn_reset (dp_awgn_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Serializes the running xoshiro256++ state s[4], so a resumed generator
   * continues the exact same noise sequence, and the two values a mutator
   * can change after create (a mutator's value is state, #2022): the seed,
   * which dp_awgn_reseed writes and dp_awgn_reset reseeds from, and the
   * amplitude, which dp_awgn_set_amplitude writes.
   * Envelope: [dp_state_hdr_t][u64 s[4]][u64 seed][f32 amplitude].
   * v2 (#2084): the seed and amplitude; the unread AVX2 stream words
   * vs[4][8] are gone. */
#define AWGN_STATE_MAGIC DP_FOURCC ('A', 'W', 'G', 'N')
#define AWGN_STATE_VERSION 2u

  size_t dp_awgn_state_bytes (const dp_awgn_state_t *state);
  void dp_awgn_get_state (const dp_awgn_state_t *state, void *blob);
  int dp_awgn_set_state (dp_awgn_state_t *state, const void *blob);

  float dp_awgn_get_amplitude (const dp_awgn_state_t *state);

  float dp_awgn_amplitude_for_snr (float snr_db, float signal_power);

  int dp_awgn_amplitude_ok (float amplitude);

  int dp_awgn_set_amplitude (dp_awgn_state_t *state, float val);

  void dp_awgn_reseed (dp_awgn_state_t *state, uint64_t seed);

  size_t dp_awgn_generate_max_out (dp_awgn_state_t *state);

  size_t dp_awgn_generate (dp_awgn_state_t *state, size_t n, float _Complex *out,
                        size_t max_out);

  int dp_awgn (uint64_t seed, float amplitude, size_t n, float _Complex *out);

#ifdef __cplusplus
}
#endif

#endif /* AWGN_CORE_H */
```


