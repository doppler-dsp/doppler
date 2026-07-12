

# Struct farrow\_state\_t



[**ClassList**](annotated.md) **>** [**farrow\_state\_t**](structfarrow__state__t.md)



_Farrow interpolator state (4-tap delay line + order)._ [More...](#detailed-description)

* `#include <farrow_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float complex | [**d**](#variable-d)  <br> |
|  int | [**order**](#variable-order)  <br> |












































## Detailed Description


Public so a timing loop can embed it by value; treat the delay line as internal (drive it through farrow\_push / farrow\_eval). 


    
## Public Attributes Documentation




### variable d 

```C++
float complex farrow_state_t::d[4];
```



delay line, `d [3]` newest. 


        

<hr>



### variable order 

```C++
int farrow_state_t::order;
```



FARROW\_LINEAR / \_PARABOLIC / \_CUBIC. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/farrow/farrow_core.h`

