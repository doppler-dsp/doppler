

# Struct wfm\_reader\_info\_t



[**ClassList**](annotated.md) **>** [**wfm\_reader\_info\_t**](structwfm__reader__info__t.md)



[More...](#detailed-description)

* `#include <wfm_reader.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**endian**](#variable-endian)  <br> |
|  double | [**fc**](#variable-fc)  <br> |
|  int | [**file\_type**](#variable-file_type)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  size\_t | [**num\_samples**](#variable-num_samples)  <br> |
|  int | [**sample\_type**](#variable-sample_type)  <br> |












































## Detailed Description


Resolved metadata for an open capture. Fields the container does not carry are 0 (`fs`/`fc` for raw/CSV, `num_samples` for a stream). 


    
## Public Attributes Documentation




### variable endian 

```C++
int wfm_reader_info_t::endian;
```



0 little, 1 big. 


        

<hr>



### variable fc 

```C++
double wfm_reader_info_t::fc;
```



centre frequency (Hz); 0 if unknown. 


        

<hr>



### variable file\_type 

```C++
int wfm_reader_info_t::file_type;
```



detected [**wfm\_filetype\_t**](wfm__writer_8h.md#enum-wfm_filetype_t). 


        

<hr>



### variable fs 

```C++
double wfm_reader_info_t::fs;
```



sample rate (Hz); 0 if unknown. 


        

<hr>



### variable num\_samples 

```C++
size_t wfm_reader_info_t::num_samples;
```



total complex samples; 0 if unknown. 


        

<hr>



### variable sample\_type 

```C++
int wfm_reader_info_t::sample_type;
```



0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_reader.h`

