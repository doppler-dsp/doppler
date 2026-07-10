/**
 * @file tlm_sink.h
 * @brief NATS PUB sink for telemetry records.
 *
 * Drains a dp_tlm_t record ring (telemetry/telemetry.h) to a NATS subject
 * using doppler's `dp_pub_*` wire layer: each pump publishes the available
 * records as TLM16 frames (SIGS header, sample_type = TLM16, num_samples =
 * record count, payload = packed 16-byte dp_tlm_rec_t). A `dp_sub_*`
 * receiver on the subject gets the same structured rows the in-process
 * Python `Telemetry.read()` returns.
 *
 * The implementation lives in the optional `libdoppler_stream` component
 * (it publishes through the vendored nats.c client) — link `doppler::stream`
 * alongside the core to use it. Unlike wfm_sink.h there is no weak-stub
 * seam: nothing in the core references these symbols, so a consumer that
 * doesn't link the stream component simply doesn't call them.
 *
 * Threading: the pump is a CONSUMER of the SPSC telemetry ring — run it on
 * the (single) consumer thread, never the DSP producer thread. It is
 * non-blocking on the ring side (drains what is available and returns) and
 * lossy end-to-end by design: a record dropped by the ring (overrun) or a
 * failed publish is gone; `dp_tlm_dropped()` / the pump's return value make
 * the losses visible.
 *
 * Lifecycle: dp_tlm_sink_open -> dp_tlm_sink_pump* -> dp_tlm_sink_close
 *
 * @code
 * dp_tlm_sink_t *sink = dp_tlm_sink_open ("nats://127.0.0.1:4222/tlm");
 * for (;;)                      // consumer-thread loop
 *   {
 *     int n = dp_tlm_sink_pump (sink, tlm);
 *     if (n < 0)
 *       break;                  // publish failure (broker gone)
 *     usleep (10000);           // pace to taste; pump is non-blocking
 *   }
 * dp_tlm_sink_close (sink);
 * @endcode
 */
#ifndef TLM_SINK_H
#define TLM_SINK_H

#include "telemetry/telemetry.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /** Opaque telemetry sink (a dp_pub_t publisher + a sent counter). */
  typedef struct dp_tlm_sink dp_tlm_sink_t;

  /**
   * @brief Open a telemetry sink (PUB) bound to a NATS subject.
   * @param endpoint  Endpoint, e.g. "nats://127.0.0.1:4222/tlm".
   * @return Sink handle, or NULL on publisher-create failure.
   * @note Caller must dp_tlm_sink_close() when done.
   */
  dp_tlm_sink_t *dp_tlm_sink_open (const char *endpoint);

  /**
   * @brief Drain every available record from @p tlm and publish them.
   *
   * Reads the ring in batches and publishes one TLM16 frame per batch
   * until the ring is empty. Non-blocking on the ring; the NATS publish
   * is fire-and-forget fan-out (PUB). On a publish failure the records
   * already drained from the ring for that frame are lost (telemetry is
   * lossy end-to-end by design) and the error is returned.
   *
   * @param sink  Sink handle.  Must be non-NULL.
   * @param tlm   Telemetry context to drain.  Must be non-NULL.
   * @return Number of records published (>= 0), or a negative DP_ERR_*
   *         code on a publish failure.
   */
  int dp_tlm_sink_pump (dp_tlm_sink_t *sink, dp_tlm_t *tlm);

  /** @brief Total records published since open. @param sink Must be
   *  non-NULL. */
  uint64_t dp_tlm_sink_sent (const dp_tlm_sink_t *sink);

  /** @brief Close the sink and destroy the publisher. @param sink May be
   *  NULL. */
  void dp_tlm_sink_close (dp_tlm_sink_t *sink);

#ifdef __cplusplus
}
#endif

#endif /* TLM_SINK_H */
