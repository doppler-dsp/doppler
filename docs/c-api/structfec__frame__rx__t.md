

# Struct fec\_frame\_rx\_t



[**ClassList**](annotated.md) **>** [**fec\_frame\_rx\_t**](structfec__frame__rx__t.md)



_What_ [_**fec\_frame\_decode**_](fec__frame_8h.md#function-fec_frame_decode) _found on the way through._[More...](#detailed-description)

* `#include <fec_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**frame\_len**](#variable-frame_len)  <br> |
|  unsigned | [**rs\_codewords**](#variable-rs_codewords)  <br> |
|  unsigned | [**rs\_ok**](#variable-rs_ok)  <br> |












































## Detailed Description


The outer code is a **check** here and not a correction: doppler has `fec_rs_codeword_ok`'s syndrome test and not yet the Berlekamp-Massey / Chien / Forney chain that would repair a codeword. So [**rs\_ok**](structfec__frame__rx__t.md#variable-rs_ok) is evidence about the link rather than a repair count, and a decode with `rs_ok < rs_codewords` returned a frame that is **wrong in a way this function knows about** — which is exactly why it is reported rather than folded into the return value. 


    
## Public Attributes Documentation




### variable frame\_len 

```C++
size_t fec_frame_rx_t::frame_len;
```



Transfer Frame octets written 
 


        

<hr>



### variable rs\_codewords 

```C++
unsigned fec_frame_rx_t::rs_codewords;
```



Codewords checked; 0 with no outer code 
 


        

<hr>



### variable rs\_ok 

```C++
unsigned fec_frame_rx_t::rs_ok;
```



How many passed the syndrome test 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/fec/fec_frame.h`

