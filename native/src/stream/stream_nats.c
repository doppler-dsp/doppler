/**
 * @file stream_nats.c
 * @brief NATS backend for the streaming API (the dpn_* hooks).
 *
 * Selected when an endpoint uses the "nats://" scheme.  This translation
 * unit is the only place nats.h is included; stream_core.c stays the
 * transport-agnostic dispatcher and never sees a nats type.
 *
 * Wire format is identical to the ZMQ backend: a 96-byte dp_header_t binary
 * prefix followed by the interleaved I/Q payload, all in one NATS message.
 * Receive stays zero-copy — dp_msg_data() points into the natsMsg past the
 * header, and dp_msg_free() does exactly one natsMsg_Destroy().
 *
 * Subjects: an endpoint "nats://host:port/{base}" yields a subject base
 * (default "default").  PUB publishes "iq.{base}.{sample_type}"; SUB
 * subscribes "iq.{base}.>" (so the broker can filter by type for free).
 * REQ/REP map onto NATS request/reply: a REQ owns a reply inbox and
 * PublishRequest()s to {base}; a REP SubscribeSync()s {base}, remembers
 * each request's reply subject, and Publish()es the answer there.
 *
 * PUSH/PULL over nats:// is the durable JetStream work-queue tier: PUSH does
 * synchronous server-acked js_Publish onto a WorkQueue/File stream; PULL is a
 * shared durable consumer with explicit ack (at-least-once), so workers
 * load-balance and a crashed consumer's un-acked frames redeliver.
 */

#include "stream/stream.h"
#include "stream_internal.h"
#include <nats.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Endpoint parsing
 * ========================================================================= */

/* Split "nats://authority[/base]" into a bare connection URL ("nats://auth")
 * and a strdup'd subject base (default "default").  Returns 0 on success. */
static int
nats_parse_endpoint (const char *endpoint, char *url, size_t url_sz,
                     char **base_out)
{
  const char *authority = endpoint + 7; /* past "nats://" */
  const char *slash     = strchr (authority, '/');
  size_t auth_len = slash ? (size_t)(slash - authority) : strlen (authority);

  int n = snprintf (url, url_sz, "nats://%.*s", (int)auth_len, authority);
  if (n < 0 || (size_t)n >= url_sz)
    return -1;

  const char *base = (slash && slash[1]) ? slash + 1 : "default";
  *base_out        = strdup (base);
  return *base_out ? 0 : -1;
}

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

/* Open a NATS connection with auto-reconnect to a single bare URL. */
static natsConnection *
dpn_connect (const char *url)
{
  natsOptions *opts = NULL;
  if (natsOptions_Create (&opts) != NATS_OK)
    return NULL;
  (void)natsOptions_SetURL (opts, url);
  (void)natsOptions_SetAllowReconnect (opts, true);
  (void)natsOptions_SetMaxReconnect (opts, -1);   /* infinite */
  (void)natsOptions_SetReconnectWait (opts, 100); /* ms        */
  (void)natsOptions_SetReconnectBufSize (opts, 8 * 1024 * 1024); /* 8 MiB */

  natsConnection *conn = NULL;
  natsStatus      s    = natsConnection_Connect (&conn, opts);
  natsOptions_Destroy (opts);
  return (s == NATS_OK) ? conn : NULL;
}

/* Synchronous subscribe with unbounded pending (best-effort). */
static natsStatus
dpn_subscribe (natsConnection *conn, const char *subj, natsSubscription **sub)
{
  natsStatus s = natsConnection_SubscribeSync (sub, conn, subj);
  if (s == NATS_OK)
    (void)natsSubscription_SetPendingLimits (*sub, -1, -1);
  return s;
}

/* JetStream stream/consumer names cannot contain . * > or space — derive a
 * safe one from the subject base. */
static void
dpn_js_name (char *out, size_t n, const char *prefix, const char *base)
{
  (void)snprintf (out, n, "%s%s", prefix, base);
  for (char *p = out; *p; p++)
    if (*p == '.' || *p == '*' || *p == '>' || *p == ' ')
      *p = '_';
}

/* Idempotently create the durable work-queue stream for `base`.  Tolerates a
 * pre-provisioned stream (e.g. a Helm-created R=3 one): if AddStream fails but
 * the stream already exists, succeed and use it as-is. */
