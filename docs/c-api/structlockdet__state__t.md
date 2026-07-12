

# Struct lockdet\_state\_t



[**ClassList**](annotated.md) **>** [**lockdet\_state\_t**](structlockdet__state__t.md)



_Lock-detector state (embeddable by value; pointer-free POD)._ 

* `#include <lockdet_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**cnt**](#variable-cnt)  <br> |
|  double | [**down\_thresh**](#variable-down_thresh)  <br> |
|  int | [**locked**](#variable-locked)  <br> |
|  uint32\_t | [**n\_down**](#variable-n_down)  <br> |
|  uint32\_t | [**n\_up**](#variable-n_up)  <br> |
|  double | [**up\_thresh**](#variable-up_thresh)  <br> |












































## Public Attributes Documentation




### variable cnt 

```C++
uint32_t lockdet_state_t::cnt;
```



running consecutive-look verify counter. 


        

<hr>



### variable down\_thresh 

```C++
double lockdet_state_t::down_thresh;
```



drop side: miss when metric &lt; down\_thresh. 


        

<hr>



### variable locked 

```C++
int lockdet_state_t::locked;
```



current decision (1 = locked). 


        

<hr>



### variable n\_down 

```C++
uint32_t lockdet_state_t::n_down;
```



consecutive misses required to drop (&gt;= 1). 


        

<hr>



### variable n\_up 

```C++
uint32_t lockdet_state_t::n_up;
```



consecutive hits required to declare (&gt;= 1). 


        

<hr>



### variable up\_thresh 

```C++
double lockdet_state_t::up_thresh;
```



declare side: hit when metric &gt; up\_thresh. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/lockdet/lockdet_core.h`

