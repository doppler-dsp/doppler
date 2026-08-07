

# Struct burst\_acq\_state\_t



[**ClassList**](annotated.md) **>** [**burst\_acq\_state\_t**](structburst__acq__state__t.md)



_BurstAcquisition state: a pure wrapper around one shared_ [_**acq\_state\_t**_](structacq__state__t.md) _engine._[More...](#detailed-description)

* `#include <burst_acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**acq\_state\_t**](structacq__state__t.md) \* | [**engine**](#variable-engine)  <br> |












































## Detailed Description


Allocate with [**burst\_acq\_create()**](burst__acq__core_8h.md#function-burst_acq_create); every other function forwards straight to the corresponding acq\_\* call on `engine`. 


    
## Public Attributes Documentation




### variable engine 

```C++
acq_state_t* burst_acq_state_t::engine;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/burst_acq/burst_acq_core.h`

