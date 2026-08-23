/**
 * @file stream_nats.c
 * @brief NATS transport for the streaming API (the nats_* hooks).
 *
 * Every endpoint uses the "nats://" scheme; stream_core.c's ctx_create()
 * calls straight into nats_ctx_create() below. This translation unit is the
 * only place nats.h is included — stream_core.c never sees a nats type.
 *
 * Wire format: a 96-byte dp_header_t binary prefix followed by the
 * interleaved I/Q payload, all in one NATS message. Receive stays
 * zero-copy — dp_msg_data() points into the natsMsg past the header, and
 * dp_msg_free() does exactly one natsMsg_Destroy().
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
 * and a strdup'd subject base (default "default").  Returns 0 on success,
 * -1 if the endpoint doesn't use the "nats://" scheme (the only backend). */
static int
nats_parse_endpoint (const char *endpoint, char *url, size_t url_sz,
                     char **base_out)
{
  if (strncmp (endpoint, "nats://", 7) != 0)
    return -1;

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
nats_connect (const char *url)
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
nats_subscribe (natsConnection *conn, const char *subj, natsSubscription **sub)
{
  natsStatus s = natsConnection_SubscribeSync (sub, conn, subj);
  if (s == NATS_OK)
    (void)natsSubscription_SetPendingLimits (*sub, -1, -1);
  return s;
}

/* JetStream stream/consumer names cannot contain . * > or space — derive a
 * safe one from the subject base. */
static void
nats_js_name (char *out, size_t n, const char *prefix, const char *base)
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
nats_ensure_stream (jsCtx *js, const char *base)
{
  char name[256], subj[256];
  nats_js_name (name, sizeof (name), "DP_WORK_", base);
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
nats_pull_subscribe (struct dp_ctx *ctx, jsCtx *js, natsSubscription **out)
{
  char durable[256], filter[256];
  nats_js_name (durable, sizeof (durable), "DP_PULL_", ctx->nats.base);
  (void)snprintf (filter, sizeof (filter), "work.%s.>", ctx->nats.base);

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
nats_wire_role (struct dp_ctx *ctx, natsConnection *conn)
{
  natsSubscription *sub = NULL;

  switch (ctx->nats.role)
    {
    case DP_ROLE_SUB:
      {
        char subj[600];
        (void)snprintf (subj, sizeof (subj), "iq.%s.>", ctx->nats.base);
        if (nats_subscribe (conn, subj, &sub) != NATS_OK)
          return -1;
        break;
      }
    case DP_ROLE_REP:
      if (nats_subscribe (conn, ctx->nats.base, &sub) != NATS_OK)
        return -1;
      break;
    case DP_ROLE_REQ:
      {
        natsInbox *inbox = NULL;
        if (natsInbox_Create (&inbox) != NATS_OK)
          return -1;
        ctx->nats.inbox = inbox;
        if (nats_subscribe (conn, inbox, &sub) != NATS_OK)
          return -1;
        break;
      }
    case DP_ROLE_PUSH:
      {
        jsCtx *js = NULL;
        if (natsConnection_JetStream (&js, conn, NULL) != NATS_OK)
          return -1;
        ctx->nats.js = js;
        if (nats_ensure_stream (js, ctx->nats.base) != DP_OK)
          return -1;
        break; /* publisher: no subscription */
      }
    case DP_ROLE_PULL:
      {
        jsCtx *js = NULL;
        if (natsConnection_JetStream (&js, conn, NULL) != NATS_OK)
          return -1;
        ctx->nats.js = js;
        if (nats_pull_subscribe (ctx, js, &sub) != 0)
          return -1;
        break;
      }
    default: /* DP_ROLE_PUB */
      break;
    }

  ctx->nats.sub = sub;
  return 0;
}

struct dp_ctx *
nats_ctx_create (dp_role_t role, const char *endpoint, dp_frame_kind_t kind,
                 dp_sample_type_t format)
{
  struct dp_ctx *ctx = (struct dp_ctx *)calloc (1, sizeof (struct dp_ctx));
  if (!ctx)
    return NULL;
  ctx->kind                 = kind;
  ctx->format               = format;
  ctx->nats.role            = role;
  ctx->nats.recv_timeout_ms = -1; /* block by default */

  char url[512];
  if (nats_parse_endpoint (endpoint, url, sizeof (url), &ctx->nats.base) != 0)
    {
      free (ctx);
      return NULL;
    }

  ctx->nats.conn = nats_connect (url);
  if (!ctx->nats.conn || nats_wire_role (ctx, ctx->nats.conn) != 0)
    {
      nats_ctx_destroy (ctx); /* frees internals (NULL-safe), not ctx itself */
      free (ctx);
      return NULL;
    }
  /* Cache the server's max message size; frames above it are chunked. */
  ctx->nats.max_payload
      = natsConnection_GetMaxPayload ((natsConnection *)ctx->nats.conn);
  return ctx;
}

void
nats_ctx_destroy (struct dp_ctx *ctx)
{
  if (ctx->nats.sub)
    natsSubscription_Destroy ((natsSubscription *)ctx->nats.sub);
  if (ctx->nats.js)
    jsCtx_Destroy ((jsCtx *)ctx->nats.js);
  if (ctx->nats.conn)
    natsConnection_Destroy ((natsConnection *)ctx->nats.conn);
  if (ctx->nats.inbox)
    natsInbox_Destroy ((natsInbox *)ctx->nats.inbox);
  free (ctx->nats.base);
  free (ctx->nats.last_reply);
}

/* =========================================================================
 * Send
 * ========================================================================= */

/* Publish one prebuilt buffer to the role's subject.  typestr is the sample
 * type name (used only by PUB to build "iq.{base}.{type}"). */
static int
nats_publish (struct dp_ctx *ctx, const char *typestr, const void *buf,
              int len)
{
  natsConnection *conn = (natsConnection *)ctx->nats.conn;
  natsStatus      s;

  switch (ctx->nats.role)
    {
    case DP_ROLE_PUB:
      {
        char subj[640];
        (void)snprintf (subj, sizeof (subj), "iq.%s.%s", ctx->nats.base,
                        typestr);
        s = natsConnection_Publish (conn, subj, buf, len);
        break;
      }
    case DP_ROLE_REQ:
      s = natsConnection_PublishRequest (
          conn, ctx->nats.base, (const char *)ctx->nats.inbox, buf, len);
      if (s == NATS_OK)
        s = natsConnection_Flush (conn); /* push the request out now */
      break;
    case DP_ROLE_REP:
      if (!ctx->nats.last_reply)
        return DP_ERR_SEND; /* no request to answer */
      s = natsConnection_Publish (conn, ctx->nats.last_reply, buf, len);
      if (s == NATS_OK)
        s = natsConnection_Flush (conn);
      break;
    case DP_ROLE_PUSH:
      {
        /* JetStream work-queue: synchronous, server-acked publish — the
         * message is persisted (and replicated) before we return, so a
         * producer-side crash never silently drops it. */
        char subj[640];
        (void)snprintf (subj, sizeof (subj), "work.%s.%s", ctx->nats.base,
                        typestr);
        jsPubAck *pa = NULL;
        s = js_Publish (&pa, (jsCtx *)ctx->nats.js, subj, buf, len, NULL,
                        NULL);
        if (pa)
          jsPubAck_Destroy (pa);
        break;
      }
    default:
      return DP_ERR_INVALID; /* SUB/PULL cannot send */
    }

  if (s == NATS_DRAINING || s == NATS_CONNECTION_CLOSED)
    return DP_ERR_CLOSED; /* a state the caller chose, not a failure */
  return (s == NATS_OK) ? DP_OK : DP_ERR_SEND;
}

/* The subject's trailing token: the frame's own format, so a consumer can
 * filter by type at the broker (`iq.base.CF64`). A telemetry frame has no
 * BLUE format, so it says what it is instead. */
static const char *
nats_type_token (const dp_header_t *h)
{
  if ((dp_frame_kind_t)h->kind == DP_KIND_TLM)
    return "TLM16";
  if ((dp_frame_kind_t)h->kind == DP_KIND_EOS)
    return "EOS";
  return dp_sample_type_str ((dp_sample_type_t)h->format);
}

/* Stage one [header][chunk?][payload] message and publish it (zero-copy send
 * is not possible over NATS — it must be one contiguous buffer). `ch` is NULL
 * for the un-chunked case, which is every frame that fits. */
static int
nats_publish_block (struct dp_ctx *ctx, const dp_header_t *h,
                    const dp_chunk_t *ch, const void *data, size_t data_len)
{
  size_t hdr_sz = sizeof (*h);
  size_t ch_sz  = ch ? sizeof (*ch) : 0u;
  char  *buf    = (char *)malloc (hdr_sz + ch_sz + data_len);
  if (!buf)
    return DP_ERR_MEMORY;
  memcpy (buf, h, hdr_sz);
  if (ch)
    memcpy (buf + hdr_sz, ch, ch_sz);
  memcpy (buf + hdr_sz + ch_sz, data, data_len);
  int rc = nats_publish (ctx, nats_type_token (h), buf,
                         (int)(hdr_sz + ch_sz + data_len));
  free (buf);
  return rc;
}

static int
nats_publish_framed (struct dp_ctx *ctx, const dp_header_t *h,
                     const void *data, size_t data_len)
{
  return nats_publish_block (ctx, h, NULL, data, data_len);
}

static int
nats_publish_chunk (struct dp_ctx *ctx, const dp_header_t *h,
                    const dp_chunk_t *ch, const void *data, size_t data_len)
{
  return nats_publish_block (ctx, h, ch, data, data_len);
}

int
nats_send_signal (struct dp_ctx *ctx, const dp_header_t *header,
                  const void *samples, size_t data_size)
{
  size_t  hdr_sz = sizeof (*header);
  int64_t maxp   = ctx->nats.max_payload;
  if (maxp <= 0)
    maxp = 1024LL * 1024; /* NATS default if the server didn't report one */

  /* Chunking is a fan-out (PUB/SUB) feature only: every subscriber receives
   * the whole in-order chunk sequence and reassembles independently.  Over a
   * load-balanced work-queue (PUSH/PULL) a frame's chunks could land on
   * different workers, so PUSH sends one frame as one message (the resilient
   * tier relies on a generous server max_payload).  REQ/REP are small. */
  if (ctx->nats.role != DP_ROLE_PUB)
    {
      /* Non-fan-out roles never chunk, so a frame that won't fit in one
       * message can't be sent — report it distinctly (the bare js_Publish
       * failure is an opaque DP_ERR_SEND) so the caller knows to raise the
       * broker max_payload or use PUB/SUB rather than chase a generic error.
       */
      if (hdr_sz + data_size > (size_t)maxp)
        return DP_ERR_TOO_LARGE;
      return nats_publish_framed (ctx, header, samples, data_size);
    }

  /* PUB that already fits: one un-chunked message (the common case). */
  if (hdr_sz + data_size <= (size_t)maxp)
    return nats_publish_framed (ctx, header, samples, data_size);

  /* Large PUB frame: split into sample-aligned chunks that each fit, all
   * sharing this frame's sequence; (sequence, chunk_index) lets each
   * subscriber reassemble idempotently. */
  size_t ss = dp_element_size ((dp_frame_kind_t)header->kind,
                               (dp_sample_type_t)header->format);
  if (ss == 0)
    return DP_ERR_INVALID;
  /* The chunk block rides between header and payload, so it comes out of
     the same budget. */
  size_t fixed = hdr_sz + sizeof (dp_chunk_t);
  if (fixed >= (size_t)maxp)
    return DP_ERR_INVALID; /* header alone exceeds max_payload */
  size_t max_data = (size_t)maxp - fixed;
  max_data -= max_data % ss; /* whole elements per chunk */
  if (max_data == 0)
    return DP_ERR_INVALID;

  size_t      nchunks = (data_size + max_data - 1) / max_data;
  const char *src     = (const char *)samples;
  for (size_t i = 0; i < nchunks; i++)
    {
      size_t off  = i * max_data;
      size_t take = (data_size - off < max_data) ? data_size - off : max_data;

      dp_header_t h = *header;
      h.flags |= DP_FLAG_CHUNKED;
      h.num_samples   = take / ss;
      h.payload_bytes = (uint32_t)take;

      dp_chunk_t ch  = { 0 };
      ch.index       = (uint32_t)i;
      ch.count       = (uint32_t)nchunks;
      ch.total_bytes = data_size;
      ch.offset      = off;

      int rc = nats_publish_chunk (ctx, &h, &ch, src + off, take);
      if (rc != DP_OK)
        return rc;
    }
  return DP_OK;
}

int
nats_drain (struct dp_ctx *ctx, int timeout_ms)
{
  natsConnection *conn = (natsConnection *)ctx->nats.conn;
  if (!conn)
    return DP_ERR_INVALID;

  natsStatus s = natsConnection_Drain (conn);
  if (s != NATS_OK)
    return DP_ERR_SEND;

  /* Drain returns immediately and finishes in the background. Waiting for
     CLOSED is the whole point: returning here would hand back a context
     whose pending publishes are still unwritten, which is what the drain
     was called to avoid. Polled rather than event-driven because the
     client offers no completion callback for it. */
  int64_t       budget = (timeout_ms > 0) ? (int64_t)timeout_ms : 5000;
  const int64_t step   = 20;
  for (int64_t waited = 0; waited < budget; waited += step)
    {
      if (natsConnection_IsClosed (conn))
        return DP_OK;
      nats_Sleep (step);
    }
  return natsConnection_IsClosed (conn) ? DP_OK : DP_ERR_TIMEOUT;
}

int
nats_flush (struct dp_ctx *ctx, int timeout_ms)
{
  natsConnection *conn = (natsConnection *)ctx->nats.conn;
  if (!conn)
    return DP_ERR_INVALID;
  int64_t    budget = (timeout_ms > 0) ? (int64_t)timeout_ms : 2000;
  natsStatus s      = natsConnection_FlushTimeout (conn, budget);
  if (s == NATS_TIMEOUT)
    return DP_ERR_TIMEOUT;
  return (s == NATS_OK) ? DP_OK : DP_ERR_SEND;
}

int
nats_send_raw (struct dp_ctx *ctx, const void *data, size_t size)
{
  return nats_publish (ctx, NULL, data, (int)size);
}

/* =========================================================================
 * Receive
 * ========================================================================= */

/* How long one wait slice may be: whatever interrupt latency the caller
 * asked for. Read per slice rather than cached, so a change takes effect
 * on a receive that is already blocked. */
static int64_t
dp_wait_slice_ms (void)
{
  return (int64_t)dp_stream_interrupt_latency_ms ();
}

/* Pull one message from the durable JetStream consumer (batch of 1).  The
 * message is NOT acked here — the caller acks via dp_msg_ack once it has been
 * processed, so a crash before ack triggers redelivery (at-least-once). */
static int
nats_pull_fetch (struct dp_ctx *ctx, natsMsg **out)
{
  natsSubscription *sub = (natsSubscription *)ctx->nats.sub;
  if (!sub)
    return DP_ERR_INVALID;

  if (dp_stream_interrupted ())
    return DP_ERR_INTERRUPTED;

  int         to        = ctx->nats.recv_timeout_ms;
  int64_t     remaining = (to < 0) ? -1 : (int64_t)(to == 0 ? 1 : to);
  natsMsgList list      = { NULL, 0 };
  natsStatus  s;

  /* Sliced for the same reason the SUB path is: a worker parked on an
     empty work queue has to be able to hear dp_stream_interrupt(). */
  for (;;)
    {
      int64_t slice = dp_wait_slice_ms ();
      if (remaining >= 0 && remaining < slice)
        slice = remaining;

      s = natsSubscription_Fetch (&list, sub, 1, (int64_t)slice, NULL);
      if (s != NATS_TIMEOUT)
        break;
      if (dp_stream_interrupted ())
        return DP_ERR_INTERRUPTED;
      if (remaining >= 0)
        {
          remaining -= slice;
          if (remaining <= 0)
            return DP_ERR_TIMEOUT;
        }
    }

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
  if (ctx->nats.role == DP_ROLE_PULL)
    return nats_pull_fetch (ctx, out);

  natsSubscription *sub = (natsSubscription *)ctx->nats.sub;
  if (!sub)
    return DP_ERR_INVALID;

  /* Checked BEFORE waiting as well as between slices: a receive started
     after the signal must not park for a slice first, which is what makes
     "interrupt then recv" behave the same as "recv then interrupt". */
  if (dp_stream_interrupted ())
    return DP_ERR_INTERRUPTED;

  int to = ctx->nats.recv_timeout_ms;

  /* Both paths are the same loop; the only difference is whether there is
     a deadline to run out of. Slicing the caller's own timeout matters as
     much as slicing an infinite wait: a five-second recv that ignores
     Ctrl+C for five seconds is the same defect, smaller. */
  int64_t remaining = (to < 0) ? -1 : (int64_t)(to == 0 ? 1 : to);

  for (;;)
    {
      int64_t slice = dp_wait_slice_ms ();
      if (remaining >= 0 && remaining < slice)
        slice = remaining;

      natsStatus s = natsSubscription_NextMsg (out, sub, (int64_t)slice);
      if (s != NATS_TIMEOUT)
        return (s == NATS_OK) ? DP_OK : DP_ERR_RECV;

      if (dp_stream_interrupted ())
        return DP_ERR_INTERRUPTED;

      if (remaining >= 0)
        {
          remaining -= slice;
          if (remaining <= 0)
            return DP_ERR_TIMEOUT;
        }
    }
}

/* A REP must answer the request it just received; remember its reply subject.
 */
static void
nats_stash_reply (struct dp_ctx *ctx, natsMsg *m)
{
  free (ctx->nats.last_reply);
  ctx->nats.last_reply = NULL;
  const char *reply    = natsMsg_GetReply (m);
  if (reply)
    ctx->nats.last_reply = strdup (reply);
}

/* Pull the buffer out of a natsMsg and hand it to dp_frame_parse(), which
 * is where the checking lives -- deliberately, so the rules can be tested
 * against a hand-built buffer with no broker in the way (see
 * native/tests/test_stream_wire.c). */
static int
nats_parse_frame (const natsMsg *m, dp_header_t *hdr, dp_chunk_t *chunk,
                  int *chunked, const char **body, size_t *body_len)
{
  const char *d   = natsMsg_GetData ((natsMsg *)m);
  int         len = natsMsg_GetDataLength ((natsMsg *)m);
  if (!d || len < 0)
    return DP_ERR_INVALID;

  const void *b = NULL;
  int rc = dp_frame_parse (d, (size_t)len, hdr, chunk, chunked, &b, body_len);
  if (rc == DP_OK)
    *body = (const char *)b;
  return rc;
}

/* Validate one chunk message and copy its payload into the reassembly buffer
 * at its byte offset.  Idempotent: a redelivered chunk (seen[idx]) is a no-op.
 * Does not destroy m. */
static int
nats_place_chunk (char *buf, size_t total_bytes, uint32_t nchunks,
                  uint64_t sequence, natsMsg *m, unsigned char *seen,
                  uint64_t *received)
{
  dp_header_t h;
  dp_chunk_t  ch;
  int         chunked = 0;
  const char *body    = NULL;
  size_t      cbytes  = 0;

  int rc = nats_parse_frame (m, &h, &ch, &chunked, &body, &cbytes);
  if (rc != DP_OK)
    return rc;
  if (!chunked || h.sequence != sequence || ch.count != nchunks)
    return DP_ERR_INVALID;
  if (ch.index >= nchunks || ch.offset + cbytes > total_bytes)
    return DP_ERR_INVALID;

  if (!seen[ch.index])
    {
      memcpy (buf + ch.offset, body, cbytes);
      seen[ch.index] = 1;
      (*received)++;
    }
  return DP_OK;
}

/* Reassemble a chunked frame into one doppler-owned buffer.  `first` is the
 * already-received chunk; fhdr/fch are its parsed header and chunk block.
 * Consumes `first` and any further chunks fetched.  Returns a DP_MSG_OWNED
 * message on success. */
static int
nats_reassemble (struct dp_ctx *ctx, natsMsg *first, const dp_header_t *fhdr,
                 const dp_chunk_t *fch, dp_msg_t **out_msg,
                 dp_header_t *out_hdr)
{
  size_t   elem        = dp_element_size ((dp_frame_kind_t)fhdr->kind,
                                          (dp_sample_type_t)fhdr->format);
  uint32_t nchunks     = fch->count;
  size_t   total_bytes = (size_t)fch->total_bytes;
  if (elem == 0 || nchunks == 0 || total_bytes % elem != 0)
    {
      natsMsg_Destroy (first);
      return DP_ERR_INVALID;
    }

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
      rc = nats_place_chunk (buf, total_bytes, nchunks, fhdr->sequence, m,
                             seen, &received);
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
  msg->owner       = DP_MSG_OWNED;
  msg->u.owned.ptr = buf;
  msg->u.owned.len = total_bytes;
  msg->data_offset = 0;
  msg->kind        = (dp_frame_kind_t)fhdr->kind;
  msg->format      = (dp_sample_type_t)fhdr->format;
  msg->num_samples = total_bytes / elem;

  *out_msg = msg;
  if (out_hdr)
    {
      *out_hdr = *fhdr; /* present a clean logical-frame header */
      out_hdr->flags &= (uint16_t)~DP_FLAG_CHUNKED;
      out_hdr->num_samples   = msg->num_samples;
      out_hdr->payload_bytes = (uint32_t)total_bytes;
    }
  return DP_OK;
}

int
nats_recv_signal (struct dp_ctx *ctx, dp_msg_t **out_msg, dp_header_t *out_hdr)
{
  natsMsg *m  = NULL;
  int      rc = nats_next (ctx, &m);
  if (rc != DP_OK)
    return rc;

  dp_header_t hdr;
  dp_chunk_t  chunk    = { 0 };
  int         chunked  = 0;
  const char *body     = NULL;
  size_t      body_len = 0;

  rc = nats_parse_frame (m, &hdr, &chunk, &chunked, &body, &body_len);
  if (rc != DP_OK)
    {
      natsMsg_Destroy (m);
      return rc;
    }

  if (ctx->nats.role == DP_ROLE_REP)
    nats_stash_reply (ctx, m);

  /* End of stream is a STATEMENT, so it is reported rather than handed back
     as an empty frame a caller would have to recognise for itself. Checked
     here -- after the envelope is validated, before anything sizes a payload
     -- because an EOS frame has no format and no samples, so the element-size
     arithmetic below has nothing to work with. */
  if ((dp_frame_kind_t)hdr.kind == DP_KIND_EOS)
    {
      if (out_hdr)
        memcpy (out_hdr, &hdr, sizeof (dp_header_t));
      /* Ack it HERE, which is the one place that can. PULL is an
         explicit-ack consumer on a work-queue stream, and the caller is
         handed no message -- so if this frame is not acked now, nothing
         can ever ack it: it redelivers every AckWait forever and is never
         removed from the stream, and the NEXT run against the subject
         opens onto an ending that belongs to the previous one. The other
         roles have no ack to give. */
      if (ctx->nats.role == DP_ROLE_PULL)
        (void)natsMsg_Ack (m, NULL);
      natsMsg_Destroy (m);
      *out_msg = NULL;
      return DP_ERR_EOF;
    }

  /* Large fan-out frames arrive as several chunks — reassemble into one owned
   * buffer.  (PULL never chunks: the work-queue carries whole frames.) */
  if (chunked && ctx->nats.role != DP_ROLE_PULL)
    return nats_reassemble (ctx, m, &hdr, &chunk, out_msg, out_hdr);

  /* Single message: zero-copy, data lives in the natsMsg past the header. */
  dp_msg_t *msg = (dp_msg_t *)malloc (sizeof (dp_msg_t));
  if (!msg)
    {
      natsMsg_Destroy (m);
      return DP_ERR_MEMORY;
    }
  msg->owner       = DP_MSG_NATS;
  msg->u.nats      = m;
  msg->data_offset = (size_t)(body - natsMsg_GetData (m));
  msg->kind        = (dp_frame_kind_t)hdr.kind;
  msg->format      = (dp_sample_type_t)hdr.format;
  msg->num_samples = hdr.num_samples;

  *out_msg = msg;
  if (out_hdr)
    memcpy (out_hdr, &hdr, sizeof (dp_header_t));
  return DP_OK;
}

int
nats_recv_raw (struct dp_ctx *ctx, dp_msg_t **out_msg, size_t *out_size)
{
  natsMsg *m  = NULL;
  int      rc = nats_next (ctx, &m);
  if (rc != DP_OK)
    return rc;

  if (ctx->nats.role == DP_ROLE_REP)
    nats_stash_reply (ctx, m);

  dp_msg_t *msg = (dp_msg_t *)malloc (sizeof (dp_msg_t));
  if (!msg)
    {
      natsMsg_Destroy (m);
      return DP_ERR_MEMORY;
    }
  msg->owner       = DP_MSG_NATS;
  msg->u.nats      = m;
  msg->data_offset = 0;
  msg->kind        = DP_KIND_IQ; /* not meaningful for raw recv */
  msg->format      = CF64;
  msg->num_samples = 0;

  *out_msg  = msg;
  *out_size = (size_t)natsMsg_GetDataLength (m);
  return DP_OK;
}

void
nats_set_recv_timeout (struct dp_ctx *ctx, int timeout_ms)
{
  ctx->nats.recv_timeout_ms = timeout_ms;
}

/* =========================================================================
 * dp_msg accessors for DP_MSG_NATS
 * ========================================================================= */

void *
nats_msg_data (dp_msg_t *msg)
{
  natsMsg *m = (natsMsg *)msg->u.nats;
  return (void *)((char *)natsMsg_GetData (m) + msg->data_offset);
}

size_t
nats_msg_size (dp_msg_t *msg)
{
  natsMsg *m = (natsMsg *)msg->u.nats;
  return (size_t)natsMsg_GetDataLength (m) - msg->data_offset;
}

void
nats_msg_free (dp_msg_t *msg)
{
  natsMsg_Destroy ((natsMsg *)msg->u.nats);
}

int
nats_msg_ack (dp_msg_t *msg)
{
  natsStatus s = natsMsg_Ack ((natsMsg *)msg->u.nats, NULL);
  return (s == NATS_OK) ? DP_OK : DP_ERR_SEND;
}
