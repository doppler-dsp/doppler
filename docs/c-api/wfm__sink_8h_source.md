

# File wfm\_sink.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_sink.h**](wfm__sink_8h.md)

[Go to the documentation of this file](wfm__sink_8h.md)


```C++

#ifndef WFM_SINK_H
#define WFM_SINK_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The real stream sink lives in the optional `libdoppler_stream` component
 * (it pulls in the vendored nats.c client). The pure-C core embeds wfmgen,
 * which references these symbols on the `--output nats://` path.  So that
 * the core stays self-contained and links *everywhere* with no special
 * linker flags (ELF allows undefined symbols in a .so; Mach-O's ld64 does
 * not, neither in a dylib nor in a downstream's executable that statically
 * links libdoppler.a), the core ships **weak no-op definitions** of every
 * wfm_stream_sink_* symbol (see wfm_sink_stub.c).  Linking `libdoppler_stream`
 * supplies the STRONG real definitions, which override the weak stubs.
 * wfmgen gates the path with `wfm_stream_sink_available()` (0 from the stub,
 * 1 from the real component).
 *
 * NB: when linking the *static* stream archive, pull it whole
 * (`-Wl,--whole-archive` / `-Wl,-force_load`) or prefer the shared
 * libdoppler_stream — otherwise the linker keeps the core's weak stubs. */

typedef struct wfm_stream_sink wfm_stream_sink_t;

int wfm_stream_sink_available(void);

wfm_stream_sink_t *wfm_stream_sink_open(const char *endpoint, int sample_type);

int wfm_stream_sink_send(wfm_stream_sink_t *sink, const float _Complex *iq,
                         size_t n, double fs, double fc);

void wfm_stream_sink_close(wfm_stream_sink_t *sink);

/* Clip detection, mirroring wfm_writer (peak always tracked on the integer
 * paths, where saturation can occur; the per-component fraction is opt-in). The
 * cf32 path is left untouched — it never clips and is the streaming hot path. */

void wfm_stream_sink_track_clipping(wfm_stream_sink_t *sink, int on);

void wfm_stream_sink_set_gain(wfm_stream_sink_t *sink, double gain);

double wfm_stream_sink_peak(const wfm_stream_sink_t *sink);

double wfm_stream_sink_clip_fraction(const wfm_stream_sink_t *sink);

#ifdef __cplusplus
}
#endif

#endif /* WFM_SINK_H */
```


