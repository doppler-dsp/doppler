

# File wfm\_reader.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_reader.h**](wfm__reader_8h.md)

[Go to the source code of this file](wfm__reader_8h_source.md)

_Input containers for generated IQ — the dual of wfm\_writer._ [More...](#detailed-description)

* `#include <complex.h>`
* `#include <stddef.h>`
* `#include "wfm/wfm_writer.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**wfm\_reader\_info\_t**](structwfm__reader__info__t.md) <br> |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct wfm\_reader | [**wfm\_reader\_t**](#typedef-wfm_reader_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**wfm\_reader\_close**](#function-wfm_reader_close) ([**wfm\_reader\_t**](wfm__reader_8h.md#typedef-wfm_reader_t) \* r) <br>_Close the file and free the reader._  |
|  void | [**wfm\_reader\_info**](#function-wfm_reader_info) (const [**wfm\_reader\_t**](wfm__reader_8h.md#typedef-wfm_reader_t) \* r, [**wfm\_reader\_info\_t**](structwfm__reader__info__t.md) \* info) <br>_Copy the resolved capture metadata into_ `info` _._ |
|  [**wfm\_reader\_t**](wfm__reader_8h.md#typedef-wfm_reader_t) \* | [**wfm\_reader\_open**](#function-wfm_reader_open) (const char \* path, int hint\_sample\_type, int hint\_endian) <br>_Open a capture, auto-detecting its container._  |
|  size\_t | [**wfm\_reader\_read**](#function-wfm_reader_read) ([**wfm\_reader\_t**](wfm__reader_8h.md#typedef-wfm_reader_t) \* r, float \_Complex \* out, size\_t max) <br>_Read up to_ `max` _complex samples into_`out` _(unit-scale_`float _Complex` _), converting from the wire type. Returns the count read; 0 at end of file._ |




























## Detailed Description


Reads back what wfm\_writer wrote: raw interleaved I/Q, CSV, BLUE type-1000 (attached or detached), and SigMF. The container is **auto-detected** from the file (BLUE magic / `.sigmf-meta` sidecar / `.csv` extension), and self-describing containers (BLUE, SigMF) recover the sample type, byte order, sample rate and centre frequency from their metadata. Headerless containers (raw, CSV) take the sample type / byte order as hints.


Samples come out as `float _Complex` at unit scale: float wire types are reinterpreted, integer wire types are rescaled by their full-scale (the exact inverse of the writer's quantiser).



```C++
wfm_reader_t *r = wfm_reader_open("cap.sigmf-data", 0, 0);
wfm_reader_info_t info;
wfm_reader_info(r, &info);                 // info.fs, info.sample_type, ...
float _Complex buf[4096];
size_t n;
while ((n = wfm_reader_read(r, buf, 4096)) > 0)
  consume(buf, n);
wfm_reader_close(r);
```
 


    
## Public Types Documentation




### typedef wfm\_reader\_t 

```C++
typedef struct wfm_reader wfm_reader_t;
```



Opaque reader handle. 


        

<hr>
## Public Functions Documentation




### function wfm\_reader\_close 

_Close the file and free the reader._ 
```C++
void wfm_reader_close (
    wfm_reader_t * r
) 
```




<hr>



### function wfm\_reader\_info 

_Copy the resolved capture metadata into_ `info` _._
```C++
void wfm_reader_info (
    const wfm_reader_t * r,
    wfm_reader_info_t * info
) 
```




<hr>



### function wfm\_reader\_open 

_Open a capture, auto-detecting its container._ 
```C++
wfm_reader_t * wfm_reader_open (
    const char * path,
    int hint_sample_type,
    int hint_endian
) 
```





**Parameters:**


* `path` file to read. A BLUE `.det` or SigMF `.sigmf-data` data file resolves its `.hdr` / `.sigmf-meta` sidecar automatically. 
* `hint_sample_type` sample type (0..4) for headerless raw/CSV; ignored once BLUE/SigMF metadata is parsed. 
* `hint_endian` byte order (0 le, 1 be) for headerless raw. 



**Returns:**

a reader, or NULL on open/parse failure. 





        

<hr>



### function wfm\_reader\_read 

_Read up to_ `max` _complex samples into_`out` _(unit-scale_`float _Complex` _), converting from the wire type. Returns the count read; 0 at end of file._
```C++
size_t wfm_reader_read (
    wfm_reader_t * r,
    float _Complex * out,
    size_t max
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_reader.h`

