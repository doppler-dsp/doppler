

# File frame\_meter\_core.h

[**File List**](files.md) **>** [**frame\_meter**](dir_7d049e2511dda4d27f50479ac6f6567b.md) **>** [**frame\_meter\_core.h**](frame__meter__core_8h.md)

[Go to the documentation of this file](frame__meter__core_8h.md)


```C++

#ifndef FRAME_METER_CORE_H
#define FRAME_METER_CORE_H

#include "ber/ber_core.h"
#include "dp_state.h"

#include <stddef.h>
#include <stdint.h>
#include "detection/detection_core.h"
#include "ber_meter/ber_meter_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define FRAME_METER_STATE_MAGIC   DP_FOURCC ('F', 'R', 'M', 'M')
#define FRAME_METER_STATE_VERSION 1u

  typedef struct
  {
    size_t target_errors; 
    double conf;          
    size_t frames;        
    size_t sync_detected; 
    size_t crc_passed;    
    size_t errors;        
  } frame_meter_state_t;

  frame_meter_state_t *frame_meter_create (size_t target_errors, double conf);

  void frame_meter_destroy (frame_meter_state_t *state);

  void frame_meter_reset (frame_meter_state_t *state);

  void frame_meter_add (frame_meter_state_t *state, int sync_ok, int crc);

  size_t frame_meter_get_frames (const frame_meter_state_t *state);
  size_t frame_meter_get_sync_detected (const frame_meter_state_t *state);
  size_t frame_meter_get_crc_passed (const frame_meter_state_t *state);
  size_t frame_meter_get_errors (const frame_meter_state_t *state);

  int frame_meter_get_enough (const frame_meter_state_t *state);

  ber_interval_t frame_meter_fer (const frame_meter_state_t *state);

  ber_interval_t frame_meter_sync_miss (const frame_meter_state_t *state);

  size_t frame_meter_state_bytes (const frame_meter_state_t *state);
  void frame_meter_get_state (const frame_meter_state_t *state, void *blob);
  int frame_meter_set_state (frame_meter_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* FRAME_METER_CORE_H */
```


