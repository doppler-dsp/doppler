#include <dp/stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zmq.h>

#define DP_MAGIC 0x53494753   /* "SIGS" */
#define DP_VERSION 0x00010000 /* v1.0.0 */

/* =========================================================================
 * Internal context structure (shared by all socket types)
 * ========================================================================= */

struct dp_ctx
{
  void *zmq_context;
  void *zmq_socket;
  dp_sample_type_t sample_type;
  uint64_t sequence;
  int socket_type;
};

/* =========================================================================
 * dp_msg_t — zero-copy message handle
 * ========================================================================= */

struct dp_msg
{
  zmq_msg_t zmq_msg;
  dp_sample_type_t sample_type;
  size_t num_samples;
};

void *
dp_msg_data (dp_msg_t *msg)
{
  return msg ? zmq_msg_data (&msg->zmq_msg) : NULL;
}

size_t
dp_msg_size (dp_msg_t *msg)
{
  return msg ? zmq_msg_size (&msg->zmq_msg) : 0;
}

size_t
dp_msg_num_samples (dp_msg_t *msg)
{
  return msg ? msg->num_samples : 0;
}

dp_sample_type_t
dp_msg_sample_type (dp_msg_t *msg)
{
  return msg ? msg->sample_type : DP_CF64;
}

void
dp_msg_free (dp_msg_t *msg)
{
  if (!msg)
    return;
  zmq_msg_close (&msg->zmq_msg);
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
    case DP_CI8:
      return sizeof (dp_ci8_t);
    case DP_CI16:
      return sizeof (dp_ci16_t);
    case DP_CI32:
      return sizeof (dp_ci32_t);
    case DP_CF32:
      return sizeof (dp_cf32_t);
    case DP_CF64:
      return sizeof (dp_cf64_t);
    case DP_CF128:
      return sizeof (dp_cf128_t);
    default:
      return 0;
    }
}

