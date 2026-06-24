#include "stream/stream.h"
#include "stream_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zmq.h>

/* =========================================================================
 * Backend seam
 *
 * The transport is selected per context by the endpoint scheme (see
 * parse_backend): a "nats://…" endpoint routes to the NATS backend
 * (stream_nats.c, dpn_*), anything else (tcp://, ipc://, inproc://) to the
 * ZMQ backend (dpz_*, below).  struct dp_ctx / struct dp_msg and the enums
 * live in stream_internal.h so both translation units share them.
 * ========================================================================= */

/* =========================================================================
 * dp_msg_t — zero-copy message accessors (dispatch on how the buffer is owned)
 * ========================================================================= */

void *
dp_msg_data (dp_msg_t *msg)
{
  if (!msg)
    return NULL;
  switch (msg->kind)
    {
    case DP_MSG_ZMQ:
      return zmq_msg_data (&msg->u.zmq);
    case DP_MSG_NATS:
      return dpn_msg_data (msg);
    default:
      return NULL;
    }
}

size_t
dp_msg_size (dp_msg_t *msg)
{
  if (!msg)
    return 0;
  switch (msg->kind)
    {
    case DP_MSG_ZMQ:
      return zmq_msg_size (&msg->u.zmq);
    case DP_MSG_NATS:
      return dpn_msg_size (msg);
    default:
      return 0;
    }
}

size_t
dp_msg_num_samples (dp_msg_t *msg)
{
  return msg ? msg->num_samples : 0;
}

dp_sample_type_t
dp_msg_sample_type (dp_msg_t *msg)
{
  return msg ? msg->sample_type : CF64;
}

void
dp_msg_free (dp_msg_t *msg)
{
  if (!msg)
    return;
  switch (msg->kind)
    {
    case DP_MSG_ZMQ:
      zmq_msg_close (&msg->u.zmq);
      break;
    case DP_MSG_NATS:
      dpn_msg_free (msg);
      break;
    }
  free (msg);
}

/* =========================================================================
 * Utilities
 * ========================================================================= */

