

# Struct ccsds\_tm\_frame\_rx\_t



[**ClassList**](annotated.md) **>** [**ccsds\_tm\_frame\_rx\_t**](structccsds__tm__frame__rx__t.md)



_What_ [_**ccsds\_tm\_frame\_decode**_](ccsds__tm__frame_8h.md#function-ccsds_tm_frame_decode) _found on the way through._[More...](#detailed-description)

* `#include <ccsds_tm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**frame\_len**](#variable-frame_len)  <br> |
|  unsigned | [**rs\_codewords**](#variable-rs_codewords)  <br> |
|  unsigned | [**rs\_corrected**](#variable-rs_corrected)  <br> |
|  unsigned | [**rs\_ok**](#variable-rs_ok)  <br> |
|  unsigned | [**rs\_symbols**](#variable-rs_symbols)  <br> |












































## Detailed Description


The outer code **corrects** (`rs/rs_core.h`), so [**rs\_ok**](structccsds__tm__frame__rx__t.md#variable-rs_ok) counts the codewords that are good _afterwards_ — clean or repaired — and `rs_ok < rs_codewords` means the returned frame is **wrong in a way this function knows about**: at least one codeword was too far from any codeword to name. That is reported rather than folded into the return value because a caller doing frame accounting wants the count, and a caller wanting only good frames can compare the two.


[**rs\_corrected**](structccsds__tm__frame__rx__t.md#variable-rs_corrected) and [**rs\_symbols**](structccsds__tm__frame__rx__t.md#variable-rs_symbols) are the work the outer code actually did. They are the honest measure of how hard the link is running: `rs_ok == rs_codewords` with a rising [**rs\_symbols**](structccsds__tm__frame__rx__t.md#variable-rs_symbols) is a margin being spent, and it is spent before it is lost. 


    
## Public Attributes Documentation




### variable frame\_len 

```C++
size_t ccsds_tm_frame_rx_t::frame_len;
```



Transfer Frame octets written 
 


        

<hr>



### variable rs\_codewords 

```C++
unsigned ccsds_tm_frame_rx_t::rs_codewords;
```



Codewords decoded; 0 with no outer code 
 


        

<hr>



### variable rs\_corrected 

```C++
unsigned ccsds_tm_frame_rx_t::rs_corrected;
```



How many of those needed repair 
 


        

<hr>



### variable rs\_ok 

```C++
unsigned ccsds_tm_frame_rx_t::rs_ok;
```



How many are valid after decoding 
 


        

<hr>



### variable rs\_symbols 

```C++
unsigned ccsds_tm_frame_rx_t::rs_symbols;
```



Symbol errors repaired across the block 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ccsds_tm/ccsds_tm_frame.h`

