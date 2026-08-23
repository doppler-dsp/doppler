

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
 * live in clib_common.h — the streaming API uses the one doppler-wide scheme.
 */
#include "clib_common.h"
#include "dp_interrupt.h"
#include "dp_format.h"

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

  /* The sample formats themselves live in dp_format.h: they are BLUE's
     vocabulary, shared with the file writer, and a container is the wrong
     owner for the names of what it carries. */

  typedef enum
  {
    DP_KIND_IQ  = 0, 
    DP_KIND_TLM = 1, 
  } dp_frame_kind_t;


#define DP_STREAM_MAGIC 0x4D41455254535044ULL /* "DPSTREAM" little-endian */

#define DP_WIRE_VERSION 2u

#define DP_REP_LE "EEEI"
#define DP_REP_BE "IEEE"

#define DP_FLAG_CHUNKED                                                       \
  0x0001u 
#define DP_FLAG_KNOWN (DP_FLAG_CHUNKED)


  typedef struct
  {
    uint64_t magic;         
    char     data_rep[4];   
    uint16_t format;        
    uint16_t kind;          
    uint16_t version;       
    uint16_t flags;         
    uint32_t payload_bytes; 
    uint64_t sequence;      
    uint64_t timestamp_ns;  
    double   sample_rate;   
    double   center_freq;   
    uint64_t num_samples;   
  } dp_header_t;

  typedef struct
  {
    uint32_t index;       
    uint32_t count;       
    uint64_t total_bytes; 
    uint64_t offset;      
  } dp_chunk_t;

  typedef struct dp_msg dp_msg_t;

  typedef struct dp_ctx dp_pub_t;
  typedef struct dp_ctx dp_sub_t;
  typedef struct dp_ctx dp_push_t;
  typedef struct dp_ctx dp_pull_t;
  typedef struct dp_ctx dp_req_t;
  typedef struct dp_ctx dp_rep_t;


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

  double dp_mean_power (dp_sample_type_t format, const void *data, size_t n);

  double dp_msg_mean_power (dp_msg_t *msg);

  dp_frame_kind_t dp_msg_kind (dp_msg_t *msg);

  int dp_msg_ack (dp_msg_t *msg);

  void dp_msg_free (dp_msg_t *msg);


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

  int dp_pub_send_ci8 (dp_pub_t *ctx, const int8_t *samples,
                       size_t num_samples, double sample_rate,
                       double center_freq);

  int dp_pub_send_ci16 (dp_pub_t *ctx, const int16_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_pub_send_cf32 (dp_pub_t *ctx, const float _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_pub_send_tlm16 (dp_pub_t *ctx, const void *records,
                         size_t num_records, double sample_rate,
                         double center_freq);

  dp_pub_t *dp_pub_create_tlm (const char *endpoint);

  int dp_pub_flush (dp_pub_t *ctx, int timeout_ms);

  int dp_stream_drain (dp_pub_t *ctx, int timeout_ms);

  void dp_pub_destroy (dp_pub_t *ctx);

  dp_sub_t *dp_sub_create (const char *endpoint);

  int dp_sub_recv (dp_sub_t *ctx, dp_msg_t **msg, dp_header_t *header);

  void dp_sub_set_timeout (dp_sub_t *ctx, int timeout_ms);

  void dp_sub_destroy (dp_sub_t *ctx);


  /* -------------------------------------------------------------------------
   * Push / Pull  (PUSH/PULL — pipeline / load-balanced)
   * ---------------------------------------------------------------------- */

  dp_push_t *dp_push_create (const char      *endpoint,
                             dp_sample_type_t sample_type);

  dp_pull_t *dp_pull_create (const char *endpoint);

  int dp_push_send_ci32 (dp_push_t *ctx, const int32_t *samples,
                         size_t num_samples, double sample_rate,
                         double center_freq);

  int dp_push_send_cf64 (dp_push_t *ctx, const double _Complex *samples,
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
  int dp_req_send_cf64 (
      dp_req_t *ctx, const double _Complex *samples, size_t num_samples,
      double sample_rate,
      double center_freq); 
  int dp_req_send_ci8 (dp_req_t *ctx, const int8_t *samples,
                       size_t num_samples, double sample_rate,
                       double center_freq);
  int dp_req_send_ci16 (dp_req_t *ctx, const int16_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  int dp_req_send_cf32 (dp_req_t *ctx, const float _Complex *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);

  int dp_rep_send_ci32 (dp_rep_t *ctx, const int32_t *samples,
                        size_t num_samples, double sample_rate,
                        double center_freq);
  int dp_rep_send_cf64 (
      dp_rep_t *ctx, const double _Complex *samples, size_t num_samples,
      double sample_rate,
      double center_freq); 
  int dp_rep_send_ci8 (dp_rep_t *ctx, const int8_t *samples,
                       size_t num_samples, double sample_rate,
                       double center_freq);
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


  /* -------------------------------------------------------------------------
   * Utilities
   * ---------------------------------------------------------------------- */

  const char *dp_sample_type_str (dp_sample_type_t type);

  size_t dp_sample_size (dp_sample_type_t type);

  int dp_sample_type_is_valid (dp_sample_type_t type);

  size_t dp_element_size (dp_frame_kind_t kind, dp_sample_type_t format);

  const char *dp_host_rep (void);


  void dp_stream_interrupt (void);

/* Defined by dp_interrupt.h, which owns the primitive. */

  void dp_stream_set_interrupt_latency_ms (unsigned ms);

  unsigned dp_stream_interrupt_latency_ms (void);

  void dp_stream_resume (void);

  int dp_stream_interrupted (void);

  int dp_stream_interrupt_on_signal (int sig);

  int dp_stream_restore_signal (int sig);


  uint64_t dp_get_timestamp_ns (void);

  void dp_ctx_set_timestamp_ns (dp_pub_t *ctx, uint64_t timestamp_ns);

  const char *dp_strerror (int err);


#ifdef __cplusplus
}
#endif

#endif /* DP_STREAM_H */
```


