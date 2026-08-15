

# Struct frame\_state\_t



[**ClassList**](annotated.md) **>** [**frame\_state\_t**](structframe__state__t.md)



_Frame state._ [More...](#detailed-description)

* `#include <frame_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**wfm\_frame\_t**](structwfm__frame__t.md) | [**f**](#variable-f)  <br> |
|  [**wfm\_frame\_layout\_t**](structwfm__frame__layout__t.md) | [**l**](#variable-l)  <br> |
|  size\_t | [**nbits**](#variable-nbits)  <br> |
|  uint8\_t \* | [**one**](#variable-one)  <br> |
|  uint8\_t \* | [**payload\_own**](#variable-payload_own)  <br> |
|  uint8\_t \* | [**preamble\_own**](#variable-preamble_own)  <br> |
|  uint8\_t \* | [**sync\_own**](#variable-sync_own)  <br> |












































## Detailed Description


Allocate with [**frame\_create()**](frame__core_8h.md#function-frame_create). 


    
## Public Attributes Documentation




### variable f 

```C++
wfm_frame_t frame_state_t::f;
```



The descriptor, handed to the `wfm_frame_*` calls verbatim. Its three `bits` pointers address the owned copies below, never the caller's arrays — a Python buffer is released the moment the constructor returns. 


        

<hr>



### variable l 

```C++
wfm_frame_layout_t frame_state_t::l;
```



Computed once, at create. Nothing in this component recomputes a field offset; `layout()` hands this back and `crc_ok()` lets `wfm_frame.c` derive its own from the same descriptor. 


        

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



One materialised frame, built at create — which is also the proof the descriptor CAN be materialised. `bits()` repeats this rather than regenerating, so every repeat is bit-identical by construction and a PN field cannot advance its register between them. 


        

<hr>



### variable payload\_own 

```C++
uint8_t * frame_state_t::payload_own;
```




<hr>



### variable preamble\_own 

```C++
uint8_t* frame_state_t::preamble_own;
```



Owned copies of the literal fields; NULL for a generated kind. 


        

<hr>



### variable sync\_own 

```C++
uint8_t * frame_state_t::sync_own;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/frame/frame_core.h`

