/**
 * @file stream_internal.h
 * @brief Private definitions shared by the stream backends.
 *
 * Not a public header — it carries the tagged-union context/message structs
 * and the cross-TU backend hooks that stream_core.c (dispatch + ZMQ) and
 * stream_nats.c (NATS) both need.  The ZMQ backend lives entirely inside
 * stream_core.c as dpz_* statics; the NATS backend is implemented in
 * stream_nats.c and exposed here as dpn_* declarations.  nats.h is included
 * only by stream_nats.c — never leaks into the core dispatcher.
 */

#ifndef DP_STREAM_INTERNAL_H
#define DP_STREAM_INTERNAL_H

#include "stream/stream.h"
#include <zmq.h>

#define DP_MAGIC 0x53494753   /* "SIGS" */
#define DP_VERSION 0x00010000 /* v1.0.0 */

/* Chunking (NATS only — ZMQ has no payload cap).  A logical frame larger than
 * the server max_payload is split into several messages that share one
 * sequence; dp_header_t.flags carries DP_FLAG_CHUNKED and reserved[] carries
 * the reassembly geometry.  reserved[] is otherwise zero/uninterpreted. */
#define DP_FLAG_CHUNKED 0x1u
#define DP_CHUNK_IDX 0 /* reserved[] index: 0-based chunk number  */
#define DP_CHUNK_CNT 1 /* reserved[] index: chunks in this frame  */
#define DP_CHUNK_TOT 2 /* reserved[] index: total frame samples   */
#define DP_CHUNK_OFF 3 /* reserved[] index: this chunk byte offset */

/* Transport selected per context by the endpoint scheme: "nats://…" -> NATS,
 * anything else (tcp://, ipc://, inproc://) -> ZMQ. */
typedef enum
{
  DP_BACKEND_ZMQ  = 0,
  DP_BACKEND_NATS = 1
} dp_backend_t;

/* Messaging role, decoupled from any backend's socket-type constants.  The
 * ZMQ backend maps these onto ZMQ_PUB/ZMQ_SUB/…; bind-vs-connect is implied
 * by the role (PUB/PUSH/REP bind, SUB/PULL/REQ connect). */
typedef enum
{
  DP_ROLE_PUB = 0,
  DP_ROLE_SUB,
  DP_ROLE_PUSH,
  DP_ROLE_PULL,
  DP_ROLE_REQ,
  DP_ROLE_REP
} dp_role_t;

struct dp_zmq_state
{
  void *context;
  void *socket;
};

/* NATS-backed context.  Opaque nats.c handles are held as void* so this
 * header stays nats.h-free; stream_nats.c casts them back. */
struct dp_nats_state
{
  void     *conn;            /* natsConnection *                            */
  void     *sub;             /* natsSubscription * (SUB/REP/REQ-inbox/PULL) */
  void     *js;              /* jsCtx * (JetStream ctx for PUSH/PULL)       */
  dp_role_t role;            /* drives subject choice in send/recv          */
  char     *base;            /* subject base parsed from the endpoint path  */
  char     *inbox;           /* REQ: reply-to inbox subject                 */
  char     *last_reply;      /* REP: reply subject of the last request      */
  int       recv_timeout_ms; /* <0 = block (ZMQ RCVTIMEO default)           */
  int64_t   max_payload;     /* server max message size (bytes); chunk above */
};

struct dp_ctx
{
  dp_backend_t     backend;
  dp_sample_type_t sample_type;
  uint64_t         sequence; /* shared, backend-agnostic per-sender count. */
  union
  {
    struct dp_zmq_state  zmq;
    struct dp_nats_state nats;
  } u;
};

/* How a received message's buffer is owned (tags struct dp_msg). */
typedef enum
{
  DP_MSG_ZMQ   = 0, /* buffer owned by a zmq_msg_t (zero-copy).             */
  DP_MSG_NATS  = 1, /* buffer owned by a natsMsg * (zero-copy past offset). */
  DP_MSG_OWNED = 2  /* malloc'd by doppler (chunk reassembly); plain free. */
} dp_msg_kind_t;

struct dp_msg
{
  dp_msg_kind_t    kind;
  dp_sample_type_t sample_type;
  size_t           num_samples;
  size_t           data_offset; /* bytes to skip at the front (NATS header). */
  union
  {
    zmq_msg_t zmq;
    void     *nats; /* natsMsg *                       */
    struct
    {
      void  *ptr;
      size_t len;
    } owned; /* reassembled, doppler-owned buffer */
  } u;
};

/* ---- NATS backend hooks (implemented in stream_nats.c) ----------------- */

struct dp_ctx *dpn_ctx_create (dp_role_t role, const char *endpoint,
                               dp_sample_type_t sample_type);
void           dpn_ctx_destroy (struct dp_ctx *ctx);
int            dpn_send_signal (struct dp_ctx *ctx, const dp_header_t *header,
                                const void *samples, size_t data_size);
int            dpn_recv_signal (struct dp_ctx *ctx, dp_msg_t **out_msg,
                                dp_header_t *out_hdr);
int  dpn_recv_raw (struct dp_ctx *ctx, dp_msg_t **out_msg, size_t *out_size);
int  dpn_send_raw (struct dp_ctx *ctx, const void *data, size_t size);
void dpn_set_recv_timeout (struct dp_ctx *ctx, int timeout_ms);

/* dp_msg accessors for DP_MSG_NATS (called from the core's switch). */
void  *dpn_msg_data (dp_msg_t *msg);
size_t dpn_msg_size (dp_msg_t *msg);
void   dpn_msg_free (dp_msg_t *msg); /* destroys the natsMsg only, not msg */
int    dpn_msg_ack (dp_msg_t *msg);  /* JetStream explicit ack (PULL)      */

#endif /* DP_STREAM_INTERNAL_H */