uint64_t
dp_get_timestamp_ns (void)
{
  struct timespec ts;
  clock_gettime (CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

size_t
dp_sample_size (dp_sample_type_t type)
{
  switch (type)
    {
    case CI8:
      return 2 * sizeof (int8_t);
    case CI16:
      return 2 * sizeof (int16_t);
    case CI32:
      return 2 * sizeof (int32_t);
    case CF32:
      return sizeof (float _Complex);
    case CF64:
      return sizeof (double _Complex);
    case CF128:
      return sizeof (long double _Complex);
    default:
      return 0;
    }
}

const char *
dp_sample_type_str (dp_sample_type_t type)
{
  switch (type)
    {
    case CI8:
      return "CI8";
    case CI16:
      return "CI16";
    case CI32:
      return "CI32";
    case CF32:
      return "CF32";
    case CF64:
      return "CF64";
    case CF128:
      return "CF128";
    default:
      return "UNKNOWN";
    }
}

const char *
dp_strerror (int err)
{
  switch (err)
    {
    case DP_OK:
      return "Success";
    case DP_ERR_INIT:
      return "Initialization error";
    case DP_ERR_SEND:
      return "Send error";
    case DP_ERR_RECV:
      return "Receive error";
    case DP_ERR_INVALID:
      return "Invalid argument";
    case DP_ERR_TIMEOUT:
      return "Timeout";
    case DP_ERR_MEMORY:
      return "Memory allocation error";
    default:
      return "Unknown error";
    }
}

/* =========================================================================
 * Backend selection
 * ========================================================================= */

static dp_backend_t
parse_backend (const char *endpoint)
{
  return (strncmp (endpoint, "nats://", 7) == 0) ? DP_BACKEND_NATS
                                                 : DP_BACKEND_ZMQ;
}

/* =========================================================================
 * ZMQ backend (dpz_*)  — named dpz_ rather than zmq_ to stay clear of
 * libzmq's own zmq_* symbol namespace.
 * ========================================================================= */

static int
dpz_role_to_socket_type (dp_role_t role)
{
  switch (role)
    {
    case DP_ROLE_PUB:
      return ZMQ_PUB;
    case DP_ROLE_SUB:
      return ZMQ_SUB;
    case DP_ROLE_PUSH:
      return ZMQ_PUSH;
    case DP_ROLE_PULL:
      return ZMQ_PULL;
    case DP_ROLE_REQ:
      return ZMQ_REQ;
    case DP_ROLE_REP:
      return ZMQ_REP;
    }
  return -1;
}

static int
role_binds (dp_role_t role)
{
  return role == DP_ROLE_PUB || role == DP_ROLE_PUSH || role == DP_ROLE_REP;
}

static struct dp_ctx *
dpz_ctx_create (dp_role_t role, const char *endpoint,
                dp_sample_type_t sample_type)
{
  int zmq_type = dpz_role_to_socket_type (role);

  struct dp_ctx *ctx = (struct dp_ctx *)calloc (1, sizeof (struct dp_ctx));
  if (!ctx)
    return NULL;
  ctx->backend = DP_BACKEND_ZMQ;

  ctx->u.zmq.context = zmq_ctx_new ();
  if (!ctx->u.zmq.context)
    {
      free (ctx);
      return NULL;
    }

  ctx->u.zmq.socket = zmq_socket (ctx->u.zmq.context, zmq_type);
  if (!ctx->u.zmq.socket)
    {
      zmq_ctx_destroy (ctx->u.zmq.context);
      free (ctx);
      return NULL;
    }

  /* High water mark */
  int hwm = 1000;
  if (zmq_type == ZMQ_PUB || zmq_type == ZMQ_PUSH)
    zmq_setsockopt (ctx->u.zmq.socket, ZMQ_SNDHWM, &hwm, sizeof (hwm));
  else if (zmq_type == ZMQ_SUB || zmq_type == ZMQ_PULL)
    zmq_setsockopt (ctx->u.zmq.socket, ZMQ_RCVHWM, &hwm, sizeof (hwm));

  /* Subscribe to all messages for SUB sockets */
  if (zmq_type == ZMQ_SUB)
    zmq_setsockopt (ctx->u.zmq.socket, ZMQ_SUBSCRIBE, "", 0);

  int rc = role_binds (role) ? zmq_bind (ctx->u.zmq.socket, endpoint)
                             : zmq_connect (ctx->u.zmq.socket, endpoint);
  if (rc != 0)
    {
      zmq_close (ctx->u.zmq.socket);
      zmq_ctx_destroy (ctx->u.zmq.context);
      free (ctx);
      return NULL;
    }

  ctx->sample_type = sample_type;
  ctx->sequence    = 0;
  return ctx;
}

static void
dpz_ctx_destroy (struct dp_ctx *ctx)
{
  if (ctx->u.zmq.socket)
    zmq_close (ctx->u.zmq.socket);
  if (ctx->u.zmq.context)
    zmq_ctx_destroy (ctx->u.zmq.context);
}

static int
dpz_send_signal (struct dp_ctx *ctx, const dp_header_t *header,
                 const void *samples, size_t data_size)
{
  int rc = zmq_send (ctx->u.zmq.socket, header, sizeof (*header), ZMQ_SNDMORE);
  if (rc == -1)
    return DP_ERR_SEND;

  rc = zmq_send (ctx->u.zmq.socket, samples, data_size, 0);
  if (rc == -1)
    return DP_ERR_SEND;

  return DP_OK;
}

static int
dpz_recv_signal (struct dp_ctx *ctx, dp_msg_t **out_msg, dp_header_t *out_hdr)
{
  /* Receive header frame */
  dp_header_t hdr;
  int         rc = zmq_recv (ctx->u.zmq.socket, &hdr, sizeof (hdr), 0);
  if (rc == -1)
    {
      if (zmq_errno () == EAGAIN)
        return DP_ERR_TIMEOUT;
      return DP_ERR_RECV;
    }
  if ((size_t)rc != sizeof (hdr))
    return DP_ERR_INVALID;

  /* Validate magic */
  if (hdr.magic != DP_MAGIC)
    return DP_ERR_INVALID;

  /* Check for data frame */
  int    more;
  size_t more_size = sizeof (more);
  zmq_getsockopt (ctx->u.zmq.socket, ZMQ_RCVMORE, &more, &more_size);
  if (!more)
    return DP_ERR_INVALID;

  /* Allocate message wrapper (tiny struct, not the data) */
  dp_msg_t *msg = (dp_msg_t *)malloc (sizeof (dp_msg_t));
  if (!msg)
    {
      /* Drain the data frame to keep socket state clean */
      zmq_msg_t discard;
      zmq_msg_init (&discard);
      zmq_msg_recv (&discard, ctx->u.zmq.socket, 0);
      zmq_msg_close (&discard);
      return DP_ERR_MEMORY;
    }

  /* Zero-copy recv: ZMQ owns the buffer */
  msg->kind        = DP_MSG_ZMQ;
  msg->data_offset = 0;
  zmq_msg_init (&msg->u.zmq);
  rc = zmq_msg_recv (&msg->u.zmq, ctx->u.zmq.socket, 0);
  if (rc == -1)
    {
      zmq_msg_close (&msg->u.zmq);
      free (msg);
      return DP_ERR_RECV;
    }

  msg->sample_type = (dp_sample_type_t)hdr.sample_type;
  msg->num_samples = hdr.num_samples;

  *out_msg = msg;
  if (out_hdr)
    memcpy (out_hdr, &hdr, sizeof (dp_header_t));

  return DP_OK;
}

static int
dpz_recv_raw (struct dp_ctx *ctx, dp_msg_t **out_msg, size_t *out_size)
{
  dp_msg_t *msg = (dp_msg_t *)malloc (sizeof (dp_msg_t));
  if (!msg)
    return DP_ERR_MEMORY;

  msg->kind        = DP_MSG_ZMQ;
  msg->data_offset = 0;
  zmq_msg_init (&msg->u.zmq);
  int rc = zmq_msg_recv (&msg->u.zmq, ctx->u.zmq.socket, 0);
  if (rc == -1)
    {
      zmq_msg_close (&msg->u.zmq);
      free (msg);
      if (zmq_errno () == EAGAIN)
        return DP_ERR_TIMEOUT;
      return DP_ERR_RECV;
    }

  msg->sample_type = CF64; /* not meaningful for raw recv */
  msg->num_samples = 0;

  *out_msg  = msg;
  *out_size = zmq_msg_size (&msg->u.zmq);

  return DP_OK;
}

static int
dpz_send_raw (struct dp_ctx *ctx, const void *data, size_t size)
{
  int rc = zmq_send (ctx->u.zmq.socket, data, size, 0);
  return (rc == -1) ? DP_ERR_SEND : DP_OK;
}

static void
dpz_set_recv_timeout (struct dp_ctx *ctx, int timeout_ms)
{
  zmq_setsockopt (ctx->u.zmq.socket, ZMQ_RCVTIMEO, &timeout_ms,
                  sizeof (timeout_ms));
}

/* =========================================================================
 * Shared dispatch funnels — backend-agnostic framing + a one-line branch
 * ========================================================================= */

static struct dp_ctx *
ctx_create (dp_role_t role, const char *endpoint, dp_sample_type_t sample_type)
{
  if (!endpoint)
    return NULL;
  if (parse_backend (endpoint) == DP_BACKEND_NATS)
    return dpn_ctx_create (role, endpoint, sample_type);
  return dpz_ctx_create (role, endpoint, sample_type);
}

static void
ctx_destroy (struct dp_ctx *ctx)
{
  if (!ctx)
    return;
  if (ctx->backend == DP_BACKEND_NATS)
    dpn_ctx_destroy (ctx);
  else
    dpz_ctx_destroy (ctx);
  free (ctx);
}

static int
send_signal (struct dp_ctx *ctx, const void *samples, size_t num_samples,
             double sample_rate, double center_freq, dp_sample_type_t type)
{
  if (!ctx || !samples || num_samples == 0)
    return DP_ERR_INVALID;

  dp_header_t header  = { 0 };
  header.magic        = DP_MAGIC;
  header.version      = DP_VERSION;
  header.protocol     = DP_PROTO_SIGS;
  header.stream_id    = 0;
  header.sample_type  = type;
  header.flags        = 0;
  header.sequence     = ctx->sequence++;
  header.timestamp_ns = dp_get_timestamp_ns ();
  header.sample_rate  = sample_rate;
  header.center_freq  = center_freq;
  header.num_samples  = num_samples;

  size_t data_size = num_samples * dp_sample_size (type);

  if (ctx->backend == DP_BACKEND_NATS)
    return dpn_send_signal (ctx, &header, samples, data_size);
  return dpz_send_signal (ctx, &header, samples, data_size);
}

static int
recv_signal (struct dp_ctx *ctx, dp_msg_t **out_msg, dp_header_t *out_hdr)
{
  if (!ctx || !out_msg)
    return DP_ERR_INVALID;
  if (ctx->backend == DP_BACKEND_NATS)
    return dpn_recv_signal (ctx, out_msg, out_hdr);
  return dpz_recv_signal (ctx, out_msg, out_hdr);
}

static int
recv_raw (struct dp_ctx *ctx, dp_msg_t **out_msg, size_t *out_size)
{
  if (!ctx || !out_msg || !out_size)
    return DP_ERR_INVALID;
  if (ctx->backend == DP_BACKEND_NATS)
    return dpn_recv_raw (ctx, out_msg, out_size);
  return dpz_recv_raw (ctx, out_msg, out_size);
}

static int
send_raw (struct dp_ctx *ctx, const void *data, size_t size)
{
  if (!ctx || !data || size == 0)
    return DP_ERR_INVALID;
  if (ctx->backend == DP_BACKEND_NATS)
    return dpn_send_raw (ctx, data, size);
  return dpz_send_raw (ctx, data, size);
}

static void
set_recv_timeout (struct dp_ctx *ctx, int timeout_ms)
{
  if (!ctx)
    return;
  if (ctx->backend == DP_BACKEND_NATS)
    dpn_set_recv_timeout (ctx, timeout_ms);
  else
    dpz_set_recv_timeout (ctx, timeout_ms);
}

/* =========================================================================
 * PUB/SUB
 * ========================================================================= */

dp_pub_t *
dp_pub_create (const char *endpoint, dp_sample_type_t sample_type)
{
  return ctx_create (DP_ROLE_PUB, endpoint, sample_type);
}

int
dp_pub_send_ci32 (dp_pub_t *ctx, const int32_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CI32);
}

int
dp_pub_send_cf64 (dp_pub_t *ctx, const double _Complex *samples,
                  size_t num_samples, double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CF64);
}

