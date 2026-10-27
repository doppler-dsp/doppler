/**
 * @file stream_internal.h
 * @brief Private definitions for the NATS-backed stream context.
 *
 * Not a public header — it carries the context/message structs and the
 * cross-TU hooks that stream_core.c (public API + message framing) and
 * stream_nats.c (transport implementation) both need.  nats.h is included
 * only by stream_nats.c — never leaks into the public API layer.
 */

#ifndef DP_STREAM_INTERNAL_H
#define DP_STREAM_INTERNAL_H

#include "doppler/stream/stream.h"

/* The wire constants -- magic, version, the flag set and the chunk block --
 * are PUBLIC (stream/stream.h). They were private here while the public
 * header described `flags` as "set to 0" and `reserved[]` as "do not
 * interpret", which meant the format could not be implemented from the
 * header doppler publishes. */

/** @brief Messaging role.  The NATS backend maps these onto pub/sub
 * subjects, a JetStream work-queue, or request/reply, as appropriate. */
typedef enum
{
  DP_ROLE_PUB = 0, /**< Publisher: core-NATS fan-out; may chunk frames.  */
  DP_ROLE_SUB,     /**< Subscriber: receives every type on its base.    */
  DP_ROLE_PUSH,    /**< Work-queue producer (JetStream publish).        */
  DP_ROLE_PULL,    /**< Work-queue consumer (durable, explicit ack).    */
  DP_ROLE_REQ,     /**< Requester: request/reply over an inbox.         */
  DP_ROLE_REP      /**< Replier: answers the last request received.     */
} dp_role_t;

/** @brief Reassembly of a chunked frame from its chunks, over parsed chunks
 * rather than a transport -- the receive side's other rule, beside
 * dp_frame_parse(), and testable the same way (test_stream_wire.c).
 *
 * ONE frame is in progress at a time. A frame is identified by its whole
 * header except the two per-chunk fields (payload_bytes, num_samples),
 * plus its chunk count and total size: so the sequence AND the timestamp,
 * rate and centre frequency. Two publishers that share a subject and a
 * sequence are therefore not merged -- unless their headers are
 * bit-identical, which the same explicit timestamp_ns (a shared clock, or
 * 0) can make them; only a sender id would close that (#2017). A chunk of
 * a DIFFERENT frame that passes the grid check as a frame of its own
 * abandons the one in progress (counted) and STARTS the new one: it is
 * never discarded with it, which is what made one lost chunk lose every
 * later chunked frame (#2010). A chunk that fails the check is rejected
 * and costs the frame in progress nothing. An unchunked frame of the same
 * stream, or an end-of-stream, abandons it too (dp_reasm_unchunked). The
 * state lives as long as the socket, so a receive that times out mid-frame
 * resumes it.
 *
 * Chunks must sit on the sender's grid: chunk i covers
 * [i*S, min((i+1)*S, total)), S being the bytes of every chunk but the
 * last, taken from the frame's first chunk. So distinct indices cover
 * disjoint ranges, and "every index seen" means every byte written: no
 * uninitialised byte is ever handed out, and no offset arithmetic can wrap
 * (#2016). The sender (dp__nats_send_signal) always chunks this way. */
typedef struct
{
  dp_header_t key;           /**< the frame in progress, per-chunk fields
                                   zeroed */
  dp_chunk_t       shape;    /**< its count and total_bytes                 */
  uint64_t         stride;   /**< bytes per chunk but the last (the grid)   */
  char            *buf;      /**< NULL when no frame is in progress         */
  unsigned char   *seen;     /**< one flag per chunk index                  */
  uint32_t         received; /**< distinct chunks placed                    */
  dp_reasm_stats_t stats;    /**< what was lost, and how; kept by reset   */
} dp_reasm_t;

/** @brief Place one parsed chunk. Sets *complete when it finishes a frame:
 * then *frame is the reassembled payload (malloc'd; the caller frees it) and
 * *frame_hdr its logical, unchunked header. A redelivered chunk is a
 * no-op.
 * @param r               The reassembler (the socket's).
 * @param hdr             The chunk's frame header, from dp_frame_parse().
 * @param chunk           The chunk block, from dp_frame_parse().
 * @param body            The chunk's payload bytes.
 * @param body_len        Bytes in @p body.
 * @param[out] complete   Set to 1 when this chunk completes a frame, else 0.
 * @param[out] frame      On completion, the malloc'd payload (caller frees).
 * @param[out] frame_hdr  On completion, the frame's unchunked header.
 * @return DP_ERR_INVALID for a NULL argument or a chunk no frame could hold
 *         (counted in `rejected`; the frame in progress is untouched),
 *         DP_ERR_MEMORY, or DP_OK. */
