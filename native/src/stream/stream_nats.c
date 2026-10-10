/**
 * @file stream_nats.c
 * @brief NATS transport for the streaming API (the nats_* hooks).
 *
 * Every endpoint uses the "nats://" scheme; stream_core.c's ctx_create()
 * calls straight into dp__nats_ctx_create() below. This translation unit is
 * the only place nats.h is included — stream_core.c never sees a nats type.
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

#include "doppler/dp_thread.h"
#include "doppler/stream/stream.h"
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

/* Record the backend's own account of a failure, so DP_ERR_SEND stops
 * being the whole story. natsStatus_GetText() names the class ("Timeout");
 * nats_GetLastError() adds the detail the client last recorded. */
static void
nats_note_error (struct dp_ctx *ctx, natsStatus s)
{
  natsStatus  last   = NATS_OK;
  const char *detail = nats_GetLastError (&last);
  (void)snprintf (ctx->last_error, sizeof (ctx->last_error), "%s%s%s",
                  natsStatus_GetText (s), (detail && *detail) ? " -- " : "",
                  (detail && *detail) ? detail : "");
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
  /* A work queue drops a frame when a consumer ACKS it -- so a frame
     nobody consumes is kept forever, and this stream is FILE-backed and
     was created with jsStreamConfig_Init's defaults, i.e. no MaxMsgs, no
     MaxBytes and no MaxAge. A producer with no consumer was therefore an
     unbounded disk sink: doppler#1136, 40 GB of residue observed from
     repeated test runs. An age bound is the one limit that cannot silently
     drop a frame a live consumer was about to take.

     An operator who wants different retention PRE-PROVISIONS the stream;
     the AddStream-then-GetStreamInfo path below adopts it as-is, which is
     the override mechanism this already had. */
  cfg.MaxAge = DP_WORK_QUEUE_MAX_AGE_NS;

  natsStatus s = js_AddStream (NULL, js, &cfg, NULL, NULL);
  if (s == NATS_OK)
    return DP_OK;

  jsStreamInfo *si = NULL; /* already exists? use it as-is. */
  s                = js_GetStreamInfo (&si, js, name, NULL, NULL);
  if (si)
    jsStreamInfo_Destroy (si);
  return (s == NATS_OK) ? DP_OK : DP_ERR_INIT;
}

/* Delete the work-queue stream backing this context's subject.
 * Deliberately NOT called from the close path: a work queue is shared
 * infrastructure and outliving one producer is the feature, so ending it
 * has to be something a caller asks for. */
int
dp__nats_delete_stream (struct dp_ctx *ctx)
{
  if (!ctx || !ctx->nats.js)
    return DP_ERR_INVALID;
  char name[256];
  nats_js_name (name, sizeof (name), "DP_WORK_", ctx->nats.base);
  natsStatus s = js_DeleteStream ((jsCtx *)ctx->nats.js, name, NULL, NULL);
  if (s != NATS_OK)
    {
      nats_note_error (ctx, s);
      return DP_ERR_INVALID;
    }
  return DP_OK;
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
  so.Config.MaxAckPending = DP_WORK_QUEUE_MAX_ACK_PENDING; /* backpressure */
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
        /* A worker may start before any producer, so it provisions the
           work queue too: through the same idempotent helper Push uses, so
           there is one stream configuration and a pre-provisioned stream is
           adopted as-is by either side. Binding alone failed on a broker
           that had never carried the stream (#956). Either failure is the
           same failure -- no consumer -- so they share one return. */
        if (nats_ensure_stream (js, ctx->nats.base) != DP_OK
            || nats_pull_subscribe (ctx, js, &sub) != 0)
          return -1;
        break;
      }
    default: /* DP_ROLE_PUB */
      break;
    }

  ctx->nats.sub = sub;
  return 0;
}

/* =========================================================================
 * The context <-> message link (#2016)
 * ========================================================================= */

