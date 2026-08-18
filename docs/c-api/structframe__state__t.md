

# Struct frame\_state\_t



[**ClassList**](annotated.md) **>** [**frame\_state\_t**](structframe__state__t.md)



_Frame state._ [More...](#detailed-description)

* `#include <frame_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) | [**d**](#variable-d)  <br> |
|  [**wfm\_frame\_desc\_layout\_t**](structwfm__frame__desc__layout__t.md) | [**dl**](#variable-dl)  <br> |
|  [**wfm\_frame\_t**](structwfm__frame__t.md) | [**f**](#variable-f)  <br> |
|  [**wfm\_frame\_layout\_t**](structwfm__frame__layout__t.md) | [**l**](#variable-l)  <br> |
|  int | [**named**](#variable-named)  <br> |
|  size\_t | [**nbits**](#variable-nbits)  <br> |
|  uint8\_t \* | [**one**](#variable-one)  <br> |
|  uint8\_t \* | [**own**](#variable-own)  <br> |












































## Detailed Description


Allocate with [**frame\_create()**](frame__core_8h.md#function-frame_create). 


    
## Public Attributes Documentation




### variable d 

```C++
wfm_frame_desc_t frame_state_t::d;
```



The DESCRIPTION — fields and stages — which is what everything here delegates on. `wfm_frame_t` is one configuration of it, so the thirty-odd-argument constructor and the field-by-field builder produce the same kind of thing and share every method below. Its `bits` pointers address the owned copies, never the caller's arrays: a Python buffer is released the moment the call that supplied it returns. 


        

<hr>



### variable dl 

```C++
wfm_frame_desc_layout_t frame_state_t::dl;
```



The general layout, derived at build. 


        

<hr>



### variable f 

```C++
wfm_frame_t frame_state_t::f;
```



The four-field configuration, kept only when the object was built that way — it is what `layout()`'s NAMED view reports. A description built field by field has no preamble/sync/payload/crc to name, and `layout()` says so by reporting a zero `total_bits` rather than inventing offsets for fields that do not exist. 


        

<hr>



### variable l 

```C++
wfm_frame_layout_t frame_state_t::l;
```



Computed at create for the configured path; zero otherwise. 


        

<hr>



### variable named 

```C++
int frame_state_t::named;
```



Non-zero once the configured path filled `f` and `l`. 


        

<hr>



### variable nbits 

```C++
size_t frame_state_t::nbits;
```




<hr>



### variable one 

```C++
uint8_t* frame_state_t::one;
```



One materialised frame, built at create (configured) or at `build()` (described) — which is also the proof the description CAN be materialised. `bits()` repeats this rather than regenerating, so every repeat is bit-identical by construction and a PN field cannot advance its register between them. 


        

<hr>



### variable own 

```C++
uint8_t* frame_state_t::own[WFM_FRAME_MAX_FIELDS];
```



Owned copies of every literal field; NULL for a generated kind. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/frame/frame_core.h`

