

# Struct dp\_state\_filter\_kalman\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_state\_filter\_kalman\_state\_t**](structdp__state__filter__kalman__state__t.md)



_A third-order loop whose gains come from a covariance recursion._ [More...](#detailed-description)

* `#include <state_filter_kalman_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) | [**base**](#variable-base)  <br> |
|  double | [**p**](#variable-p)  <br> |
|  double | [**p0**](#variable-p0)  <br> |
|  double | [**q**](#variable-q)  <br> |
|  double | [**r**](#variable-r)  <br> |












































## Detailed Description


The base is first: its `kp`, `ki`, `kr` are the gains in force (rewritten each update), its `integ` and `rate` the estimates, its `bn` and `t` the steady-state bandwidth and the update period. The covariance is of the PREDICTED state `(phase, frequency, rate)`, its six distinct entries in the order `p00 p01 p02 p11 p12 p22`.


Public so an embedder can hold one by value (the init functions need no heap). Pass a pointer to this type to the `kalman` functions; the base's functions that only read ([**dp\_state\_filter\_stable()**](state__filter__core_8h.md#function-dp_state_filter_stable), [**dp\_state\_filter\_bandwidth()**](state__filter__core_8h.md#function-dp_state_filter_bandwidth), the fields) accept `&k->base`, and those that write do not (they do not know the covariance). 


    
## Public Attributes Documentation




### variable base 

```C++
dp_state_filter_state_t dp_state_filter_kalman_state_t::base;
```



gains in force, estimates, bn, t 
 


        

<hr>



### variable p 

```C++
double dp_state_filter_kalman_state_t::p[6];
```



covariance of the predicted state 
 


        

<hr>



### variable p0 

```C++
double dp_state_filter_kalman_state_t::p0[6];
```



covariance reset() returns to 
 


        

<hr>



### variable q 

```C++
double dp_state_filter_kalman_state_t::q;
```



rate-walk variance per update 
 


        

<hr>



### variable r 

```C++
double dp_state_filter_kalman_state_t::r;
```



detector-error variance per update 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/state_filter_kalman/state_filter_kalman_core.h`

