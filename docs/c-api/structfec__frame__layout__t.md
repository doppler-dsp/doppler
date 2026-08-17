

# Struct fec\_frame\_layout\_t



[**ClassList**](annotated.md) **>** [**fec\_frame\_layout\_t**](structfec__frame__layout__t.md)



_The shape of one CADU, and what each stage covered._ [More...](#detailed-description)

* `#include <fec_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**block\_bits**](#variable-block_bits)  <br> |
|  size\_t | [**cadu\_bits**](#variable-cadu_bits)  <br> |
|  [**fec\_frame\_span\_t**](structfec__frame__span__t.md) | [**inner**](#variable-inner)  <br> |
|  [**fec\_frame\_span\_t**](structfec__frame__span__t.md) | [**marker**](#variable-marker)  <br> |
|  size\_t | [**out\_bits**](#variable-out_bits)  <br> |
|  [**fec\_frame\_span\_t**](structfec__frame__span__t.md) | [**outer**](#variable-outer)  <br> |
|  [**fec\_frame\_span\_t**](structfec__frame__span__t.md) | [**randomised**](#variable-randomised)  <br> |












































## Detailed Description


The three stage spans are the point: [**inner**](structfec__frame__layout__t.md#variable-inner) starts at bit 0 while [**outer**](structfec__frame__layout__t.md#variable-outer) and [**randomised**](structfec__frame__layout__t.md#variable-randomised) start after the marker, and that single difference is the whole content of 9.2.1.5 and 10.3.4. 


    
## Public Attributes Documentation




### variable block\_bits 

```C++
size_t fec_frame_layout_t::block_bits;
```



The codeblock — or the frame, with no outer code 


        

<hr>



### variable cadu\_bits 

```C++
size_t fec_frame_layout_t::cadu_bits;
```



Marker plus block, i.e. the whole CADU 


        

<hr>



### variable inner 

```C++
fec_frame_span_t fec_frame_layout_t::inner;
```



What the inner code covered (3.2.1) 


        

<hr>



### variable marker 

```C++
fec_frame_span_t fec_frame_layout_t::marker;
```



The ASM itself (9.4.1) 


        

<hr>



### variable out\_bits 

```C++
size_t fec_frame_layout_t::out_bits;
```



Channel symbols [**fec\_frame\_encode**](fec__frame_8h.md#function-fec_frame_encode) writes 


        

<hr>



### variable outer 

```C++
fec_frame_span_t fec_frame_layout_t::outer;
```



The R-S encoded data space (9.5.1) 


        

<hr>



### variable randomised 

```C++
fec_frame_span_t fec_frame_layout_t::randomised;
```



What the randomiser covered (10.3.2) 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/fec/fec_frame.h`

