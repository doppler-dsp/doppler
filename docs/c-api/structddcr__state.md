

# Struct ddcr\_state



[**ClassList**](annotated.md) **>** [**ddcr\_state**](structddcr__state.md)



_DdcR state — the real-to-complex front end, an LO and a cascade._ [More...](#detailed-description)

* `#include <ddcr_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**lo\_state\_t**](structlo__state__t.md) \* | [**lo**](#variable-lo)  <br> |
|  bool | [**narrow\_pulse**](#variable-narrow_pulse)  <br> |
|  [**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* | [**r2c**](#variable-r2c)  <br> |
|  double | [**rate**](#variable-rate)  <br> |
|  [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* | [**rc**](#variable-rc)  <br> |












































## Detailed Description


Do not initialise directly; use [**ddcr\_create()**](ddcr__core_8h.md#function-ddcr_create) or [**ddcr\_create\_matched()**](ddcr__core_8h.md#function-ddcr_create_matched). 


    
## Public Attributes Documentation




### variable lo 

```C++
lo_state_t* ddcr_state::lo;
```



fine tune, at the intermediate rate 
 


        

<hr>



### variable narrow\_pulse 

```C++
bool ddcr_state::narrow_pulse;
```



As [**ddc\_state\_t::narrow\_pulse**](structddc__state.md#variable-narrow_pulse) — a rectangular pulse too narrow to be worth much, surfaced by the binding as a construction UserWarning. 


        

<hr>



### variable r2c 

```C++
hbdecim_r2c_state_t* ddcr_state::r2c;
```



2:1 real-&gt;complex, fs/4 shift baked in 


        

<hr>



### variable rate 

```C++
double ddcr_state::rate;
```



total fs\_out / fs\_in 
 


        

<hr>



### variable rc 

```C++
RateConverter_state_t* ddcr_state::rc;
```



the cascade, running at 2\*rate 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ddcr/ddcr_core.h`

