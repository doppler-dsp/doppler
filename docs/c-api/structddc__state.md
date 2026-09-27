

# Struct ddc\_state



[**ClassList**](annotated.md) **>** [**ddc\_state**](structddc__state.md)



_Ddc state — an LO and the cascade it feeds._ [More...](#detailed-description)

* `#include <ddc_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* | [**lo**](#variable-lo)  <br> |
|  bool | [**narrow\_pulse**](#variable-narrow_pulse)  <br> |
|  [**dp\_RateConverter\_state\_t**](structdp__RateConverter__state__t.md) \* | [**rc**](#variable-rc)  <br> |












































## Detailed Description


Do not initialise directly; use [**dp\_ddc\_create()**](ddc__core_8h.md#function-dp_ddc_create) or [**ddc\_create\_matched()**](ddc__core_8h.md#function-ddc_create_matched). 


    
## Public Attributes Documentation




### variable lo 

```C++
dp_lo_state_t* ddc_state::lo;
```



carrier wipe-off, at the input rate 


        

<hr>



### variable narrow\_pulse 

```C++
bool ddc_state::narrow_pulse;
```



Set when the matched flavor was built with a rectangular pulse too narrow to be worth much — see [**ddc\_create\_matched()**](ddc__core_8h.md#function-ddc_create_matched). Read by the binding, which turns it into a UserWarning at construction. 


        

<hr>



### variable rc 

```C++
dp_RateConverter_state_t* ddc_state::rc;
```



the cascade; matched when a pulse was selected at construction 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ddc/ddc_core.h`

