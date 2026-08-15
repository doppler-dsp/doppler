

# Struct wfm\_frame\_layout\_t



[**ClassList**](annotated.md) **>** [**wfm\_frame\_layout\_t**](structwfm__frame__layout__t.md)



_Where each field lands, in bits from the start of the frame._ 

* `#include <wfm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**crc\_bits**](#variable-crc_bits)  <br> |
|  size\_t | [**crc\_off**](#variable-crc_off)  <br> |
|  size\_t | [**payload\_bits**](#variable-payload_bits)  <br> |
|  size\_t | [**payload\_off**](#variable-payload_off)  <br> |
|  size\_t | [**preamble\_bits**](#variable-preamble_bits)  <br> |
|  size\_t | [**preamble\_off**](#variable-preamble_off)  <br> |
|  size\_t | [**sync\_bits**](#variable-sync_bits)  <br> |
|  size\_t | [**sync\_off**](#variable-sync_off)  <br> |
|  size\_t | [**total\_bits**](#variable-total_bits)  <br> |












































## Public Attributes Documentation




### variable crc\_bits 

```C++
size_t wfm_frame_layout_t::crc_bits;
```



16, or 0 when `crc` is unset or the payload is empty — a CRC over nothing protects nothing 
 


        

<hr>



### variable crc\_off 

```C++
size_t wfm_frame_layout_t::crc_off;
```




<hr>



### variable payload\_bits 

```C++
size_t wfm_frame_layout_t::payload_bits;
```




<hr>



### variable payload\_off 

```C++
size_t wfm_frame_layout_t::payload_off;
```




<hr>



### variable preamble\_bits 

```C++
size_t wfm_frame_layout_t::preamble_bits;
```




<hr>



### variable preamble\_off 

```C++
size_t wfm_frame_layout_t::preamble_off;
```




<hr>



### variable sync\_bits 

```C++
size_t wfm_frame_layout_t::sync_bits;
```




<hr>



### variable sync\_off 

```C++
size_t wfm_frame_layout_t::sync_off;
```




<hr>



### variable total\_bits 

```C++
size_t wfm_frame_layout_t::total_bits;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

