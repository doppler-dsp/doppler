/**
 * @file stream.h
 * @brief Streaming API for doppler — PUB/SUB, PUSH/PULL, REQ/REP.
 *
 * Provides ZMQ-backed signal streaming using three messaging patterns:
 *
 * | Pattern   | Sender function  | Receiver function | Use case |
 * |-----------|------------------|-------------------|------------------------|
 * | PUB/SUB   | dp_pub_*         | dp_sub_*          | Fan-out broadcast |
 * | PUSH/PULL | dp_push_*        | dp_pull_*         | Pipeline /
 * load-balance| | REQ/REP   | dp_req_*         | dp_rep_*          | Control /
 * metadata |
 *
 * ### Quick start (C)
 * ```c
 * #include <dp/stream.h>
 *
 * // Transmitter
 * dp_pub *pub = dp_pub_create("tcp://\*:5555", DP_CF64);
 * dp_cf64_t samples[1024] = { ... };
 * dp_pub_send_cf64(pub, samples, 1024, 1e6, 2.4e9);
 * dp_pub_destroy(pub);
 *
 * // Receiver (zero-copy)
 * dp_sub *sub = dp_sub_create("tcp://localhost:5555");
 * dp_msg_t *msg;  dp_header_t hdr;
 * dp_sub_recv(sub, &msg, &hdr);
 * dp_cf64_t *cf64 = (dp_cf64_t *)dp_msg_data(msg);
 * size_t n = dp_msg_num_samples(msg);
 * // use cf64[0..n-1] ...
 * dp_msg_free(msg);
 * dp_sub_destroy(sub);
 * ```
 */

#ifndef DP_STREAM_H
#define DP_STREAM_H

