/*
 * wfm_sink_stub.c — weak no-op fallbacks for the stream sink, compiled into
 * the pure-C core (libdoppler).
 *
 * wfmgen lives in the core and references the wfm_stream_sink_* symbols on
 * its `--output nats://` path, but the real sink (which pulls in the
 * vendored nats.c client) lives in the optional libdoppler_stream component.
 * Shipping WEAK DEFINITIONS here keeps libdoppler self-contained — it links
 * everywhere with no special linker flags, on ELF and on Mach-O (whose ld64
 * rejects undefined symbols in both a dylib and a downstream's static-linked
 * executable).
 *
 * Linking libdoppler_stream supplies the STRONG real definitions (wfm_sink.c),
 * which override these weak stubs.  wfmgen never calls the operational stubs:
 * it gates the whole path on wfm_stream_sink_available(), which the stub
 * reports as 0 (unavailable) and the real component reports as 1.
 *
 * These stubs are pure C (no nats.c, no vendored client), so the core stays
 * self-contained.
 */

#include "wfm/wfm_sink.h"

#if defined(__GNUC__)
#define WFM_STUB __attribute__ ((weak))
#else
#define WFM_STUB
#endif

WFM_STUB int
wfm_stream_sink_available (void)
{
  return 0;
}

WFM_STUB wfm_stream_sink_t *
wfm_stream_sink_open (const char *endpoint, int sample_type)
{
  (void)endpoint;
  (void)sample_type;
  return NULL;
}

WFM_STUB int
wfm_stream_sink_send (wfm_stream_sink_t *sink, const float _Complex *iq,
                      size_t n, double fs, double fc)
{
  (void)sink;
  (void)iq;
  (void)n;
  (void)fs;
  (void)fc;
  return -1;
}

WFM_STUB void
wfm_stream_sink_close (wfm_stream_sink_t *sink)
{
  (void)sink;
}

WFM_STUB void
wfm_stream_sink_track_clipping (wfm_stream_sink_t *sink, int on)
{
  (void)sink;
  (void)on;
}

WFM_STUB void
wfm_stream_sink_set_gain (wfm_stream_sink_t *sink, double gain)
{
  (void)sink;
  (void)gain;
}

WFM_STUB double
wfm_stream_sink_peak (const wfm_stream_sink_t *sink)
{
  (void)sink;
  return 0.0;
}

WFM_STUB double
wfm_stream_sink_clip_fraction (const wfm_stream_sink_t *sink)
{
  (void)sink;
  return 0.0;
}
