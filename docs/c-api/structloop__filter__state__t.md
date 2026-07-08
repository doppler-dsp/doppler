

# Struct loop\_filter\_state\_t



[**ClassList**](annotated.md) **>** [**loop\_filter\_state\_t**](structloop__filter__state__t.md)



_Second-order PI loop filter state (embeddable by value)._ 

* `#include <loop_filter_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**bn**](#variable-bn)  <br> |
|  double | [**integ**](#variable-integ)  <br> |
|  double | [**ki**](#variable-ki)  <br> |
|  double | [**kp**](#variable-kp)  <br> |
|  double | [**t**](#variable-t)  <br> |
|  double | [**zeta**](#variable-zeta)  <br> |












































## Public Attributes Documentation




### variable bn 

```C++
double loop_filter_state_t::bn;
```



loop noise bandwidth, normalized cycles/sample. 


        

<hr>



### variable integ 

```C++
double loop_filter_state_t::integ;
```



integrator memory = running rate/freq estimate. 


        

<hr>



### variable ki 

```C++
double loop_filter_state_t::ki;
```



integral gain (derived from bn, zeta, t). 


        

<hr>



### variable kp 

```C++
double loop_filter_state_t::kp;
```



proportional gain (derived from bn, zeta, t). 


        

<hr>



### variable t 

```C++
double loop_filter_state_t::t;
```



update period in samples. 


        

<hr>



### variable zeta 

```C++
double loop_filter_state_t::zeta;
```



damping factor (0.707 = critically damped). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/loop_filter/loop_filter_core.h`