int dp_reasm_feed (dp_reasm_t *r, const dp_header_t *hdr,
                   const dp_chunk_t *chunk, const void *body, size_t body_len,
                   int *complete, char **frame, dp_header_t *frame_hdr);
/** @brief Free the frame in progress, if any; the counters are kept.
 *  @param r  The reassembler; NULL is a no-op. */
void dp_reasm_reset (dp_reasm_t *r);
/** @brief Give up the frame in progress, if any, and count it in
 *  `abandoned`.
 *  @param r  The reassembler; NULL, or no frame in progress, is a no-op. */
void dp_reasm_abandon (dp_reasm_t *r);
/** @brief An unchunked frame (or an end-of-stream) `hdr` arrived. If it is
 * of the frame in progress's own stream -- the same kind and format, so the
 * same subject -- then with one publisher per subject that frame lost a chunk
 * and can never complete: give it up, counted. A frame of another type on
 * the same base says nothing about it, and leaves it alone. An EOS names
 * no stream (#2039) and ends the receive, so it ends the frame too.
 * @param r    The reassembler; NULL, or no frame in progress, is a no-op.
 * @param hdr  Header of the unchunked frame or EOS; NULL is a no-op. */
void dp_reasm_unchunked (dp_reasm_t *r, const dp_header_t *hdr);

/** @brief The tie between a context and the messages it handed out (#2016). A
   natsMsg does not keep its subscription alive, and nats.c's ack reads the
   subscription, its JetStream context and its connection (js.c's _ackMsg),
   so an ack after the context is destroyed read freed memory. The context
   and every DP_MSG_NATS message it hands out each hold a reference; the
   last to let go frees it. Opaque here: it carries a mutex, which only the
   NATS backend links, and stream_core.c never needs to see one. */
typedef struct dp_msg_link dp_msg_link_t;

/** @brief NATS-backed context.  Opaque nats.c handles are held as void* so
 * this header stays nats.h-free; stream_nats.c casts them back. */
struct dp_nats_state
{
  void *conn;                 /**< natsConnection *                          */
  void *sub;                  /**< natsSubscription *
                                   (SUB/REP/REQ-inbox/PULL) */
  void      *js;              /**< jsCtx * (JetStream ctx for PUSH/PULL)     */
  dp_role_t  role;            /**< drives subject choice in send/recv        */
  char      *base;            /**< subject base from the endpoint path   */
  char      *inbox;           /**< REQ: reply-to inbox subject               */
  char      *last_reply;      /**< REP: reply subject of the last request    */
  int        recv_timeout_ms; /**< ms; <0 = block (the default)              */
  int64_t    max_payload; /**< server max message size (bytes); chunk above */
  dp_reasm_t reasm;       /**< the chunked frame in progress, if any       */
  dp_msg_link_t *link;    /**< shared with every message this hands out    */
};

/** @brief A stream socket: the struct behind every dp_pub_t ... dp_rep_t. */
struct dp_ctx
{
  dp_frame_kind_t  kind;     /**< what this socket sends: I/Q or telemetry */
  dp_sample_type_t format;   /**< BLUE code; 0 when kind is not DP_KIND_IQ */
  uint64_t         sequence; /**< per-sender count; stamped into each
                                  header, then incremented. */
  uint64_t timestamp_override_ns; /**< one-shot; consumed by the next
                                      send_signal() call (dp_ctx_set_
                                      timestamp_ns()). */
  int timestamp_override_set; /**< 1 while timestamp_override_ns is pending;
                                   cleared by the next send, used or not. */
  /** Backend detail for the last failed call, for dp_ctx_last_error().
     A transport status ("Timeout", "No responders") is what distinguishes
     a broker that is slow from one that is absent, and collapsing every
     natsStatus into DP_ERR_SEND threw that away at the point of failure. */
  char                 last_error[192];
  struct dp_nats_state nats; /**< the NATS transport state */
};

