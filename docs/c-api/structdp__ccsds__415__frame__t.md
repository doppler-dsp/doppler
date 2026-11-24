

# Struct dp\_ccsds\_415\_frame\_t



[**ClassList**](annotated.md) **>** [**dp\_ccsds\_415\_frame\_t**](structdp__ccsds__415__frame__t.md)



[More...](#detailed-description)

* `#include <ccsds_415_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) | [**desc**](#variable-desc)  <br> |
|  uint8\_t | [**header**](#variable-header)  <br> |
|  uint8\_t | [**start**](#variable-start)  <br> |
|  uint8\_t | [**zero**](#variable-zero)  <br> |












































## Detailed Description


A frame description and the bits it borrows: use it in place. 


    
## Public Attributes Documentation




### variable desc 

```C++
wfm_frame_desc_t dp_ccsds_415_frame_t::desc;
```



The description; its literal fields point into the arrays below. 


        

<hr>



### variable header 

```C++
uint8_t dp_ccsds_415_frame_t::header[DP_CCSDS_415_HEADER_BITS];
```



The primary header, one bit a byte, MSB-first. 


        

<hr>



### variable start 

```C++
uint8_t dp_ccsds_415_frame_t::start[DP_CCSDS_415_START_BITS];
```



The start sequence, one bit a byte, MSB-first. 


        

<hr>



### variable zero 

```C++
uint8_t dp_ccsds_415_frame_t::zero;
```



The gap's one bit, a zero, repeated `gap_sym` times. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ccsds/ccsds_415_frame.h`

