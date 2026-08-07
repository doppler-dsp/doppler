

# File dp\_tlm\_core.h

[**File List**](files.md) **>** [**dp\_tlm**](dir_76b7d6d4427bc094138fa987d2f2ac6b.md) **>** [**dp\_tlm\_core.h**](dp__tlm__core_8h.md)

[Go to the documentation of this file](dp__tlm__core_8h.md)


```C++

#ifndef DP_TELEMETRY_H
#define DP_TELEMETRY_H

#include "buffer/buffer.h"
#include "clib_common.h" /* DP_OK, DP_ERR_INVALID */
#include "jm_perf.h"      /* JM_FORCEINLINE */

/* 16-byte ring slots: sizeof(uint64_t)*2 per "complex sample" — exactly one
 * telemetry record each, buying the VM-mirrored contiguity, acquire/release
 * correctness and the dropped counter for free. */
DECLARE_DP_BUFFER (tlmr, uint64_t)


typedef struct
{
  uint64_t n;     
  float    value; 
  uint16_t probe; 
  uint16_t flags; 
} dp_tlm_rec_t;

/* One record must fill exactly one ring slot (C99-portable assert). */
typedef char dp_tlm_rec_fits_slot[sizeof (dp_tlm_rec_t)
                                          == 2 * sizeof (uint64_t)
                                      ? 1
                                      : -1];

#define DP_TLM_MAX_PROBES 64
#define DP_TLM_NAME_MAX 32

typedef struct
{
  char     name[DP_TLM_NAME_MAX]; 
  uint32_t decim;                 
  uint32_t phase;                 
  uint64_t emitted;               
} dp_tlm_probe_t;

typedef struct dp_tlm_capture dp_tlm_capture_t;

typedef struct dp_tlm
{
  dp_tlmr_t     *ring;    
  uint64_t       now;     
  uint32_t       n_probes;
  dp_tlm_probe_t probes[DP_TLM_MAX_PROBES];
  dp_tlm_capture_t *capture;
  int (*capture_drain) (dp_tlm_capture_t *);
} dp_tlm_t;

typedef dp_tlm_t dp_tlm_state_t;

dp_tlm_t *dp_tlm_create (size_t ring_records);

void dp_tlm_destroy (dp_tlm_t *t);

int dp_tlm_probe (dp_tlm_t *t, const char *name, uint32_t decim);

int dp_tlm_probe_id (const dp_tlm_t *t, const char *name);

int dp_tlm_emit_checked (dp_tlm_t *t, int32_t id, double v);

int dp_tlm_set_decim (dp_tlm_t *t, const char *name, uint32_t decim);

const char *dp_tlm_probe_name (const dp_tlm_t *t, int id);

size_t dp_tlm_probe_count (const dp_tlm_t *t);

size_t dp_tlm_capacity (const dp_tlm_t *t);

int dp_tlm_probe_id_at (const dp_tlm_t *t, size_t i);

size_t dp_tlm_block_bound (const dp_tlm_t *t, size_t block_samples);

size_t dp_tlm_avail (const dp_tlm_t *t);

int dp_tlm_resize (dp_tlm_t *t, size_t records);

typedef struct
{
  uint64_t dropped;  
  uint64_t emitted;  
  size_t   capacity; 
  size_t   probes;   
} dp_tlm_stats_t;

dp_tlm_stats_t dp_tlm_stats (const dp_tlm_t *t);

size_t dp_tlm_read_max_out (dp_tlm_t *t);

size_t dp_tlm_read (dp_tlm_t *t, size_t n, dp_tlm_rec_t *out,
                    size_t max_out);

uint64_t dp_tlm_dropped (const dp_tlm_t *t);

uint64_t dp_tlm_emitted (const dp_tlm_t *t, int id);

static inline void
dp_tlm_set_now (dp_tlm_t *t, uint64_t n)
{
  if (!t)
    return;
  if (t->capture_drain)
    t->capture_drain (t->capture);
  t->now = n;
}

JM_FORCEINLINE void
dp_tlm_emit (dp_tlm_t *t, int32_t id, double v)
{
  if (!t || (uint32_t) id >= DP_TLM_MAX_PROBES)
    return;
  dp_tlm_probe_t *p = &t->probes[id];
  if (++p->phase < p->decim)
    return;
  p->phase = 0;
  dp_tlm_rec_t r = { t->now, (float) v, (uint16_t) id, 0u };
  if (dp_tlmr_write (t->ring, (const uint64_t *) &r, 1))
    p->emitted++;
}

#ifndef DP_TLM_DISABLE
#define DP_TLM(ctx, id, v) dp_tlm_emit ((ctx), (id), (v))
#else
#define DP_TLM(ctx, id, v) ((void) 0)
#endif

#endif /* DP_TELEMETRY_H */
```