const char *
dp_sample_type_str (dp_sample_type_t type)
{
  switch (type)
    {
    case DP_CI8:
      return "CI8";
    case DP_CI16:
      return "CI16";
    case DP_CI32:
      return "CI32";
    case DP_CF32:
      return "CF32";
    case DP_CF64:
      return "CF64";
    case DP_CF128:
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
 * De-duplicated socket creation / destruction
 * ========================================================================= */

static struct dp_ctx *
ctx_create (int zmq_type, const char *endpoint, int do_bind,
            dp_sample_type_t sample_type)
{
  if (!endpoint)
    return NULL;

  struct dp_ctx *ctx = (struct dp_ctx *)calloc (1, sizeof (struct dp_ctx));
  if (!ctx)
    return NULL;

  ctx->zmq_context = zmq_ctx_new ();
  if (!ctx->zmq_context)
    {
      free (ctx);
      return NULL;
    }

  ctx->zmq_socket = zmq_socket (ctx->zmq_context, zmq_type);
  if (!ctx->zmq_socket)
    {
      zmq_ctx_destroy (ctx->zmq_context);
      free (ctx);
      return NULL;
    }

  /* Set high water mark */
  int hwm = 1000;
  if (zmq_type == ZMQ_PUB || zmq_type == ZMQ_PUSH)
    zmq_setsockopt (ctx->zmq_socket, ZMQ_SNDHWM, &hwm, sizeof (hwm));
  else if (zmq_type == ZMQ_SUB || zmq_type == ZMQ_PULL)
    zmq_setsockopt (ctx->zmq_socket, ZMQ_RCVHWM, &hwm, sizeof (hwm));

  /* Subscribe to all messages for SUB sockets */
  if (zmq_type == ZMQ_SUB)
    zmq_setsockopt (ctx->zmq_socket, ZMQ_SUBSCRIBE, "", 0);

  int rc = do_bind ? zmq_bind (ctx->zmq_socket, endpoint)
                   : zmq_connect (ctx->zmq_socket, endpoint);
  if (rc != 0)
    {
      zmq_close (ctx->zmq_socket);
      zmq_ctx_destroy (ctx->zmq_context);
      free (ctx);
      return NULL;
    }

  ctx->sample_type = sample_type;
  ctx->sequence = 0;
  ctx->socket_type = zmq_type;

  return ctx;
}

static void
ctx_destroy (struct dp_ctx *ctx)
{
  if (!ctx)
    return;
  if (ctx->zmq_socket)
    zmq_close (ctx->zmq_socket);
  if (ctx->zmq_context)
    zmq_ctx_destroy (ctx->zmq_context);
  free (ctx);
}

/* =========================================================================
 * Shared send / recv internals
 * ========================================================================= */

static int
send_signal (struct dp_ctx *ctx, const void *samples, size_t num_samples,
             double sample_rate, double center_freq, dp_sample_type_t type)
{
  if (!ctx || !samples || num_samples == 0)
    return DP_ERR_INVALID;

  dp_header_t header = { 0 };
  header.magic = DP_MAGIC;
  header.version = DP_VERSION;
  header.protocol = DP_PROTO_SIGS;
  header.stream_id = 0;
  header.sample_type = type;
  header.flags = 0;
  header.sequence = ctx->sequence++;
  header.timestamp_ns = dp_get_timestamp_ns ();
  header.sample_rate = sample_rate;
  header.center_freq = center_freq;
  header.num_samples = num_samples;

  size_t data_size = num_samples * dp_sample_size (type);

  int rc = zmq_send (ctx->zmq_socket, &header, sizeof (header), ZMQ_SNDMORE);
  if (rc == -1)
    return DP_ERR_SEND;

  rc = zmq_send (ctx->zmq_socket, samples, data_size, 0);
  if (rc == -1)
    return DP_ERR_SEND;

  return DP_OK;
}

static int
recv_signal (struct dp_ctx *ctx, dp_msg_t **out_msg, dp_header_t *out_hdr)
{
  if (!ctx || !out_msg)
    return DP_ERR_INVALID;

  /* Receive header frame */
  dp_header_t hdr;
  int rc = zmq_recv (ctx->zmq_socket, &hdr, sizeof (hdr), 0);
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
  int more;
  size_t more_size = sizeof (more);
  zmq_getsockopt (ctx->zmq_socket, ZMQ_RCVMORE, &more, &more_size);
  if (!more)
    return DP_ERR_INVALID;

  /* Allocate message wrapper (tiny struct, not the data) */
  dp_msg_t *msg = (dp_msg_t *)malloc (sizeof (dp_msg_t));
  if (!msg)
    {
      /* Drain the data frame to keep socket state clean */
      zmq_msg_t discard;
      zmq_msg_init (&discard);
      zmq_msg_recv (&discard, ctx->zmq_socket, 0);
      zmq_msg_close (&discard);
      return DP_ERR_MEMORY;
    }

  /* Zero-copy recv: ZMQ owns the buffer */
  zmq_msg_init (&msg->zmq_msg);
  rc = zmq_msg_recv (&msg->zmq_msg, ctx->zmq_socket, 0);
  if (rc == -1)
    {
      zmq_msg_close (&msg->zmq_msg);
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
recv_raw (struct dp_ctx *ctx, dp_msg_t **out_msg, size_t *out_size)
{
  if (!ctx || !out_msg || !out_size)
    return DP_ERR_INVALID;

  dp_msg_t *msg = (dp_msg_t *)malloc (sizeof (dp_msg_t));
  if (!msg)
    return DP_ERR_MEMORY;

  zmq_msg_init (&msg->zmq_msg);
  int rc = zmq_msg_recv (&msg->zmq_msg, ctx->zmq_socket, 0);
  if (rc == -1)
    {
      zmq_msg_close (&msg->zmq_msg);
      free (msg);
      if (zmq_errno () == EAGAIN)
        return DP_ERR_TIMEOUT;
      return DP_ERR_RECV;
    }

  msg->sample_type = DP_CF64; /* not meaningful for raw recv */
  msg->num_samples = 0;

  *out_msg = msg;
  *out_size = zmq_msg_size (&msg->zmq_msg);

  return DP_OK;
}

static void
set_recv_timeout (struct dp_ctx *ctx, int timeout_ms)
{
  if (ctx && ctx->zmq_socket)
    zmq_setsockopt (ctx->zmq_socket, ZMQ_RCVTIMEO, &timeout_ms,
                    sizeof (timeout_ms));
}

/* =========================================================================
 * PUB/SUB
 * ========================================================================= */

dp_pub *
dp_pub_create (const char *endpoint, dp_sample_type_t sample_type)
{
  return ctx_create (ZMQ_PUB, endpoint, 1, sample_type);
}

int
dp_pub_send_ci32 (dp_pub *ctx, const dp_ci32_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CI32);
}

int
dp_pub_send_cf64 (dp_pub *ctx, const dp_cf64_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CF64);
}

int
dp_pub_send_cf128 (dp_pub *ctx, const dp_cf128_t *samples, size_t num_samples,
                   double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CF128);
}

int
dp_pub_send_ci8 (dp_pub *ctx, const dp_ci8_t *samples, size_t num_samples,
                 double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CI8);
}

