

# Struct agc\_tlm\_t



[**ClassList**](annotated.md) **>** [**agc\_tlm\_t**](structagc__tlm__t.md)



_Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then a single predicted-not-taken branch. Zeroed in state blobs and preserved across set\_state (DP\_DEFINE\_POD\_STATE\_TLM); telemetry is observation, not DSP state that migrates._ 

* `#include <agc_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* | [**ctx**](#variable-ctx)  <br> |
|  int32\_t | [**id\_gain**](#variable-id_gain)  <br> |
|  int32\_t | [**id\_level**](#variable-id_level)  <br> |












































## Public Attributes Documentation




### variable ctx 

```C++
dp_tlm_t* agc_tlm_t::ctx;
```




<hr>



### variable id\_gain 

```C++
int32_t agc_tlm_t::id_gain;
```




<hr>



### variable id\_level 

```C++
int32_t agc_tlm_t::id_level;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/agc/agc_core.h`

