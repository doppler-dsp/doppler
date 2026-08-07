

# Struct ratesync\_state\_t



[**ClassList**](annotated.md) **>** [**ratesync\_state\_t**](structratesync__state__t.md)



_RateSync state: a matched-filter cascade and the timing loop._ [More...](#detailed-description)

* `#include <ratesync_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**beta**](#variable-beta)  <br> |
|  [**ratesync\_loop\_t**](structratesync__loop__t.md) | [**loop**](#variable-loop)  <br> |
|  [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* | [**mf**](#variable-mf)  <br> |
|  size\_t | [**num\_phases**](#variable-num_phases)  <br> |
|  int | [**pulse**](#variable-pulse)  <br> |
|  size\_t | [**span**](#variable-span)  <br> |












































## Detailed Description


The matched filter is a heap `RateConverter` child (it owns the cascade, the banks and every delay line); the loop is embedded by value. 


    
## Public Attributes Documentation




### variable beta 

```C++
double ratesync_state_t::beta;
```



RRC roll-off. 
 


        

<hr>



### variable loop 

```C++
ratesync_loop_t ratesync_state_t::loop;
```



timing loop closed around it. 
 


        

<hr>



### variable mf 

```C++
RateConverter_state_t* ratesync_state_t::mf;
```



cascade; terminal stage is the MF. 
 


        

<hr>



### variable num\_phases 

```C++
size_t ratesync_state_t::num_phases;
```



bank arms (power of two). 
 


        

<hr>



### variable pulse 

```C++
int ratesync_state_t::pulse;
```



RATESYNC\_PULSE\_IANDD / \_RRC. 
 


        

<hr>



### variable span 

```C++
size_t ratesync_state_t::span;
```



one-sided RRC span, symbols. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ratesync/ratesync_core.h`

