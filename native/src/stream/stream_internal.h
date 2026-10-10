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

/* Messaging role.  The NATS backend maps these onto pub/sub subjects, a
 * JetStream work-queue, or request/reply, as appropriate. */
typedef enum
{
  DP_ROLE_PUB = 0,
  DP_ROLE_SUB,
  DP_ROLE_PUSH,
  DP_ROLE_PULL,
  DP_ROLE_REQ,
  DP_ROLE_REP
} dp_role_t;

/* Reassembly of a chunked frame from its chunks, over parsed chunks rather
 * than a transport -- the receive side's other rule, beside
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
  dp_header_t      key;    /* the frame in progress, per-chunk fields zeroed */
  dp_chunk_t       shape;  /* its count and total_bytes                 */
  uint64_t         stride; /* bytes per chunk but the last (the grid)   */
  char            *buf;    /* NULL when no frame is in progress         */
  unsigned char   *seen;   /* one flag per chunk index                  */
  uint32_t         received; /* distinct chunks placed                    */
  dp_reasm_stats_t stats;    /* what was lost, and how                    */
} dp_reasm_t;

/* Place one parsed chunk. Sets *complete when it finishes a frame: then
 * *frame is the reassembled payload (malloc'd; the caller frees it) and
 * *frame_hdr its logical, unchunked header. A redelivered chunk is a
 * no-op. Returns DP_ERR_INVALID for a chunk no frame could hold (counted
 * in `rejected`; the frame in progress is untouched), DP_ERR_MEMORY, or
 * DP_OK. */
int dp_reasm_feed (dp_reasm_t *r, const dp_header_t *hdr,
                   const dp_chunk_t *chunk, const void *body, size_t body_len,
                   int *complete, char **frame, dp_header_t *frame_hdr);
/* Free the frame in progress, if any; the counters are kept. */
void dp_reasm_reset (dp_reasm_t *r);
/* Give up the frame in progress, if any, and count it in `abandoned`. */
void dp_reasm_abandon (dp_reasm_t *r);
/* An unchunked frame (or an end-of-stream) `hdr` arrived. If it is of the
 * frame in progress's own stream -- the same kind and format, so the same
 * subject -- then with one publisher per subject that frame lost a chunk
 * and can never complete: give it up, counted. A frame of another type on
 * the same base says nothing about it, and leaves it alone. An EOS names
 * no stream (#2039) and ends the receive, so it ends the frame too. */
void dp_reasm_unchunked (dp_reasm_t *r, const dp_header_t *hdr);

/* The tie between a context and the messages it handed out (#2016). A
   natsMsg does not keep its subscription alive, and nats.c's ack reads the
   subscription, its JetStream context and its connection (js.c's _ackMsg),
   so an ack after the context is destroyed read freed memory. The context
   and every DP_MSG_NATS message it hands out each hold a reference; the
   last to let go frees it. Opaque here: it carries a mutex, which only the
   NATS backend links, and stream_core.c never needs to see one. */
typedef struct dp_msg_link dp_msg_link_t;

/* NATS-backed context.  Opaque nats.c handles are held as void* so this
 * header stays nats.h-free; stream_nats.c casts them back. */
struct dp_nats_state
{
  void      *conn;            /* natsConnection *                            */
  void      *sub;             /* natsSubscription * (SUB/REP/REQ-inbox/PULL) */
  void      *js;              /* jsCtx * (JetStream ctx for PUSH/PULL)       */
  dp_role_t  role;            /* drives subject choice in send/recv          */
  char      *base;            /* subject base parsed from the endpoint path  */
  char      *inbox;           /* REQ: reply-to inbox subject                 */
  char      *last_reply;      /* REP: reply subject of the last request      */
  int        recv_timeout_ms; /* <0 = block                                  */
  int64_t    max_payload; /* server max message size (bytes); chunk above */
  dp_reasm_t reasm;       /* the chunked frame in progress, if any       */
  dp_msg_link_t *link;    /* shared with every message this hands out    */
};

