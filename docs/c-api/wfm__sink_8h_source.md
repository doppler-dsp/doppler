

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

typedef struct wfm_zmq_sink wfm_zmq_sink_t;

wfm_zmq_sink_t *wfm_zmq_sink_open(const char *endpoint, int sample_type);

int wfm_zmq_sink_send(wfm_zmq_sink_t *sink, const float _Complex *iq, size_t n,
                      double fs, double fc);

void wfm_zmq_sink_close(wfm_zmq_sink_t *sink);

/* Clip detection, mirroring wfm_writer (peak always tracked on the integer
 * paths, where saturation can occur; the per-component fraction is opt-in). The
 * cf32 path is left untouched — it never clips and is the streaming hot path. */

void wfm_zmq_sink_track_clipping(wfm_zmq_sink_t *sink, int on);

void wfm_zmq_sink_set_gain(wfm_zmq_sink_t *sink, double gain);

double wfm_zmq_sink_peak(const wfm_zmq_sink_t *sink);

double wfm_zmq_sink_clip_fraction(const wfm_zmq_sink_t *sink);

#ifdef __cplusplus
}
#endif

#endif /* WFM_SINK_H */
```


