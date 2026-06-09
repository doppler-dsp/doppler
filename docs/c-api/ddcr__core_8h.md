

# File ddcr\_core.h



[**FileList**](files.md) **>** [**ddcr**](dir_46c04c942eb84c8716610cebe515b046.md) **>** [**ddcr\_core.h**](ddcr__core_8h.md)

[Go to the source code of this file](ddcr__core_8h_source.md)

_DDCR (real-input DDC) — re-exports from_ [_**ddc\_core.h**_](ddc__core_8h.md) _._[More...](#detailed-description)

* `#include "ddc/ddc_core.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**ddcr\_execute\_max\_out**](#function-ddcr_execute_max_out) ([**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) \* state) <br>_Return the maximum output samples for one execute call._  |




























## Detailed Description


All state and lifecycle declarations live in [**ddc/ddc\_core.h**](ddc__core_8h.md). This header exists so jm-managed ddc\_ext\_ddcr.c can include a component-specific path while still resolving against the shared [**ddcr\_state\_t**](ddc__core_8h.md#typedef-ddcr_state_t) defined alongside [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t). 


    
## Public Functions Documentation




### function ddcr\_execute\_max\_out 

_Return the maximum output samples for one execute call._ 
```C++
size_t ddcr_execute_max_out (
    ddcr_state_t * state
) 
```



Returns 0, which signals the Python extension to fall back to allocating n\_in samples — always sufficient for a decimating DDC. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ddcr/ddcr_core.h`