int
dp_pub_send_ci16 (dp_pub *ctx, const dp_ci16_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CI16);
}

int
dp_pub_send_cf32 (dp_pub *ctx, const dp_cf32_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CF32);
}

void
dp_pub_destroy (dp_pub *ctx)
{
  ctx_destroy (ctx);
}

dp_sub *
dp_sub_create (const char *endpoint)
{
  return ctx_create (ZMQ_SUB, endpoint, 0, DP_CF64);
}

int
dp_sub_recv (dp_sub *ctx, dp_msg_t **msg, dp_header_t *header)
{
  return recv_signal (ctx, msg, header);
}

void
dp_sub_set_timeout (dp_sub *ctx, int timeout_ms)
{
  set_recv_timeout (ctx, timeout_ms);
}

void
dp_sub_destroy (dp_sub *ctx)
{
  ctx_destroy (ctx);
}

/* =========================================================================
 * PUSH/PULL
 * ========================================================================= */

dp_push *
dp_push_create (const char *endpoint, dp_sample_type_t sample_type)
{
  return ctx_create (ZMQ_PUSH, endpoint, 1, sample_type);
}

int
dp_push_send_ci32 (dp_push *ctx, const dp_ci32_t *samples, size_t num_samples,
                   double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CI32);
}

int
dp_push_send_cf64 (dp_push *ctx, const dp_cf64_t *samples, size_t num_samples,
                   double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CF64);
}

int
dp_push_send_cf128 (dp_push *ctx, const dp_cf128_t *samples,
                    size_t num_samples, double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CF128);
}

int
dp_push_send_ci8 (dp_push *ctx, const dp_ci8_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CI8);
}

int
dp_push_send_ci16 (dp_push *ctx, const dp_ci16_t *samples, size_t num_samples,
                   double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CI16);
}

int
dp_push_send_cf32 (dp_push *ctx, const dp_cf32_t *samples, size_t num_samples,
                   double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CF32);
}

dp_pull *
dp_pull_create (const char *endpoint)
{
  return ctx_create (ZMQ_PULL, endpoint, 0, DP_CF64);
}

int
dp_pull_recv (dp_pull *ctx, dp_msg_t **msg, dp_header_t *header)
{
  return recv_signal (ctx, msg, header);
}

void
dp_pull_set_timeout (dp_pull *ctx, int timeout_ms)
{
  set_recv_timeout (ctx, timeout_ms);
}

void
dp_push_destroy (dp_push *ctx)
{
  ctx_destroy (ctx);
}

void
dp_pull_destroy (dp_pull *ctx)
{
  ctx_destroy (ctx);
}