int
dp_pub_send_cf128 (dp_pub_t *ctx, const long double _Complex *samples,
                   size_t num_samples, double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CF128);
}

int
dp_pub_send_ci8 (dp_pub_t *ctx, const int8_t *samples, size_t num_samples,
                 double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CI8);
}

int
dp_pub_send_ci16 (dp_pub_t *ctx, const int16_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CI16);
}

int
dp_pub_send_cf32 (dp_pub_t *ctx, const float _Complex *samples,
                  size_t num_samples, double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CF32);
}

void
dp_pub_destroy (dp_pub_t *ctx)
{
  ctx_destroy (ctx);
}

dp_sub_t *
dp_sub_create (const char *endpoint)
{
  return ctx_create (DP_ROLE_SUB, endpoint, CF64);
}

int
dp_sub_recv (dp_sub_t *ctx, dp_msg_t **msg, dp_header_t *header)
{
  return recv_signal (ctx, msg, header);
}

void
dp_sub_set_timeout (dp_sub_t *ctx, int timeout_ms)
{
  set_recv_timeout (ctx, timeout_ms);
}

void
dp_sub_destroy (dp_sub_t *ctx)
{
  ctx_destroy (ctx);
}

/* =========================================================================
 * PUSH/PULL
 * ========================================================================= */

