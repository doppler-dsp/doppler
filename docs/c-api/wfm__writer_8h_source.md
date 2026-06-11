

# File wfm\_writer.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_writer.h**](wfm__writer_8h.md)

[Go to the documentation of this file](wfm__writer_8h.md)


```C++

#ifndef WFM_WRITER_H
#define WFM_WRITER_H

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

typedef struct wfm_writer wfm_writer_t;

wfm_writer_t *wfm_writer_open(FILE *fp, wfm_filetype_t ft, int sample_type,
                             int endian, double fs, double fc,
                             size_t total_samples);

size_t wfm_writer_write(wfm_writer_t *w, const float _Complex *iq, size_t n);

int wfm_writer_close(wfm_writer_t *w);

/* ── clip detection ───────────────────────────────────────────────────────
 * Full-scale is ±1.0 per axis; integer wire types saturate to it. The writer
 * always tracks the running peak |I|/|Q| (a fused max, free in the write loop),
 * so peak > 1.0 means an integer capture clipped — and the remedy is exactly
 * ceil(20*log10(peak)) dB of headroom. The per-component clipped *fraction* is
 * the one extra per-sample compare, so it is opt-in via
 * wfm_writer_track_clipping(); off, clip_fraction() returns 0. Float types
 * (cf32/cf64) never clip but still report a peak. Call after writing. */

void wfm_writer_track_clipping(wfm_writer_t *w, int on);

/* ── headroom ──────────────────────────────────────────────────────────────
 * A common output gain applied to every sample just before quantisation, so
 * peaks fit under full-scale. `--headroom H` (dB) backs the composite off to
 * −H dBFS: gain = 10^(−H/20). It is a single scale, so it does not change any
 * power ratio (SNR is invariant); it only moves the absolute level. Default
 * gain 1.0 (H = 0) is a bit-exact no-op (×1.0), so output stays byte-identical.
 * Floats scale too (they just never clip); peak/clip tracking sees the scaled
 * values. */

void wfm_writer_set_gain(wfm_writer_t *w, double gain);

double wfm_writer_peak(const wfm_writer_t *w);

double wfm_writer_clip_fraction(const wfm_writer_t *w);

int wfm_blue_write_hcb(FILE *fp, int sample_type, int endian, double fs,
                       double fc, double data_start, size_t total_samples,
                       int detached);

char *wfm_sigmf_meta_json(int sample_type, int endian, double fs, double fc,
                          const wfm_segment_t *segs, size_t n_segs);

#ifdef __cplusplus
}
#endif

#endif /* WFM_WRITER_H */
```


