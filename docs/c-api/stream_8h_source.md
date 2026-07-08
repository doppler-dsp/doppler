

# File stream.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**stream**](dir_21b896cdbc030a0ded493211142b7733.md) **>** [**stream.h**](stream_8h.md)

[Go to the documentation of this file](stream_8h.md)


```C++


#ifndef DP_STREAM_H
#define DP_STREAM_H

#include <complex.h>
#include <stddef.h>
#include <stdint.h>

/* CMPLXF/CMPLX/CMPLXL fallbacks and the shared DP_OK/DP_ERR_* error codes
 * live in clib_common.h — the streaming API uses the one doppler-wide scheme. */
#include "clib_common.h"


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
    CI32 = 0,  
    CF64 = 1,  
    CF128 = 2, 
    CI8 = 3,   
    CI16 = 4,  
    CF32 = 5,  
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

  typedef struct dp_ctx dp_pub_t;
  typedef struct dp_ctx dp_sub_t;
  typedef struct dp_ctx dp_push_t;
  typedef struct dp_ctx dp_pull_t;
  typedef struct dp_ctx dp_req_t;
  typedef struct dp_ctx dp_rep_t;
 /* end group types */

  /* -------------------------------------------------------------------------
   * Error codes
   * ---------------------------------------------------------------------- */


  /* -------------------------------------------------------------------------
   * dp_msg_t — zero-copy message accessors
   * ---------------------------------------------------------------------- */


  void *dp_msg_data (dp_msg_t *msg);

  size_t dp_msg_size (dp_msg_t *msg);

  size_t dp_msg_num_samples (dp_msg_t *msg);

  dp_sample_type_t dp_msg_sample_type (dp_msg_t *msg);

  int dp_msg_ack (dp_msg_t *msg);

  void dp_msg_free (dp_msg_t *msg);
 /* end group msg */

  /* -------------------------------------------------------------------------
   * Publisher / Subscriber  (PUB/SUB — fan-out broadcast)
   * ---------------------------------------------------------------------- */


  dp_pub_t *dp_pub_create (const char *endpoint, dp_sample_type_t sample_type);

  int dp_pub_send_ci32 (dp_pub_t *ctx, const int32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_pub_send_cf64 (dp_pub_t *ctx, const double _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_pub_send_cf128 (dp_pub_t *ctx, const long double _Complex *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  int dp_pub_send_ci8 (dp_pub_t *ctx, const int8_t *samples, size_t num_samples,
                       double sample_rate, double center_freq);

  int dp_pub_send_ci16 (dp_pub_t *ctx, const int16_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_pub_send_cf32 (dp_pub_t *ctx, const float _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  void dp_pub_destroy (dp_pub_t *ctx);

  dp_sub_t *dp_sub_create (const char *endpoint);

  int dp_sub_recv (dp_sub_t *ctx, dp_msg_t **msg, dp_header_t *header);

  void dp_sub_set_timeout (dp_sub_t *ctx, int timeout_ms);

  void dp_sub_destroy (dp_sub_t *ctx);
 /* end group pubsub */

  /* -------------------------------------------------------------------------
   * Push / Pull  (PUSH/PULL — pipeline / load-balanced)
   * ---------------------------------------------------------------------- */


  dp_push_t *dp_push_create (const char *endpoint, dp_sample_type_t sample_type);

  dp_pull_t *dp_pull_create (const char *endpoint);

  int dp_push_send_ci32 (dp_push_t *ctx, const int32_t *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  int dp_push_send_cf64 (dp_push_t *ctx, const double _Complex *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  int dp_push_send_cf128 (dp_push_t *ctx, const long double _Complex *samples,
                          size_t num_samples, double sample_rate,
                          double center_freq);

  int dp_push_send_ci8 (dp_push_t *ctx, const int8_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_push_send_ci16 (dp_push_t *ctx, const int16_t *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  int dp_push_send_cf32 (dp_push_t *ctx, const float _Complex *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  int dp_pull_recv (dp_pull_t *ctx, dp_msg_t **msg, dp_header_t *header);

  void dp_pull_set_timeout (dp_pull_t *ctx, int timeout_ms);

  void dp_push_destroy (dp_push_t *ctx);

  void dp_pull_destroy (dp_pull_t *ctx);
 /* end group pipeline */

  /* -------------------------------------------------------------------------
   * Request / Reply  (REQ/REP — control and metadata)
   * ---------------------------------------------------------------------- */


  dp_req_t *dp_req_create (const char *endpoint);

  dp_rep_t *dp_rep_create (const char *endpoint);

  /* -- Raw-bytes send/recv (control plane) ------------------------------ */

  int dp_req_send (dp_req_t *ctx, const void *data, size_t size);

  int dp_req_recv (dp_req_t *ctx, dp_msg_t **msg, size_t *size);

  int dp_rep_recv (dp_rep_t *ctx, dp_msg_t **msg, size_t *size);

  int dp_rep_send (dp_rep_t *ctx, const void *data, size_t size);

  /* -- Signal-frame send/recv (data plane) ------------------------------ */

  int dp_req_send_ci32 (dp_req_t *ctx, const int32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  int dp_req_send_cf64 (dp_req_t *ctx, const double _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  int dp_req_send_cf128 (dp_req_t *ctx, const long double _Complex *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);
  int dp_req_send_ci8 (dp_req_t *ctx, const int8_t *samples, size_t num_samples,
                       double sample_rate, double center_freq);
  int dp_req_send_ci16 (dp_req_t *ctx, const int16_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  int dp_req_send_cf32 (dp_req_t *ctx, const float _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_rep_send_ci32 (dp_rep_t *ctx, const int32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  int dp_rep_send_cf64 (dp_rep_t *ctx, const double _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  int dp_rep_send_cf128 (dp_rep_t *ctx, const long double _Complex *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);
  int dp_rep_send_ci8 (dp_rep_t *ctx, const int8_t *samples, size_t num_samples,
                       double sample_rate, double center_freq);
  int dp_rep_send_ci16 (dp_rep_t *ctx, const int16_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  int dp_rep_send_cf32 (dp_rep_t *ctx, const float _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_req_recv_signal (dp_req_t *ctx, dp_msg_t **msg, dp_header_t *header);

  int dp_rep_recv_signal (dp_rep_t *ctx, dp_msg_t **msg, dp_header_t *header);

  void dp_req_set_timeout (dp_req_t *ctx, int timeout_ms);

  void dp_rep_set_timeout (dp_rep_t *ctx, int timeout_ms);

  void dp_req_destroy (dp_req_t *ctx);

  void dp_rep_destroy (dp_rep_t *ctx);
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


