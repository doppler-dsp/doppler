

# File stream.h

[**File List**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**stream.h**](stream_8h.md)

[Go to the documentation of this file](stream_8h.md)


```C++


#ifndef DP_STREAM_H
#define DP_STREAM_H

#include <complex.h>
#include <stddef.h>
#include <stdint.h>

/* CMPLXF/CMPLX/CMPLXL are C11.  Provide them for C99 builds via the
 * GCC/Clang __builtin_complex extension (available since GCC 4.7). */
#ifndef CMPLXF
#  define CMPLXF(x, y) __builtin_complex((float)(x), (float)(y))
#endif
#ifndef CMPLX
#  define CMPLX(x, y)  __builtin_complex((double)(x), (double)(y))
#endif
#ifndef CMPLXL
#  define CMPLXL(x, y) __builtin_complex((long double)(x), (long double)(y))
#endif

#ifdef __cplusplus
extern "C"
{
#endif


#define DP_VERSION_MAJOR 2
#define DP_VERSION_MINOR 0
#define DP_VERSION_PATCH 0


  /* -------------------------------------------------------------------------
   * Sample types
   * ---------------------------------------------------------------------- */


  typedef enum
  {
    DP_CI32 = 0,
    DP_CF64 = 1,
    DP_CF128 = 2,
    DP_CI8 = 3,
    DP_CI16 = 4,
    DP_CF32 = 5,
  } dp_sample_type_t;

  typedef enum
  {
    DP_PROTO_SIGS = 0,
    DP_PROTO_DIFI = 1,
  } dp_protocol_t;
 /* end group sampletypes */

  typedef struct
  {
    uint32_t magic;
    uint32_t version;
    uint32_t protocol;
    uint32_t stream_id;
    uint32_t sample_type;
    uint32_t flags;
    uint64_t sequence;
    uint64_t
        timestamp_ns;
    double sample_rate;
    double center_freq;
    uint64_t num_samples;
    uint64_t reserved[4];
  } dp_header_t;

  typedef struct dp_msg dp_msg_t;

  typedef struct dp_ctx dp_pub;
  typedef struct dp_ctx dp_sub;
  typedef struct dp_ctx dp_push;
  typedef struct dp_ctx dp_pull;
  typedef struct dp_ctx dp_req;
  typedef struct dp_ctx dp_rep;
 /* end group types */

  /* -------------------------------------------------------------------------
   * Error codes
   * ---------------------------------------------------------------------- */

#define DP_OK 0
#define DP_ERR_INIT -1
#define DP_ERR_SEND -2
#define DP_ERR_RECV -3
#define DP_ERR_INVALID -4
#define DP_ERR_TIMEOUT -5
#define DP_ERR_MEMORY -6

  /* -------------------------------------------------------------------------
   * dp_msg_t — zero-copy message accessors
   * ---------------------------------------------------------------------- */


  void *dp_msg_data (dp_msg_t *msg);

  size_t dp_msg_size (dp_msg_t *msg);

  size_t dp_msg_num_samples (dp_msg_t *msg);

  dp_sample_type_t dp_msg_sample_type (dp_msg_t *msg);

  void dp_msg_free (dp_msg_t *msg);
 /* end group msg */

  /* -------------------------------------------------------------------------
   * Publisher / Subscriber  (PUB/SUB — fan-out broadcast)
   * ---------------------------------------------------------------------- */


  dp_pub *dp_pub_create (const char *endpoint, dp_sample_type_t sample_type);

  int dp_pub_send_ci32 (dp_pub *ctx, const int32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_pub_send_cf64 (dp_pub *ctx, const double _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_pub_send_cf128 (dp_pub *ctx, const long double _Complex *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  int dp_pub_send_ci8 (dp_pub *ctx, const int8_t *samples,
                       size_t num_samples, double sample_rate,
                       double center_freq);

  int dp_pub_send_ci16 (dp_pub *ctx, const int16_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_pub_send_cf32 (dp_pub *ctx, const float _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  void dp_pub_destroy (dp_pub *ctx);

  dp_sub *dp_sub_create (const char *endpoint);

  int dp_sub_recv (dp_sub *ctx, dp_msg_t **msg, dp_header_t *header);

  void dp_sub_set_timeout (dp_sub *ctx, int timeout_ms);

  void dp_sub_destroy (dp_sub *ctx);
 /* end group pubsub */

  /* -------------------------------------------------------------------------
   * Push / Pull  (PUSH/PULL — pipeline / load-balanced)
   * ---------------------------------------------------------------------- */


  dp_push *dp_push_create (const char *endpoint, dp_sample_type_t sample_type);

  dp_pull *dp_pull_create (const char *endpoint);

  int dp_push_send_ci32 (dp_push *ctx, const int32_t *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  int dp_push_send_cf64 (dp_push *ctx, const double _Complex *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  int dp_push_send_cf128 (dp_push *ctx, const long double _Complex *samples,
                          size_t num_samples, double sample_rate,
                          double center_freq);

  int dp_push_send_ci8 (dp_push *ctx, const int8_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_push_send_ci16 (dp_push *ctx, const int16_t *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  int dp_push_send_cf32 (dp_push *ctx, const float _Complex *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  int dp_pull_recv (dp_pull *ctx, dp_msg_t **msg, dp_header_t *header);

  void dp_pull_set_timeout (dp_pull *ctx, int timeout_ms);

  void dp_push_destroy (dp_push *ctx);

  void dp_pull_destroy (dp_pull *ctx);
 /* end group pipeline */

  /* -------------------------------------------------------------------------
   * Request / Reply  (REQ/REP — control and metadata)
   * ---------------------------------------------------------------------- */


  dp_req *dp_req_create (const char *endpoint);

  dp_rep *dp_rep_create (const char *endpoint);

  /* -- Raw-bytes send/recv (control plane) ------------------------------ */

  int dp_req_send (dp_req *ctx, const void *data, size_t size);

  int dp_req_recv (dp_req *ctx, dp_msg_t **msg, size_t *size);

  int dp_rep_recv (dp_rep *ctx, dp_msg_t **msg, size_t *size);

  int dp_rep_send (dp_rep *ctx, const void *data, size_t size);

  /* -- Signal-frame send/recv (data plane) ------------------------------ */

  int dp_req_send_ci32 (dp_req *ctx, const int32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  int dp_req_send_cf64 (dp_req *ctx, const double _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  int dp_req_send_cf128 (dp_req *ctx, const long double _Complex *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);
  int dp_req_send_ci8 (dp_req *ctx, const int8_t *samples,
                       size_t num_samples, double sample_rate,
                       double center_freq);
  int dp_req_send_ci16 (dp_req *ctx, const int16_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  int dp_req_send_cf32 (dp_req *ctx, const float _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_rep_send_ci32 (dp_rep *ctx, const int32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  int dp_rep_send_cf64 (dp_rep *ctx, const double _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  int dp_rep_send_cf128 (dp_rep *ctx, const long double _Complex *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);
  int dp_rep_send_ci8 (dp_rep *ctx, const int8_t *samples,
                       size_t num_samples, double sample_rate,
                       double center_freq);
  int dp_rep_send_ci16 (dp_rep *ctx, const int16_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  int dp_rep_send_cf32 (dp_rep *ctx, const float _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_req_recv_signal (dp_req *ctx, dp_msg_t **msg, dp_header_t *header);

  int dp_rep_recv_signal (dp_rep *ctx, dp_msg_t **msg, dp_header_t *header);

  void dp_req_set_timeout (dp_req *ctx, int timeout_ms);

  void dp_rep_set_timeout (dp_rep *ctx, int timeout_ms);

  void dp_req_destroy (dp_req *ctx);

  void dp_rep_destroy (dp_rep *ctx);
 /* end group reqrep */

  /* -------------------------------------------------------------------------
   * Utilities
   * ---------------------------------------------------------------------- */


  const char *dp_sample_type_str (dp_sample_type_t type);

  size_t dp_sample_size (dp_sample_type_t type);

  uint64_t dp_get_timestamp_ns (void);

  const char *dp_strerror (int err);
 /* end group utils */

#ifdef __cplusplus
}
#endif

#endif /* DP_STREAM_H */
```
