#include "stream/stream.h"
#include "stream_internal.h"

#include "dp_interrupt.h"
#include <complex.h>
#include <signal.h>
#include <stddef.h> /* offsetof — the wire-layout assertions */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* =========================================================================
 * dp_msg_t — zero-copy message accessors (dispatch on how the buffer is owned)
 * ========================================================================= */

void *
dp_msg_data (dp_msg_t *msg)
{
  if (!msg)
    return NULL;
  switch (msg->owner)
    {
    case DP_MSG_NATS:
      return nats_msg_data (msg);
    case DP_MSG_OWNED:
      return (char *)msg->u.owned.ptr + msg->data_offset;
    default:
      return NULL;
    }
}

size_t
dp_msg_size (dp_msg_t *msg)
{
  if (!msg)
    return 0;
  switch (msg->owner)
    {
    case DP_MSG_NATS:
      return nats_msg_size (msg);
    case DP_MSG_OWNED:
      return msg->u.owned.len - msg->data_offset;
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
  return msg ? msg->format : CF64;
}

dp_frame_kind_t
dp_msg_kind (dp_msg_t *msg)
{
  return msg ? msg->kind : DP_KIND_IQ;
}

int
dp_msg_ack (dp_msg_t *msg)
{
  if (!msg)
    return DP_ERR_INVALID;
  if (msg->owner == DP_MSG_NATS)
    return nats_msg_ack (msg);
  return DP_OK; /* core-NATS / reassembled: nothing to ack */
}

void
dp_msg_free (dp_msg_t *msg)
{
  if (!msg)
    return;
  switch (msg->owner)
    {
    case DP_MSG_NATS:
      nats_msg_free (msg);
      break;
    case DP_MSG_OWNED:
      free (msg->u.owned.ptr);
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

/* The wire layout is a promise, so the compiler holds it. Every one of
   these was true before and asserted nowhere: the previous header's size
   was whatever padding produced, and its documented `version` value
   disagreed with the one actually written. */
_Static_assert (sizeof (dp_header_t) == 64, "dp_header_t must be 64 bytes");
_Static_assert (sizeof (dp_chunk_t) == 24, "dp_chunk_t must be 24 bytes");
_Static_assert (offsetof (dp_header_t, magic) == 0, "magic at 0");
_Static_assert (offsetof (dp_header_t, data_rep) == 8, "data_rep at 8");
_Static_assert (offsetof (dp_header_t, format) == 12, "format at 12");
_Static_assert (offsetof (dp_header_t, kind) == 14, "kind at 14");
_Static_assert (offsetof (dp_header_t, version) == 16, "version at 16");
_Static_assert (offsetof (dp_header_t, flags) == 18, "flags at 18");
_Static_assert (offsetof (dp_header_t, payload_bytes) == 20, "payload at 20");
_Static_assert (offsetof (dp_header_t, sequence) == 24, "sequence at 24");
_Static_assert (offsetof (dp_header_t, timestamp_ns) == 32, "timestamp at 32");
_Static_assert (offsetof (dp_header_t, sample_rate) == 40, "fs at 40");
_Static_assert (offsetof (dp_header_t, center_freq) == 48, "fc at 48");
_Static_assert (offsetof (dp_header_t, num_samples) == 56, "n at 56");

size_t
dp_sample_size (dp_sample_type_t type)
{
  return dp_format_size (type); /* the one table, in dp_format.h */
}

int
dp_sample_type_is_valid (dp_sample_type_t type)
{
  return dp_format_is_valid (type);
}

size_t
dp_element_size (dp_frame_kind_t kind, dp_sample_type_t format)
{
  switch (kind)
    {
    case DP_KIND_IQ:
      return dp_format_size (format);
    case DP_KIND_TLM:
      return 16; /* one packed dp_tlm_rec_t per record */
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
    default:
      return "UNKNOWN";
    }
}

double
dp_mean_power (dp_sample_type_t format, const void *data, size_t n)
{
  double fs = dp_format_full_scale (format);
  if (!data || n == 0 || fs == 0.0)
    return 0.0;

  /* Normalising the integer formats by full scale is what makes the answer
     comparable across the wire types: a caller that does not care which one
     it got still gets a number that means the same thing. */
  double inv = 1.0 / fs;
  double p   = 0.0;

  switch (format)
    {
    case CF64:
      {
        const double _Complex *x = (const double _Complex *)data;
        for (size_t i = 0; i < n; i++)
          {
            double re = creal (x[i]), im = cimag (x[i]);
            p += re * re + im * im;
          }
        break;
      }
    case CF32:
      {
        const float _Complex *x = (const float _Complex *)data;
        for (size_t i = 0; i < n; i++)
          {
            double re = (double)crealf (x[i]), im = (double)cimagf (x[i]);
            p += re * re + im * im;
          }
        break;
      }
    case CI32:
      {
        const int32_t *x = (const int32_t *)data;
        for (size_t i = 0; i < n; i++)
          {
            double re = (double)x[2 * i] * inv,
                   im = (double)x[2 * i + 1] * inv;
            p += re * re + im * im;
          }
        break;
      }
    case CI16:
      {
        const int16_t *x = (const int16_t *)data;
        for (size_t i = 0; i < n; i++)
          {
            double re = (double)x[2 * i] * inv,
                   im = (double)x[2 * i + 1] * inv;
            p += re * re + im * im;
          }
        break;
      }
    case CI8:
      {
        const int8_t *x = (const int8_t *)data;
        for (size_t i = 0; i < n; i++)
          {
            double re = (double)x[2 * i] * inv,
                   im = (double)x[2 * i + 1] * inv;
            p += re * re + im * im;
          }
        break;
      }
    default:
      return 0.0;
    }

  return p / (double)n;
}

double
dp_msg_mean_power (dp_msg_t *msg)
{
  if (!msg || dp_msg_kind (msg) != DP_KIND_IQ)
    return 0.0; /* telemetry records are not samples */
  return dp_mean_power (dp_msg_sample_type (msg), dp_msg_data (msg),
                        dp_msg_num_samples (msg));
}

/* Every check a receiver makes on an arriving frame, over a plain buffer.
 *
 * In the core rather than beside the transport because none of it is about
 * NATS: it is the format's own rules, it is the part that decides whether a
 * length can be trusted, and a caller with a buffer -- a test, a different
 * transport, a tool -- must be able to run it. The v1 receiver checked the
 * magic and nothing else, which is how a header claiming more samples than
 * its message carried produced an out-of-bounds read on both faces. */
int
dp_frame_parse (const void *buf, size_t len, dp_header_t *hdr,
                dp_chunk_t *chunk, int *chunked, const void **body,
                size_t *body_len)
{
  if (!buf || !hdr || !chunk || !chunked || !body || !body_len)
    return DP_ERR_INVALID;
  if (len < sizeof (*hdr))
    return DP_ERR_INVALID;

  const char *d = (const char *)buf;
  memcpy (hdr, d, sizeof (*hdr));

  if (hdr->magic != DP_STREAM_MAGIC)
    return DP_ERR_INVALID; /* not ours, or the opposite byte order */
  if (hdr->version != DP_WIRE_VERSION)
    return DP_ERR_INVALID;
  if (hdr->flags & (uint16_t)~DP_FLAG_KNOWN)
    return DP_ERR_INVALID; /* a block we do not know moves the payload */
  if (memcmp (hdr->data_rep, dp_host_rep (), 4) != 0)
    return DP_ERR_INVALID; /* the magic implies this; say it anyway */

  size_t fixed = sizeof (*hdr);
  *chunked     = (hdr->flags & DP_FLAG_CHUNKED) ? 1 : 0;
  if (*chunked)
    {
      if (len < fixed + sizeof (*chunk))
        return DP_ERR_INVALID;
      memcpy (chunk, d + fixed, sizeof (*chunk));
      fixed += sizeof (*chunk);
    }

  size_t avail = len - fixed;
  if (hdr->payload_bytes != avail)
    return DP_ERR_INVALID; /* the header's claim vs the transport's truth */

  /* End of stream carries nothing, so the element-size question does not
     apply to it -- but "does not apply" is not "unchecked": a frame that
     says it is an ending and still claims a payload is refused. */
  if ((dp_frame_kind_t)hdr->kind == DP_KIND_EOS)
    {
      /* Every field the kind fixes is CHECKED, not merely documented:
         `format` is part of that claim, so a frame that says it is an
         ending and also names a sample type is refused like one that
         claims a payload. Otherwise the one kind that skips the
         element-size arithmetic would be the one kind whose format
         nothing ever looks at. */
      if (avail != 0 || hdr->num_samples != 0 || hdr->format != 0)
        return DP_ERR_INVALID;
    }
  else
    {
      size_t elem = dp_element_size ((dp_frame_kind_t)hdr->kind,
                                     (dp_sample_type_t)hdr->format);
      if (elem == 0 || hdr->num_samples * elem != avail)
        return DP_ERR_INVALID;
    }

  *body     = d + fixed;
  *body_len = avail;
  return DP_OK;
}

/* The interrupt primitive moved to native/src/dp_interrupt.c so a core-only
   build -- a file writer, a ring buffer, no NATS -- can link it. Nothing was
   rewritten: it never had a NATS dependency. What stays here are the
   dp_stream_* spellings, which are DEPRECATED and forward to it verbatim.
   They are removed once their callers migrate; see
   docs/design/io-termination.md. */

void
dp_stream_interrupt (void)
{
  dp_interrupt ();
}

void
dp_stream_resume (void)
{
  dp_resume ();
}

int
dp_stream_interrupted (void)
{
  return dp_interrupted ();
}

void
dp_stream_set_interrupt_latency_ms (unsigned ms)
{
  dp_set_interrupt_latency_ms (ms);
}

unsigned
dp_stream_interrupt_latency_ms (void)
{
  return dp_interrupt_latency_ms ();
}

int
dp_stream_interrupt_on_signal (int sig)
{
  return dp_interrupt_on_signal (sig);
}

int
dp_stream_restore_signal (int sig)
{
  return dp_restore_signal (sig);
}

const char *
dp_host_rep (void)
{
  /* Derived, not declared: the same union trick a reader would use, so a
     build on a big-endian target tags its frames honestly instead of
     inheriting a constant nobody revisited. */
  const uint16_t one = 1u;
  return (*(const unsigned char *)&one) ? DP_REP_LE : DP_REP_BE;
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
    case DP_ERR_TOO_LARGE:
      return "Frame exceeds transport max_payload";
    case DP_ERR_INTERRUPTED:
      return "Interrupted by dp_stream_interrupt";
    case DP_ERR_CLOSED:
      return "Context is draining or closed";
    case DP_ERR_EOF:
      return "End of stream: the sender has finished";
    default:
      return "Unknown error";
    }
}

/* =========================================================================
 * Shared framing funnels — backend-agnostic header construction, delegating
 * the actual transport I/O to the NATS implementation (stream_nats.c).
 * ========================================================================= */

static struct dp_ctx *
ctx_create (dp_role_t role, const char *endpoint, dp_frame_kind_t kind,
            dp_sample_type_t format)
{
  if (!endpoint)
    return NULL;
  /* A socket declares what it will send at construction, so an invalid
     format is refused here rather than at the first send -- which is also
     where a retired or unknown code is caught. */
  if (kind == DP_KIND_IQ && !dp_sample_type_is_valid (format))
    return NULL;
  return nats_ctx_create (role, endpoint, kind, format);
}

static void
ctx_destroy (struct dp_ctx *ctx)
{
  if (!ctx)
    return;
  nats_ctx_destroy (ctx);
  free (ctx);
}

static int
send_signal (struct dp_ctx *ctx, const void *samples, size_t num_samples,
             double sample_rate, double center_freq, dp_sample_type_t type)
{
  if (!ctx || !samples || num_samples == 0)
    return DP_ERR_INVALID;

  size_t elem = dp_element_size (ctx->kind, type);
  if (elem == 0)
    return DP_ERR_INVALID;

  size_t data_size = num_samples * elem;
  if (data_size > UINT32_MAX)
    return DP_ERR_TOO_LARGE; /* payload_bytes is 32-bit by design: a frame
                                this large cannot cross any broker anyway */

  dp_header_t header = { 0 };
  header.magic       = DP_STREAM_MAGIC;
  memcpy (header.data_rep, dp_host_rep (), 4);
  header.format               = (uint16_t)(ctx->kind == DP_KIND_IQ ? type : 0);
  header.kind                 = (uint16_t)ctx->kind;
  header.version              = DP_WIRE_VERSION;
  header.flags                = 0;
  header.payload_bytes        = (uint32_t)data_size;
  header.sequence             = ctx->sequence++;
  header.timestamp_ns         = ctx->timestamp_override_set
                                    ? ctx->timestamp_override_ns
                                    : dp_get_timestamp_ns ();
  ctx->timestamp_override_set = 0; /* one-shot, whether used or not */
  header.sample_rate          = sample_rate;
  header.center_freq          = center_freq;
  header.num_samples          = num_samples;

  return nats_send_signal (ctx, &header, samples, data_size);
}

void
dp_ctx_set_timestamp_ns (dp_pub_t *ctx, uint64_t timestamp_ns)
{
  if (!ctx)
    return;
  ctx->timestamp_override_ns  = timestamp_ns;
  ctx->timestamp_override_set = 1;
}

static int
recv_signal (struct dp_ctx *ctx, dp_msg_t **out_msg, dp_header_t *out_hdr)
{
  if (!ctx || !out_msg)
    return DP_ERR_INVALID;
  return nats_recv_signal (ctx, out_msg, out_hdr);
}

static int
recv_raw (struct dp_ctx *ctx, dp_msg_t **out_msg, size_t *out_size)
{
  if (!ctx || !out_msg || !out_size)
    return DP_ERR_INVALID;
  return nats_recv_raw (ctx, out_msg, out_size);
}

static int
send_raw (struct dp_ctx *ctx, const void *data, size_t size)
{
  if (!ctx || !data || size == 0)
    return DP_ERR_INVALID;
  return nats_send_raw (ctx, data, size);
}

static void
set_recv_timeout (struct dp_ctx *ctx, int timeout_ms)
{
  if (!ctx)
    return;
  nats_set_recv_timeout (ctx, timeout_ms);
}

/* =========================================================================
 * PUB/SUB
 * ========================================================================= */

dp_pub_t *
dp_pub_create (const char *endpoint, dp_sample_type_t sample_type)
{
  return ctx_create (DP_ROLE_PUB, endpoint, DP_KIND_IQ, sample_type);
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

int
dp_pub_send_tlm16 (dp_pub_t *ctx, const void *records, size_t num_records,
                   double sample_rate, double center_freq)
{
  return send_signal (ctx, records, num_records, sample_rate, center_freq,
                      (dp_sample_type_t)0);
}

int
dp_pub_send_eos (dp_pub_t *ctx)
{
  if (!ctx)
    return DP_ERR_INVALID;

  /* A statement, not data: no payload, no format, no samples. It does NOT
     go through send_signal, which rejects a zero-length send -- correctly,
     for a frame that is supposed to carry something. */
  dp_header_t header = { 0 };
  header.magic       = DP_STREAM_MAGIC;
  memcpy (header.data_rep, dp_host_rep (), 4);
  header.format               = 0;
  header.kind                 = (uint16_t)DP_KIND_EOS;
  header.version              = DP_WIRE_VERSION;
  header.flags                = 0;
  header.payload_bytes        = 0;
  header.sequence             = ctx->sequence++;
  header.timestamp_ns         = ctx->timestamp_override_set
                                    ? ctx->timestamp_override_ns
                                    : dp_get_timestamp_ns ();
  ctx->timestamp_override_set = 0;
  header.sample_rate          = 0.0;
  header.center_freq          = 0.0;
  header.num_samples          = 0;

  return nats_send_signal (ctx, &header, NULL, 0);
}

dp_pub_t *
dp_pub_create_tlm (const char *endpoint)
{
  return ctx_create (DP_ROLE_PUB, endpoint, DP_KIND_TLM, (dp_sample_type_t)0);
}

int
dp_stream_drain (dp_pub_t *ctx, int timeout_ms)
{
  if (!ctx)
    return DP_ERR_INVALID;
  return nats_drain (ctx, timeout_ms);
}

int
dp_pub_flush (dp_pub_t *ctx, int timeout_ms)
{
  if (!ctx)
    return DP_ERR_INVALID;
  return nats_flush (ctx, timeout_ms);
}

void
dp_pub_destroy (dp_pub_t *ctx)
{
  ctx_destroy (ctx);
}

dp_sub_t *
dp_sub_create (const char *endpoint)
{
  return ctx_create (DP_ROLE_SUB, endpoint, DP_KIND_IQ, CF64);
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
  return ctx_create (DP_ROLE_PUSH, endpoint, DP_KIND_IQ, sample_type);
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
  return ctx_create (DP_ROLE_PULL, endpoint, DP_KIND_IQ, CF64);
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
  return ctx_create (DP_ROLE_REQ, endpoint, DP_KIND_IQ, CF64);
}

dp_rep_t *
dp_rep_create (const char *endpoint)
{
  return ctx_create (DP_ROLE_REP, endpoint, DP_KIND_IQ, CF64);
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
