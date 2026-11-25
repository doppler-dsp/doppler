

# File ccsds\_core.h

[**File List**](files.md) **>** [**ccsds**](dir_c033bcb1c91e23f54b625bfe2cf0448c.md) **>** [**ccsds\_core.h**](ccsds__core_8h.md)

[Go to the documentation of this file](ccsds__core_8h.md)


```C++

#ifndef DP_CCSDS_CORE_H
#define DP_CCSDS_CORE_H

#include "doppler/ccsds/ccsds_415.h"
#include "doppler/clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Declare module-level functions here. */

void dp_asm_bits(uint8_t *out);

double dp_ccsds415_chip_rate(double fc);

double dp_ccsds415_cn0(double esn0_db);


dp_ccsds_415_scene_t dp_ccsds415_scene(double fc, double spc, double start_hz);

#ifdef __cplusplus
}
#endif

#endif /* CCSDS_CORE_H */
```