/** @brief How a received message's buffer is owned (tags struct dp_msg). */
typedef enum
{
  DP_MSG_NATS = 1, /**< buffer owned by a natsMsg * (zero-copy past
                        offset). */
  DP_MSG_OWNED = 2 /**< malloc'd by doppler (chunk reassembly); plain free. */
} dp_msg_kind_t;

/** @brief A received message: the struct behind dp_msg_t. */
struct dp_msg
{
  dp_msg_kind_t    owner;  /**< who owns the buffer (NOT the frame kind) */
  dp_frame_kind_t  kind;   /**< the frame's own kind, from its header */
  dp_sample_type_t format; /**< BLUE code; 0 when kind is not DP_KIND_IQ */
  size_t           num_samples; /**< samples in the frame (0 for raw recv) */
  size_t           data_offset; /**< bytes to skip at the front (NATS
                                     header). */
  dp_msg_link_t *link; /**< DP_MSG_NATS: its context's; NULL when OWNED  */
  /** The buffer, selected by @c owner. */
  union
  {
    void *nats; /**< natsMsg *                       */
    /** A doppler-owned buffer. */
    struct
    {
      void  *ptr; /**< malloc'd payload  */
      size_t len; /**< bytes in @c ptr   */
    } owned;      /**< reassembled, doppler-owned buffer */
  } u;
};

/* ---- NATS transport (implemented in stream_nats.c) --------------------- */

/** @brief Every check a receiver makes on an arriving frame, over a plain
 * buffer -- the format's rules, with no transport in them.
 * @param buf            The received bytes.
 * @param len            Bytes in @p buf.
 * @param[out] hdr       The frame header (copied out before validation).
 * @param[out] chunk     The chunk block, when @p chunked.
 * @param[out] chunked   1 when the frame carries DP_FLAG_CHUNKED, else 0.
 * @param[out] body      Points into @p buf at the payload.
 * @param[out] body_len  Payload bytes.
 * @return DP_OK, or DP_ERR_INVALID for a NULL argument or a frame that
 *         breaks a rule of the format. */
int dp_frame_parse (const void *buf, size_t len, dp_header_t *hdr,
                    dp_chunk_t *chunk, int *chunked, const void **body,
                    size_t *body_len);

/**
 * @brief Allocate a context and connect it to the broker in @p endpoint.
 * @param role      What the socket does (selects subjects and JetStream use).
 * @param endpoint  `nats://host:port/<base>` endpoint.
 * @param kind      What the socket sends: I/Q or telemetry.
 * @param format    BLUE sample code; 0 when @p kind is not DP_KIND_IQ.
 * @return The context (receive timeout: block), or NULL on a bad endpoint,
 *         a failed connection/role setup, or allocation failure.
 */
struct dp_ctx *dp__nats_ctx_create (dp_role_t role, const char *endpoint,
                                    dp_frame_kind_t  kind,
                                    dp_sample_type_t format);
/**
 * @brief Release the transport inside @p ctx (subscription, JetStream,
 *        connection, inbox, reassembly) and close its message link; does
 *        NOT free @p ctx itself.  Messages already handed out stay valid.
 * @param ctx  Must be non-NULL.
 */
void dp__nats_ctx_destroy (struct dp_ctx *ctx);
/**
 * @brief Delete the JetStream work-queue stream behind this context.
 *        Never called on close: the queue outlives its producers.
 * @param ctx  A PUSH or PULL context.
 * @return DP_OK, or DP_ERR_INVALID with no JetStream context or when the
 *         server refuses (detail in dp_ctx_last_error()).
 */
int dp__nats_delete_stream (struct dp_ctx *ctx);
/**
 * @brief Publish one framed signal message, chunking above max_payload.
 *
 * Only a PUB context chunks; the other roles send one message and refuse
 * a frame that does not fit.
 *
 * @param ctx        Sending context.
 * @param header     Fully filled frame header.
 * @param samples    Payload bytes.
 * @param data_size  Bytes in @p samples.
 * @return DP_OK; DP_ERR_TOO_LARGE for a non-PUB frame above max_payload;
 *         DP_ERR_INVALID when no whole element fits a chunk; or the
 *         publish error.
 */