static int
dpn_ensure_stream (jsCtx *js, const char *base)
{
  char name[256], subj[256];
  dpn_js_name (name, sizeof (name), "DP_WORK_", base);
  (void)snprintf (subj, sizeof (subj), "work.%s.>", base);

  jsStreamConfig cfg;
  jsStreamConfig_Init (&cfg);
  const char *subjects[1] = { subj };
  cfg.Name                = name;
  cfg.Subjects            = subjects;
  cfg.SubjectsLen         = 1;
  cfg.Retention           = js_WorkQueuePolicy;
  cfg.Storage             = js_FileStorage; /* survives a broker restart */
  cfg.Replicas            = 1; /* dev default; prod pre-provisions R=3 */

  natsStatus s = js_AddStream (NULL, js, &cfg, NULL, NULL);
  if (s == NATS_OK)
    return DP_OK;

  jsStreamInfo *si = NULL; /* already exists? use it as-is. */
  s                = js_GetStreamInfo (&si, js, name, NULL, NULL);
  if (si)
    jsStreamInfo_Destroy (si);
  return (s == NATS_OK) ? DP_OK : DP_ERR_INIT;
}

/* Create/attach the shared durable pull consumer for `base` (explicit ack,
 * at-least-once; workers sharing the durable load-balance + redeliver). */
static int
dpn_pull_subscribe (struct dp_ctx *ctx, jsCtx *js, natsSubscription **out)
{
  char durable[256], filter[256];
  dpn_js_name (durable, sizeof (durable), "DP_PULL_", ctx->u.nats.base);
  (void)snprintf (filter, sizeof (filter), "work.%s.>", ctx->u.nats.base);

  jsSubOptions so;
  jsSubOptions_Init (&so);
  so.ManualAck            = true; /* caller acks via dp_msg_ack */
  so.Config.Durable       = durable;
  so.Config.AckPolicy     = js_AckExplicit;
  so.Config.MaxAckPending = 1000;               /* HWM-style backpressure */
  so.Config.AckWait = 5LL * 1000 * 1000 * 1000; /* 5s ns -> fast redeliver */

  return (js_PullSubscribe (out, js, filter, durable, NULL, &so, NULL)
          == NATS_OK)
             ? 0
             : -1;
}

/* Role-specific subscription wiring after connect.  Returns 0 on success.
 * PUB needs no subscription; SUB/REP/REQ each get one (REQ also an inbox);
 * PUSH/PULL set up the JetStream work-queue tier. */
static int
dpn_wire_role (struct dp_ctx *ctx, natsConnection *conn)
{
  natsSubscription *sub = NULL;

  switch (ctx->u.nats.role)
    {
    case DP_ROLE_SUB:
      {
        char subj[600];
        (void)snprintf (subj, sizeof (subj), "iq.%s.>", ctx->u.nats.base);
        if (dpn_subscribe (conn, subj, &sub) != NATS_OK)
          return -1;
        break;
      }
    case DP_ROLE_REP:
      if (dpn_subscribe (conn, ctx->u.nats.base, &sub) != NATS_OK)
        return -1;
      break;
    case DP_ROLE_REQ:
      {
        natsInbox *inbox = NULL;
        if (natsInbox_Create (&inbox) != NATS_OK)
          return -1;
        ctx->u.nats.inbox = inbox;
        if (dpn_subscribe (conn, inbox, &sub) != NATS_OK)
          return -1;
        break;
      }
    case DP_ROLE_PUSH:
      {
        jsCtx *js = NULL;
        if (natsConnection_JetStream (&js, conn, NULL) != NATS_OK)
          return -1;
        ctx->u.nats.js = js;
        if (dpn_ensure_stream (js, ctx->u.nats.base) != DP_OK)
          return -1;
        break; /* publisher: no subscription */
      }
    case DP_ROLE_PULL:
      {
        jsCtx *js = NULL;
        if (natsConnection_JetStream (&js, conn, NULL) != NATS_OK)
          return -1;
        ctx->u.nats.js = js;
        if (dpn_pull_subscribe (ctx, js, &sub) != 0)
          return -1;
        break;
      }
    default: /* DP_ROLE_PUB */
      break;
    }

  ctx->u.nats.sub = sub;
  return 0;
}

