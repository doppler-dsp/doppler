

# Struct dp\_burst\_acq\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_burst\_acq\_state\_t**](structdp__burst__acq__state__t.md)



_BurstAcquisition state: a pure wrapper around one shared_ [_**dp\_acq\_state\_t**_](structdp__acq__state__t.md) _engine._[More...](#detailed-description)

* `#include <burst_acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_acq\_state\_t**](structdp__acq__state__t.md) \* | [**engine**](#variable-engine)  <br> |
|  uint8\_t | [**underpowered**](#variable-underpowered)  <br> |












































## Detailed Description


Allocate with [**dp\_burst\_acq\_create()**](burst__acq__core_8h.md#function-dp_burst_acq_create); every other function forwards straight to the corresponding acq\_\* call on `engine`. 


    
## Public Attributes Documentation




### variable engine 

```C++
dp_acq_state_t* dp_burst_acq_state_t::engine;
```




<hr>



### variable underpowered 

```C++
uint8_t dp_burst_acq_state_t::underpowered;
```



The engine's `underpowered` at construction  a field of THIS struct because a declared jm warning's condition must be one; it is what raises the under-powered UserWarning after **init**. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/burst_acq/burst_acq_core.h`

