

# Struct dp\_u8\_to\_f32\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_u8\_to\_f32\_state\_t**](structdp__u8__to__f32__state__t.md)



_U8ToF32 state._ [More...](#detailed-description)

* `#include <u8_to_f32_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float | [**iscale**](#variable-iscale)  <br> |
|  int | [**mode**](#variable-mode)  <br> |












































## Detailed Description


Allocate with [**dp\_u8\_to\_f32\_create()**](u8__to__f32__core_8h.md#function-dp_u8_to_f32_create). 


    
## Public Attributes Documentation




### variable iscale 

```C++
float dp_u8_to_f32_state_t::iscale;
```



`1/127.5f`, set at create for the midpoint multiply (unused in shift mode). 


        

<hr>



### variable mode 

```C++
int dp_u8_to_f32_state_t::mode;
```



Mapping, a u8\_to\_f32\_mode\_t value; set and validated at create, never changed. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/u8_to_f32/u8_to_f32_core.h`