struct dp_ctx *
dpn_ctx_create (dp_role_t role, const char *endpoint,
                dp_sample_type_t sample_type)
{
  struct dp_ctx *ctx = (struct dp_ctx *)calloc (1, sizeof (struct dp_ctx));
  if (!ctx)
    return NULL;
  ctx->backend                = DP_BACKEND_NATS;
  ctx->sample_type            = sample_type;
  ctx->u.nats.role            = role;
  ctx->u.nats.recv_timeout_ms = -1; /* block by default, like ZMQ RCVTIMEO */

  char url[512];
  if (nats_parse_endpoint (endpoint, url, sizeof (url), &ctx->u.nats.base)
      != 0)
    {
      free (ctx);
      return NULL;
    }

  ctx->u.nats.conn = dpn_connect (url);
  if (!ctx->u.nats.conn || dpn_wire_role (ctx, ctx->u.nats.conn) != 0)
    {
      dpn_ctx_destroy (ctx); /* frees internals (NULL-safe), not ctx itself */
      free (ctx);
      return NULL;
    }
  /* Cache the server's max message size; frames above it are chunked. */
  ctx->u.nats.max_payload
      = natsConnection_GetMaxPayload ((natsConnection *)ctx->u.nats.conn);
  return ctx;
}

void
dpn_ctx_destroy (struct dp_ctx *ctx)
{
  if (ctx->u.nats.sub)
    natsSubscription_Destroy ((natsSubscription *)ctx->u.nats.sub);
  if (ctx->u.nats.js)
    jsCtx_Destroy ((jsCtx *)ctx->u.nats.js);
  if (ctx->u.nats.conn)
    natsConnection_Destroy ((natsConnection *)ctx->u.nats.conn);
  if (ctx->u.nats.inbox)
    natsInbox_Destroy ((natsInbox *)ctx->u.nats.inbox);
  free (ctx->u.nats.base);
  free (ctx->u.nats.last_reply);
}

/* =========================================================================
 * Send
 * ========================================================================= */

/* Publish one prebuilt buffer to the role's subject.  typestr is the sample
 * type name (used only by PUB to build "iq.{base}.{type}"). */
static int
dpn_publish (struct dp_ctx *ctx, const char *typestr, const void *buf, int len)
{
  natsConnection *conn = (natsConnection *)ctx->u.nats.conn;
  natsStatus      s;

  switch (ctx->u.nats.role)
    {
    case DP_ROLE_PUB:
      {
        char subj[640];
        (void)snprintf (subj, sizeof (subj), "iq.%s.%s", ctx->u.nats.base,
                        typestr);
        s = natsConnection_Publish (conn, subj, buf, len);
        break;
      }
    case DP_ROLE_REQ:
      s = natsConnection_PublishRequest (
          conn, ctx->u.nats.base, (const char *)ctx->u.nats.inbox, buf, len);
      if (s == NATS_OK)
        s = natsConnection_Flush (conn); /* push the request out now */
      break;
    case DP_ROLE_REP:
      if (!ctx->u.nats.last_reply)
        return DP_ERR_SEND; /* no request to answer */
      s = natsConnection_Publish (conn, ctx->u.nats.last_reply, buf, len);
      if (s == NATS_OK)
        s = natsConnection_Flush (conn);
      break;
    case DP_ROLE_PUSH:
      {
        /* JetStream work-queue: synchronous, server-acked publish — the
         * message is persisted (and replicated) before we return, so a
         * producer-side crash never silently drops it. */
        char subj[640];
        (void)snprintf (subj, sizeof (subj), "work.%s.%s", ctx->u.nats.base,
                        typestr);
        jsPubAck *pa = NULL;
        s = js_Publish (&pa, (jsCtx *)ctx->u.nats.js, subj, buf, len, NULL,
                        NULL);
        if (pa)
          jsPubAck_Destroy (pa);
        break;
      }
    default:
      return DP_ERR_INVALID; /* SUB/PULL cannot send */
    }

  return (s == NATS_OK) ? DP_OK : DP_ERR_SEND;
}

/* Stage one [header][payload] message and publish it (zero-copy send is not
 * possible over NATS — header and data must be one contiguous buffer). */
static int
dpn_publish_framed (struct dp_ctx *ctx, const dp_header_t *h, const void *data,
                    size_t data_len)
{
  size_t hdr_sz = sizeof (*h);
  char  *buf    = (char *)malloc (hdr_sz + data_len);
  if (!buf)
    return DP_ERR_MEMORY;
  memcpy (buf, h, hdr_sz);
  memcpy (buf + hdr_sz, data, data_len);
  int rc = dpn_publish (ctx, dp_sample_type_str (h->sample_type), buf,
                        (int)(hdr_sz + data_len));
  free (buf);
  return rc;
}

