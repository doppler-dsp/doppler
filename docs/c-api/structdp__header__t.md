

# Struct dp\_header\_t



[**ClassList**](annotated.md) **>** [**dp\_header\_t**](structdp__header__t.md)



_Frame metadata header carried in every ZMQ message._ [More...](#detailed-description)

* `#include <stream.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**center\_freq**](#variable-center_freq)  <br> |
|  uint32\_t | [**flags**](#variable-flags)  <br> |
|  uint32\_t | [**magic**](#variable-magic)  <br> |
|  uint64\_t | [**num\_samples**](#variable-num_samples)  <br> |
|  uint32\_t | [**protocol**](#variable-protocol)  <br> |
|  uint64\_t | [**reserved**](#variable-reserved)  <br> |
|  double | [**sample\_rate**](#variable-sample_rate)  <br> |
|  uint32\_t | [**sample\_type**](#variable-sample_type)  <br> |
|  uint64\_t | [**sequence**](#variable-sequence)  <br> |
|  uint32\_t | [**stream\_id**](#variable-stream_id)  <br> |
|  uint64\_t | [**timestamp\_ns**](#variable-timestamp_ns)  <br> |
|  uint32\_t | [**version**](#variable-version)  <br> |












































## Detailed Description


The first 4 bytes of the wire header are always the magic value `0x53494753` ("SIGS"), which receivers can use for basic validation. Future-proofed for DIFI / VITA 49 with protocol and stream\_id fields. 


    
## Public Attributes Documentation




### variable center\_freq 

```C++
double dp_header_t::center_freq;
```



Centre frequency in Hz. 


        

<hr>



### variable flags 

```C++
uint32_t dp_header_t::flags;
```



Reserved flags — set to 0. 


        

<hr>



### variable magic 

```C++
uint32_t dp_header_t::magic;
```



Magic number: 0x53494753 "SIGS". 


        

<hr>



### variable num\_samples 

```C++
uint64_t dp_header_t::num_samples;
```



Number of complex samples in this message. 


        

<hr>



### variable protocol 

```C++
uint32_t dp_header_t::protocol;
```



[**dp\_protocol\_t**](group__types.md#enum-dp_protocol_t) (0 = SIGS, 1 = DIFI). 


        

<hr>



### variable reserved 

```C++
uint64_t dp_header_t::reserved[4];
```



Reserved — set to zero, do not interpret. 


        

<hr>



### variable sample\_rate 

```C++
double dp_header_t::sample_rate;
```



Sample rate in Hz. 


        

<hr>



### variable sample\_type 

```C++
uint32_t dp_header_t::sample_type;
```



Wire sample type ([**dp\_sample\_type\_t**](group__types.md#enum-dp_sample_type_t)). 


        

<hr>



### variable sequence 

```C++
uint64_t dp_header_t::sequence;
```



Monotonically increasing per-sender count. 


        

<hr>



### variable stream\_id 

```C++
uint32_t dp_header_t::stream_id;
```



DIFI stream ID; 0 for SIGS. 


        

<hr>



### variable timestamp\_ns 

```C++
uint64_t dp_header_t::timestamp_ns;
```



UNIX timestamp in nanoseconds (CLOCK\_REALTIME). 


        

<hr>



### variable version 

```C++
uint32_t dp_header_t::version;
```



Protocol version (currently 1). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/stream/stream.h`

