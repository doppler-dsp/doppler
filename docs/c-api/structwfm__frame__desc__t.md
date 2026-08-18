

# Struct wfm\_frame\_desc\_t



[**ClassList**](annotated.md) **>** [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md)



_A frame as a description: what is on the wire, and what covers it._ [More...](#detailed-description)

* `#include <wfm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**wfm\_field\_t**](structwfm__field__t.md) | [**field**](#variable-field)  <br> |
|  unsigned | [**n\_fields**](#variable-n_fields)  <br> |
|  unsigned | [**n\_stages**](#variable-n_stages)  <br> |
|  [**wfm\_stage\_t**](structwfm__stage__t.md) | [**stage**](#variable-stage)  <br> |












































## Detailed Description


Two lists, ordered independently, because order and coverage are independent axes: `field` is ordered by POSITION on the wire and `stage` by APPLICATION. In a CCSDS CADU the marker is inserted third and covered by the stage applied fourth, which a single ordered list cannot say.


A standard's framing is a CONFIGURATION of this, in the same way `CCSDS_TM_CONV` configures `conv_code_t` and `CCSDS_TM_RS` configures `rs_code_t`. [**wfm\_frame\_t**](structwfm__frame__t.md) is the first such configuration and is built by [**wfm\_frame\_describe**](wfm__frame_8h.md#function-wfm_frame_describe).




**See also:** docs/design/frame-description.md 



    
## Public Attributes Documentation




### variable field 

```C++
wfm_field_t wfm_frame_desc_t::field[WFM_FRAME_MAX_FIELDS];
```




<hr>



### variable n\_fields 

```C++
unsigned wfm_frame_desc_t::n_fields;
```




<hr>



### variable n\_stages 

```C++
unsigned wfm_frame_desc_t::n_stages;
```




<hr>



### variable stage 

```C++
wfm_stage_t wfm_frame_desc_t::stage[WFM_FRAME_MAX_STAGES];
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

