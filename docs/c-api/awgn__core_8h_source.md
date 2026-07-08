

# File awgn\_core.h

[**File List**](files.md) **>** [**awgn**](dir_b535f71dd6c18f769df9e4bf89a97331.md) **>** [**awgn\_core.h**](awgn__core_8h.md)

[Go to the documentation of this file](awgn__core_8h.md)


```C++

#ifndef AWGN_CORE_H
#define AWGN_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    uint64_t s[4];      /* xoshiro256++ scalar state             */
    uint64_t seed;      /* initial seed stored for awgn_reset()  */
    float    amplitude;
    /* 8 independent xoshiro256++ streams for the AVX2 path.
     * vs[word][stream]: word ∈ {0,1,2,3}, stream ∈ {0..7}. */
    uint64_t vs[4][8];
  } awgn_state_t;

  awgn_state_t *awgn_create (uint64_t seed, float amplitude);

  void awgn_destroy (awgn_state_t *state);

  void awgn_reset (awgn_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Serializes the running RNG state — the scalar xoshiro256++ state s[4] and
   * the 8 AVX2 stream states vs[4][8] — so a resumed generator continues the
   * exact same noise sequence.  seed / amplitude are config (constructor).
   * Envelope: [dp_state_hdr_t][u64 s[4]][u64 vs[4][8]]. */
#define AWGN_STATE_MAGIC DP_FOURCC ('A', 'W', 'G', 'N')
#define AWGN_STATE_VERSION 1u

  size_t awgn_state_bytes (const awgn_state_t *state);
  void awgn_get_state (const awgn_state_t *state, void *blob);
  int awgn_set_state (awgn_state_t *state, const void *blob);

  float awgn_get_amplitude (const awgn_state_t *state);

  void awgn_set_amplitude (awgn_state_t *state, float val);

  void awgn_reseed (awgn_state_t *state, uint64_t seed);

  size_t awgn_generate_max_out (awgn_state_t *state);

  size_t awgn_generate (awgn_state_t *state, size_t n, float complex *out);

  int awgn (uint64_t seed, float amplitude, size_t n, float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* AWGN_CORE_H */
```


