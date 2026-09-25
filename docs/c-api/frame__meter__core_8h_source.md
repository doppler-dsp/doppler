

# File frame\_meter\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**frame\_meter**](dir_ea37d4cf1a6c64bbd4d366fb73a4d95d.md) **>** [**frame\_meter\_core.h**](frame__meter__core_8h.md)

[Go to the documentation of this file](frame__meter__core_8h.md)


```C++

#ifndef FRAME_METER_CORE_H
#define FRAME_METER_CORE_H

#include "doppler/ber/ber_core.h"
#include "doppler/dp_state.h"

#include <stddef.h>
#include <stdint.h>
#include "doppler/detection/detection_core.h"
#include "doppler/ber_meter/ber_meter_core.h"

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


