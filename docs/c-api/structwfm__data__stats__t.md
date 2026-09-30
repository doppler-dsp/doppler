

# Struct wfm\_data\_stats\_t



[**ClassList**](annotated.md) **>** [**wfm\_data\_stats\_t**](structwfm__data__stats__t.md)



_What a source has done so far: the truth a record carries._ 

* `#include <wfm_data.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**bits**](#variable-bits)  <br> |
|  uint64\_t | [**frames**](#variable-frames)  <br> |
|  uint64\_t | [**hash**](#variable-hash)  <br> |
|  int | [**hashed**](#variable-hashed)  <br> |
|  uint64\_t | [**idle\_frames**](#variable-idle_frames)  <br> |
|  uint64\_t | [**pad\_bits**](#variable-pad_bits)  <br> |
|  int | [**stream**](#variable-stream)  <br> |
|  uint64\_t | [**total\_bits**](#variable-total_bits)  <br> |












































## Public Attributes Documentation




### variable bits 

```C++
uint64_t wfm_data_stats_t::bits;
```



source bits consumed so far (no fill) 
 


        

<hr>



### variable frames 

```C++
uint64_t wfm_data_stats_t::frames;
```



data frames written, the padded last included 


        

<hr>



### variable hash 

```C++
uint64_t wfm_data_stats_t::hash;
```



`dp_hash64()` of the octets read (fd only) 
 


        

<hr>



### variable hashed 

```C++
int wfm_data_stats_t::hashed;
```



non-zero when `hash` is meaningful 
 


        

<hr>



### variable idle\_frames 

```C++
uint64_t wfm_data_stats_t::idle_frames;
```



idle frames written ([**dp\_wfm\_data\_idle**](wfm__data_8h.md#function-dp_wfm_data_idle)) 
 


        

<hr>



### variable pad\_bits 

```C++
uint64_t wfm_data_stats_t::pad_bits;
```



fill bits in the padded last frame, else 0 
 


        

<hr>



### variable stream 

```C++
int wfm_data_stats_t::stream;
```



non-zero for a stream (pipe, `pn:0`) 
 


        

<hr>



### variable total\_bits 

```C++
uint64_t wfm_data_stats_t::total_bits;
```



a finite source's length; 0 for a stream 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_data.h`