#include <complex.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @defgroup version Version
   *  @{ */

#define DP_VERSION_MAJOR 2 /**< Major version number. */
#define DP_VERSION_MINOR 0 /**< Minor version number. */
#define DP_VERSION_PATCH 0 /**< Patch version number. */

  /** @} */

  /* -------------------------------------------------------------------------
   * Sample types
   * ---------------------------------------------------------------------- */

  /** @defgroup types Types
   *  @{
   */

  /**
   * @brief Selects the wire format of complex samples.
   *
   * All sample arrays are packed as interleaved I/Q pairs.
   *
   * Values are fixed — new types are appended to preserve wire
   * compatibility with older receivers.
   */
  typedef enum
  {
    DP_CI32 = 0,  /**< Complex int32: int32_t I/Q   (8 bytes/sample). */
    DP_CF64 = 1,  /**< Complex float64: double I/Q  (16 bytes/sample). */
    DP_CF128 = 2, /**< Complex long double I/Q      (32 bytes/sample). */
    DP_CI8 = 3,   /**< Complex int8: int8_t I/Q     (2 bytes/sample).  */
    DP_CI16 = 4,  /**< Complex int16: int16_t I/Q   (4 bytes/sample).  */
    DP_CF32 = 5,  /**< Complex float32: float I/Q   (8 bytes/sample).  */
  } dp_sample_type_t;

  /**
   * @brief Protocol identifier for the wire header.
   */
  typedef enum
  {
    DP_PROTO_SIGS = 0, /**< Native SIGS protocol. */
    DP_PROTO_DIFI = 1, /**< DIFI / VITA 49 (reserved for future use). */
  } dp_protocol_t;

  /** @brief Complex int32 sample: signed 32-bit interleaved I/Q. */
  typedef struct
  {
    int32_t i; /**< In-phase component. */
    int32_t q; /**< Quadrature component. */
  } dp_ci32_t;

  /** @brief Complex float64 sample: double-precision interleaved I/Q. */
  typedef struct
  {
    double i; /**< In-phase component. */
    double q; /**< Quadrature component. */
  } dp_cf64_t;

  /** @brief Complex long-double sample: extended-precision interleaved I/Q. */
  typedef struct
  {
    long double i; /**< In-phase component. */
    long double q; /**< Quadrature component. */
  } dp_cf128_t;

  /** @brief Complex int8 sample: signed 8-bit interleaved I/Q.
   *
   * Lowest-weight IQ transport format.  Used by RTL-SDR and HackRF;
   * fits 32 complex samples in a single AVX-512 register.
   */
  typedef struct
  {
    int8_t i; /**< In-phase component. */
    int8_t q; /**< Quadrature component. */
  } dp_ci8_t;

  /** @brief Complex int16 sample: signed 16-bit interleaved I/Q.
   *
   * Standard IQ transport for LimeSDR, USRP, and PlutoSDR;
   * fits 16 complex samples in a single AVX-512 register.
   */
  typedef struct
  {
    int16_t i; /**< In-phase component. */
    int16_t q; /**< Quadrature component. */
  } dp_ci16_t;

  /** @brief Complex float32 sample: single-precision interleaved I/Q.
   *
   * GNU Radio wire-compatible format; fits 8 complex samples in a
   * single AVX-512 register.
   */
  typedef struct
  {
    float i; /**< In-phase component. */
    float q; /**< Quadrature component. */
  } dp_cf32_t;

  /**
   * @brief Frame metadata header carried in every ZMQ message.
   *
   * The first 4 bytes of the wire header are always the magic value
   * `0x53494753` ("SIGS"), which receivers can use for basic validation.
   * Future-proofed for DIFI / VITA 49 with protocol and stream_id fields.
   */
  typedef struct
  {
    uint32_t magic;       /**< Magic number: 0x53494753 "SIGS". */
    uint32_t version;     /**< Protocol version (currently 1). */
    uint32_t protocol;    /**< dp_protocol_t (0 = SIGS, 1 = DIFI). */
    uint32_t stream_id;   /**< DIFI stream ID; 0 for SIGS. */
    uint32_t sample_type; /**< Wire sample type (dp_sample_type_t). */
    uint32_t flags;       /**< Reserved flags — set to 0. */
    uint64_t sequence;    /**< Monotonically increasing per-sender count. */
    uint64_t
        timestamp_ns;   /**< UNIX timestamp in nanoseconds (CLOCK_REALTIME). */
    double sample_rate; /**< Sample rate in Hz. */
    double center_freq; /**< Centre frequency in Hz. */
    uint64_t num_samples; /**< Number of complex samples in this message. */
    uint64_t reserved[4]; /**< Reserved — set to zero, do not interpret. */
  } dp_header_t;

  /** @brief Opaque zero-copy message handle returned by recv functions.
   *
   * The data buffer is valid until dp_msg_free() is called. Use the accessor
   * functions to retrieve a pointer to the sample data, size, etc.
   */
  typedef struct dp_msg dp_msg_t;

  /** @brief Opaque streaming socket handle returned by all create functions.
   */
  typedef struct dp_ctx dp_pub;
  typedef struct dp_ctx dp_sub;
  typedef struct dp_ctx dp_push;
  typedef struct dp_ctx dp_pull;
  typedef struct dp_ctx dp_req;
  typedef struct dp_ctx dp_rep;

  /** @} */ /* end group types */

  /* -------------------------------------------------------------------------
   * Error codes
   * ---------------------------------------------------------------------- */

  /** @defgroup errors Error codes
   *  @{
   *
   * Every send/recv function returns one of these values.
   * Use dp_strerror() to obtain a human-readable description.
   */
#define DP_OK 0           /**< Success. */
#define DP_ERR_INIT -1    /**< Initialisation failed (ZMQ context/socket). */
#define DP_ERR_SEND -2    /**< Send failed. */
#define DP_ERR_RECV -3    /**< Receive failed or timed out (EAGAIN). */
#define DP_ERR_INVALID -4 /**< Invalid argument. */
#define DP_ERR_TIMEOUT -5 /**< Operation timed out. */
#define DP_ERR_MEMORY -6  /**< Memory allocation failure. */
  /** @} */

  /* -------------------------------------------------------------------------
   * dp_msg_t — zero-copy message accessors
   * ---------------------------------------------------------------------- */

  /** @defgroup msg Message handle
   *  @{
   */

  /**
   * @brief Return a pointer to the raw sample data inside the message.
   * @param msg Message handle returned by a recv function.
   * @return Pointer to contiguous sample data (valid until dp_msg_free).
   */
  void *dp_msg_data (dp_msg_t *msg);

  /**
   * @brief Return the byte size of the sample data.
   * @param msg Message handle.
   * @return Total data bytes.
   */
  size_t dp_msg_size (dp_msg_t *msg);

  /**
   * @brief Return the number of complex samples in the message.
   * @param msg Message handle.
   * @return Number of samples (header num_samples).
   */
  size_t dp_msg_num_samples (dp_msg_t *msg);

  /**
   * @brief Return the sample type of the message.
   * @param msg Message handle.
   * @return Sample type enum value.
   */
  dp_sample_type_t dp_msg_sample_type (dp_msg_t *msg);

  /**
   * @brief Free a message handle and release the underlying ZMQ buffer.
   * @param msg Message handle (may be NULL).
   */
  void dp_msg_free (dp_msg_t *msg);

  /** @} */ /* end group msg */

  /* -------------------------------------------------------------------------
   * Publisher / Subscriber  (PUB/SUB — fan-out broadcast)
   * ---------------------------------------------------------------------- */

  /** @defgroup pubsub PUB/SUB — fan-out broadcast
   *  @{
   *
   * The Publisher binds to an endpoint and fans out every message to all
   * connected Subscribers.  Subscribers connect and receive every frame.
   * Slow subscribers will drop frames (ZMQ_HWM behaviour).
   */

  /**
   * @brief Create a Publisher socket and bind to @p endpoint.
   *
   * @param endpoint  ZMQ endpoint string, e.g. `"tcp://\*:5555"` or
   *                  `"ipc:///tmp/feed"`.
   * @param sample_type  Sample format that will be sent.
   * @return Non-NULL context on success, NULL on failure.
   */
  dp_pub *dp_pub_create (const char *endpoint, dp_sample_type_t sample_type);

  /**
   * @brief Send an array of CI32 samples via a Publisher.
   *
   * @param ctx         Publisher context.
   * @param samples     Pointer to an array of @p num_samples dp_ci32_t values.
   * @param num_samples Number of complex samples.
   * @param sample_rate Sample rate in Hz.
   * @param center_freq Centre frequency in Hz.
   * @return DP_OK (0) on success, negative error code on failure.
   */
  int dp_pub_send_ci32 (dp_pub *ctx, const dp_ci32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  /**
   * @brief Send an array of CF64 samples via a Publisher.
   * @copydetails dp_pub_send_ci32
   */
  int dp_pub_send_cf64 (dp_pub *ctx, const dp_cf64_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  /**
   * @brief Send an array of CF128 samples via a Publisher.
   * @copydetails dp_pub_send_ci32
   */
  int dp_pub_send_cf128 (dp_pub *ctx, const dp_cf128_t *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  /**
   * @brief Send an array of CI8 samples via a Publisher.
   * @copydetails dp_pub_send_ci32
   */
  int dp_pub_send_ci8 (dp_pub *ctx, const dp_ci8_t *samples,
                       size_t num_samples, double sample_rate,
                       double center_freq);

  /**
   * @brief Send an array of CI16 samples via a Publisher.
   * @copydetails dp_pub_send_ci32
   */
  int dp_pub_send_ci16 (dp_pub *ctx, const dp_ci16_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  /**
   * @brief Send an array of CF32 samples via a Publisher.
   * @copydetails dp_pub_send_ci32
   */
  int dp_pub_send_cf32 (dp_pub *ctx, const dp_cf32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  /**
   * @brief Destroy a Publisher context and release all resources.
   * @param ctx Publisher context (may be NULL).
   */
  void dp_pub_destroy (dp_pub *ctx);

  /**
   * @brief Create a Subscriber socket and connect to @p endpoint.
   *
   * Subscribes to all topics (empty topic filter).
   *
   * @param endpoint  ZMQ endpoint to connect to, e.g.
   * `"tcp://localhost:5555"`.
   * @return Non-NULL context on success, NULL on failure.
   */
  dp_sub *dp_sub_create (const char *endpoint);

  /**
   * @brief Receive one frame from a Subscriber socket (zero-copy).
   *
   * On success, `*msg` is set to a message handle whose data buffer is
   * valid until dp_msg_free() is called.  Use dp_msg_data() to access
   * the sample pointer.
   *
   * @param ctx         Subscriber context.
   * @param[out] msg    Set to a zero-copy message handle.
   * @param[out] header Set to the frame metadata.
   * @return DP_OK on success, DP_ERR_TIMEOUT on timeout, negative on error.
   */
  int dp_sub_recv (dp_sub *ctx, dp_msg_t **msg, dp_header_t *header);

  /**
   * @brief Set receive timeout for a Subscriber socket.
   * @param ctx        Subscriber context.
   * @param timeout_ms Timeout in milliseconds (-1 = infinite, 0 =
   * non-blocking).
   */
  void dp_sub_set_timeout (dp_sub *ctx, int timeout_ms);

  /**
   * @brief Destroy a Subscriber context and release all resources.
   * @param ctx Subscriber context (may be NULL).
   */
  void dp_sub_destroy (dp_sub *ctx);

  /** @} */ /* end group pubsub */

  /* -------------------------------------------------------------------------
   * Push / Pull  (PUSH/PULL — pipeline / load-balanced)
   * ---------------------------------------------------------------------- */

  /** @defgroup pipeline PUSH/PULL — pipeline
   *  @{
   *
   * Push sockets distribute work across all connected Pull workers in a
   * round-robin fashion.  Unlike PUB/SUB, each frame is delivered to exactly
   * one Pull consumer.
   */

  /**
   * @brief Create a Push socket and bind to @p endpoint.
   *
   * @param endpoint    ZMQ endpoint to bind, e.g. `"tcp://\*:5556"`.
   * @param sample_type Sample format that will be sent.
   * @return Non-NULL context on success, NULL on failure.
   */
  dp_push *dp_push_create (const char *endpoint, dp_sample_type_t sample_type);

  /**
   * @brief Create a Pull socket and connect to @p endpoint.
   * @param endpoint ZMQ endpoint to connect to, e.g. `"tcp://localhost:5556"`.
   * @return Non-NULL context on success, NULL on failure.
   */
  dp_pull *dp_pull_create (const char *endpoint);

  /**
   * @brief Send CI32 samples via a Push socket.
   * @copydetails dp_pub_send_ci32
   */
  int dp_push_send_ci32 (dp_push *ctx, const dp_ci32_t *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  /**
   * @brief Send CF64 samples via a Push socket.
   * @copydetails dp_pub_send_ci32
   */
  int dp_push_send_cf64 (dp_push *ctx, const dp_cf64_t *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  /**
   * @brief Send CF128 samples via a Push socket.
   * @copydetails dp_pub_send_ci32
   */
  int dp_push_send_cf128 (dp_push *ctx, const dp_cf128_t *samples,
                          size_t num_samples, double sample_rate,
                          double center_freq);

  /** @brief Send CI8 samples via a Push socket.
   *  @copydetails dp_pub_send_ci32 */
  int dp_push_send_ci8 (dp_push *ctx, const dp_ci8_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  /** @brief Send CI16 samples via a Push socket.
   *  @copydetails dp_pub_send_ci32 */
  int dp_push_send_ci16 (dp_push *ctx, const dp_ci16_t *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  /** @brief Send CF32 samples via a Push socket.
   *  @copydetails dp_pub_send_ci32 */
  int dp_push_send_cf32 (dp_push *ctx, const dp_cf32_t *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  /**
   * @brief Receive one frame from a Pull socket (zero-copy).
   * @copydetails dp_sub_recv
   */
  int dp_pull_recv (dp_pull *ctx, dp_msg_t **msg, dp_header_t *header);

  /**
   * @brief Set receive timeout for a Pull socket.
   * @param ctx        Pull context.
   * @param timeout_ms Timeout in milliseconds (-1 = infinite, 0 =
   * non-blocking).
   */
  void dp_pull_set_timeout (dp_pull *ctx, int timeout_ms);

  /**
   * @brief Destroy a Push context and release all resources.
   * @param ctx Push context (may be NULL).
   */
  void dp_push_destroy (dp_push *ctx);

  /**
   * @brief Destroy a Pull context and release all resources.
   * @param ctx Pull context (may be NULL).
   */
  void dp_pull_destroy (dp_pull *ctx);

  /** @} */ /* end group pipeline */

  /* -------------------------------------------------------------------------
   * Request / Reply  (REQ/REP — control and metadata)
   * ---------------------------------------------------------------------- */

  /** @defgroup reqrep REQ/REP — request/reply
   *  @{
   *
   * Strict synchronous request/reply.  The Requester sends a message and
   * must call recv before sending again.  Useful for control plane messages
   * (e.g. tuning commands, metadata queries) and signal-frame RPC.
   */

  /**
   * @brief Create a Requester socket and connect to @p endpoint.
   * @param endpoint ZMQ endpoint to connect to, e.g. `"tcp://localhost:5557"`.
   * @return Non-NULL context on success, NULL on failure.
   */
  dp_req *dp_req_create (const char *endpoint);

  /**
   * @brief Create a Replier socket and bind to @p endpoint.
   * @param endpoint ZMQ endpoint to bind, e.g. `"tcp://\*:5557"`.
   * @return Non-NULL context on success, NULL on failure.
   */
  dp_rep *dp_rep_create (const char *endpoint);

  /* -- Raw-bytes send/recv (control plane) ------------------------------ */

  /**
   * @brief Send raw bytes as a request.
   * @param ctx  Requester context.
   * @param data Pointer to payload bytes.
   * @param size Byte count.
   * @return DP_OK on success.
   */
  int dp_req_send (dp_req *ctx, const void *data, size_t size);

  /**
   * @brief Receive the reply to a previously sent request (zero-copy).
   * @param ctx       Requester context.
   * @param[out] msg  Set to a zero-copy message handle.
   * @param[out] size Set to the reply byte count.
   * @return DP_OK on success.
   */
  int dp_req_recv (dp_req *ctx, dp_msg_t **msg, size_t *size);

  /**
   * @brief Block until an incoming request arrives on the Replier (zero-copy).
   * @param ctx       Replier context.
   * @param[out] msg  Set to a zero-copy message handle.
   * @param[out] size Set to the request byte count.
   * @return DP_OK on success.
   */
  int dp_rep_recv (dp_rep *ctx, dp_msg_t **msg, size_t *size);

  /**
   * @brief Send the reply to the most recent request.
   * @param ctx  Replier context.
   * @param data Pointer to reply payload bytes.
   * @param size Byte count.
   * @return DP_OK on success.
   */
  int dp_rep_send (dp_rep *ctx, const void *data, size_t size);

  /* -- Signal-frame send/recv (data plane) ------------------------------ */

  /** @brief Send CI32 signal frame as a request. */
  int dp_req_send_ci32 (dp_req *ctx, const dp_ci32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  /** @brief Send CF64 signal frame as a request. */
  int dp_req_send_cf64 (dp_req *ctx, const dp_cf64_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  /** @brief Send CF128 signal frame as a request. */
  int dp_req_send_cf128 (dp_req *ctx, const dp_cf128_t *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);
  /** @brief Send CI8 signal frame as a request. */
  int dp_req_send_ci8 (dp_req *ctx, const dp_ci8_t *samples,
                       size_t num_samples, double sample_rate,
                       double center_freq);
  /** @brief Send CI16 signal frame as a request. */
  int dp_req_send_ci16 (dp_req *ctx, const dp_ci16_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  /** @brief Send CF32 signal frame as a request. */
  int dp_req_send_cf32 (dp_req *ctx, const dp_cf32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  /** @brief Send CI32 signal frame as a reply. */
  int dp_rep_send_ci32 (dp_rep *ctx, const dp_ci32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  /** @brief Send CF64 signal frame as a reply. */
  int dp_rep_send_cf64 (dp_rep *ctx, const dp_cf64_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  /** @brief Send CF128 signal frame as a reply. */
  int dp_rep_send_cf128 (dp_rep *ctx, const dp_cf128_t *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);
  /** @brief Send CI8 signal frame as a reply. */
  int dp_rep_send_ci8 (dp_rep *ctx, const dp_ci8_t *samples,
                       size_t num_samples, double sample_rate,
                       double center_freq);
  /** @brief Send CI16 signal frame as a reply. */
  int dp_rep_send_ci16 (dp_rep *ctx, const dp_ci16_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  /** @brief Send CF32 signal frame as a reply. */
  int dp_rep_send_cf32 (dp_rep *ctx, const dp_cf32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  /**
   * @brief Receive a signal frame reply (zero-copy).
   * @param ctx         Requester context.
   * @param[out] msg    Set to a zero-copy message handle.
   * @param[out] header Set to the frame metadata.
   * @return DP_OK on success, DP_ERR_TIMEOUT on timeout, negative on error.
   */
  int dp_req_recv_signal (dp_req *ctx, dp_msg_t **msg, dp_header_t *header);

  /**
   * @brief Receive a signal frame request (zero-copy).
   * @param ctx         Replier context.
   * @param[out] msg    Set to a zero-copy message handle.
   * @param[out] header Set to the frame metadata.
   * @return DP_OK on success, DP_ERR_TIMEOUT on timeout, negative on error.
   */
  int dp_rep_recv_signal (dp_rep *ctx, dp_msg_t **msg, dp_header_t *header);

  /**
   * @brief Set receive timeout for a Requester socket.
   * @param ctx        Requester context.
   * @param timeout_ms Timeout in milliseconds (-1 = infinite).
   */
  void dp_req_set_timeout (dp_req *ctx, int timeout_ms);

  /**
   * @brief Set receive timeout for a Replier socket.
   * @param ctx        Replier context.
   * @param timeout_ms Timeout in milliseconds (-1 = infinite).
   */
  void dp_rep_set_timeout (dp_rep *ctx, int timeout_ms);

  /**
   * @brief Destroy a Requester context and release all resources.
   * @param ctx Requester context (may be NULL).
   */
  void dp_req_destroy (dp_req *ctx);

  /**
   * @brief Destroy a Replier context and release all resources.
   * @param ctx Replier context (may be NULL).
   */
  void dp_rep_destroy (dp_rep *ctx);

  /** @} */ /* end group reqrep */

  /* -------------------------------------------------------------------------
   * Utilities
   * ---------------------------------------------------------------------- */

  /** @defgroup utils Utilities
   *  @{ */

  /**
   * @brief Return a short string name for @p type
   *        ("CI8", "CI16", "CI32", "CF32", "CF64", "CF128").
   * @param type Sample type enum value.
   * @return Statically allocated, null-terminated string.
   */
  const char *dp_sample_type_str (dp_sample_type_t type);

  /**
   * @brief Return the byte size of one complex sample for @p type.
   * @param type Sample type enum value.
   * @return Byte count (e.g. 2 for CI8, 4 for CI16, 8 for CI32/CF32,
   *         16 for CF64, 32 for CF128).
   */
  size_t dp_sample_size (dp_sample_type_t type);

  /**
   * @brief Return the current wall-clock time as nanoseconds since the UNIX
   * epoch.
   *
   * Uses CLOCK_REALTIME.  Useful for timestamping samples before calling a
   * send function, or for measuring round-trip latency.
   *
   * @return Nanoseconds since epoch.
   */
  uint64_t dp_get_timestamp_ns (void);

  /**
   * @brief Return a human-readable description of an error code.
   * @param err Negative error code returned by any dp_* function.
   * @return Statically allocated, null-terminated string.
   */
  const char *dp_strerror (int err);

  /** @} */ /* end group utils */

#ifdef __cplusplus
}
#endif

#endif /* DP_STREAM_H */
