

# Struct wfm\_reader\_info\_t



[**ClassList**](annotated.md) **>** [**wfm\_reader\_info\_t**](structwfm__reader__info__t.md)



[More...](#detailed-description)

* `#include <wfm_reader_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**endian**](#variable-endian)  <br> |
|  double | [**fc**](#variable-fc)  <br> |
|  int | [**fc\_source**](#variable-fc_source)  <br> |
|  int | [**file\_type**](#variable-file_type)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  int | [**fs\_source**](#variable-fs_source)  <br> |
|  int | [**mode**](#variable-mode)  <br> |
|  size\_t | [**num\_samples**](#variable-num_samples)  <br> |
|  int | [**sample\_type**](#variable-sample_type)  <br> |
|  int | [**t0\_source**](#variable-t0_source)  <br> |
|  double | [**t0\_unix\_sec**](#variable-t0_unix_sec)  <br> |
|  size\_t | [**trailing\_bytes**](#variable-trailing_bytes)  <br> |












































## Detailed Description


Resolved metadata for an open capture. Fields the file type does not carry are 0 (`fs`/`fc` for raw/CSV, `num_samples` for a stream). 


    
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



### variable fc\_source 

```C++
int wfm_reader_info_t::fc_source;
```



wfm\_fc\_source\_t: where `fc` was read from. 


        

<hr>



### variable file\_type 

```C++
int wfm_reader_info_t::file_type;
```



detected wfm\_filetype\_t. 


        

<hr>



### variable fs 

```C++
double wfm_reader_info_t::fs;
```



sample rate (Hz); 0 if unknown. 


        

<hr>



### variable fs\_source 

```C++
int wfm_reader_info_t::fs_source;
```



wfm\_fs\_source\_t: where `fs` was read from. 


        

<hr>



### variable mode 

```C++
int wfm_reader_info_t::mode;
```



wfm\_mode\_t: 0 complex, 1 scalar (BLUE 'S'). 


        

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



Element type only  0 f32, 1 f64, 2 i32, 3 i16, 4 i8. How many components make a sample is `mode`, never this: the two are independent axes and the BLUE format field spells them as two separate characters for the same reason. 


        

<hr>



### variable t0\_source 

```C++
int wfm_reader_info_t::t0_source;
```



wfm\_t0\_source\_t: where `t0` was read from. 


        

<hr>



### variable t0\_unix\_sec 

```C++
double wfm_reader_info_t::t0_unix_sec;
```



capture start, UNIX seconds; 0 if unknown. 


        

<hr>



### variable trailing\_bytes 

```C++
size_t wfm_reader_info_t::trailing_bytes;
```



payload bytes past the last whole sample. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm_reader/wfm_reader_core.h`

