

# File telemetry.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**telemetry**](dir_d4543964ddc0423cd91d16ab74a4089e.md) **>** [**telemetry.h**](telemetry_8h.md)

[Go to the documentation of this file](telemetry_8h.md)


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

typedef struct dp_tlm
{
  dp_tlmr_t     *ring;    
  uint64_t       now;     
  uint32_t       n_probes;
  dp_tlm_probe_t probes[DP_TLM_MAX_PROBES];
} dp_tlm_t;

dp_tlm_t *dp_tlm_create (size_t ring_records);

void dp_tlm_destroy (dp_tlm_t *t);

int dp_tlm_probe (dp_tlm_t *t, const char *name, uint32_t decim);

int dp_tlm_lookup (const dp_tlm_t *t, const char *name);

const char *dp_tlm_probe_name (const dp_tlm_t *t, int id);

size_t dp_tlm_probe_count (const dp_tlm_t *t);

size_t dp_tlm_capacity (const dp_tlm_t *t);

size_t dp_tlm_read (dp_tlm_t *t, dp_tlm_rec_t *out, size_t max_recs);

uint64_t dp_tlm_dropped (const dp_tlm_t *t);

uint64_t dp_tlm_emitted (const dp_tlm_t *t, int id);

static inline void
dp_tlm_set_now (dp_tlm_t *t, uint64_t n)
{
  if (t)
    t->now = n;
}

JM_FORCEINLINE void
dp_tlm_emit (dp_tlm_t *t, int32_t id, double v)
{
  if (!t)
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


