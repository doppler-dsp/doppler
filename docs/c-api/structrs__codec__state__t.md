

# Struct rs\_codec\_state\_t



[**ClassList**](annotated.md) **>** [**rs\_codec\_state\_t**](structrs__codec__state__t.md)



_A code and the tables derived from it._ [More...](#detailed-description)

* `#include <rs_codec_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**rs\_t**](structrs__t.md) | [**rs**](#variable-rs)  <br> |












































## Detailed Description


One `rs_t` and nothing else. The derived sizes — `n`, `k`, `e` — are read back through the accessors below rather than mirrored into fields here, because two copies of a derived number is how they come to disagree.


Allocate with [**rs\_codec\_create()**](rs__codec__core_8h.md#function-rs_codec_create). 


    
## Public Attributes Documentation




### variable rs 

```C++
rs_t rs_codec_state_t::rs;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/rs_codec/rs_codec_core.h`

