/**
 * @file stream.h
 * @brief Streaming API for doppler — PUB/SUB, PUSH/PULL, REQ/REP.
 *
 * Provides NATS-backed signal streaming using three messaging patterns:
 *
 * | Pattern   | Sender function | Receiver function | Use case              |
 * |-----------|-----------------|-------------------|-----------------------|
 * | PUB/SUB   | dp_pub_*        | dp_sub_*          | Fan-out broadcast     |
 * | PUSH/PULL | dp_push_*       | dp_pull_*         | Pipeline load-balance |
 * | REQ/REP   | dp_req_*        | dp_rep_*          | Control metadata      |
 *
 * Requires a running `nats-server` (`nats-server -js` for the PUSH/PULL
 * JetStream work-queue tier). An endpoint is `"nats://host:port[/subject]"`;
 * the subject defaults to `"default"` if omitted.
 *
 * ### Quick start (C)
 * ```c
 * #include "stream/stream.h"
 *
 * // Transmitter
 * dp_pub_t *pub = dp_pub_create("nats://127.0.0.1:4222/iq", CF64);
 * double _Complex samples[1024] = { ... };
 * dp_pub_send_cf64(pub, samples, 1024, 1e6, 2.4e9);
 * dp_pub_destroy(pub);
 *
 * // Receiver (zero-copy)
 * dp_sub_t *sub = dp_sub_create("nats://127.0.0.1:4222/iq");
 * dp_msg_t *msg;  dp_header_t hdr;
 * dp_sub_recv(sub, &msg, &hdr);
 * double _Complex *cf64 = (double _Complex *)dp_msg_data(msg);
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

/* CMPLXF/CMPLX/CMPLXL fallbacks and the shared DP_OK/DP_ERR_* error codes
 * live in clib_common.h — the streaming API uses the one doppler-wide scheme.
 */
#include "clib_common.h"
#include "dp_interrupt.h"
#include "dp_format.h"

/**
 * @defgroup streaming Streaming
 * @brief NATS-backed signal streaming: PUB/SUB, PUSH/PULL, REQ/REP.
 */

