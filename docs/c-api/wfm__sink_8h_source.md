

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

/* The ZMQ sink lives in the optional `libdoppler_stream` component (it pulls in
 * the vendored C++ libzmq).  The pure-C core embeds wfmgen, which references
 * these symbols only on the `--output zmq://` path; declaring them WEAK lets the
 * core link with the references unresolved (address 0) so a downstream that does
 * NOT link the stream component still gets a C++-free libdoppler.  wfmgen guards
 * the path with `&wfm_zmq_sink_open == NULL`.  Link `libdoppler_stream` (whole-
 * archive, or the object directly) to supply the definitions and enable zmq. */
#if defined(__APPLE__)
/* Mach-O: a plain `weak` symbol is still a hard undefined reference at
 * dylib-link time (ld64 rejects it). weak_import marks it as an optional
 * import that binds to address 0 when the stream component is absent, so the
 * `&wfm_zmq_sink_open == NULL` guard in wfmgen works the same as on ELF. */
#define WFM_WEAK __attribute__ ((weak_import))
#elif defined(__GNUC__)
#define WFM_WEAK __attribute__ ((weak))
#else
#define WFM_WEAK
#endif

typedef struct wfm_zmq_sink wfm_zmq_sink_t;

WFM_WEAK wfm_zmq_sink_t *wfm_zmq_sink_open(const char *endpoint,
                                           int sample_type);

WFM_WEAK int wfm_zmq_sink_send(wfm_zmq_sink_t *sink, const float _Complex *iq,
                               size_t n, double fs, double fc);

WFM_WEAK void wfm_zmq_sink_close(wfm_zmq_sink_t *sink);

/* Clip detection, mirroring wfm_writer (peak always tracked on the integer
 * paths, where saturation can occur; the per-component fraction is opt-in). The
 * cf32 path is left untouched — it never clips and is the streaming hot path. */

WFM_WEAK void wfm_zmq_sink_track_clipping(wfm_zmq_sink_t *sink, int on);

WFM_WEAK void wfm_zmq_sink_set_gain(wfm_zmq_sink_t *sink, double gain);

WFM_WEAK double wfm_zmq_sink_peak(const wfm_zmq_sink_t *sink);

WFM_WEAK double wfm_zmq_sink_clip_fraction(const wfm_zmq_sink_t *sink);

#ifdef __cplusplus
}
#endif

#endif /* WFM_SINK_H */
```