/* `closed` is set under the mutex BEFORE the context's NATS objects are torn
   down, and a Pull message's ack reads it under the same mutex, so an ack
   already in flight finishes before teardown begins and one that comes
   after is refused (DP_ERR_CLOSED) rather than reading a freed
   subscription. `role` is the context's: only a PULL message is a
   JetStream message, so only it is acked. nats.c's ack is wrong for any
   other, close or no close: a SUB message has no reply subject, so it is
   refused (NATS_ILLEGAL_STATE); a REP request has one, so nats.c reads its
   subscription's JetStream context, which is NULL -- undefined, and in a
   Release build "+ACK" published to the requester's inbox. */
struct dp_msg_link
{
  dp_mutex_t mu;
  size_t     refs;
  int        closed;
  dp_role_t  role;
};

static dp_msg_link_t *
link_create (dp_role_t role)
{
  /* Fixed size, so only OOM can fail it: the abort-on-OOM helper. */
  dp_msg_link_t *l = (dp_msg_link_t *)dp_xcalloc (1, sizeof *l);
  dp_mutex_init (&l->mu);
  l->refs = 1;
  l->role = role;
  return l;
}

static dp_msg_link_t *
link_retain (dp_msg_link_t *l)
{
  dp_mutex_lock (&l->mu);
  l->refs++;
  dp_mutex_unlock (&l->mu);
  return l;
}

/* The count drops under the mutex, but the mutex is destroyed only after
   it is unlocked, and only by the holder of the last reference: nobody
   else can be waiting on it then. */
static void
link_release (dp_msg_link_t *l)
{
  if (!l)
    return;
  dp_mutex_lock (&l->mu);
  int last = --l->refs == 0;
  dp_mutex_unlock (&l->mu);
  if (last)
    {
      dp_mutex_destroy (&l->mu);
      free (l);
    }
}

struct dp_ctx *
dp__nats_ctx_create (dp_role_t role, const char *endpoint,
                     dp_frame_kind_t kind, dp_sample_type_t format)
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
      dp__nats_ctx_destroy (
          ctx); /* frees internals (NULL-safe), not ctx itself */
      free (ctx);
      return NULL;
    }
  /* Cache the server's max message size; frames above it are chunked. */
  ctx->nats.max_payload
      = natsConnection_GetMaxPayload ((natsConnection *)ctx->nats.conn);
  ctx->nats.link = link_create (role);
  return ctx;
}

