

# Struct wfm\_frame\_desc\_layout\_t



[**ClassList**](annotated.md) **>** [**wfm\_frame\_desc\_layout\_t**](structwfm__frame__desc__layout__t.md)



_Where every field and every stage landed._ 

* `#include <wfm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**field\_bits**](#variable-field_bits)  <br> |
|  size\_t | [**field\_off**](#variable-field_off)  <br> |
|  size\_t | [**frame\_bits**](#variable-frame_bits)  <br> |
|  unsigned | [**n\_fields**](#variable-n_fields)  <br> |
|  unsigned | [**n\_stages**](#variable-n_stages)  <br> |
|  size\_t | [**out\_bits**](#variable-out_bits)  <br> |
|  [**wfm\_frame\_span\_t**](structwfm__frame__span__t.md) | [**stage**](#variable-stage)  <br> |












































## Public Attributes Documentation




### variable field\_bits 

```C++
size_t wfm_frame_desc_layout_t::field_bits[WFM_FRAME_MAX_FIELDS];
```



bits per field 
 


        

<hr>



### variable field\_off 

```C++
size_t wfm_frame_desc_layout_t::field_off[WFM_FRAME_MAX_FIELDS];
```



bit offset per field 
 


        

<hr>



### variable frame\_bits 

```C++
size_t wfm_frame_desc_layout_t::frame_bits;
```



the assembled frame, every field end to end 
 


        

<hr>



### variable n\_fields 

```C++
unsigned wfm_frame_desc_layout_t::n_fields;
```




<hr>



### variable n\_stages 

```C++
unsigned wfm_frame_desc_layout_t::n_stages;
```




<hr>



### variable out\_bits 

```C++
size_t wfm_frame_desc_layout_t::out_bits;
```



what leaves the last stage that emits a new stream; equals `frame_bits` when none does 
 


        

<hr>



### variable stage 

```C++
wfm_frame_span_t wfm_frame_desc_layout_t::stage[WFM_FRAME_MAX_STAGES];
```



what each stage covers 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

