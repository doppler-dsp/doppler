

# Struct wfm\_frame\_stage\_rx\_t



[**ClassList**](annotated.md) **>** [**wfm\_frame\_stage\_rx\_t**](structwfm__frame__stage__rx__t.md)



_What undoing one stage found._ [More...](#detailed-description)

* `#include <wfm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**checked**](#variable-checked)  <br> |
|  unsigned | [**corrected**](#variable-corrected)  <br> |
|  unsigned | [**ok**](#variable-ok)  <br> |
|  unsigned | [**symbols**](#variable-symbols)  <br> |
|  unsigned | [**units**](#variable-units)  <br> |












































## Detailed Description


One shape for every checking stage, because a caller doing frame accounting wants to compare them rather than learn a struct per code. A CRC reports one unit that is either good or not; an interleaved outer code reports one unit per codeword, with the repair work it did.


`corrected` and `symbols` are the honest measure of how hard the link is running: `ok == units` with a rising `symbols` is a margin being spent, and it is spent before it is lost. 


    
## Public Attributes Documentation




### variable checked 

```C++
int wfm_frame_stage_rx_t::checked;
```



0 when the receiver does not reverse this stage here; its counts are then meaningless 
 


        

<hr>



### variable corrected 

```C++
unsigned wfm_frame_stage_rx_t::corrected;
```



how many needed and received repair 
 


        

<hr>



### variable ok 

```C++
unsigned wfm_frame_stage_rx_t::ok;
```



how many are good AFTERWARDS — clean or fixed 


        

<hr>



### variable symbols 

```C++
unsigned wfm_frame_stage_rx_t::symbols;
```



symbol errors repaired across the span 
 


        

<hr>



### variable units 

```C++
unsigned wfm_frame_stage_rx_t::units;
```



things checked: codewords, or 1 for a CRC 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

