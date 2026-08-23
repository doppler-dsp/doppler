

# Struct dp\_header\_t



[**ClassList**](annotated.md) **>** [**dp\_header\_t**](structdp__header__t.md)



_Frame metadata carried in every stream message._ [More...](#detailed-description)

* `#include <stream.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**center\_freq**](#variable-center_freq)  <br> |
|  char | [**data\_rep**](#variable-data_rep)  <br> |
|  uint16\_t | [**flags**](#variable-flags)  <br> |
|  uint16\_t | [**format**](#variable-format)  <br> |
|  uint16\_t | [**kind**](#variable-kind)  <br> |
|  uint64\_t | [**magic**](#variable-magic)  <br> |
|  uint64\_t | [**num\_samples**](#variable-num_samples)  <br> |
|  uint32\_t | [**payload\_bytes**](#variable-payload_bytes)  <br> |
|  double | [**sample\_rate**](#variable-sample_rate)  <br> |
|  uint64\_t | [**sequence**](#variable-sequence)  <br> |
|  uint64\_t | [**timestamp\_ns**](#variable-timestamp_ns)  <br> |
|  uint16\_t | [**version**](#variable-version)  <br> |












































## Detailed Description


64 bytes, in declaration order, memcpy'd whole  there is no padding on any ABI doppler builds for, and a static assertion in `stream_core.c` fails the build if that ever stops being true.


Numbers are in HOST byte order and [**data\_rep**](structdp__header__t.md#variable-data_rep) says which order that was; the magic catches the mismatch first, so a wrong-endian frame is rejected rather than silently misread. 


    
## Public Attributes Documentation




### variable center\_freq 

```C++
double dp_header_t::center_freq;
```



Centre frequency in Hz, 0 if unknown. 


        

<hr>



### variable data\_rep 

```C++
char dp_header_t::data_rep[4];
```



[**DP\_REP\_LE**](group__wire.md#define-dp_rep_le) or [**DP\_REP\_BE**](group__wire.md#define-dp_rep_be), no NUL. 


        

<hr>



### variable flags 

```C++
uint16_t dp_header_t::flags;
```



Bitwise OR of the DP\_FLAG\_\* set. 


        

<hr>



### variable format 

```C++
uint16_t dp_header_t::format;
```



BLUE code (dp\_sample\_type\_t); 0 when the kind is not sample data. 


        

<hr>



### variable kind 

```C++
uint16_t dp_header_t::kind;
```



dp\_frame\_kind\_t: what the payload IS. 


        

<hr>



### variable magic 

```C++
uint64_t dp_header_t::magic;
```



[**DP\_STREAM\_MAGIC**](group__wire.md#define-dp_stream_magic). 


        

<hr>



### variable num\_samples 

```C++
uint64_t dp_header_t::num_samples;
```



Complex samples (DP\_KIND\_IQ) or records (DP\_KIND\_TLM) in THIS message. 


        

<hr>



### variable payload\_bytes 

```C++
uint32_t dp_header_t::payload_bytes;
```



Bytes of payload in THIS message. The transport also knows the message length; a receiver requires the two to agree, which is what stops a header claiming more samples than were sent. 


        

<hr>



### variable sample\_rate 

```C++
double dp_header_t::sample_rate;
```



Sample rate in Hz, 0 if unknown. 


        

<hr>



### variable sequence 

```C++
uint64_t dp_header_t::sequence;
```



Per-socket frame counter, from 0. A chunked frame consumes one number, not one per chunk. 


        

<hr>



### variable timestamp\_ns 

```C++
uint64_t dp_header_t::timestamp_ns;
```



UNIX nanoseconds (CLOCK\_REALTIME), or 0 for "no capture time"  the same unset convention [**wfm\_time.h**](wfm__time_8h.md) uses, so a frame that never had one does not claim 1970. 


        

<hr>



### variable version 

```C++
uint16_t dp_header_t::version;
```



[**DP\_WIRE\_VERSION**](group__wire.md#define-dp_wire_version). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/stream/stream.h`

