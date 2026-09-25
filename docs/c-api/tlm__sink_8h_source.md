

# File tlm\_sink.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**stream**](dir_2fbcc177cba4f14addc502f26acbb8f7.md) **>** [**tlm\_sink.h**](tlm__sink_8h.md)

[Go to the documentation of this file](tlm__sink_8h.md)


```C++

#ifndef TLM_SINK_H
#define TLM_SINK_H

#include "doppler/dp_tlm/dp_tlm_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct dp_tlm_sink dp_tlm_sink_t;

  dp_tlm_sink_t *dp_tlm_sink_open (const char *endpoint);

  int dp_tlm_sink_pump (dp_tlm_sink_t *sink, dp_tlm_t *tlm);

  uint64_t dp_tlm_sink_sent (const dp_tlm_sink_t *sink);

  void dp_tlm_sink_close (dp_tlm_sink_t *sink);

#ifdef __cplusplus
}
#endif

#endif /* TLM_SINK_H */
```


