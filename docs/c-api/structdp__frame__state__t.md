

# Struct dp\_frame\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_frame\_state\_t**](structdp__frame__state__t.md)



_Frame state._ [More...](#detailed-description)

* `#include <frame_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**built**](#variable-built)  <br> |
|  [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) | [**d**](#variable-d)  <br> |
|  [**wfm\_frame\_desc\_layout\_t**](structwfm__frame__desc__layout__t.md) | [**dl**](#variable-dl)  <br> |
|  size\_t | [**nbits**](#variable-nbits)  <br> |
|  uint8\_t \* | [**one**](#variable-one)  <br> |
|  uint8\_t \* | [**own**](#variable-own)  <br> |
|  int | [**rx\_checked**](#variable-rx_checked)  <br> |
|  int | [**rx\_ok**](#variable-rx_ok)  <br> |
|  int | [**rx\_symbols**](#variable-rx_symbols)  <br> |
|  int | [**rx\_units**](#variable-rx_units)  <br> |












































## Detailed Description


Allocate with [**dp\_frame\_create()**](frame__core_8h.md#function-dp_frame_create). 


    
## Public Attributes Documentation




### variable built 

```C++
int dp_frame_state_t::built;
```



Non-zero once the description is fixed: set by [**dp\_frame\_create**](frame__core_8h.md#function-dp_frame_create) and by a successful [**dp\_frame\_build**](frame__core_8h.md#function-dp_frame_build). It is NOT `one != NULL`, because a description with a data field is built, laid out and checkable, yet has no single frame to hold  its data field has no bits of its own, so `one` stays NULL and [**dp\_frame\_bits**](frame__core_8h.md#function-dp_frame_bits) writes none. 


        

<hr>



### variable d 

```C++
wfm_frame_desc_t dp_frame_state_t::d;
```



The DESCRIPTION — fields and stages — which is what everything here delegates on. The constructor describes the common frame through `dp_wfm_frame_fixed()` and the builder appends field by field, so the two produce the same kind of thing and share every method below. Its `bits` pointers address the owned copies, never the caller's arrays: a Python buffer is released the moment the call that supplied it returns. 


        

<hr>



### variable dl 

```C++
wfm_frame_desc_layout_t dp_frame_state_t::dl;
```



The general layout, derived at build. 


        

<hr>



### variable nbits 

```C++
size_t dp_frame_state_t::nbits;
```




<hr>



### variable one 

```C++
uint8_t* dp_frame_state_t::one;
```



One materialised frame, built at create (configured) or at `build()` (described) — which is also the proof the description CAN be materialised. `bits()` repeats this rather than regenerating, so every repeat is bit-identical by construction and a PN field cannot advance its register between them. 


        

<hr>



### variable own 

```C++
uint8_t* dp_frame_state_t::own[WFM_FRAME_MAX_FIELDS];
```



Owned copies of every literal field; NULL for a generated kind. 


        

<hr>



### variable rx\_checked 

```C++
int dp_frame_state_t::rx_checked;
```



What the last [**dp\_frame\_deframe()**](frame__core_8h.md#function-dp_frame_deframe) found, summed across the stages it reversed. Read-backs rather than a returned record: the call hands back BITS, and jm binds one return value. `rx_checked == 0` means the description carries no reversible stage — which is not the same fact as a failed check, and an FER conflating them would score every unprotected frame as an error. 


        

<hr>



### variable rx\_ok 

```C++
int dp_frame_state_t::rx_ok;
```




<hr>



### variable rx\_symbols 

```C++
int dp_frame_state_t::rx_symbols;
```




<hr>



### variable rx\_units 

```C++
int dp_frame_state_t::rx_units;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/frame/frame_core.h`

