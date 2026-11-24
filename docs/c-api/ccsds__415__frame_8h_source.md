

# File ccsds\_415\_frame.h

[**File List**](files.md) **>** [**ccsds**](dir_c033bcb1c91e23f54b625bfe2cf0448c.md) **>** [**ccsds\_415\_frame.h**](ccsds__415__frame_8h.md)

[Go to the documentation of this file](ccsds__415__frame_8h.md)


```C++

#ifndef DP_CCSDS_415_FRAME_H
#define DP_CCSDS_415_FRAME_H

#include "doppler/ccsds/ccsds_415.h"
#include "doppler/wfm/wfm_frame.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define DP_CCSDS_415_ACQ_BITS 128u
#define DP_CCSDS_415_START_SEQ 0xEB90u
#define DP_CCSDS_415_START_BITS 16u
#define DP_CCSDS_415_HEADER_BITS 40u
#define DP_CCSDS_415_DATA_MIN 1u
#define DP_CCSDS_415_DATA_MAX 1017u

typedef struct
{
  unsigned scid;
  unsigned vcid;
  unsigned seq;
  size_t gap_sym;
} dp_ccsds_415_frame_cfg_t;

#define DP_CCSDS_415_FRAME_CFG_DEFAULTS                                       \
  { 42u, 1u, 0u, DP_CCSDS_415_CODE_ONLY_SYM }

typedef struct
{
  wfm_frame_desc_t desc;
  uint8_t zero;
  uint8_t start[DP_CCSDS_415_START_BITS];
  uint8_t header[DP_CCSDS_415_HEADER_BITS];
} dp_ccsds_415_frame_t;

int dp_ccsds_415_frame_desc (dp_ccsds_415_frame_t           *f,
                             const dp_ccsds_415_frame_cfg_t *cfg,
                             size_t data_octets, const char **why);

#ifdef __cplusplus
}
#endif

#endif /* DP_CCSDS_415_FRAME_H */
```