struct dp_ctx
{
  dp_frame_kind_t  kind;     /* what this socket sends: I/Q or telemetry */
  dp_sample_type_t format;   /* BLUE code; 0 when kind is not DP_KIND_IQ */
  uint64_t         sequence; /* per-sender count. */
  uint64_t         timestamp_override_ns; /* one-shot; consumed by the next
                                              send_signal() call (dp_ctx_set_
                                              timestamp_ns()). */
  int timestamp_override_set;
  /* Backend detail for the last failed call, for dp_ctx_last_error().
     A transport status ("Timeout", "No responders") is what distinguishes
     a broker that is slow from one that is absent, and collapsing every
     natsStatus into DP_ERR_SEND threw that away at the point of failure. */
  char                 last_error[192];
  struct dp_nats_state nats;
};

/* How a received message's buffer is owned (tags struct dp_msg). */
typedef enum
{
  DP_MSG_NATS  = 1, /* buffer owned by a natsMsg * (zero-copy past offset). */
  DP_MSG_OWNED = 2  /* malloc'd by doppler (chunk reassembly); plain free. */
} dp_msg_kind_t;

struct dp_msg
{
  dp_msg_kind_t    owner;  /* who owns the buffer (NOT the frame kind) */
  dp_frame_kind_t  kind;   /* the frame's own kind, from its header */
  dp_sample_type_t format; /* BLUE code; 0 when kind is not DP_KIND_IQ */
  size_t           num_samples;
  size_t           data_offset; /* bytes to skip at the front (NATS header). */
  dp_msg_link_t   *link; /* DP_MSG_NATS: its context's; NULL when OWNED  */
  union
  {
    void *nats; /* natsMsg *                       */
    struct
    {
      void  *ptr;
      size_t len;
    } owned; /* reassembled, doppler-owned buffer */
  } u;
};

/* ---- NATS transport (implemented in stream_nats.c) --------------------- */

/* Every check a receiver makes on an arriving frame, over a plain buffer --
 * the format's rules, with no transport in them. Returns DP_OK and points
 * *body at the payload, or DP_ERR_INVALID. */
int dp_frame_parse (const void *buf, size_t len, dp_header_t *hdr,
                    dp_chunk_t *chunk, int *chunked, const void **body,
                    size_t *body_len);

struct dp_ctx *dp__nats_ctx_create (dp_role_t role, const char *endpoint,
                                    dp_frame_kind_t  kind,
                                    dp_sample_type_t format);
void           dp__nats_ctx_destroy (struct dp_ctx *ctx);
int            dp__nats_delete_stream (struct dp_ctx *ctx);
int  dp__nats_send_signal (struct dp_ctx *ctx, const dp_header_t *header,
                           const void *samples, size_t data_size);
int  dp__nats_recv_signal (struct dp_ctx *ctx, dp_msg_t **out_msg,
                           dp_header_t *out_hdr);
int  dp__nats_recv_raw (struct dp_ctx *ctx, dp_msg_t **out_msg,
                        size_t *out_size);
int  dp__nats_send_raw (struct dp_ctx *ctx, const void *data, size_t size);
int  dp__nats_flush (struct dp_ctx *ctx, int timeout_ms);
int  dp__nats_drain (struct dp_ctx *ctx, int timeout_ms);
void dp__nats_set_recv_timeout (struct dp_ctx *ctx, int timeout_ms);

/* dp_msg accessors for DP_MSG_NATS (called from the core's switch). */
void  *dp__nats_msg_data (dp_msg_t *msg);
size_t dp__nats_msg_size (dp_msg_t *msg);
void
dp__nats_msg_free (dp_msg_t *msg);    /* destroys the natsMsg only, not msg */
int dp__nats_msg_ack (dp_msg_t *msg); /* JetStream explicit ack (PULL)      */

#endif /* DP_STREAM_INTERNAL_H */