#ifdef __cplusplus
extern "C"
{
#endif

  /** @defgroup version Version
   *  @ingroup streaming
   *  @{ */

#define DP_VERSION_MAJOR 2 /**< Major version number. */
#define DP_VERSION_MINOR 0 /**< Minor version number. */
#define DP_VERSION_PATCH 0 /**< Patch version number. */

  /** @} */

  /* -------------------------------------------------------------------------
   * Sample types
   * ---------------------------------------------------------------------- */

  /** @defgroup types Types
   *  @ingroup streaming
   *  @{
   */

  /* The sample formats themselves live in dp_format.h: they are BLUE's
     vocabulary, shared with the file writer, and a container is the wrong
     owner for the names of what it carries. */

  /**
   * @brief What a frame's payload IS, independent of how its elements are
   * encoded.
   *
   * `TLM16` used to be a sixth `dp_sample_type_t`, which made every I/Q-only
   * sender carry an exception for it and left the format field holding a
   * value BLUE does not define. Telemetry is not a sample encoding, it is a
   * different kind of frame, so it says so here and the format field stays
   * purely a BLUE sample code.
   */
  typedef enum
  {
    DP_KIND_IQ  = 0, /**< Interleaved complex samples; `format` is the code. */
    DP_KIND_TLM = 1, /**< Packed 16-byte `dp_tlm_rec_t` records; `format` is 0
                        and `num_samples` counts records. Published by the
                        `dp_tlm_sink_*` helper (stream/tlm_sink.h). */
    DP_KIND_EOS = 2, /**< End of stream: the sender has finished. Carries no
                        payload (`num_samples` is 0 and `format` is 0), so it
                        is a statement rather than data. A receiver reports it
                        as @ref DP_ERR_EOF instead of handing back an empty
                        frame.

                        A KIND rather than a flag bit, and that is forced by
                        §3's own rules: a receiver refuses any `flags` bit
                        outside `DP_FLAG_KNOWN`, because an unknown block
                        moves where the payload starts -- so a new flag would
                        be rejected by every existing receiver, while a new
                        kind is additive. Same precedent as TLM, which stopped
                        pretending to be a sample format for the same reason.

                        **Not reliable on PUB/SUB.** That tier is at-most-once
                        (§9), so this frame can be dropped like any other. It
                        turns the common case from "wait forever" into "finish
                        promptly"; a subscriber that must not hang on a lost
                        marker still needs a timeout. On PUSH/PULL it is
                        at-least-once, so it may arrive more than once and
                        handling it must be idempotent. */
  } dp_frame_kind_t;

  /**
   * @defgroup sampletypes Sample C types
   *
   * Floating-point complex types use C99 @c <complex.h>:
   *
   * | Wire type | C type               | bytes/sample |
   * |-----------|----------------------|--------------|
   * | CF32   | @c float _Complex    | 8            |
   * | CF64   | @c double _Complex   | 16           |
   *
   * Integer complex types have no C99 equivalent.  They are represented
   * as interleaved I/Q arrays where each complex sample occupies two
   * consecutive elements of the underlying integer type:
   *
   * | Wire type | C element type | bytes/sample | array length |
   * |-----------|----------------|--------------|--------------|
   * | CI8    | @c int8_t      | 2            | 2×n          |
   * | CI16   | @c int16_t     | 4            | 2×n          |
   * | CI32   | @c int32_t     | 8            | 2×n          |
   *
   * For @p n complex samples the send functions accept a pointer to
   * @c 2*n elements of the integer type (element 2k = I, 2k+1 = Q).
   *
   * @{
   */
  /** @} */ /* end group sampletypes */

  /** @defgroup wire Wire constants
   *  @ingroup streaming
   *  @{ */

  /** Magic: the eight ASCII bytes `DPSTREAM`, as one @c uint64_t.
   *
   *  An integer rather than an eight-character array on purpose: the header is written
   *  in host byte order with no conversion, so a peer of the opposite
   *  endianness reads this field byte-swapped and it no longer matches. The
   *  magic is therefore the endianness probe as well as the format tag, and
   *  costs nothing to be both. (An array of characters would read identically either
   *  way and detect nothing.) */
#define DP_STREAM_MAGIC 0x4D41455254535044ULL /* "DPSTREAM" little-endian */

  /** Wire revision. A receiver rejects a frame whose major differs: within a
   *  major, changes are additive and announced by an unrecognised flag bit,
   *  which is also rejected (see @ref DP_FLAG_KNOWN). */
#define DP_WIRE_VERSION 2u

  /** Byte-order tag, BLUE's own token (HCB `data_rep`): IEEE
   *  little-endian. Written as four ASCII characters so a hex dump says
   *  which order the numbers are in without decoding anything else. */
#define DP_REP_LE "EEEI"
  /** Byte-order tag: IEEE big-endian. */
#define DP_REP_BE "IEEE"

  /** Frame flags. */
#define DP_FLAG_CHUNKED                                                       \
  0x0001u /**< A 24-byte chunk block follows the header; see §4 of           \
               docs/design/streaming.md. */

  /** Every flag bit this build understands.
   *
   *  A receiver REJECTS a frame carrying a bit outside this mask rather than
   *  guessing. That is what makes a later additive change safe: a frame with
   *  a new optional block cannot be mistaken for one without it, because the
   *  block changes where the payload starts. */
#define DP_FLAG_KNOWN (DP_FLAG_CHUNKED)

  /** @} */ /* end group wire */

  /**
   * @brief Frame metadata carried in every stream message.
   *
   * 64 bytes, in declaration order, memcpy'd whole -- there is no padding
   * on any ABI doppler builds for, and a static assertion in
   * `stream_core.c` fails the build if that ever stops being true.
   *
   * Numbers are in HOST byte order and @ref data_rep says which order that
   * was; the magic catches the mismatch first, so a wrong-endian frame is
   * rejected rather than silently misread.
   */
  typedef struct
  {
    uint64_t magic;         /**< @ref DP_STREAM_MAGIC. */
    char     data_rep[4];   /**< @ref DP_REP_LE or @ref DP_REP_BE, no NUL. */
    uint16_t format;        /**< BLUE code (dp_sample_type_t); 0 when the
                                 kind is not sample data. */
    uint16_t kind;          /**< dp_frame_kind_t: what the payload IS. */
    uint16_t version;       /**< @ref DP_WIRE_VERSION. */
    uint16_t flags;         /**< Bitwise OR of the DP_FLAG_* set. */
    uint32_t payload_bytes; /**< Bytes of payload in THIS message. The
                                 transport also knows the message length;
                                 a receiver requires the two to agree,
                                 which is what stops a header claiming
                                 more samples than were sent. */
    uint64_t sequence;      /**< Per-socket frame counter, from 0. A chunked
                                 frame consumes one number, not one per
                                 chunk. */
    uint64_t timestamp_ns;  /**< UNIX nanoseconds (CLOCK_REALTIME), or 0 for
                                 "no capture time" -- the same unset
                                 convention wfm_time.h uses, so a frame that
                                 never had one does not claim 1970. */
    double   sample_rate;   /**< Sample rate in Hz, 0 if unknown. */
    double   center_freq;   /**< Centre frequency in Hz, 0 if unknown. */
    uint64_t num_samples;   /**< Complex samples (DP_KIND_IQ) or records
                                 (DP_KIND_TLM) in THIS message. */
  } dp_header_t;

  /**
   * @brief Reassembly geometry, present only when @ref DP_FLAG_CHUNKED.
   *
   * Immediately follows the header and precedes the chunk's own payload
   * bytes. It rides only chunked frames rather than sitting in every
   * header: the previous format spent a third of its 96 bytes on four
   * `reserved[]` words that were documented as "do not interpret" and were
   * in fact this, zeroed on every unchunked frame.
   */
  typedef struct
  {
    uint32_t index;       /**< 0-based chunk number. */
    uint32_t count;       /**< Chunks in this frame. */
    uint64_t total_bytes; /**< Payload bytes in the whole logical frame. */
    uint64_t offset;      /**< This chunk's byte offset into that payload. */
  } dp_chunk_t;

  /** @brief Opaque zero-copy message handle returned by recv functions.
   *
   * The data buffer is valid until dp_msg_free() is called. Use the accessor
   * functions to retrieve a pointer to the sample data, size, etc.
   */
  typedef struct dp_msg dp_msg_t;

  /** @brief Opaque streaming socket handle returned by all create functions.
   */
  typedef struct dp_ctx dp_pub_t;
  typedef struct dp_ctx dp_sub_t;
  typedef struct dp_ctx dp_push_t;
  typedef struct dp_ctx dp_pull_t;
  typedef struct dp_ctx dp_req_t;
  typedef struct dp_ctx dp_rep_t;

  /** @} */ /* end group types */

  /* -------------------------------------------------------------------------
   * Error codes
   * ---------------------------------------------------------------------- */

  /** @defgroup errors Error codes
   *  @{
   *
   * Every send/recv function returns one of the shared doppler error codes
   * (DP_OK, DP_ERR_INIT, DP_ERR_SEND, DP_ERR_RECV, DP_ERR_INVALID,
   * DP_ERR_TIMEOUT, DP_ERR_MEMORY) defined in clib_common.h, included above.
   * Use dp_strerror() to obtain a human-readable description.
   */
  /** @} */

  /* -------------------------------------------------------------------------
   * dp_msg_t — zero-copy message accessors
   * ---------------------------------------------------------------------- */

  /** @defgroup msg Message handle
   *  @ingroup streaming
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
   * @brief Mean power of a complex sample block, normalised to full scale.
   *
   * `mean(|x|^2)`, with the integer formats divided by
   * dp_format_full_scale() first so the answer means the same thing
   * whatever the format is — `10*log10()` of it is dBFS in every case.
   * The frame-level dp_msg_mean_power() is this over a received message.
   *
   * @param format Sample format of @p data.
   * @param data   @p n complex samples, interleaved for an integer format.
   * @param n      Sample count.
   * @return Mean power, or 0 for a NULL pointer, an empty block, or a
   *         format this build does not know.
   */
  double dp_mean_power (dp_sample_type_t format, const void *data, size_t n);

  /**
   * @brief Mean power of a received I/Q frame, normalised to full scale.
   *
   * dp_mean_power() over the frame's own samples: the format and the count
   * come from its header, so a subscriber that takes whatever arrives can
   * compare frames without branching on the type. `10*log10()` of it is
   * dBFS.
   *
   * Exists because every consumer was writing this loop: the C receiver
   * example carried one copy per wire type and a switch to pick between
   * them, which is the same duplication the library forbids internally.
   *
   * @param msg Message handle from a recv.
   * @return Mean power, or 0 for a NULL handle, an empty frame, or a
   *         non-I/Q kind (telemetry records are not samples).
   */
  double dp_msg_mean_power (dp_msg_t *msg);

  /**
   * @brief What the message's payload IS (dp_frame_kind_t).
   *
   * Ask this before dp_msg_sample_type(): a telemetry frame's format field
   * is 0, because BLUE has no code for a record stream.
   *
   * @param msg Message handle.
   * @return The frame's kind, or DP_KIND_IQ for a NULL handle.
   */
  dp_frame_kind_t dp_msg_kind (dp_msg_t *msg);

  /**
   * @brief Acknowledge a message on a durable (JetStream) consumer.
   *
   * For the resilient NATS work-queue tier (a `nats://` Pull consumer),
   * delivery is at-least-once: a message stays pending until acked, and is
   * redelivered if the consumer dies before acking.  Call this once the
   * message has been fully processed, then dp_msg_free().
   *
   * A no-op (returns DP_OK) for transports without acks — NATS core
   * PUB/SUB and reassembled chunked frames — so callers can ack
   * unconditionally.
   *
   * @param msg Message handle returned by a recv function.
   * @return DP_OK on success, negative error code on failure.
   */
  int dp_msg_ack (dp_msg_t *msg);

  /**
   * @brief Free a message handle and release the underlying buffer.
   * @param msg Message handle (may be NULL).
   */
  void dp_msg_free (dp_msg_t *msg);

  /** @} */ /* end group msg */

  /* -------------------------------------------------------------------------
   * Publisher / Subscriber  (PUB/SUB — fan-out broadcast)
   * ---------------------------------------------------------------------- */

  /** @defgroup pubsub PUB/SUB — fan-out broadcast
   *  @ingroup streaming
   *  @{
   *
   * The Publisher publishes to a subject and fans out every message to all
   * connected Subscribers.  Subscribers subscribe and receive every frame
   * published after they connect — a slow or absent subscriber simply
   * misses frames (core NATS PUB/SUB has no queuing/replay).
   */

  /**
   * @brief Create a Publisher and connect to @p endpoint.
   *
   * @param endpoint  NATS endpoint, e.g. `"nats://127.0.0.1:4222/iq"`.
   * @param sample_type  Sample format that will be sent.
   * @return Non-NULL context on success, NULL on failure.
   */
  dp_pub_t *dp_pub_create (const char *endpoint, dp_sample_type_t sample_type);

  /**
   * @brief Send an array of CI32 samples via a Publisher.
   *
   * @param ctx         Publisher context.
   * @param samples     Interleaved int32_t I/Q pairs; length 2×num_samples.
   * @param num_samples Number of complex samples.
   * @param sample_rate Sample rate in Hz.
   * @param center_freq Centre frequency in Hz.
   * @return DP_OK (0) on success, negative error code on failure.
   */
  int dp_pub_send_ci32 (dp_pub_t *ctx, const int32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  /**
   * @brief Send an array of CF64 samples via a Publisher.
   * @copydetails dp_pub_send_ci32
   */
  int dp_pub_send_cf64 (dp_pub_t *ctx, const double _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  /**
   * @brief Send an array of CI8 samples via a Publisher.
   * @copydetails dp_pub_send_ci32
   */
  int dp_pub_send_ci8 (dp_pub_t *ctx, const int8_t *samples,
                       size_t num_samples, double sample_rate,
                       double center_freq);

  /**
   * @brief Send an array of CI16 samples via a Publisher.
   * @copydetails dp_pub_send_ci32
   */
  int dp_pub_send_ci16 (dp_pub_t *ctx, const int16_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  /**
   * @brief Send an array of CF32 samples via a Publisher.
   * @copydetails dp_pub_send_ci32
   */
  int dp_pub_send_cf32 (dp_pub_t *ctx, const float _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  /**
   * @brief Send an array of 16-byte telemetry records via a Publisher.
   *
   * The payload is @p num_records packed dp_tlm_rec_t (see
   * dp_tlm/dp_tlm_core.h) — the header's num_samples counts records and
   * sample_type is TLM16. Kept `const void *` so the wire layer stays
   * decoupled from the telemetry component; the dp_tlm_sink_* helper
   * (stream/tlm_sink.h) is the intended caller.
   *
   * @param ctx          Publisher context.
   * @param records      Packed 16-byte records.
   * @param num_records  Record count.
   * @param sample_rate  Wire-header field; 0.0 if not meaningful.
   * @param center_freq  Wire-header field; 0.0 if not meaningful.
   * @return DP_OK (0) on success, negative error code on failure.
   */
  int dp_pub_send_tlm16 (dp_pub_t *ctx, const void *records,
                         size_t num_records, double sample_rate,
                         double center_freq);

  /**
   * @brief Create a Publisher that emits telemetry frames.
   *
   * A separate constructor because @ref DP_KIND_TLM is a frame kind rather
   * than a sample format: there is no BLUE code to pass to dp_pub_create(),
   * and a publisher that emits records does not also emit I/Q. Send with
   * dp_pub_send_tlm16().
   *
   * @param endpoint `nats://host:port/subject`.
   * @return Publisher handle, or NULL on failure.
   */
  dp_pub_t *dp_pub_create_tlm (const char *endpoint);

  /**
   * @brief Wait until the server has everything published so far.
   *
   * `dp_pub_send_*` hands the frame to the client and returns; the client
   * writes it in the background. That is what makes publishing fast, and
   * it means "the send returned" is not "the server has it". This waits
   * for a round trip, so when it returns DP_OK everything published
   * before it has arrived.
   *
   * You do NOT need this before destroy: the NATS client flushes what is
   * buffered when the connection closes. But it does so best-effort with
   * a **500 ms cap and no way to report failure**, so a backlog that
   * cannot drain in half a second is dropped silently — and on a link
   * slower than loopback that is not a large backlog. Call this when
   * losing the tail would matter, and you get a budget you chose and an
   * answer you can act on.
   *
   * It is also the only way to ask the question WITHOUT closing: a
   * long-lived publisher that wants "everything up to here is on the
   * server" has nothing else to call. (PUSH does not need it — the
   * JetStream publish is server-acked before it returns — and REQ/REP
   * flush on every message already.)
   *
   * **A drained shutdown does not need one.** dp_stream_drain() ends with
   * this same flush as its final phase, so flush belongs at checkpoints
   * a drain does not cover — confirming a batch is on the server before
   * treating it as complete — not in front of a drain.
   *
   * @param ctx         Any send-capable context (dp_pub_t / dp_push_t /
   *                    dp_req_t / dp_rep_t are the same underlying type).
   * @param timeout_ms  How long to wait; <= 0 uses 2000 ms.
   * @return DP_OK when the server has it, @ref DP_ERR_TIMEOUT if the
   *         budget ran out with data still pending, DP_ERR_INVALID for a
   *         NULL context.
   */
  int dp_pub_flush (dp_pub_t *ctx, int timeout_ms);

  /**
   * @brief Tell subscribers the stream has ended.
   *
   * Publishes a zero-payload @ref DP_KIND_EOS frame. A receiving
   * `*_recv` reports @ref DP_ERR_EOF instead of handing back an empty
   * frame, so a consumer learns the sender finished rather than inferring
   * it from silence — which is the inference this whole contract exists
   * to remove.
   *
   * **Send it before dp_stream_drain(), not after.** A drain cannot be
   * reversed and refuses sends once it reaches its publish-flushing
   * phase, so an EOS issued after one may simply not go. The ordered
   * shutdown is: stop producing, send EOS, drain, destroy.
   *
   * **What it does NOT promise.** PUB/SUB is at-most-once (§9), so this
   * frame can be dropped like any other: it turns the common case from
   * "wait forever" into "finish promptly", not from unreliable into
   * guaranteed, and a subscriber that must not hang on a lost marker
   * still needs a timeout. PUSH/PULL delivers it at-least-once, so it may
   * arrive more than once and a handler must be idempotent.
   *
   * @param ctx Any send-capable context.
   * @return DP_OK once handed to the client, @ref DP_ERR_INVALID for a
   *         NULL context, @ref DP_ERR_CLOSED if the context is already
   *         draining or closed.
   */
  int dp_pub_send_eos (dp_pub_t *ctx);

  /**
   * @brief Shut a context down gracefully: drain, then closed.
   *
   * The ordered shutdown, and the one a signal handler's exit path wants.
   * The client stops accepting new deliveries, lets what is in flight
   * finish, flushes everything pending, and then closes.
   *
   * **It waits for the connection to reach CLOSED before returning**, and
   * that is the part worth having in the library rather than in every
   * caller: `natsConnection_Drain` returns immediately and does the work
   * in the background, so a process that exits when it returns abandons
   * exactly the work the drain was for. Getting that wrong looks like
   * success.
   *
   * Against dp_pub_flush(): flush answers "does the server have what I
   * published", and the context keeps working afterwards. Drain answers
   * "let everything finish, then stop", and the context is finished when
   * it returns — call the matching `*_destroy` next, which is then just
   * the free.
   *
   * **Drain last, after your application has stopped producing.** A drain
   * cannot be reversed, and a send issued while one is in progress is
   * racing its phases: it may slip through while subscriptions drain, or
   * be refused once the connection reaches its publish-flushing phase.
   * Do not publish a "shutting down" notice after calling this and assume
   * it went.
   *
   * Because this waits for CLOSED, a single-threaded caller does not have
   * to reason about that race: once it has returned, a send is refused
   * with @ref DP_ERR_CLOSED, deterministically. The race is real only for
   * a thread still publishing while another drains.
   *
   * Size @p timeout_ms to the slowest thing the drain has to wait for,
   * with margin: cutting a drain off mid-write every deploy is worse
   * than waiting. doppler's own receive is synchronous — there is no
   * message handler to finish — so the wait is dominated by flushing
   * whatever is still buffered, and the 5 s default is generous for a
   * link that is keeping up. A slow or congested link, or a large
   * backlog, wants more.
   *
   * @param ctx        Any context.
   * @param timeout_ms How long to wait for CLOSED; <= 0 uses 5000 ms.
   * @return DP_OK once closed, @ref DP_ERR_TIMEOUT if the budget ran out
   *         with the drain still in progress (the context is still safe
   *         to destroy), DP_ERR_INVALID for a NULL context.
   */
  int dp_stream_drain (dp_pub_t *ctx, int timeout_ms);

  /**
   * @brief Destroy a Publisher context and release all resources.
   * @param ctx Publisher context (may be NULL).
   */
  void dp_pub_destroy (dp_pub_t *ctx);

  /**
   * @brief Create a Subscriber and connect to @p endpoint.
   *
   * Subscribes to all topics (empty topic filter).
   *
   * @param endpoint  NATS endpoint, e.g. `"nats://127.0.0.1:4222/iq"`.
   * @return Non-NULL context on success, NULL on failure.
   */
  dp_sub_t *dp_sub_create (const char *endpoint);

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
   * @return DP_OK on success, @ref DP_ERR_TIMEOUT on timeout, @ref
   *         DP_ERR_EOF when the sender has finished (no message is
   *         produced, so there is nothing to free), negative on error.
   */
  int dp_sub_recv (dp_sub_t *ctx, dp_msg_t **msg, dp_header_t *header);

  /**
   * @brief Set receive timeout for a Subscriber socket.
   * @param ctx        Subscriber context.
   * @param timeout_ms Timeout in milliseconds (-1 = infinite, 0 =
   * non-blocking).
   */
  void dp_sub_set_timeout (dp_sub_t *ctx, int timeout_ms);

  /**
   * @brief Destroy a Subscriber context and release all resources.
   * @param ctx Subscriber context (may be NULL).
   */
  void dp_sub_destroy (dp_sub_t *ctx);

  /** @} */ /* end group pubsub */

  /* -------------------------------------------------------------------------
   * Push / Pull  (PUSH/PULL — pipeline / load-balanced)
   * ---------------------------------------------------------------------- */

  /** @defgroup pipeline PUSH/PULL — pipeline
   *  @ingroup streaming
   *  @{
   *
   * Push sockets distribute work across all connected Pull workers in a
   * round-robin fashion.  Unlike PUB/SUB, each frame is delivered to exactly
   * one Pull consumer.
   */

  /**
   * @brief Create a Push producer and connect to @p endpoint.
   *
   * @param endpoint    NATS endpoint, e.g. `"nats://127.0.0.1:4222/work"`.
   * @param sample_type Sample format that will be sent.
   * @return Non-NULL context on success, NULL on failure.
   *
   * @note On the NATS (`nats://`) work-queue tier the per-frame payload must
   * fit one message: `header + data <= server max_payload` (default 1 MiB).
   * Unlike PUB/SUB, PUSH does not chunk (the work-queue load-balances frames
   * across workers, which cannot reassemble a split frame), so an oversized
   * `dp_push_send_*` returns @ref DP_ERR_TOO_LARGE. Raise the broker
   * `max_payload` for larger durable frames, or use PUB/SUB (which chunks).
   */
  dp_push_t *dp_push_create (const char      *endpoint,
                             dp_sample_type_t sample_type);

  /**
   * @brief Create a Pull consumer and connect to @p endpoint.
   * @param endpoint NATS endpoint, e.g. `"nats://127.0.0.1:4222/work"`.
   * @return Non-NULL context on success, NULL on failure.
   */
  dp_pull_t *dp_pull_create (const char *endpoint);

  /**
   * @brief Send CI32 samples via a Push socket.
   * @copydetails dp_pub_send_ci32
   */
  int dp_push_send_ci32 (dp_push_t *ctx, const int32_t *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  /**
   * @brief Send CF64 samples via a Push socket.
   * @copydetails dp_pub_send_ci32
   */
  int dp_push_send_cf64 (dp_push_t *ctx, const double _Complex *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  /** @brief Send CI8 samples via a Push socket.
   *  @copydetails dp_pub_send_ci32 */
  int dp_push_send_ci8 (dp_push_t *ctx, const int8_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  /** @brief Send CI16 samples via a Push socket.
   *  @copydetails dp_pub_send_ci32 */
  int dp_push_send_ci16 (dp_push_t *ctx, const int16_t *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  /** @brief Send CF32 samples via a Push socket.
   *  @copydetails dp_pub_send_ci32 */
  int dp_push_send_cf32 (dp_push_t *ctx, const float _Complex *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  /**
   * @brief Receive one frame from a Pull socket (zero-copy).
   * @copydetails dp_sub_recv
   */
  int dp_pull_recv (dp_pull_t *ctx, dp_msg_t **msg, dp_header_t *header);

  /**
   * @brief Set receive timeout for a Pull socket.
   * @param ctx        Pull context.
   * @param timeout_ms Timeout in milliseconds (-1 = infinite, 0 =
   * non-blocking).
   */
  void dp_pull_set_timeout (dp_pull_t *ctx, int timeout_ms);

  /**
   * @brief Destroy a Push context and release all resources.
   * @param ctx Push context (may be NULL).
   */
  void dp_push_destroy (dp_push_t *ctx);

  /**
   * @brief Destroy a Pull context and release all resources.
   * @param ctx Pull context (may be NULL).
   */
  void dp_pull_destroy (dp_pull_t *ctx);

  /** @} */ /* end group pipeline */

  /* -------------------------------------------------------------------------
   * Request / Reply  (REQ/REP — control and metadata)
   * ---------------------------------------------------------------------- */

  /** @defgroup reqrep REQ/REP — request/reply
   *  @ingroup streaming
   *  @{
   *
   * Strict synchronous request/reply.  The Requester sends a message and
   * must call recv before sending again.  Useful for control plane messages
   * (e.g. tuning commands, metadata queries) and signal-frame RPC.
   */

  /**
   * @brief Create a Requester and connect to @p endpoint.
   * @param endpoint NATS endpoint, e.g. `"nats://127.0.0.1:4222/ctrl"`.
   * @return Non-NULL context on success, NULL on failure.
   */
  dp_req_t *dp_req_create (const char *endpoint);

  /**
   * @brief Create a Replier and connect to @p endpoint.
   * @param endpoint NATS endpoint, e.g. `"nats://127.0.0.1:4222/ctrl"`.
   * @return Non-NULL context on success, NULL on failure.
   */
  dp_rep_t *dp_rep_create (const char *endpoint);

  /* -- Raw-bytes send/recv (control plane) ------------------------------ */

  /**
   * @brief Send raw bytes as a request.
   * @param ctx  Requester context.
   * @param data Pointer to payload bytes.
   * @param size Byte count.
   * @return DP_OK on success.
   */
  int dp_req_send (dp_req_t *ctx, const void *data, size_t size);

  /**
   * @brief Receive the reply to a previously sent request (zero-copy).
   * @param ctx       Requester context.
   * @param[out] msg  Set to a zero-copy message handle.
   * @param[out] size Set to the reply byte count.
   * @return DP_OK on success.
   */
  int dp_req_recv (dp_req_t *ctx, dp_msg_t **msg, size_t *size);

  /**
   * @brief Block until an incoming request arrives on the Replier (zero-copy).
   * @param ctx       Replier context.
   * @param[out] msg  Set to a zero-copy message handle.
   * @param[out] size Set to the request byte count.
   * @return DP_OK on success.
   */
  int dp_rep_recv (dp_rep_t *ctx, dp_msg_t **msg, size_t *size);

  /**
   * @brief Send the reply to the most recent request.
   * @param ctx  Replier context.
   * @param data Pointer to reply payload bytes.
   * @param size Byte count.
   * @return DP_OK on success.
   */
  int dp_rep_send (dp_rep_t *ctx, const void *data, size_t size);

  /* -- Signal-frame send/recv (data plane) ------------------------------ */

  /** @brief Send CI32 signal frame as a request. */
  int dp_req_send_ci32 (dp_req_t *ctx, const int32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  /** @brief Send CF64 signal frame as a request. */
  int dp_req_send_cf64 (
      dp_req_t *ctx, const double _Complex *samples, size_t num_samples,
      double sample_rate,
      double center_freq); /** @brief Send CI8 signal frame as a request. */
  int dp_req_send_ci8 (dp_req_t *ctx, const int8_t *samples,
                       size_t num_samples, double sample_rate,
                       double center_freq);
  /** @brief Send CI16 signal frame as a request. */
  int dp_req_send_ci16 (dp_req_t *ctx, const int16_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  /** @brief Send CF32 signal frame as a request. */
  int dp_req_send_cf32 (dp_req_t *ctx, const float _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  /** @brief Send CI32 signal frame as a reply. */
  int dp_rep_send_ci32 (dp_rep_t *ctx, const int32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  /** @brief Send CF64 signal frame as a reply. */
  int dp_rep_send_cf64 (
      dp_rep_t *ctx, const double _Complex *samples, size_t num_samples,
      double sample_rate,
      double center_freq); /** @brief Send CI8 signal frame as a reply. */
  int dp_rep_send_ci8 (dp_rep_t *ctx, const int8_t *samples,
                       size_t num_samples, double sample_rate,
                       double center_freq);
  /** @brief Send CI16 signal frame as a reply. */
  int dp_rep_send_ci16 (dp_rep_t *ctx, const int16_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  /** @brief Send CF32 signal frame as a reply. */
  int dp_rep_send_cf32 (dp_rep_t *ctx, const float _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  /**
   * @brief Receive a signal frame reply (zero-copy).
   * @param ctx         Requester context.
   * @param[out] msg    Set to a zero-copy message handle.
   * @param[out] header Set to the frame metadata.
   * @return DP_OK on success, DP_ERR_TIMEOUT on timeout, negative on error.
   */
  int dp_req_recv_signal (dp_req_t *ctx, dp_msg_t **msg, dp_header_t *header);

  /**
   * @brief Receive a signal frame request (zero-copy).
   * @param ctx         Replier context.
   * @param[out] msg    Set to a zero-copy message handle.
   * @param[out] header Set to the frame metadata.
   * @return DP_OK on success, DP_ERR_TIMEOUT on timeout, negative on error.
   */
  int dp_rep_recv_signal (dp_rep_t *ctx, dp_msg_t **msg, dp_header_t *header);

  /**
   * @brief Set receive timeout for a Requester socket.
   * @param ctx        Requester context.
   * @param timeout_ms Timeout in milliseconds (-1 = infinite).
   */
  void dp_req_set_timeout (dp_req_t *ctx, int timeout_ms);

  /**
   * @brief Set receive timeout for a Replier socket.
   * @param ctx        Replier context.
   * @param timeout_ms Timeout in milliseconds (-1 = infinite).
   */
  void dp_rep_set_timeout (dp_rep_t *ctx, int timeout_ms);

  /**
   * @brief Destroy a Requester context and release all resources.
   * @param ctx Requester context (may be NULL).
   */
  void dp_req_destroy (dp_req_t *ctx);

  /**
   * @brief Destroy a Replier context and release all resources.
   * @param ctx Replier context (may be NULL).
   */
  void dp_rep_destroy (dp_rep_t *ctx);

  /** @} */ /* end group reqrep */

  /* -------------------------------------------------------------------------
   * Utilities
   * ---------------------------------------------------------------------- */

  /** @defgroup utils Utilities
   *  @ingroup streaming
   *  @{ */

  /**
   * @brief Return a short string name for @p type
   *        ("CI8", "CI16", "CI32", "CF32", "CF64").
   * @param type Sample type enum value.
   * @return Statically allocated, null-terminated string.
   */
  const char *dp_sample_type_str (dp_sample_type_t type);

  /**
   * @brief Return the byte size of one complex sample for @p type.
   * @param type Sample type enum value.
   * @return Byte count (e.g. 2 for CI8, 4 for CI16, 8 for CI32/CF32,
   *         16 for CF64).
   */
  size_t dp_sample_size (dp_sample_type_t type);

  /**
   * @brief True when @p type is a sample type this build knows.
   *
   * Derived from dp_sample_size(), so there is one table: a type with no
   * size is not a type. Ask this rather than range-testing the enum --
   * the values are append-only and a RETIRED one (2, the former CF128)
   * sits inside the range while being invalid, so `type <= CF32` accepts a
   * value nothing can send or decode.
   *
   * @param type Sample type enum value.
   * @return Non-zero when the type is known, 0 otherwise.
   */
  int dp_sample_type_is_valid (dp_sample_type_t type);

  /**
   * @brief Bytes per payload element for a frame of this kind.
   *
   * For @ref DP_KIND_IQ that is dp_sample_size() of @p format; for
   * @ref DP_KIND_TLM it is 16, one packed record, and @p format is not
   * consulted because a record stream has no BLUE code.
   *
   * @param kind   What the payload is (dp_frame_kind_t).
   * @param format Sample format, for an I/Q frame.
   * @return Bytes per element, or 0 when the pair is not something this
   *         build can send or decode.
   */
  size_t dp_element_size (dp_frame_kind_t kind, dp_sample_type_t format);

  /**
   * @brief This machine's byte-order tag: @ref DP_REP_LE or @ref DP_REP_BE.
   *
   * Four characters, not NUL-terminated. Derived at run time rather than
   * compiled in, so a big-endian build tags its frames honestly instead of
   * inheriting a constant nobody revisited.
   */
  const char *dp_host_rep (void);


  /** @defgroup interrupt Interrupting a blocking receive (DEPRECATED)
   *
   * @deprecated These are the `dp_stream_*` spellings of a primitive that
   * is not specific to streaming. It moved to `dp_interrupt.h` in the core
   * library so a build with no NATS can use it; use `dp_interrupt()`,
   * `dp_interrupted()`, `dp_resume()`, `dp_interrupt_on_signal()` and
   * `dp_restore_signal()` instead. These forward verbatim and are removed
   * once their callers migrate. See `docs/design/io-termination.md`.
   *  @ingroup streaming
   *  @{
   *
   * A blocking `*_recv` waits inside the NATS client, and a flag your
   * signal handler sets is read by your loop — which the blocking call is
   * keeping you out of. With traffic arriving that is invisible, because
   * every frame returns control to you; the moment a sender stops, Ctrl+C
   * stops working. That is not hypothetical: it shipped, in doppler's own
   * C receiver example.
   *
   * A bounded `*_set_timeout` is one answer, and the examples relied on
   * it, but it makes every caller trade latency against responsiveness and
   * get it wrong quietly. This is the other: the library checks a flag of
   * its own inside the wait, so a blocking receive stays blocking and
   * still returns when you ask it to.
   */

  /**
   * @brief Ask every blocking receive in this process to return now.
   *
   * Async-signal-safe by construction — it assigns to a
   * `volatile sig_atomic_t` and does nothing else — so the intended caller
   * is a signal handler:
   *
   * @code
   * static void on_sigint (int sig)
   * {
   *   (void)sig;
   *   dp_stream_interrupt ();
   * }
   * @endcode
   *
   * Every receive already blocked returns @ref DP_ERR_INTERRUPTED within
   * one internal wait slice (100 ms), and so does every one STARTED while
   * the flag is set — a receive cannot be missed by racing the signal.
   * The flag is process-wide and sticky; dp_stream_resume() clears it.
   */
  void dp_stream_interrupt (void);

  /**
   * @brief Default interrupt latency, in milliseconds.
   *
   * Ten wakeups a second on an idle receiver, and a delay no human
   * perceives when they press Ctrl+C. It is a default rather than a
   * constant of the design: see dp_stream_set_interrupt_latency_ms().
   */
/* Defined by dp_interrupt.h, which owns the primitive. */

  /**
   * @brief How soon a blocking receive must notice an interrupt.
   *
   * The library cannot be woken from the NATS client's wait, so it waits
   * in slices and checks the flag between them. This is the size of that
   * slice, expressed as the thing a caller actually cares about — the
   * worst-case delay between dp_stream_interrupt() and the receive
   * returning — rather than as an implementation detail.
   *
   * It is a knob because the right answer is not the library's to know.
   * A human pressing Ctrl+C cannot perceive 100 ms; a control loop that
   * must hand back within one symbol period can, and a battery-powered
   * sensor would rather wake once a second than ten times. The cost is
   * one wakeup per slice on an otherwise idle receiver.
   *
   * Process-wide, like the flag it serves. Takes effect on the next wait
   * slice, so a receive already blocked adopts it within one old slice.
   *
   * @param ms Milliseconds; 0 selects @ref DP_INTERRUPT_LATENCY_DEFAULT_MS.
   */
  void dp_stream_set_interrupt_latency_ms (unsigned ms);

  /**
   * @brief The interrupt latency in force.
   *
   * @return Milliseconds.
   */
  unsigned dp_stream_interrupt_latency_ms (void);

  /**
   * @brief Clear the interrupt, so blocking receives block again.
   *
   * The flag is sticky on purpose: a handler fires once and the loops it
   * unblocks may be several, so an auto-clearing flag would release one
   * caller and leave the rest parked.
   */
  void dp_stream_resume (void);

  /**
   * @brief Non-zero when an interrupt is pending.
   *
   * For a loop that wants to notice without calling recv again, and for a
   * caller that keeps its own flag and wants one source of truth.
   *
   * @return Non-zero when interrupted, 0 otherwise.
   */
  int dp_stream_interrupted (void);

  /**
   * @brief Install a handler for @p sig that calls dp_stream_interrupt().
   *
   * The handler is installed in C, and that is the whole point rather than
   * a convenience. A handler written in a higher-level language runs when
   * its interpreter next regains control, which is precisely what a
   * blocking receive is preventing -- the flag would be set only after the
   * wait it is meant to end. Measured, not reasoned: a Python
   * `signal.signal` handler calling the interrupt left a blocked `recv()`
   * blocked forever.
   *
   * Whatever handler was installed is **chained, not replaced**: it runs
   * immediately after the flag is set. Without that, a signal arriving
   * while the program is not inside a receive would set a flag nobody
   * reads and otherwise do nothing -- fixing the blocking case by breaking
   * the ordinary one. For an embedding interpreter this is what keeps its
   * own Ctrl+C behaviour intact.
   *
   * @param sig Signal number, e.g. `SIGINT`.
   * @return DP_OK, or DP_ERR_INVALID for a signal that cannot be caught.
   */
  int dp_stream_interrupt_on_signal (int sig);

  /**
   * @brief Restore the handler that was in place before.
   *
   * @param sig Signal number previously passed to
   *            dp_stream_interrupt_on_signal().
   * @return DP_OK, or DP_ERR_INVALID if that signal was never installed.
   */
  int dp_stream_restore_signal (int sig);

  /** @} */ /* end group interrupt */

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
   * @brief Override the @c timestamp_ns the NEXT send on @p ctx will stamp,
   * instead of a fresh dp_get_timestamp_ns() read.
   *
   * One-shot: consumed (and cleared) by the very next send call on this
   * context, whether or not it was actually used. Lets a hop that already
   * knows a more precise or truer origin time (e.g. a value derived from
   * dp_sample_clock_stamp_at() over an upstream message's own header, or a
   * passthrough of that upstream header's own @c timestamp_ns) propagate it
   * downstream instead of every hop silently re-stamping "now" and losing
   * the connection to when the samples actually occurred. @p ctx accepts
   * any socket role (dp_pub_t / dp_push_t / dp_req_t / dp_rep_t are the
   * same underlying context type).
   *
   * @param ctx          Allocated send-capable context (any role).
   * @param timestamp_ns Nanoseconds since the UNIX epoch to stamp on the
   *                     next send.
   */
  void dp_ctx_set_timestamp_ns (dp_pub_t *ctx, uint64_t timestamp_ns);

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