dp_push_t *
dp_push_create (const char *endpoint, dp_sample_type_t sample_type)
{
  return ctx_create (DP_ROLE_PUSH, endpoint, sample_type);
}

int
dp_push_send_ci32 (dp_push_t *ctx, const int32_t *samples, size_t num_samples,
                   double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CI32);
}

int
dp_push_send_cf64 (dp_push_t *ctx, const double _Complex *samples,
                   size_t num_samples, double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CF64);
}

int
dp_push_send_cf128 (dp_push_t *ctx, const long double _Complex *samples,
                    size_t num_samples, double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CF128);
}

int
dp_push_send_ci8 (dp_push_t *ctx, const int8_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CI8);
}

int
dp_push_send_ci16 (dp_push_t *ctx, const int16_t *samples, size_t num_samples,
                   double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CI16);
}

int
dp_push_send_cf32 (dp_push_t *ctx, const float _Complex *samples,
                   size_t num_samples, double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CF32);
}

dp_pull_t *
dp_pull_create (const char *endpoint)
{
  return ctx_create (DP_ROLE_PULL, endpoint, CF64);
}

int
dp_pull_recv (dp_pull_t *ctx, dp_msg_t **msg, dp_header_t *header)
{
  return recv_signal (ctx, msg, header);
}

void
dp_pull_set_timeout (dp_pull_t *ctx, int timeout_ms)
{
  set_recv_timeout (ctx, timeout_ms);
}

