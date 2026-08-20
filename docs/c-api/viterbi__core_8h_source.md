

# File viterbi\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**viterbi**](dir_abfb52fd33d2d22e092a3b80738d1015.md) **>** [**viterbi\_core.h**](viterbi__core_8h.md)

[Go to the documentation of this file](viterbi__core_8h.md)


```C++

#ifndef VITERBI_CORE_H
#define VITERBI_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "conv/conv_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  conv_code_t code;
  size_t      depth;
  uint32_t    nstate;

  float   *pm;   
  float   *pm2;  
  uint8_t *dec;  
  size_t   head; 
  size_t   fill; 
  /* Derived once: for each state, its two predecessors and their output
     words. The butterfly -- predecessors (ns << 1) & mask and | 1, both on
     the same input bit ns >> (k-2) -- holds for every k. */
  uint32_t *pred0;
  uint32_t *pred1;
  unsigned *out0;
  unsigned *out1;
  unsigned *inbit;
/*<<property_struct_fields>>*/
} viterbi_state_t;

viterbi_state_t *viterbi_create(const uint32_t *poly, size_t poly_len, uint32_t k, uint32_t invert, size_t depth);

void viterbi_destroy(viterbi_state_t *state);

void viterbi_reset(viterbi_state_t *state);

size_t viterbi_decode_max_out (const viterbi_state_t *state, size_t n_in);

size_t viterbi_decode(viterbi_state_t *state, const float *in, size_t n_in, uint8_t *out, size_t max_out);

/* ── hand-owned: the surface jm does not declare ───────────────────────────
 *
 * jm declares the lifecycle and `decode` from objects/viterbi.toml. What
 * follows is this component's own C API — the conv_code_t constructor its
 * internal callers use, node synchronization, and the state triplet (which
 * is hand-written per docs/design/state-serialization.md; the manifest's
 * `serializable` flag generates the PYTHON side over it).
 */

viterbi_state_t *viterbi_create_code (const conv_code_t *c, size_t depth);

const conv_code_t *viterbi_code (const viterbi_state_t *s);

size_t viterbi_depth (const viterbi_state_t *s);

/* ── node synchronization ────────────────────────────────────────────── */

typedef struct
{
  unsigned phase;   
  size_t   errors;  
  size_t   next;    
  size_t   symbols; 
  size_t   margin;  
} node_sync_t;

size_t node_sync_score (viterbi_state_t *v, const float *llr, size_t n_llr);

size_t node_sync_scored_symbols (const viterbi_state_t *v, size_t n_llr);

int node_sync_scan (viterbi_state_t *v, const float *llr, size_t n_llr,
                    node_sync_t *out);

/* ── the state bytes interface ───────────────────────────────────────────
 *
 * The decoder carries running state across calls — a path metric per state,
 * the traceback ring, and where the ring is — so it speaks the standard
 * bytes interface like every other stateful object in the tree. A decoder
 * sits inside a chain (behind the receiver, in front of the R-S decoder),
 * and one link that cannot be checkpointed is enough to make the chain
 * un-resumable. See docs/design/state-serialization.md.
 */

#define VITERBI_STATE_MAGIC DP_FOURCC ('V', 'T', 'R', 'B')
#define VITERBI_STATE_VERSION 1u

size_t viterbi_state_bytes (const viterbi_state_t *s);

void viterbi_get_state (const viterbi_state_t *s, void *blob);

int viterbi_set_state (viterbi_state_t *s, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* VITERBI_CORE_H */
```


