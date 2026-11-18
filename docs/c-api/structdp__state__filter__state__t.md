

# Struct dp\_state\_filter\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md)



_Third-order tracking-loop filter state (embeddable by value)._ 

* `#include <state_filter_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**bn**](#variable-bn)  <br> |
|  double | [**integ**](#variable-integ)  <br> |
|  double | [**ki**](#variable-ki)  <br> |
|  double | [**kp**](#variable-kp)  <br> |
|  double | [**kr**](#variable-kr)  <br> |
|  double | [**rate**](#variable-rate)  <br> |
|  double | [**t**](#variable-t)  <br> |












































## Public Attributes Documentation




### variable bn 

```C++
double dp_state_filter_state_t::bn;
```



loop noise bandwidth, normalized cycles/sample. 
 


        

<hr>



### variable integ 

```C++
double dp_state_filter_state_t::integ;
```



frequency estimate, control units per update. 
 


        

<hr>



### variable ki 

```C++
double dp_state_filter_state_t::ki;
```



frequency-path gain (from bn, t). 
 


        

<hr>



### variable kp 

```C++
double dp_state_filter_state_t::kp;
```



proportional gain (from bn, t). 
 


        

<hr>



### variable kr 

```C++
double dp_state_filter_state_t::kr;
```



rate-path gain (from bn, t). 
 


        

<hr>



### variable rate 

```C++
double dp_state_filter_state_t::rate;
```



slope of the frequency estimate: control units per update per update (a Doppler rate R in Hz/s at an update period T in seconds is R \* T^2 cycles per update per update). 
 


        

<hr>



### variable t 

```C++
double dp_state_filter_state_t::t;
```



update period in samples. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/state_filter/state_filter_core.h`

