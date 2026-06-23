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
 * Subjects: an endpoint "nats://host:port/<base>" yields a subject base
 * (default "default").  PUB publishes "iq.<base>.<sample_type>"; SUB
 * subscribes "iq.<base>.>" (so the broker can filter by type for free).
 * REQ/REP map onto NATS request/reply: a REQ owns a reply inbox and
 * PublishRequest()s to <base>; a REP SubscribeSync()s <base>, remembers
 * each request's reply subject, and Publish()es the answer there.
 *
 * PUSH/PULL over nats:// is the JetStream work-queue tier (Phase 4) and is
 * not handled here yet — dpn_ctx_create returns NULL for those roles.
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

/* Role-specific subscription wiring after connect.  Returns 0 on success.
 * PUB needs no subscription; SUB/REP/REQ each get one (REQ also an inbox). */
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
  /* PUSH/PULL over NATS is the JetStream work-queue tier (Phase 4). */
  if (role == DP_ROLE_PUSH || role == DP_ROLE_PULL)
    return NULL;

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
  return ctx;
}

void
dpn_ctx_destroy (struct dp_ctx *ctx)
{
  if (ctx->u.nats.sub)
    natsSubscription_Destroy ((natsSubscription *)ctx->u.nats.sub);
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
 * type name (used only by PUB to build "iq.<base>.<type>"). */
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
    default:
      return DP_ERR_INVALID; /* SUB/PULL cannot send */
    }

  return (s == NATS_OK) ? DP_OK : DP_ERR_SEND;
}

int
dpn_send_signal (struct dp_ctx *ctx, const dp_header_t *header,
                 const void *samples, size_t data_size)
{
  /* NATS carries header‖data in one message (zero-copy send is not possible);
   * stage them into a contiguous buffer. */
  size_t total = sizeof (*header) + data_size;
  char  *buf   = (char *)malloc (total);
  if (!buf)
    return DP_ERR_MEMORY;
  memcpy (buf, header, sizeof (*header));
  memcpy (buf + sizeof (*header), samples, data_size);

  int rc = dpn_publish (ctx, dp_sample_type_str (header->sample_type), buf,
                        (int)total);
  free (buf);
  return rc;
}

int
dpn_send_raw (struct dp_ctx *ctx, const void *data, size_t size)
{
  return dpn_publish (ctx, NULL, data, (int)size);
}

/* =========================================================================
 * Receive
 * ========================================================================= */

/* Block for the next message honouring the stored timeout.  recv_timeout_ms
 * < 0 means block indefinitely (emulated by re-polling on NATS_TIMEOUT). */
static int
nats_next (struct dp_ctx *ctx, natsMsg **out)
{
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
