/**
 * @file wfm_sink.h
 * @brief ZMQ PUB sink for generated IQ (Phase B).
 *
 * Streams cf32 blocks (from synth or the composer) to a ZMQ PUB endpoint using
 * doppler's `dp_pub_*` wire layer (SIGS header, magic "SIGS"), converting to
 * the requested wire sample type per block. This is the `--output zmq://…`
 * destination; a `dp_sub_*` receiver (e.g. examples/c/spectrum_analyzer) reads
 * the stream.
 *
 * Lifecycle: wfm_zmq_sink_open -> wfm_zmq_sink_send* -> wfm_zmq_sink_close
 *
 * @code
 * wfm_zmq_sink_t *s = wfm_zmq_sink_open("tcp://0.0.0.0:5555", 3); // ci16
 * wfm_zmq_sink_send(s, iq, 4096, 1e6, 2.4e9);
 * wfm_zmq_sink_close(s);
 * @endcode
 */
#ifndef WFM_SINK_H
#define WFM_SINK_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The real ZMQ sink lives in the optional `libdoppler_stream` component (it
 * pulls in the vendored C++ libzmq).  The pure-C core embeds wfmgen, which
 * references these symbols on the `--output zmq://` path.  So that the core
 * stays self-contained and links *everywhere* with no special linker flags
 * (ELF allows undefined symbols in a .so; Mach-O's ld64 does not, neither in a
 * dylib nor in a downstream's executable that statically links libdoppler.a),
 * the core ships **weak no-op definitions** of every wfm_zmq_sink_* symbol (see
 * wfm_sink_stub.c).  Linking `libdoppler_stream` supplies the STRONG real
 * definitions, which override the weak stubs.  wfmgen gates the path with
 * `wfm_zmq_sink_available()` (0 from the stub, 1 from the real component).
 *
 * NB: when linking the *static* stream archive, pull it whole
 * (`-Wl,--whole-archive` / `-Wl,-force_load`) or prefer the shared
 * libdoppler_stream — otherwise the linker keeps the core's weak stubs. */

/** Opaque ZMQ sink. */
typedef struct wfm_zmq_sink wfm_zmq_sink_t;

/** @brief 1 if the real ZMQ sink (libdoppler_stream) is linked, else 0 (the
 *  pure-C core links only the weak no-op stubs). wfmgen checks this before the
 *  `--output zmq://` path. */
int wfm_zmq_sink_available(void);

/**
 * @brief Open a ZMQ PUB sink.
 * @param endpoint     ZMQ bind endpoint, e.g. "tcp://0.0.0.0:5555".
 * @param sample_type  Wire type (wavegen order): 0 cf32, 1 cf64, 2 ci32,
 *                     3 ci16, 4 ci8. Integer types use full-scale ±1.0.
 * @return Sink handle, or NULL on bad type / publisher-create failure.
 * @note Caller must wfm_zmq_sink_close() when done.
 */
wfm_zmq_sink_t *wfm_zmq_sink_open(const char *endpoint, int sample_type);

/**
 * @brief Convert a cf32 block to the wire type and publish it.
 * @param sink  the sink handle.
 * @param iq  Complex-float samples; @param n complex sample count.
 * @param fs  sample rate (Hz); @param fc center frequency (Hz) — wire header.
 * @return 0 on success, non-zero on a send/allocation error.
 */
int wfm_zmq_sink_send(wfm_zmq_sink_t *sink, const float _Complex *iq, size_t n,
                      double fs, double fc);

/** @brief Close the sink and destroy the publisher. @param sink May be NULL. */
void wfm_zmq_sink_close(wfm_zmq_sink_t *sink);

/* Clip detection, mirroring wfm_writer (peak always tracked on the integer
 * paths, where saturation can occur; the per-component fraction is opt-in). The
 * cf32 path is left untouched — it never clips and is the streaming hot path. */

/** Enable the per-component clip counter (off by default; peak always on). */
void wfm_zmq_sink_track_clipping(wfm_zmq_sink_t *sink, int on);

/** Set the output gain (linear; default 1.0). For headroom H dB pass
 *  10^(−H/20). gain 1.0 sends cf32 unscaled (the direct path). */
void wfm_zmq_sink_set_gain(wfm_zmq_sink_t *sink, double gain);

/** Largest per-axis magnitude seen on an integer path (pre-clip, full-scale 1).
 *  > 1.0 ⇒ clipped; peak_dBFS = 20*log10(peak). */
double wfm_zmq_sink_peak(const wfm_zmq_sink_t *sink);

/** Fraction (0..1) of integer I/Q components that saturated; 0 unless tracked.
 *  The generated ZmqSink handle binds peak/clip_fraction directly as per-field
 *  getters (jm#320), so no stats-snapshot struct shim is needed. */
double wfm_zmq_sink_clip_fraction(const wfm_zmq_sink_t *sink);

#ifdef __cplusplus
}
#endif

#endif /* WFM_SINK_H */