/* =========================================================================
 * REQ/REP
 * ========================================================================= */

dp_req *
dp_req_create (const char *endpoint)
{
  return ctx_create (ZMQ_REQ, endpoint, 0, DP_CF64);
}

dp_rep *
dp_rep_create (const char *endpoint)
{
  return ctx_create (ZMQ_REP, endpoint, 1, DP_CF64);
}

/* -- Raw-bytes send/recv ------------------------------------------------ */

int
dp_req_send (dp_req *ctx, const void *data, size_t size)
{
  if (!ctx || !data || size == 0)
    return DP_ERR_INVALID;
  int rc = zmq_send (ctx->zmq_socket, data, size, 0);
  return (rc == -1) ? DP_ERR_SEND : DP_OK;
}

int
dp_rep_send (dp_rep *ctx, const void *data, size_t size)
{
  if (!ctx || !data || size == 0)
    return DP_ERR_INVALID;
  int rc = zmq_send (ctx->zmq_socket, data, size, 0);
  return (rc == -1) ? DP_ERR_SEND : DP_OK;
}

int
dp_req_recv (dp_req *ctx, dp_msg_t **msg, size_t *size)
{
  return recv_raw (ctx, msg, size);
}

int
dp_rep_recv (dp_rep *ctx, dp_msg_t **msg, size_t *size)
{
  return recv_raw (ctx, msg, size);
}

/* -- Signal-frame send/recv --------------------------------------------- */

int
dp_req_send_ci32 (dp_req *ctx, const dp_ci32_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CI32);
}

int
dp_req_send_cf64 (dp_req *ctx, const dp_cf64_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CF64);
}

int
dp_req_send_cf128 (dp_req *ctx, const dp_cf128_t *samples, size_t num_samples,
                   double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CF128);
}

int
dp_req_send_ci8 (dp_req *ctx, const dp_ci8_t *samples, size_t num_samples,
                 double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CI8);
}

int
dp_req_send_ci16 (dp_req *ctx, const dp_ci16_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CI16);
}

int
dp_req_send_cf32 (dp_req *ctx, const dp_cf32_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CF32);
}

int
dp_rep_send_ci32 (dp_rep *ctx, const dp_ci32_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CI32);
}

int
dp_rep_send_cf64 (dp_rep *ctx, const dp_cf64_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CF64);
}

int
dp_rep_send_cf128 (dp_rep *ctx, const dp_cf128_t *samples, size_t num_samples,
                   double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CF128);
}

int
dp_rep_send_ci8 (dp_rep *ctx, const dp_ci8_t *samples, size_t num_samples,
                 double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CI8);
}

int
dp_rep_send_ci16 (dp_rep *ctx, const dp_ci16_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CI16);
}

int
dp_rep_send_cf32 (dp_rep *ctx, const dp_cf32_t *samples, size_t num_samples,
                  double sample_rate, double center_freq)
{
  return send_signal (ctx, samples, num_samples, sample_rate, center_freq,
                      DP_CF32);
}

int
dp_req_recv_signal (dp_req *ctx, dp_msg_t **msg, dp_header_t *header)
{
  return recv_signal (ctx, msg, header);
}

int
dp_rep_recv_signal (dp_rep *ctx, dp_msg_t **msg, dp_header_t *header)
{
  return recv_signal (ctx, msg, header);
}

/* -- Timeout setters ---------------------------------------------------- */

void
dp_req_set_timeout (dp_req *ctx, int timeout_ms)
{
  set_recv_timeout (ctx, timeout_ms);
}

void
dp_rep_set_timeout (dp_rep *ctx, int timeout_ms)
{
  set_recv_timeout (ctx, timeout_ms);
}

/* -- Destroy ------------------------------------------------------------ */

void
dp_req_destroy (dp_req *ctx)
{
  ctx_destroy (ctx);
}

void
dp_rep_destroy (dp_rep *ctx)
{
  ctx_destroy (ctx);
}
