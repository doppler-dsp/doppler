

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
    uint64_t seed;      /* initial seed stored for dp_awgn_reset()  */
    float    amplitude;
    /* 8 independent xoshiro256++ streams for the AVX2 path.
     * vs[word][stream]: word ∈ {0,1,2,3}, stream ∈ {0..7}. */
    uint64_t vs[4][8];
  } dp_awgn_state_t;

  dp_awgn_state_t *dp_awgn_create (uint64_t seed, float amplitude);

  void dp_awgn_destroy (dp_awgn_state_t *state);

  void dp_awgn_reset (dp_awgn_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Serializes the running RNG state — the scalar xoshiro256++ state s[4] and
   * the 8 AVX2 stream states vs[4][8] — so a resumed generator continues the
   * exact same noise sequence.  seed / amplitude are config (constructor).
   * Envelope: [dp_state_hdr_t][u64 s[4]][u64 vs[4][8]]. */
#define AWGN_STATE_MAGIC DP_FOURCC ('A', 'W', 'G', 'N')
#define AWGN_STATE_VERSION 1u

  size_t dp_awgn_state_bytes (const dp_awgn_state_t *state);
  void dp_awgn_get_state (const dp_awgn_state_t *state, void *blob);
  int dp_awgn_set_state (dp_awgn_state_t *state, const void *blob);

  float dp_awgn_get_amplitude (const dp_awgn_state_t *state);

  float awgn_amplitude_for_snr (float snr_db, float signal_power);

  void dp_awgn_set_amplitude (dp_awgn_state_t *state, float val);

  void dp_awgn_reseed (dp_awgn_state_t *state, uint64_t seed);

  size_t dp_awgn_generate_max_out (dp_awgn_state_t *state);

  size_t dp_awgn_generate (dp_awgn_state_t *state, size_t n, float _Complex *out,
                        size_t max_out);

  int awgn (uint64_t seed, float amplitude, size_t n, float _Complex *out);

#ifdef __cplusplus
}
#endif

#endif /* AWGN_CORE_H */
```