void
dp__nats_ctx_destroy (struct dp_ctx *ctx)
{
  /* Closed first, under the mutex: an ack in flight completes before the
     subscription it reads goes, and every later one is refused. */
  if (ctx->nats.link)
    {
      dp_mutex_lock (&ctx->nats.link->mu);
      ctx->nats.link->closed = 1;
      dp_mutex_unlock (&ctx->nats.link->mu);
    }
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
  dp_reasm_reset (&ctx->nats.reasm);
  link_release (ctx->nats.link);
  ctx->nats.link = NULL;
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

  ctx->last_error[0] = '\0'; /* a stale detail must not read as this one */

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
  if (s == NATS_OK)
    return DP_OK;
  nats_note_error (ctx, s);
  return DP_ERR_SEND;
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
  /* Guarded like `ch` above: a zero-length payload arrives as (NULL, 0),
     and memcpy's second argument is declared never-null, so the call is
     undefined even though it copies nothing. */
  if (data_len)
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
dp__nats_send_signal (struct dp_ctx *ctx, const dp_header_t *header,
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
dp__nats_drain (struct dp_ctx *ctx, int timeout_ms)
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
dp__nats_flush (struct dp_ctx *ctx, int timeout_ms)
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
dp__nats_send_raw (struct dp_ctx *ctx, const void *data, size_t size)
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

/* A receive's waiting: ONE deadline, in nats_Now() milliseconds (-1 for
 * none), taken once per receive and carried across every message it
 * reads. A chunked frame takes several, and a fresh timeout per message
 * let a run of chunks that never completes a frame -- two publishers
 * interleaving, or a stream of forged chunks -- hold recv(timeout_ms) for
 * as long as messages kept coming (#2010). nats_Now() is the client's own
 * clock, the one natsSubscription_NextMsg and _Fetch time their waits
 * with, so no other clock could make the bound tighter. */
typedef struct
{
  int64_t deadline; /* nats_Now() ms, or -1: no deadline */
  int     polled;   /* this receive has waited at least once */
} nats_wait_t;

static nats_wait_t
nats_wait_begin (const struct dp_ctx *ctx)
{
  int         to = ctx->nats.recv_timeout_ms;
  nats_wait_t w  = { -1, 0 };
  if (to >= 0)
    w.deadline = nats_Now () + (int64_t)(to == 0 ? 1 : to);
  return w;
}

/* The next wait: one interrupt slice, cut to what is left of the
 * deadline; -1 once it has passed. The receive's FIRST wait always gets
 * at least 1 ms, however late it starts: a timeout of 0 is a poll, and
 * the millisecond tick turning between nats_wait_begin() and here must
 * not make it a no-op. Later waits get no such floor, which is what keeps
 * a flood of messages from holding the receive past its deadline. */
static int64_t
nats_wait_next (nats_wait_t *w)
{
  int64_t slice = dp_wait_slice_ms ();
  if (w->deadline >= 0)
    {
      int64_t left = w->deadline - nats_Now ();
      if (left <= 0)
        {
          if (w->polled)
            return -1;
          left = 1;
        }
      if (left < slice)
        slice = left;
    }
  w->polled = 1;
  return slice;
}

/* Pull one message from the durable JetStream consumer (batch of 1).  The
 * message is NOT acked here — the caller acks via dp_msg_ack once it has been
 * processed, so a crash before ack triggers redelivery (at-least-once). */
static int
nats_pull_fetch (struct dp_ctx *ctx, natsMsg **out, nats_wait_t *w)
{
  natsSubscription *sub = (natsSubscription *)ctx->nats.sub;
  if (!sub)
    return DP_ERR_INVALID;

  if (dp_stream_interrupted ())
    return DP_ERR_INTERRUPTED;

  natsMsgList list = { NULL, 0 };
  natsStatus  s;

  /* Sliced for the same reason the SUB path is: a worker parked on an
     empty work queue has to be able to hear dp_stream_interrupt(). */
  for (;;)
    {
      int64_t slice = nats_wait_next (w);
      if (slice < 0)
        return DP_ERR_TIMEOUT;

      s = natsSubscription_Fetch (&list, sub, 1, slice, NULL);
      if (s != NATS_TIMEOUT)
        break;
      if (dp_stream_interrupted ())
        return DP_ERR_INTERRUPTED;
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

/* Block for the next message until the receive's deadline (`w`, from
 * nats_wait_begin()); no deadline blocks indefinitely, emulated by
 * re-polling on NATS_TIMEOUT. */
static int
nats_next (struct dp_ctx *ctx, natsMsg **out, nats_wait_t *w)
{
  if (ctx->nats.role == DP_ROLE_PULL)
    return nats_pull_fetch (ctx, out, w);

  natsSubscription *sub = (natsSubscription *)ctx->nats.sub;
  if (!sub)
    return DP_ERR_INVALID;

  /* Checked BEFORE waiting as well as between slices: a receive started
     after the signal must not park for a slice first, which is what makes
     "interrupt then recv" behave the same as "recv then interrupt". */
  if (dp_stream_interrupted ())
    return DP_ERR_INTERRUPTED;

  /* Both paths are the same loop; the only difference is whether there is
     a deadline to run out of. Slicing the caller's own timeout matters as
     much as slicing an infinite wait: a five-second recv that ignores
     Ctrl+C for five seconds is the same defect, smaller. The deadline is
     checked BEFORE each wait, so a message already queued is not taken
     once it has passed: it waits for the next receive. */
  for (;;)
    {
      int64_t slice = nats_wait_next (w);
      if (slice < 0)
        return DP_ERR_TIMEOUT;

      natsStatus s = natsSubscription_NextMsg (out, sub, slice);
      if (s != NATS_TIMEOUT)
        return (s == NATS_OK) ? DP_OK : DP_ERR_RECV;

      if (dp_stream_interrupted ())
        return DP_ERR_INTERRUPTED;
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

/* Wrap a reassembled payload as a doppler-owned message. */
static int
nats_owned_msg (char *buf, const dp_header_t *fh, dp_msg_t **out_msg,
                dp_header_t *out_hdr)
{
  dp_msg_t *msg = (dp_msg_t *)malloc (sizeof (dp_msg_t));
  if (!msg)
    {
      free (buf);
      return DP_ERR_MEMORY;
    }
  msg->owner       = DP_MSG_OWNED;
  msg->link        = NULL; /* owns its buffer: no context to outlive */
  msg->u.owned.ptr = buf;
  msg->u.owned.len = fh->payload_bytes;
  msg->data_offset = 0;
  msg->kind        = (dp_frame_kind_t)fh->kind;
  msg->format      = (dp_sample_type_t)fh->format;
  msg->num_samples = fh->num_samples;
  *out_msg         = msg;
  if (out_hdr)
    *out_hdr = *fh;
  return DP_OK;
}

int
dp__nats_recv_signal (struct dp_ctx *ctx, dp_msg_t **out_msg,
                      dp_header_t *out_hdr)
{
  /* One message at a time until a whole frame is in hand. Each chunk of a
     large PUB frame is fed to the reassembler (stream_core.c), whose state
     outlives this call: a timeout between chunks keeps the frame in
     progress for the next receive, and a chunk of a different frame starts
     that frame rather than being thrown away (#2010). */
  nats_wait_t wait = nats_wait_begin (ctx);
  for (;;)
    {
      natsMsg *m  = NULL;
      int      rc = nats_next (ctx, &m, &wait);
      if (rc != DP_OK)
        {
          if (rc == DP_ERR_TIMEOUT && ctx->nats.reasm.buf)
            ctx->nats.reasm.stats.mid_frame_timeouts++;
          return rc;
        }

      dp_header_t hdr;
      dp_chunk_t  chunk    = { 0 };
      int         chunked  = 0;
      const char *body     = NULL;
      size_t      body_len = 0;

      rc = nats_parse_frame (m, &hdr, &chunk, &chunked, &body, &body_len);
      if (rc != DP_OK)
        {
          /* A frame nothing can parse is not work, and on an explicit-ack work
             queue leaving it unacked blocks the queue for everyone, forever:
             it redelivers every AckWait, is never removed, and occupies one of
             MaxAckPending's slots until they are all gone. This is the hazard
             the EOS branch below documents, in its other form -- there because
             the caller is handed no message to ack with, here because no
             consumer can ever succeed at this one.

             Term, not Ack: Ack means "processed", and this was not. Term tells
             the server not to redeliver regardless of MaxDeliver, which is the
             only thing that lets the queue move past it.

             The error is still returned rather than skipping to the next
             frame, so a corrupt frame is REPORTED instead of silently
             swallowed -- the queue drains and the caller learns. That is the
             trade this makes: an unparseable frame is dropped, which is a real
             loss if the parser is ever the thing at fault, and the alternative
             is a queue that no consumer can use again.

             Measured 2026-08-28: two long-lived `dp-chain-*` work queues had
             accumulated frames an older build wrote, and every recv against
             them failed instantly and permanently -- a fresh subject on the
             same broker and build round-tripped fine. */
          if (ctx->nats.role == DP_ROLE_PULL)
            (void)natsMsg_Term (m, NULL);
          natsMsg_Destroy (m);
          return rc;
        }

      if (ctx->nats.role == DP_ROLE_REP)
        nats_stash_reply (ctx, m);

      /* End of stream is a STATEMENT, so it is reported rather than handed
         back as an empty frame a caller would have to recognise for itself.
         Checked here -- after the envelope is validated, before anything
         sizes a payload -- because an EOS frame has no format and no
         samples, so the element-size arithmetic below has nothing to work
         with. */
      if ((dp_frame_kind_t)hdr.kind == DP_KIND_EOS)
        {
          /* The publisher has finished, so a frame still in progress lost
             a chunk and never will complete: give it up, counted. */
          dp_reasm_unchunked (&ctx->nats.reasm, &hdr);
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

      /* Large fan-out frames arrive as several chunks: feed this one to the
       * reassembler, and keep reading until a frame completes. (PULL never
       * chunks: the work-queue carries whole frames.) */
      if (chunked && ctx->nats.role != DP_ROLE_PULL)
        {
          int         complete = 0;
          char       *frame    = NULL;
          dp_header_t fh;
          rc = dp_reasm_feed (&ctx->nats.reasm, &hdr, &chunk, body, body_len,
                              &complete, &frame, &fh);
          natsMsg_Destroy (m); /* the reassembler copied what it needed */
          if (rc == DP_ERR_MEMORY)
            return rc;
          if (complete)
            return nats_owned_msg (frame, &fh, out_msg, out_hdr);
          continue; /* placed, or a chunk no frame could hold (counted) */
        }

      /* Single message: zero-copy, data lives in the natsMsg past the
         header. With one publisher per subject, an unchunked frame of the
         SAME stream after a chunked one proves that one lost a chunk: give
         it up, counted, rather than hold its buffer and call every idle
         timeout a mid-frame one. Another type's frame on the base leaves
         it alone. */
      dp_reasm_unchunked (&ctx->nats.reasm, &hdr);
      dp_msg_t *msg = (dp_msg_t *)malloc (sizeof (dp_msg_t));
      if (!msg)
        {
          natsMsg_Destroy (m);
          return DP_ERR_MEMORY;
        }
      msg->owner       = DP_MSG_NATS;
      msg->u.nats      = m;
      msg->link        = link_retain (ctx->nats.link);
      msg->data_offset = (size_t)(body - natsMsg_GetData (m));
      msg->kind        = (dp_frame_kind_t)hdr.kind;
      msg->format      = (dp_sample_type_t)hdr.format;
      msg->num_samples = hdr.num_samples;

      *out_msg = msg;
      if (out_hdr)
        memcpy (out_hdr, &hdr, sizeof (dp_header_t));
      return DP_OK;
    }
}

int
dp__nats_recv_raw (struct dp_ctx *ctx, dp_msg_t **out_msg, size_t *out_size)
{
  natsMsg    *m    = NULL;
  nats_wait_t wait = nats_wait_begin (ctx);
  int         rc   = nats_next (ctx, &m, &wait);
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
  msg->link        = link_retain (ctx->nats.link);
  msg->data_offset = 0;
  msg->kind        = DP_KIND_IQ; /* not meaningful for raw recv */
  msg->format      = CF64;
  msg->num_samples = 0;

  *out_msg  = msg;
  *out_size = (size_t)natsMsg_GetDataLength (m);
  return DP_OK;
}

void
dp__nats_set_recv_timeout (struct dp_ctx *ctx, int timeout_ms)
{
  ctx->nats.recv_timeout_ms = timeout_ms;
}

/* =========================================================================
 * dp_msg accessors for DP_MSG_NATS
 * ========================================================================= */

void *
dp__nats_msg_data (dp_msg_t *msg)
{
  natsMsg *m = (natsMsg *)msg->u.nats;
  return (void *)((char *)natsMsg_GetData (m) + msg->data_offset);
}

size_t
dp__nats_msg_size (dp_msg_t *msg)
{
  natsMsg *m = (natsMsg *)msg->u.nats;
  return (size_t)natsMsg_GetDataLength (m) - msg->data_offset;
}

void
dp__nats_msg_free (dp_msg_t *msg)
{
  /* natsMsg_Destroy never touches the subscription, so this is safe after
     the context is gone; the link outlives both until the last of them. */
  natsMsg_Destroy ((natsMsg *)msg->u.nats);
  link_release (msg->link);
}

int
dp__nats_msg_ack (dp_msg_t *msg)
{
  dp_msg_link_t *l = msg->link;
  /* Role first, and without the lock: it is fixed when the link is created,
     before any message can hold it. A core-NATS message has nothing to
     acknowledge, so its ack is DP_OK whether or not its context is still
     open -- the same answer a reassembled frame from that context gets. */
  if (l->role != DP_ROLE_PULL)
    return DP_OK;
  int rc;
  dp_mutex_lock (&l->mu);
  if (l->closed)
    rc = DP_ERR_CLOSED; /* the broker redelivers it: ack before close */
  else
    rc = natsMsg_Ack ((natsMsg *)msg->u.nats, NULL) == NATS_OK ? DP_OK
                                                               : DP_ERR_SEND;
  dp_mutex_unlock (&l->mu);
  return rc;
}