int
dpn_send_signal (struct dp_ctx *ctx, const dp_header_t *header,
                 const void *samples, size_t data_size)
{
  size_t  hdr_sz = sizeof (*header);
  int64_t maxp   = ctx->u.nats.max_payload;
  if (maxp <= 0)
    maxp = 1024LL * 1024; /* NATS default if the server didn't report one */

  /* Chunking is a fan-out (PUB/SUB) feature only: every subscriber receives
   * the whole in-order chunk sequence and reassembles independently.  Over a
   * load-balanced work-queue (PUSH/PULL) a frame's chunks could land on
   * different workers, so PUSH sends one frame as one message (the resilient
   * tier relies on a generous server max_payload).  REQ/REP are small. */
  if (ctx->u.nats.role != DP_ROLE_PUB || hdr_sz + data_size <= (size_t)maxp)
    return dpn_publish_framed (ctx, header, samples, data_size);

  /* Large PUB frame: split into sample-aligned chunks that each fit, all
   * sharing this frame's sequence; (sequence, chunk_index) lets each
   * subscriber reassemble idempotently. */
  size_t ss = dp_sample_size ((dp_sample_type_t)header->sample_type);
  if (ss == 0)
    return DP_ERR_INVALID;
  size_t max_data = (size_t)maxp - hdr_sz;
  max_data -= max_data % ss; /* whole samples per chunk */
  if (max_data == 0)
    return DP_ERR_INVALID; /* header alone exceeds max_payload */

  size_t      nchunks = (data_size + max_data - 1) / max_data;
  const char *src     = (const char *)samples;
  for (size_t i = 0; i < nchunks; i++)
    {
      size_t off  = i * max_data;
      size_t take = (data_size - off < max_data) ? data_size - off : max_data;

      dp_header_t h = *header;
      h.flags |= DP_FLAG_CHUNKED;
      h.num_samples            = take / ss;
      h.reserved[DP_CHUNK_IDX] = i;
      h.reserved[DP_CHUNK_CNT] = nchunks;
      h.reserved[DP_CHUNK_TOT] = header->num_samples;
      h.reserved[DP_CHUNK_OFF] = off;

      int rc = dpn_publish_framed (ctx, &h, src + off, take);
      if (rc != DP_OK)
        return rc;
    }
  return DP_OK;
}

int
dpn_send_raw (struct dp_ctx *ctx, const void *data, size_t size)
{
  return dpn_publish (ctx, NULL, data, (int)size);
}

/* =========================================================================
 * Receive
 * ========================================================================= */

/* Pull one message from the durable JetStream consumer (batch of 1).  The
 * message is NOT acked here — the caller acks via dp_msg_ack once it has been
 * processed, so a crash before ack triggers redelivery (at-least-once). */
static int
dpn_pull_fetch (struct dp_ctx *ctx, natsMsg **out)
{
  natsSubscription *sub = (natsSubscription *)ctx->u.nats.sub;
  if (!sub)
    return DP_ERR_INVALID;

  int         to   = ctx->u.nats.recv_timeout_ms;
  natsMsgList list = { NULL, 0 };
  natsStatus  s;
  if (to < 0)
    {
      do
        s = natsSubscription_Fetch (&list, sub, 1, 3600000, NULL);
      while (s == NATS_TIMEOUT);
    }
  else
    s = natsSubscription_Fetch (&list, sub, 1, to == 0 ? 1 : to, NULL);

  if (s == NATS_TIMEOUT)
    return DP_ERR_TIMEOUT;
  if (s != NATS_OK || list.Count < 1)
    {
      natsMsgList_Destroy (&list);
      return DP_ERR_RECV;
    }
  *out         = list.Msgs[0];
  list.Msgs[0] = NULL;         /* keep this message alive */
  natsMsgList_Destroy (&list); /* frees the array; NULL slot is a no-op */
  return DP_OK;
}

/* Block for the next message honouring the stored timeout.  recv_timeout_ms
 * < 0 means block indefinitely (emulated by re-polling on NATS_TIMEOUT). */
static int
nats_next (struct dp_ctx *ctx, natsMsg **out)
{
  if (ctx->u.nats.role == DP_ROLE_PULL)
    return dpn_pull_fetch (ctx, out);

  natsSubscription *sub = (natsSubscription *)ctx->u.nats.sub;
  if (!sub)
    return DP_ERR_INVALID;

  int        to = ctx->u.nats.recv_timeout_ms;
  natsStatus s;
  if (to < 0)
    {
      do
        s = natsSubscription_NextMsg (out, sub, 3600000);
      while (s == NATS_TIMEOUT);
    }
  else
    {
      s = natsSubscription_NextMsg (out, sub, to == 0 ? 1 : to);
    }

  if (s == NATS_TIMEOUT)
    return DP_ERR_TIMEOUT;
  return (s == NATS_OK) ? DP_OK : DP_ERR_RECV;
}

