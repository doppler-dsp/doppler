

# File carrier\_acq\_private.h

[**File List**](files.md) **>** [**carrier\_acq**](dir_92c8a71a0b308d7c304bd53eb19bc933.md) **>** [**carrier\_acq\_private.h**](carrier__acq__private_8h.md)

[Go to the documentation of this file](carrier__acq__private_8h.md)


```C++

#ifndef DP_CARRIER_ACQ_PRIVATE_H
#define DP_CARRIER_ACQ_PRIVATE_H

#include "doppler/carrier_acq/carrier_acq_core.h"
#include <stddef.h>

void dp__carrier_acq_derive_threshold (dp_carrier_acq_state_t *ca);

double dp__carrier_acq_derived_threshold (const dp_carrier_acq_state_t *ca,
                                          size_t n_blocks);

#endif /* DP_CARRIER_ACQ_PRIVATE_H */
```


