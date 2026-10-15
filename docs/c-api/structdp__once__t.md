

# Struct dp\_once\_t



[**ClassList**](annotated.md) **>** [**dp\_once\_t**](structdp__once__t.md)



_The state of one one-time build; start it at_ [_**DP\_ONCE\_INIT**_](dp__once_8h.md#define-dp_once_init) _._

* `#include <dp_once.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  atomic\_int | [**state**](#variable-state)  <br> |












































## Public Attributes Documentation




### variable state 

```C++
atomic_int dp_once_t::state;
```



0 not started, 1 building, 2 built 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dp_once.h`

