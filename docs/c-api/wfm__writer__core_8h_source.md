

# File wfm\_writer\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm\_writer**](dir_a59bfdc441aa05aed9607457147ad53f.md) **>** [**wfm\_writer\_core.h**](wfm__writer__core_8h.md)

[Go to the documentation of this file](wfm__writer__core_8h.md)


```C++

#ifndef WFM_WRITER_H
#define WFM_WRITER_H

#include <stdbool.h>
#include <stdio.h>

#include "clib_common.h"
#include "wfm/wfm_compose.h" /* wfm_segment_t for SigMF annotations */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WFM_FT_RAW = 0,  
    WFM_FT_CSV = 1,  
    WFM_FT_BLUE = 2, 
    WFM_FT_SIGMF = 3 
} wfm_filetype_t;

typedef struct wfm_writer_state wfm_writer_state_t;


wfm_writer_state_t *wfm_writer_open(FILE *fp, wfm_filetype_t ft, int sample_type,
                             int endian, double fs, double fc,
                             size_t total_samples, double t0_unix_sec);

size_t wfm_writer_write(wfm_writer_state_t *state, const float complex *x, size_t x_len);

int wfm_writer_add_keyword(wfm_writer_state_t *w, const char *tag, char type,
                          const void *value, size_t count);

int wfm_writer_close(wfm_writer_state_t *w);

int wfm_writer_destroy(wfm_writer_state_t *state);

/* ── clip detection ───────────────────────────────────────────────────────
 * Full-scale is ±1.0 per axis; integer wire types saturate to it. The writer
 * always tracks the running peak |I|/|Q| (a fused max, free in the write loop),
 * so peak > 1.0 means an integer capture clipped — and the remedy is exactly
 * ceil(20*log10(peak)) dB of headroom. The per-component clipped *fraction* is
 * the one extra per-sample compare, so it is opt-in via
 * wfm_writer_track_clipping(); off, clip_fraction() returns 0. Float types
 * (cf32/cf64) never clip but still report a peak. Call after writing. */

void wfm_writer_track_clipping(wfm_writer_state_t *state, int on);

/* ── headroom ──────────────────────────────────────────────────────────────
 * A common output gain applied to every sample just before quantisation, so
 * peaks fit under full-scale. `--headroom H` (dB) backs the composite off to
 * −H dBFS: gain = 10^(−H/20). It is a single scale, so it does not change any
 * power ratio (SNR is invariant); it only moves the absolute level. Default
 * gain 1.0 (H = 0) is a bit-exact no-op (×1.0), so output stays byte-identical.
 * Floats scale too (they just never clip); peak/clip tracking sees the scaled
 * values. */

void wfm_writer_set_gain(wfm_writer_state_t *w, double gain);

double wfm_writer_peak(const wfm_writer_state_t *w);

double wfm_writer_clip_fraction(const wfm_writer_state_t *w);

wfm_writer_state_t *wfm_writer_create(const char *path, double fs, int file_type, int sample_type, int endian, double fc, size_t total, double headroom, double t0, bool sidecar);

int wfm_blue_write_hcb(FILE *fp, int sample_type, int endian, double fs,
                       double fc, double data_start, size_t total_samples,
                       int detached, double t0_unix_sec);

char *wfm_sigmf_meta_json(int sample_type, int endian, double fs, double fc,
                          double t0_unix_sec, const wfm_segment_t *segs,
                          size_t n_segs);

/* No wfm_writer_reset: the object declares `no_reset` (gh-542), so jm emits no
   reset() binding and no call site. A writer has nothing coherent to reset --
   the samples are on disk and the written count drives the BLUE data_size patch
   -- so the method is absent rather than a no-op or a raise. */
double wfm_writer_get_clip_fraction(const wfm_writer_state_t *state);
double wfm_writer_get_peak_dbfs(const wfm_writer_state_t *state);
bool wfm_writer_get_clipped(const wfm_writer_state_t *state);
int write_blue_header(const char *path, double fs, int sample_type, int endian, double fc, double data_start, size_t total, int detached, double t0);
#ifdef __cplusplus
}
#endif

#endif /* WFM_WRITER_H */
```


