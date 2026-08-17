

# Struct ccsds\_tm\_frame\_layout\_t



[**ClassList**](annotated.md) **>** [**ccsds\_tm\_frame\_layout\_t**](structccsds__tm__frame__layout__t.md)



_The shape of one CADU, and what each stage covered._ [More...](#detailed-description)

* `#include <ccsds_tm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**block\_bits**](#variable-block_bits)  <br> |
|  size\_t | [**cadu\_bits**](#variable-cadu_bits)  <br> |
|  [**ccsds\_tm\_frame\_span\_t**](structccsds__tm__frame__span__t.md) | [**inner**](#variable-inner)  <br> |
|  [**ccsds\_tm\_frame\_span\_t**](structccsds__tm__frame__span__t.md) | [**marker**](#variable-marker)  <br> |
|  size\_t | [**out\_bits**](#variable-out_bits)  <br> |
|  [**ccsds\_tm\_frame\_span\_t**](structccsds__tm__frame__span__t.md) | [**outer**](#variable-outer)  <br> |
|  [**ccsds\_tm\_frame\_span\_t**](structccsds__tm__frame__span__t.md) | [**randomised**](#variable-randomised)  <br> |












































## Detailed Description


The three stage spans are the point: [**inner**](structccsds__tm__frame__layout__t.md#variable-inner) starts at bit 0 while [**outer**](structccsds__tm__frame__layout__t.md#variable-outer) and [**randomised**](structccsds__tm__frame__layout__t.md#variable-randomised) start after the marker, and that single difference is the whole content of 9.2.1.5 and 10.3.4. 


    
## Public Attributes Documentation




### variable block\_bits 

```C++
size_t ccsds_tm_frame_layout_t::block_bits;
```



The codeblock — or the frame, with no outer code 


        

<hr>



### variable cadu\_bits 

```C++
size_t ccsds_tm_frame_layout_t::cadu_bits;
```



Marker plus block, i.e. the whole CADU 


        

<hr>



### variable inner 

```C++
ccsds_tm_frame_span_t ccsds_tm_frame_layout_t::inner;
```



What the inner code covered (3.2.1) 
 


        

<hr>



### variable marker 

```C++
ccsds_tm_frame_span_t ccsds_tm_frame_layout_t::marker;
```



The ASM itself (9.4.1) 


        

<hr>



### variable out\_bits 

```C++
size_t ccsds_tm_frame_layout_t::out_bits;
```



Channel symbols the encode writes 
 


        

<hr>



### variable outer 

```C++
ccsds_tm_frame_span_t ccsds_tm_frame_layout_t::outer;
```



The R-S encoded data space (9.5.1) 
 


        

<hr>



### variable randomised 

```C++
ccsds_tm_frame_span_t ccsds_tm_frame_layout_t::randomised;
```



What the randomiser covered (10.3.2) 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ccsds_tm/ccsds_tm_frame.h`

