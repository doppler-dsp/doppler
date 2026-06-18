

# File wfm\_writer.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_writer.h**](wfm__writer_8h.md)

[Go to the source code of this file](wfm__writer_8h_source.md)

_Output containers for generated IQ: raw / csv / BLUE-1000 + SigMF meta._ [More...](#detailed-description)

* `#include <stdio.h>`
* `#include "clib_common.h"`
* `#include "wfm/wfm_compose.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**wfm\_filetype\_t**](#enum-wfm_filetype_t)  <br> |
| typedef struct wfm\_writer | [**wfm\_writer\_t**](#typedef-wfm_writer_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**wfm\_blue\_write\_hcb**](#function-wfm_blue_write_hcb) (FILE \* fp, int sample\_type, int endian, double fs, double fc, double data\_start, size\_t total\_samples, int detached) <br>_Write a complete 512-byte BLUE/Platinum type-1000 Header Control Block._  |
|  char \* | [**wfm\_sigmf\_meta\_json**](#function-wfm_sigmf_meta_json) (int sample\_type, int endian, double fs, double fc, const [**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n\_segs) <br>_Build a SigMF_ `.sigmf-meta` _JSON document for a generated capture._ |
|  double | [**wfm\_writer\_clip\_fraction**](#function-wfm_writer_clip_fraction) (const [**wfm\_writer\_t**](wfm__writer_8h.md#typedef-wfm_writer_t) \* w) <br> |
|  int | [**wfm\_writer\_close**](#function-wfm_writer_close) ([**wfm\_writer\_t**](wfm__writer_8h.md#typedef-wfm_writer_t) \* w) <br>_Flush, patch the BLUE data\_size from the actual count (if seekable), and free the writer (does not close the FILE\*)._  |
|  [**wfm\_writer\_t**](wfm__writer_8h.md#typedef-wfm_writer_t) \* | [**wfm\_writer\_open**](#function-wfm_writer_open) (FILE \* fp, [**wfm\_filetype\_t**](wfm__writer_8h.md#enum-wfm_filetype_t) ft, int sample\_type, int endian, double fs, double fc, size\_t total\_samples) <br>_Open a writer on an already-open stream._  |
|  [**wfm\_writer\_t**](wfm__writer_8h.md#typedef-wfm_writer_t) \* | [**wfm\_writer\_open\_path**](#function-wfm_writer_open_path) (const char \* path, [**wfm\_filetype\_t**](wfm__writer_8h.md#enum-wfm_filetype_t) ft, int sample\_type, int endian, double fs, double fc, size\_t total\_samples, double headroom) <br> |
|  double | [**wfm\_writer\_peak**](#function-wfm_writer_peak) (const [**wfm\_writer\_t**](wfm__writer_8h.md#typedef-wfm_writer_t) \* w) <br> |
|  void | [**wfm\_writer\_set\_gain**](#function-wfm_writer_set_gain) ([**wfm\_writer\_t**](wfm__writer_8h.md#typedef-wfm_writer_t) \* w, double gain) <br> |
|  void | [**wfm\_writer\_track\_clipping**](#function-wfm_writer_track_clipping) ([**wfm\_writer\_t**](wfm__writer_8h.md#typedef-wfm_writer_t) \* w, int on) <br> |
|  size\_t | [**wfm\_writer\_write**](#function-wfm_writer_write) ([**wfm\_writer\_t**](wfm__writer_8h.md#typedef-wfm_writer_t) \* w, const float \_Complex \* iq, size\_t n) <br>_Convert and write_ `n` _complex samples._ |




























## Detailed Description


A streaming writer over a FILE\* that serialises cf32 blocks into one of three on-disk containers, in the chosen wire sample type and byte order. The fourth file-type, SigMF, writes its samples as `raw` (into `<base>.sigmf-data`) and pairs with a sidecar `<base>.sigmf-meta` JSON emitted by [**wfm\_sigmf\_meta\_json()**](wfm__writer_8h.md#function-wfm_sigmf_meta_json).


Axes (orthogonal to the container):
* sample\_type (wavegen order): 0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8. Integer types quantise full-scale ±1.0 (ci32 2^31-1, ci16 32767, ci8 127).
* endian: 0 little, 1 big (csv is text, so endian is ignored there).





```C++
wfm_writer_t *w = wfm_writer_open(fp, WFM_FT_BLUE, 3, 0, 1e6, 2.4e9, 4096);
wfm_writer_write(w, iq, 4096);
wfm_writer_close(w);   // patches the BLUE data_size from the actual count
```
 


    
## Public Types Documentation




### enum wfm\_filetype\_t 

```C++
enum wfm_filetype_t {
    WFM_FT_RAW = 0,
    WFM_FT_CSV = 1,
    WFM_FT_BLUE = 2,
    WFM_FT_SIGMF = 3
};
```



Output container. 


        

<hr>



### typedef wfm\_writer\_t 

```C++
typedef struct wfm_writer wfm_writer_t;
```



Opaque writer. 


        

<hr>
## Public Functions Documentation




### function wfm\_blue\_write\_hcb 

_Write a complete 512-byte BLUE/Platinum type-1000 Header Control Block._ 
```C++
int wfm_blue_write_hcb (
    FILE * fp,
    int sample_type,
    int endian,
    double fs,
    double fc,
    double data_start,
    size_t total_samples,
    int detached
) 
```



Used for the `blue` container — both attached (the writer calls this with `data_start = 512`, `detached = 0`, then streams the data after it) and detached (the caller writes the data to a separate `.det` file and this HCB to a `.hdr` file with `data_start = 0`, `detached = 1`). Every standard field is written; the header byte order follows `endian`.




**Parameters:**


* `fp` destination (binary). 
* `sample_type` wire type (wavegen order) → BLUE format char C{B,I,L,F,D}. 
* `endian` 0 little (`EEEI`) / 1 big (`IEEE`). 
* `fs` sample rate (Hz) → `xdelta = 1/fs`. 
* `fc` reserved (no standard type-1000 field). 
* `data_start` `data_start` field: 512 attached, 0 detached. 
* `total_samples` complex-sample count → `data_size`. 
* `detached` non-zero sets the HCB `detached` flag. 



**Returns:**

0 on success, non-zero on a write error. 





        

<hr>



### function wfm\_sigmf\_meta\_json 

_Build a SigMF_ `.sigmf-meta` _JSON document for a generated capture._
```C++
char * wfm_sigmf_meta_json (
    int sample_type,
    int endian,
    double fs,
    double fc,
    const wfm_segment_t * segs,
    size_t n_segs
) 
```



`global` carries core:datatype (from sample\_type+endian, e.g. "ci16\_le"), core:sample\_rate, core:version "1.0.0", and a wfmgen description/author. `captures` is a single capture at sample 0 / frequency `fc`. `annotations` has one entry per composer segment — sample span, frequency edges (fc + freq ± bandwidth/2, bandwidth ≈ fs/sps for symbol/chip types), a core:label of the waveform type, and custom `wfmgen:*` parameters.




**Returns:**

malloc'd JSON string (caller frees), or NULL on allocation failure. 





        

<hr>



### function wfm\_writer\_clip\_fraction 

```C++
double wfm_writer_clip_fraction (
    const wfm_writer_t * w
) 
```



Fraction (0..1) of I/Q components that saturated (\|v\| &gt; 1). Always 0 unless [**wfm\_writer\_track\_clipping()**](wfm__writer_8h.md#function-wfm_writer_track_clipping) was enabled. 


        

<hr>



### function wfm\_writer\_close 

_Flush, patch the BLUE data\_size from the actual count (if seekable), and free the writer (does not close the FILE\*)._ 
```C++
int wfm_writer_close (
    wfm_writer_t * w
) 
```





**Returns:**

0 on success, non-zero on a write/seek error. 





        

<hr>



### function wfm\_writer\_open 

_Open a writer on an already-open stream._ 
```C++
wfm_writer_t * wfm_writer_open (
    FILE * fp,
    wfm_filetype_t ft,
    int sample_type,
    int endian,
    double fs,
    double fc,
    size_t total_samples
) 
```





**Parameters:**


* `fp` destination (binary mode for raw/blue; text-safe for csv). 
* `ft` container; SIGMF is treated as RAW here. 
* `sample_type` wire type (wavegen order); see file header. 
* `endian` 0 little, 1 big (ignored for csv). 
* `fs` sample rate (Hz) — BLUE xdelta = 1/fs. 
* `fc` center frequency (Hz) — reserved (BLUE/raw ignore it). 
* `total_samples` expected complex-sample count for the BLUE header (0 if unknown; close() patches the actual count when fp is seekable). 



**Returns:**

Writer handle, or NULL on bad args / allocation. BLUE writes its 512-byte header here. 





        

<hr>



### function wfm\_writer\_open\_path 

```C++
wfm_writer_t * wfm_writer_open_path (
    const char * path,
    wfm_filetype_t ft,
    int sample_type,
    int endian,
    double fs,
    double fc,
    size_t total_samples,
    double headroom
) 
```



Path-opening + FILE-owning ctor for the generated `Writer` handle (jm kind="handle"): opens `path` ("wb"), delegates to wfm\_writer\_open, and marks the FILE owned so wfm\_writer\_close fclose's it. Returns NULL on open failure. 


        

<hr>



### function wfm\_writer\_peak 

```C++
double wfm_writer_peak (
    const wfm_writer_t * w
) 
```



Largest per-axis magnitude max(\|I\|,\|Q\|) written so far (pre-clip, full-scale 1.0). &gt; 1.0 ⇒ integer output clipped; peak\_dBFS = 20\*log10(peak). 


        

<hr>



### function wfm\_writer\_set\_gain 

```C++
void wfm_writer_set_gain (
    wfm_writer_t * w,
    double gain
) 
```



Set the output gain (linear; default 1.0). For headroom H dB pass 10^(−H/20). 


        

<hr>



### function wfm\_writer\_track\_clipping 

```C++
void wfm_writer_track_clipping (
    wfm_writer_t * w,
    int on
) 
```



Enable the per-component clip _counter_ (off by default; peak is always on). 


        

<hr>



### function wfm\_writer\_write 

_Convert and write_ `n` _complex samples._
```C++
size_t wfm_writer_write (
    wfm_writer_t * w,
    const float _Complex * iq,
    size_t n
) 
```





**Returns:**

Number of complex samples written (== n on success, else short). 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_writer.h`