/* A REP must answer the request it just received; remember its reply subject.
 */
static void
nats_stash_reply (struct dp_ctx *ctx, natsMsg *m)
{
  free (ctx->u.nats.last_reply);
  ctx->u.nats.last_reply = NULL;
  const char *reply      = natsMsg_GetReply (m);
  if (reply)
    ctx->u.nats.last_reply = strdup (reply);
}

/* Validate one chunk message and copy its payload into the reassembly buffer
 * at its byte offset.  Idempotent: a redelivered chunk (seen[idx]) is a no-op.
 * Does not destroy m. */
static int
dpn_place_chunk (char *buf, size_t total_bytes, uint64_t nchunks,
                 uint64_t sequence, natsMsg *m, unsigned char *seen,
                 uint64_t *received)
{
  size_t      hdr_sz = sizeof (dp_header_t);
  const char *d      = natsMsg_GetData (m);
  int         len    = natsMsg_GetDataLength (m);
  if (len < (int)hdr_sz)
    return DP_ERR_INVALID;

  dp_header_t ch;
  memcpy (&ch, d, hdr_sz);
  if (ch.magic != DP_MAGIC || !(ch.flags & DP_FLAG_CHUNKED)
      || ch.sequence != sequence || ch.reserved[DP_CHUNK_CNT] != nchunks)
    return DP_ERR_INVALID;

  uint64_t idx    = ch.reserved[DP_CHUNK_IDX];
  uint64_t off    = ch.reserved[DP_CHUNK_OFF];
  size_t   cbytes = (size_t)len - hdr_sz;
  if (idx >= nchunks || off + cbytes > total_bytes)
    return DP_ERR_INVALID;

  if (!seen[idx])
    {
      memcpy (buf + off, d + hdr_sz, cbytes);
      seen[idx] = 1;
      (*received)++;
    }
  return DP_OK;
}

/* Reassemble a chunked frame into one doppler-owned buffer.  `first` is the
 * already-received chunk; fhdr is its parsed header.  Consumes `first` and any
 * further chunks fetched.  Returns a DP_MSG_OWNED message on success. */
static int
dpn_reassemble (struct dp_ctx *ctx, natsMsg *first, const dp_header_t *fhdr,
                dp_msg_t **out_msg, dp_header_t *out_hdr)
{
  size_t   ss      = dp_sample_size ((dp_sample_type_t)fhdr->sample_type);
  uint64_t nchunks = fhdr->reserved[DP_CHUNK_CNT];
  uint64_t total_samples = fhdr->reserved[DP_CHUNK_TOT];
  if (ss == 0 || nchunks == 0)
    {
      natsMsg_Destroy (first);
      return DP_ERR_INVALID;
    }
  size_t total_bytes = (size_t)total_samples * ss;

  char          *buf  = (char *)malloc (total_bytes ? total_bytes : 1);
  unsigned char *seen = (unsigned char *)calloc ((size_t)nchunks, 1);
  if (!buf || !seen)
    {
      free (buf);
      free (seen);
      natsMsg_Destroy (first);
      return DP_ERR_MEMORY;
    }

  natsMsg *m        = first;
  uint64_t received = 0;
  int      rc       = DP_OK;
  for (;;)
    {
      rc = dpn_place_chunk (buf, total_bytes, nchunks, fhdr->sequence, m, seen,
                            &received);
      natsMsg_Destroy (m);
      m = NULL;
      if (rc != DP_OK || received == nchunks)
        break;
      rc = nats_next (ctx, &m); /* next chunk of this frame */
      if (rc != DP_OK)
        break;
    }

  free (seen);
  if (rc != DP_OK)
    {
      free (buf);
      return rc;
    }

  dp_msg_t *msg = (dp_msg_t *)malloc (sizeof (dp_msg_t));
  if (!msg)
    {
      free (buf);
      return DP_ERR_MEMORY;
    }
  msg->kind        = DP_MSG_OWNED;
  msg->u.owned.ptr = buf;
  msg->u.owned.len = total_bytes;
  msg->data_offset = 0;
  msg->sample_type = (dp_sample_type_t)fhdr->sample_type;
  msg->num_samples = total_samples;

  *out_msg = msg;
  if (out_hdr)
    {
      *out_hdr = *fhdr; /* present a clean logical-frame header */
      out_hdr->flags &= ~DP_FLAG_CHUNKED;
      out_hdr->num_samples = total_samples;
      memset (out_hdr->reserved, 0, sizeof (out_hdr->reserved));
    }
  return DP_OK;
}

