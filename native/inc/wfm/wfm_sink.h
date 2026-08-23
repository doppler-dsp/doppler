/**
 * @file wfm_sink.h
 * @brief NATS PUB sink for generated IQ (Phase B).
 *
 * Streams cf32 blocks (from synth or the composer) to a NATS subject using
 * doppler's `dp_pub_*` wire layer (SIGS header, magic "SIGS"), converting to
 * the requested wire sample type per block. This is the `--output nats://…`
 * destination; a `dp_sub_*` receiver (e.g. native/examples/spectrum_analyzer) reads
 * the stream.
 *
 * Lifecycle: wfm_stream_sink_open -> wfm_stream_sink_send* -> wfm_stream_sink_close
 *
 * @code
 * wfm_stream_sink_t *s = wfm_stream_sink_open("nats://127.0.0.1:4222/iq", 3); // ci16
 * wfm_stream_sink_send(s, iq, 4096, 1e6, 2.4e9);
 * wfm_stream_sink_close(s);
 * @endcode
 */
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

/** Opaque stream sink. */
typedef struct wfm_stream_sink wfm_stream_sink_t;

/** @brief 1 if the real stream sink (libdoppler_stream) is linked, else 0
 *  (the pure-C core links only the weak no-op stubs). wfmgen checks this
 *  before the `--output nats://` path. */
int wfm_stream_sink_available(void);

/**
 * @brief Open a stream sink (PUB) bound to a NATS subject.
 * @param endpoint     Endpoint, e.g. "nats://127.0.0.1:4222/iq".
 * @param sample_type  Wire type (wavegen order): 0 cf32, 1 cf64, 2 ci32,
 *                     3 ci16, 4 ci8. Integer types use full-scale ±1.0.
 * @return Sink handle, or NULL on bad type / publisher-create failure.
 * @note Caller must wfm_stream_sink_close() when done.
 */
wfm_stream_sink_t *wfm_stream_sink_open(const char *endpoint, int sample_type);

/**
 * @brief Convert a cf32 block to the wire type and publish it.
 * @param sink  the sink handle.
 * @param iq  Complex-float samples; @param n complex sample count.
 * @param fs  sample rate (Hz); @param fc center frequency (Hz) — wire header.
 * @return 0 on success, non-zero on a send/allocation error.
 */
int wfm_stream_sink_send(wfm_stream_sink_t *sink, const float _Complex *iq,
                         size_t n, double fs, double fc);

/**
 * @brief Tell subscribers this stream has ended.
 *
 * Publishes an end-of-stream frame, so a consumer learns the sender
 * finished rather than inferring it from silence. Send it BEFORE draining:
 * a drain cannot be reversed and refuses sends once it reaches its
 * publish-flushing phase.
 *
 * @param sink Sink; NULL is DP_OK (there is no stream to end).
 * @return DP_OK, or the stream layer's error.
 */
int wfm_stream_sink_send_eos(wfm_stream_sink_t *sink);

/**
 * @brief Let everything already sent reach the server, then stop.
 *
 * A send hands a block to the NATS client and returns; the
 * client writes it in the background. So "send returned" is not "the server
 * has it", and closing without asking relies on the client's own
 * best-effort flush -- capped at 500 ms, with no way to report failure, so
 * a backlog that cannot clear in half a second is dropped silently.
 *
 * Call this before closing the sink on any run whose tail matters.
 * After it returns the sink is finished: close it next, which is then just
 * the free.
 *
 * @param sink       Sink; NULL is DP_OK (nothing was buffered).
 * @param timeout_ms Budget; <= 0 uses the stream layer's 5 s default.
 * @return DP_OK once drained, or the stream layer's error -- DP_ERR_TIMEOUT
 *         if the budget ran out with the drain still in progress, in which
 *         case the sink is still safe to close.
 */
int wfm_stream_sink_drain(wfm_stream_sink_t *sink, int timeout_ms);

/** @brief Close the sink and destroy the publisher. @param sink May be NULL. */
void wfm_stream_sink_close(wfm_stream_sink_t *sink);

/* Clip detection, mirroring wfm_writer (peak always tracked on the integer
 * paths, where saturation can occur; the per-component fraction is opt-in). The
 * cf32 path is left untouched — it never clips and is the streaming hot path. */

/** Enable the per-component clip counter (off by default; peak always on). */
void wfm_stream_sink_track_clipping(wfm_stream_sink_t *sink, int on);

/** Set the output gain (linear; default 1.0). For headroom H dB pass
 *  10^(−H/20). gain 1.0 sends cf32 unscaled (the direct path). */
void wfm_stream_sink_set_gain(wfm_stream_sink_t *sink, double gain);

/** Largest per-axis magnitude seen on an integer path (pre-clip, full-scale 1).
 *  > 1.0 ⇒ clipped; peak_dBFS = 20*log10(peak). */
double wfm_stream_sink_peak(const wfm_stream_sink_t *sink);

/** Fraction (0..1) of integer I/Q components that saturated; 0 unless tracked.
 *  The generated StreamSink handle binds peak/clip_fraction directly as per-field
 *  getters (jm#320), so no stats-snapshot struct shim is needed. */
double wfm_stream_sink_clip_fraction(const wfm_stream_sink_t *sink);

#ifdef __cplusplus
}
#endif

#endif /* WFM_SINK_H */
