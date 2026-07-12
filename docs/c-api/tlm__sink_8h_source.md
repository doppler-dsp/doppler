

# File tlm\_sink.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**telemetry**](dir_d4543964ddc0423cd91d16ab74a4089e.md) **>** [**tlm\_sink.h**](tlm__sink_8h.md)

[Go to the documentation of this file](tlm__sink_8h.md)


```C++

#ifndef TLM_SINK_H
#define TLM_SINK_H

#include "telemetry/telemetry.h"

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