void
dp_push_destroy (dp_push_t *ctx)
{
  ctx_destroy (ctx);
}

void
dp_pull_destroy (dp_pull_t *ctx)
{
  ctx_destroy (ctx);
}

/* =========================================================================
 * REQ/REP
 * ========================================================================= */

dp_req_t *
dp_req_create (const char *endpoint)
{
  return ctx_create (DP_ROLE_REQ, endpoint, CF64);
}

dp_rep_t *
dp_rep_create (const char *endpoint)
{
  return ctx_create (DP_ROLE_REP, endpoint, CF64);
}

/* -- Raw-bytes send/recv ------------------------------------------------ */

int
dp_req_send (dp_req_t *ctx, const void *data, size_t size)
{
  return send_raw (ctx, data, size);
}

int
dp_rep_send (dp_rep_t *ctx, const void *data, size_t size)
{
  return send_raw (ctx, data, size);
}

int
dp_req_recv (dp_req_t *ctx, dp_msg_t **msg, size_t *size)
{
  return recv_raw (ctx, msg, size);
}

int
dp_rep_recv (dp_rep_t *ctx, dp_msg_t **msg, size_t *size)
{
  return recv_raw (ctx, msg, size);
}

/* -- Signal-frame send/recv --------------------------------------------- */

int
dp_req_send_ci32 (dp_req_t *ctx, const int32_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CI32);
}

int
dp_req_send_cf64 (dp_req_t *ctx, const double _Complex *samples,
                  size_t num_samples, double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CF64);
}

int
dp_req_send_cf128 (dp_req_t *ctx, const long double _Complex *samples,
                   size_t num_samples, double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CF128);
}

int
dp_req_send_ci8 (dp_req_t *ctx, const int8_t *samples, size_t num_samples,
                 double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CI8);
}

int
dp_req_send_ci16 (dp_req_t *ctx, const int16_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CI16);
}

int
dp_req_send_cf32 (dp_req_t *ctx, const float _Complex *samples,
                  size_t num_samples, double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CF32);
}

int
dp_rep_send_ci32 (dp_rep_t *ctx, const int32_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CI32);
}

int
dp_rep_send_cf64 (dp_rep_t *ctx, const double _Complex *samples,
                  size_t num_samples, double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CF64);
}

int
dp_rep_send_cf128 (dp_rep_t *ctx, const long double _Complex *samples,
                   size_t num_samples, double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CF128);
}

int
dp_rep_send_ci8 (dp_rep_t *ctx, const int8_t *samples, size_t num_samples,
                 double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CI8);
}

int
dp_rep_send_ci16 (dp_rep_t *ctx, const int16_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CI16);
}

int
dp_rep_send_cf32 (dp_rep_t *ctx, const float _Complex *samples,
                  size_t num_samples, double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      CF32);
}

int
dp_req_recv_signal (dp_req_t *ctx, dp_msg_t **msg, dp_header_t *header)
{
  return recv_signal (ctx, msg, header);
}

int
dp_rep_recv_signal (dp_rep_t *ctx, dp_msg_t **msg, dp_header_t *header)
{
  return recv_signal (ctx, msg, header);
}

/* -- Timeout setters ---------------------------------------------------- */

void
dp_req_set_timeout (dp_req_t *ctx, int timeout_ms)
{
  set_recv_timeout (ctx, timeout_ms);
}

void
dp_rep_set_timeout (dp_rep_t *ctx, int timeout_ms)
{
  set_recv_timeout (ctx, timeout_ms);
}

/* -- Destroy ------------------------------------------------------------ */

void
dp_req_destroy (dp_req_t *ctx)
{
  ctx_destroy (ctx);
}

void
dp_rep_destroy (dp_rep_t *ctx)
{
  ctx_destroy (ctx);
}