int dp__nats_send_signal (struct dp_ctx *ctx, const dp_header_t *header,
                          const void *samples, size_t data_size);
/**
 * @brief Receive the next whole signal frame, reassembling chunks.
 * @param ctx           Receiving context; honors its receive timeout.
 * @param[out] out_msg  The message (zero-copy NATS or doppler-owned), or
 *                      NULL on end-of-stream.
 * @param[out] out_hdr  The frame's header; may be NULL.
 * @return DP_OK; DP_ERR_EOF on an end-of-stream frame; DP_ERR_TIMEOUT;
 *         DP_ERR_INVALID for an unparseable frame (terminated on PULL so
 *         the queue moves on); DP_ERR_MEMORY; or a transport error.
 */
int dp__nats_recv_signal (struct dp_ctx *ctx, dp_msg_t **out_msg,
                          dp_header_t *out_hdr);
/**
 * @brief Receive the next message as raw bytes (no frame parsing).  A REP
 *        context remembers its reply subject.
 * @param ctx            Receiving context.
 * @param[out] out_msg   The zero-copy message.
 * @param[out] out_size  Its byte count.
 * @return DP_OK, DP_ERR_TIMEOUT, DP_ERR_MEMORY, or a transport error.
 */
int dp__nats_recv_raw (struct dp_ctx *ctx, dp_msg_t **out_msg,
                       size_t *out_size);
/**
 * @brief Publish raw bytes to the role's subject (or the last reply
 *        subject for REP).
 * @param ctx   Sending context.
 * @param data  Payload bytes.
 * @param size  Bytes in @p data; above INT_MAX is DP_ERR_TOO_LARGE.
 * @return DP_OK or a negative DP_ERR_* code.
 */
int dp__nats_send_raw (struct dp_ctx *ctx, const void *data, size_t size);
/**
 * @brief Wait until the server has received everything published so far.
 * @param ctx         The context.
 * @param timeout_ms  Budget in ms; <= 0 selects 2000.
 * @return DP_OK, DP_ERR_TIMEOUT, DP_ERR_INVALID with no connection, or
 *         DP_ERR_SEND.
 */
int dp__nats_flush (struct dp_ctx *ctx, int timeout_ms);
/**
 * @brief Drain the connection and wait for it to close, so pending
 *        publishes are written before teardown.
 * @param ctx         The context.
 * @param timeout_ms  Budget in ms; <= 0 selects 5000.
 * @return DP_OK once closed, DP_ERR_TIMEOUT, DP_ERR_INVALID with no
 *         connection, or DP_ERR_SEND if the drain could not start.
 */
int dp__nats_drain (struct dp_ctx *ctx, int timeout_ms);
/**
 * @brief Set the receive timeout used by later receives.
 * @param ctx         The context.
 * @param timeout_ms  Milliseconds; negative blocks indefinitely.
 */
void dp__nats_set_recv_timeout (struct dp_ctx *ctx, int timeout_ms);

/* dp_msg accessors for DP_MSG_NATS (called from the core's switch). */
/** @brief Payload pointer of a DP_MSG_NATS message, past data_offset.
 *  @param msg  A DP_MSG_NATS message.
 *  @return Pointer into the natsMsg; valid until the message is freed. */
void *dp__nats_msg_data (dp_msg_t *msg);
/** @brief Payload bytes of a DP_MSG_NATS message, past data_offset.
 *  @param msg  A DP_MSG_NATS message.
 *  @return The byte count. */
size_t dp__nats_msg_size (dp_msg_t *msg);
/** @brief Destroy the natsMsg and release the link; the dp_msg struct
 *  itself is freed by the caller.
 *  @param msg  A DP_MSG_NATS message. */
void
dp__nats_msg_free (dp_msg_t *msg); /* destroys the natsMsg only, not msg */
/** @brief JetStream explicit ack (PULL); a no-op success for other roles.
 *  @param msg  A DP_MSG_NATS message.
 *  @return DP_OK; DP_ERR_CLOSED when its context is already destroyed (the
 *          broker redelivers it); DP_ERR_SEND if the ack fails. */
int dp__nats_msg_ack (dp_msg_t *msg); /* JetStream explicit ack (PULL)      */

#endif /* DP_STREAM_INTERNAL_H */
