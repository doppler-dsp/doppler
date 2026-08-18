

# Struct wfm\_frame\_t



[**ClassList**](annotated.md) **>** [**wfm\_frame\_t**](structwfm__frame__t.md)



_A frame's bit layout:_ `[preamble × reps | sync | payload | crc]` _._[More...](#detailed-description)

* `#include <wfm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**crc**](#variable-crc)  <br> |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**payload**](#variable-payload)  <br> |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**preamble**](#variable-preamble)  <br> |
|  size\_t | [**preamble\_reps**](#variable-preamble_reps)  <br> |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**sync**](#variable-sync)  <br> |












































## Detailed Description


The preamble sits OUTSIDE the sync/payload/CRC group, matching the DSSS contract this generalises: it is unmodulated, it is not covered by the CRC, and in the spread case it is not spread. It is the coherent-integration target.


This is a **configuration** of [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) — four fields and one stage — not a second descriptor. [**wfm\_frame\_layout**](wfm__frame_8h.md#function-wfm_frame_layout) builds it through [**wfm\_frame\_describe**](wfm__frame_8h.md#function-wfm_frame_describe) and reads the general layout back, so there is one implementation of the arithmetic and the two cannot drift. 


    
## Public Attributes Documentation




### variable crc 

```C++
int wfm_frame_t::crc;
```



non-zero: CRC-16-CCITT over the payload, MSB-first 


        

<hr>



### variable payload 

```C++
wfm_seq_t wfm_frame_t::payload;
```




<hr>



### variable preamble 

```C++
wfm_seq_t wfm_frame_t::preamble;
```



len 0 = none 
 


        

<hr>



### variable preamble\_reps 

```C++
size_t wfm_frame_t::preamble_reps;
```



repetitions of `preamble`; 0 = none 
 


        

<hr>



### variable sync 

```C++
wfm_seq_t wfm_frame_t::sync;
```



len 0 = unsynced — BER then needs an external alignment 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

