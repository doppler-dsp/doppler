

# Struct wfm\_frame\_spec\_t



[**ClassList**](annotated.md) **>** [**wfm\_frame\_spec\_t**](structwfm__frame__spec__t.md)



_The frame knobs a FACE offers, before they become a description._ [More...](#detailed-description)

* `#include <wfm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  unsigned | [**conv\_den**](#variable-conv_den)  <br> |
|  unsigned | [**conv\_num**](#variable-conv_num)  <br> |
|  int | [**convolutional**](#variable-convolutional)  <br> |
|  int | [**crc**](#variable-crc)  <br> |
|  const uint8\_t \* | [**marker**](#variable-marker)  <br> |
|  size\_t | [**n\_marker**](#variable-n_marker)  <br> |
|  size\_t | [**n\_payload**](#variable-n_payload)  <br> |
|  size\_t | [**n\_preamble**](#variable-n_preamble)  <br> |
|  size\_t | [**n\_sync**](#variable-n_sync)  <br> |
|  const uint8\_t \* | [**payload**](#variable-payload)  <br> |
|  const uint8\_t \* | [**preamble**](#variable-preamble)  <br> |
|  size\_t | [**preamble\_reps**](#variable-preamble_reps)  <br> |
|  int | [**randomise**](#variable-randomise)  <br> |
|  unsigned | [**rs\_depth**](#variable-rs_depth)  <br> |
|  size\_t | [**rs\_parity\_bits**](#variable-rs_parity_bits)  <br> |
|  const uint8\_t \* | [**sync**](#variable-sync)  <br> |












































## Detailed Description


Both directions of a link answer the same question  "which fields, and
which stage covers what"  from the same handful of choices, and the answer is [**wfm\_frame\_desc\_of**](wfm__frame_8h.md#function-wfm_frame_desc_of). A generator fills this from its scene; a receiver fills it from its own configuration. They cannot disagree about the layout because neither one computes it.


A field is a literal the caller supplies. On the RECEIVE side the payload is exactly what is not known yet, so `payload` may be NULL and only `n_payload` matters  the description is a geometry, and a receiver needs the geometry, not the contents.


The two CCSDS numbers arrive as numbers rather than as a dependency: `wfm_frame.c` cannot call `ccsds_tm` (ccsds\_tm depends on it), which is the same reason the stage KERNELS arrive as a table. Each caller passes what the standard says. 


    
## Public Attributes Documentation




### variable conv\_den 

```C++
unsigned wfm_frame_spec_t::conv_den;
```




<hr>



### variable conv\_num 

```C++
unsigned wfm_frame_spec_t::conv_num;
```



Inner-code emit ratio; 0/0 means the rate-1/2 default. 


        

<hr>



### variable convolutional 

```C++
int wfm_frame_spec_t::convolutional;
```



Non-zero: an inner code over the whole frame, marker included. 


        

<hr>



### variable crc 

```C++
int wfm_frame_spec_t::crc;
```



Non-zero: a CRC-16 trailer over the payload group. 


        

<hr>



### variable marker 

```C++
const uint8_t* wfm_frame_spec_t::marker;
```



Marker bits — found, not decoded (a CCSDS ASM, or anything like it). 


        

<hr>



### variable n\_marker 

```C++
size_t wfm_frame_spec_t::n_marker;
```




<hr>



### variable n\_payload 

```C++
size_t wfm_frame_spec_t::n_payload;
```




<hr>



### variable n\_preamble 

```C++
size_t wfm_frame_spec_t::n_preamble;
```




<hr>



### variable n\_sync 

```C++
size_t wfm_frame_spec_t::n_sync;
```




<hr>



### variable payload 

```C++
const uint8_t* wfm_frame_spec_t::payload;
```



The payload. NULL on the receive side; `n_payload` always matters. 


        

<hr>



### variable preamble 

```C++
const uint8_t* wfm_frame_spec_t::preamble;
```



Preamble, repeated `preamble_reps` times. A DSSS burst leaves this NULL: its preamble is unmodulated and unspread, so it is not part of the frame at all (see [**wfm\_dsss\_desc\_chips**](wfm__frame_8h.md#function-wfm_dsss_desc_chips)). 


        

<hr>



### variable preamble\_reps 

```C++
size_t wfm_frame_spec_t::preamble_reps;
```




<hr>



### variable randomise 

```C++
int wfm_frame_spec_t::randomise;
```



Randomiser generator: 0 = off, else the generator's own index. 


        

<hr>



### variable rs\_depth 

```C++
unsigned wfm_frame_spec_t::rs_depth;
```



Outer-code interleaving depth; 0 = no outer code. 


        

<hr>



### variable rs\_parity\_bits 

```C++
size_t wfm_frame_spec_t::rs_parity_bits;
```



The outer code's check symbols, in BITS (the caller's standard). 


        

<hr>



### variable sync 

```C++
const uint8_t* wfm_frame_spec_t::sync;
```



Frame-sync word — what a receiver correlates to find the payload. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

