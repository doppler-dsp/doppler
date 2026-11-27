

# File ratesync\_private.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**ratesync**](dir_6debd7d9ad90888bb5b8a40a122d95e8.md) **>** [**ratesync\_private.h**](ratesync__private_8h.md)

[Go to the source code of this file](ratesync__private_8h_source.md)

_Where the terminal stage's next output reads the input._ [More...](#detailed-description)

* `#include "doppler/ratesync/ratesync_core.h"`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  double | [**dp\_ratesync\_next\_read**](#function-dp_ratesync_next_read) (const [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md) \* s) <br> |


























## Detailed Description


Not part of the public API. [**dp\_ratesync\_seed()**](ratesync__core_8h.md#function-dp_ratesync_seed) places an output by this mapping, and the harnesses that measure an output's timing error read it back (native/tests/ratesync/ratesync\_settle\_test.h), so there is one statement of it and a change to it moves both. 


    
## Public Static Functions Documentation




### function dp\_ratesync\_next\_read 

```C++
static inline double dp_ratesync_next_read (
    const dp_ratesync_state_t * s
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ratesync/ratesync_private.h`

