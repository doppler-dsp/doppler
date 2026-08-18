

# Struct wfm\_frame\_rx\_t



[**ClassList**](annotated.md) **>** [**wfm\_frame\_rx\_t**](structwfm__frame__rx__t.md)



_What_ [_**wfm\_frame\_check**_](wfm__frame_8h.md#function-wfm_frame_check) _found, stage by stage._[More...](#detailed-description)

* `#include <wfm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  unsigned | [**checked**](#variable-checked)  <br> |
|  unsigned | [**n\_stages**](#variable-n_stages)  <br> |
|  [**wfm\_frame\_stage\_rx\_t**](structwfm__frame__stage__rx__t.md) | [**stage**](#variable-stage)  <br> |












































## Detailed Description


Indexed the same as the description's stages, so a caller reads the result beside the declaration that produced it. 


    
## Public Attributes Documentation




### variable checked 

```C++
unsigned wfm_frame_rx_t::checked;
```



stages actually reversed here 
 


        

<hr>



### variable n\_stages 

```C++
unsigned wfm_frame_rx_t::n_stages;
```




<hr>



### variable stage 

```C++
wfm_frame_stage_rx_t wfm_frame_rx_t::stage[WFM_FRAME_MAX_STAGES];
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