int
dpn_recv_signal (struct dp_ctx *ctx, dp_msg_t **out_msg, dp_header_t *out_hdr)
{
  natsMsg *m  = NULL;
  int      rc = nats_next (ctx, &m);
  if (rc != DP_OK)
    return rc;

  const char *data = natsMsg_GetData (m);
  int         len  = natsMsg_GetDataLength (m);
  if (len < (int)sizeof (dp_header_t))
    {
      natsMsg_Destroy (m);
      return DP_ERR_INVALID;
    }

  dp_header_t hdr;
  memcpy (&hdr, data, sizeof (hdr));
  if (hdr.magic != DP_MAGIC)
    {
      natsMsg_Destroy (m);
      return DP_ERR_INVALID;
    }

  if (ctx->u.nats.role == DP_ROLE_REP)
    nats_stash_reply (ctx, m);

  /* Large fan-out frames arrive as several chunks — reassemble into one owned
   * buffer.  (PULL never chunks: the work-queue carries whole frames.) */
  if ((hdr.flags & DP_FLAG_CHUNKED) && ctx->u.nats.role != DP_ROLE_PULL)
    return dpn_reassemble (ctx, m, &hdr, out_msg, out_hdr);

  /* Single message: zero-copy, data lives in the natsMsg past the header. */
  dp_msg_t *msg = (dp_msg_t *)malloc (sizeof (dp_msg_t));
  if (!msg)
    {
      natsMsg_Destroy (m);
      return DP_ERR_MEMORY;
    }
  msg->kind        = DP_MSG_NATS;
  msg->u.nats      = m;
  msg->data_offset = sizeof (dp_header_t);
  msg->sample_type = (dp_sample_type_t)hdr.sample_type;
  msg->num_samples = hdr.num_samples;

  *out_msg = msg;
  if (out_hdr)
    memcpy (out_hdr, &hdr, sizeof (dp_header_t));
  return DP_OK;
}

int
dpn_recv_raw (struct dp_ctx *ctx, dp_msg_t **out_msg, size_t *out_size)
{
  natsMsg *m  = NULL;
  int      rc = nats_next (ctx, &m);
  if (rc != DP_OK)
    return rc;

  if (ctx->u.nats.role == DP_ROLE_REP)
    nats_stash_reply (ctx, m);

  dp_msg_t *msg = (dp_msg_t *)malloc (sizeof (dp_msg_t));
  if (!msg)
    {
      natsMsg_Destroy (m);
      return DP_ERR_MEMORY;
    }
  msg->kind        = DP_MSG_NATS;
  msg->u.nats      = m;
  msg->data_offset = 0;
  msg->sample_type = CF64; /* not meaningful for raw recv */
  msg->num_samples = 0;

  *out_msg  = msg;
  *out_size = (size_t)natsMsg_GetDataLength (m);
  return DP_OK;
}

void
dpn_set_recv_timeout (struct dp_ctx *ctx, int timeout_ms)
{
  ctx->u.nats.recv_timeout_ms = timeout_ms;
}

/* =========================================================================
 * dp_msg accessors for DP_MSG_NATS
 * ========================================================================= */

void *
dpn_msg_data (dp_msg_t *msg)
{
  natsMsg *m = (natsMsg *)msg->u.nats;
  return (void *)((char *)natsMsg_GetData (m) + msg->data_offset);
}

size_t
dpn_msg_size (dp_msg_t *msg)
{
  natsMsg *m = (natsMsg *)msg->u.nats;
  return (size_t)natsMsg_GetDataLength (m) - msg->data_offset;
}

void
dpn_msg_free (dp_msg_t *msg)
{
  natsMsg_Destroy ((natsMsg *)msg->u.nats);
}

int
dpn_msg_ack (dp_msg_t *msg)
{
  natsStatus s = natsMsg_Ack ((natsMsg *)msg->u.nats, NULL);
  return (s == NATS_OK) ? DP_OK : DP_ERR_SEND;
}
