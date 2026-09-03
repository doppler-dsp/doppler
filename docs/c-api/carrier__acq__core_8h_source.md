

# File carrier\_acq\_core.h

[**File List**](files.md) **>** [**carrier\_acq**](dir_fda2da85aa46b94cfd09d911f4a8e3eb.md) **>** [**carrier\_acq\_core.h**](carrier__acq__core_8h.md)

[Go to the documentation of this file](carrier__acq__core_8h.md)


```C++

#ifndef CARRIER_ACQ_CORE_H
#define CARRIER_ACQ_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "psd/psd_core.h"
#include "detector/detector_core.h"
#include "detection/detection_core.h"
#include "spectral/spectral_core.h"
#include "corr/corr_core.h"
#include "fft/fft_core.h"
#include "acc_trace/acc_trace_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Composed children (owned, by pointer). */
    psd_state_t      *psd; 
    detector_state_t *det; 
    /* Scratch, not state -- sized nfft/psd->n, allocated once at create
     * (no allocation in the steps() hot path). */
    float         *pwr_buf;    
    float _Complex *power_buf;  
    float _Complex *carry_buf;  
    size_t         carry_len;  
    /* Config, fixed at construction (restored by create(), not the
     * blob -- validated against the blob's own copy in set_state). */
    double sample_rate_hz;
    double pfa;
    bool   sequential;
    double s_t;   
    double s_t2;  
    /* Public (property-backed) running/result fields. */
    bool   ready;
    double residual_hz;
    size_t n_blocks;
    size_t dwell_target;  
    size_t max_n_blocks;  
    size_t nfft;
} carrier_acq_state_t;

carrier_acq_state_t *carrier_acq_create(
    double sample_rate_hz, double symbol_rate_hz, double resolution_hz,
    size_t zero_pad, int window, float beta, const float *psd_template,
    size_t psd_template_len, double pfa, double pd, double design_snr,
    bool sequential, size_t max_n_blocks);

void carrier_acq_destroy(carrier_acq_state_t *state);

void carrier_acq_reset(carrier_acq_state_t *state);

void carrier_acq_steps(carrier_acq_state_t *state, const float _Complex *x,
                       size_t x_len);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Composition: psd + detector children (self-contained sub-blobs) + own
 * running fields (n_blocks/ready/residual_hz/carry_len) + the carry
 * buffer (fixed psd->n capacity, only the first carry_len samples
 * meaningful). nfft/dwell_target/max_n_blocks are config -- validated
 * against the live instance, not restored from the blob (a resumed
 * instance must be
 * constructed with the same sample_rate_hz/symbol_rate_hz/psd_template/
 * pfa/pd/design_snr as the one that produced the blob -- the same class
 * of precondition dsss_receiver's own segments/sps/n check documents;
 * neither corr_core nor detector_core hash-verify their own reference
 * spectra either). */
#define CARRIER_ACQ_STATE_MAGIC DP_FOURCC('C', 'A', 'Q', 'R')
#define CARRIER_ACQ_STATE_VERSION 1u
size_t carrier_acq_state_bytes(const carrier_acq_state_t *state);
void   carrier_acq_get_state(const carrier_acq_state_t *state, void *blob);
int    carrier_acq_set_state(carrier_acq_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* CARRIER_ACQ_CORE_H */
```


