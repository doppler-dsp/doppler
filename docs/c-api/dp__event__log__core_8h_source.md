

# File dp\_event\_log\_core.h

[**File List**](files.md) **>** [**dp\_event\_log**](dir_f94295323d6f0149be6a261903cfcf6a.md) **>** [**dp\_event\_log\_core.h**](dp__event__log__core_8h.md)

[Go to the documentation of this file](dp__event__log__core_8h.md)


```C++

#ifndef DP_EVENT_LOG_CORE_H
#define DP_EVENT_LOG_CORE_H

#include <stddef.h>
#include <stdint.h>

#include "clib_common.h" /* DP_OK, DP_ERR_INVALID, DP_ERR_SEND */
#include "wfm_writer/wfm_writer_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DP_EVENT_LOG_MAX_FIELDS 16
#define DP_EVENT_LOG_NAME_MAX 32
#define DP_EVENT_LOG_STR_MAX 64
#define DP_EVENT_LOG_LINE_MAX 16384

typedef struct dp_event_log dp_event_log_t;

typedef dp_event_log_t dp_event_log_state_t;

dp_event_log_t *dp_event_log_open (const char *path, double fc);

int dp_event_log_close (dp_event_log_t *log);

int dp_event_log_destroy (dp_event_log_t *log);

int dp_event_log_field (dp_event_log_t *log, const char *name, double value);

int dp_event_log_field_str (dp_event_log_t *log, const char *name,
                            const char *value);

int dp_event_log_append (dp_event_log_t *log, uint64_t sample_start,
                         const char *label, uint64_t sample_count,
                         double freq_hz, double bandwidth_hz);

size_t dp_event_log_count (const dp_event_log_t *log);

int dp_event_log_finalize (dp_event_log_t *log, const char *meta_path,
                           int sample_type, int endian, double fs,
                           double t0_unix_sec);

int dp_event_log_set_dataset (dp_event_log_t *log, const char *name);

int dp_event_log_set_telemetry (dp_event_log_t *log, const char *path);

int dp_event_log_write_meta (const char *log_path, const char *meta_path,
                             int sample_type, int endian, double fs, double fc,
                             double t0_unix_sec, const char *dataset,
                             const char *telemetry);

#ifdef __cplusplus
}
#endif

#endif /* DP_EVENT_LOG_CORE_H */
```


