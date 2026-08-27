

# Struct ccsds\_tm\_frame\_spec\_t



[**ClassList**](annotated.md) **>** [**ccsds\_tm\_frame\_spec\_t**](structccsds__tm__frame__spec__t.md)



_A framed waveform's choices, before they become a description._ [More...](#detailed-description)

* `#include <ccsds_tm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**attach\_asm**](#variable-attach_asm)  <br> |
|  int | [**convolutional**](#variable-convolutional)  <br> |
|  int | [**crc**](#variable-crc)  <br> |
|  unsigned | [**interleave\_depth**](#variable-interleave_depth)  <br> |
|  unsigned | [**interleave\_unit\_bits**](#variable-interleave_unit_bits)  <br> |
|  const uint8\_t \* | [**payload**](#variable-payload)  <br> |
|  size\_t | [**payload\_len**](#variable-payload_len)  <br> |
|  const uint8\_t \* | [**preamble**](#variable-preamble)  <br> |
|  size\_t | [**preamble\_len**](#variable-preamble_len)  <br> |
|  size\_t | [**preamble\_reps**](#variable-preamble_reps)  <br> |
|  int | [**randomise**](#variable-randomise)  <br> |
|  unsigned | [**rs\_depth**](#variable-rs_depth)  <br> |
|  const uint8\_t \* | [**sync**](#variable-sync)  <br> |
|  size\_t | [**sync\_len**](#variable-sync_len)  <br> |












































## Detailed Description


The wider family `ccsds_tm_frame_describe` is the CADU case of: a frame that may open with a marker, may carry a preamble and a sync word a receiver FINDS, carries a payload, and applies some subset of this standard's four stages to it.


**The fields are the caller's; the COVERS are the standard's**, which is the whole reason this lives here rather than in `wfm/wfm_frame.h`. That header knows what a field and a stage are and nothing about which covers which — a general description cannot, because the answer is a specification's:  The middle row is 10.3.4 generalised: the randomiser does not cover the ASM, and the reason the standard gives — a marker a receiver correlates against must not vary between frames — is exactly as true of a preamble and a sync word.


A receiver does NOT hold one of these. It stops at hard and soft decisions; the frame is undone one layer up, by whoever holds the description. 


    
## Public Attributes Documentation




### variable attach\_asm 

```C++
int ccsds_tm_frame_spec_t::attach_asm;
```



Non-zero: the frame opens with the ASM (0x1ACFFC1D). 


        

<hr>



### variable convolutional 

```C++
int ccsds_tm_frame_spec_t::convolutional;
```



Non-zero: the K=7 rate-1/2 inner code, over the whole frame. 


        

<hr>



### variable crc 

```C++
int ccsds_tm_frame_spec_t::crc;
```



Non-zero: a CRC-16 trailer over the payload group. 


        

<hr>



### variable interleave\_depth 

```C++
unsigned ccsds_tm_frame_spec_t::interleave_depth;
```



A BLOCK INTERLEAVER over the data group; 0 = none.


Not a CCSDS pick, and here for the reason `crc` and `sync` are: this spec is the one frame description every doppler face reaches, and 131.0-B-6's own choices are a CONFIGURATION of it rather than the whole of its vocabulary.


Applied AFTER the outer code and the randomiser and before the inner code, which is the only order that buys anything: an interleaver exists so a burst on the CHANNEL arrives spread across the outer code's codewords, so it must be the last thing between them and the wire. 


        

<hr>



### variable interleave\_unit\_bits 

```C++
unsigned ccsds_tm_frame_spec_t::interleave_unit_bits;
```



Bits per permuted unit; 0 reads as 1. Match it to the outer code's symbol — 8 for Reed-Solomon over GF(256), because permuting bits inside a symbol that is already wrong buys nothing. 


        

<hr>



### variable payload 

```C++
const uint8_t* ccsds_tm_frame_spec_t::payload;
```



The payload. May be NULL when only the geometry is wanted. 


        

<hr>



### variable payload\_len 

```C++
size_t ccsds_tm_frame_spec_t::payload_len;
```




<hr>



### variable preamble 

```C++
const uint8_t* ccsds_tm_frame_spec_t::preamble;
```



Preamble, repeated `preamble_reps` times; NULL for none. 


        

<hr>



### variable preamble\_len 

```C++
size_t ccsds_tm_frame_spec_t::preamble_len;
```




<hr>



### variable preamble\_reps 

```C++
size_t ccsds_tm_frame_spec_t::preamble_reps;
```




<hr>



### variable randomise 

```C++
int ccsds_tm_frame_spec_t::randomise;
```



Randomiser generator: 0 = off, else 10.4.1 (1) or 10.4.2 (2). 


        

<hr>



### variable rs\_depth 

```C++
unsigned ccsds_tm_frame_spec_t::rs_depth;
```



Outer-code interleaving depth; 0 = no outer code. 


        

<hr>



### variable sync 

```C++
const uint8_t* ccsds_tm_frame_spec_t::sync;
```



Frame-sync word — what a receiver correlates to find the payload. 


        

<hr>



### variable sync\_len 

```C++
size_t ccsds_tm_frame_spec_t::sync_len;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ccsds_tm/ccsds_tm_frame.h`

