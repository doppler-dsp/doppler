

# File wfm\_compose.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfmgen**](dir_2784f51dc2a964fe71c3814677da8805.md) **>** [**wfm\_compose.h**](wfm__compose_8h.md)

[Go to the source code of this file](wfm__compose_8h_source.md)

_Multi-segment waveform composer (Phase B)._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "synth/synth_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**wfm\_segment\_t**](structwfm__segment__t.md) <br>_One composer segment: a_ `synth` _config + on/off sample counts._ |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct wfm\_compose\_state | [**wfm\_compose\_state\_t**](#typedef-wfm_compose_state_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-wfm_compose_state_t) \* | [**wfm\_compose\_create**](#function-wfm_compose_create) (const [**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n\_segs, int repeat, int continuous) <br>_Build a composer over a copy of_ `segs` _._ |
|  void | [**wfm\_compose\_destroy**](#function-wfm_compose_destroy) ([**wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-wfm_compose_state_t) \* state) <br>_Destroy a composer and its active synth._  |
|  size\_t | [**wfm\_compose\_execute**](#function-wfm_compose_execute) ([**wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-wfm_compose_state_t) \* state, float complex \* out, size\_t max) <br>_Emit up to_ `max` _samples of the composed stream._ |
|  [**wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-wfm_compose_state_t) \* | [**wfm\_compose\_from\_file**](#function-wfm_compose_from_file) (const char \* path) <br>_Build a composer from a JSON spec file._  |
|  [**wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-wfm_compose_state_t) \* | [**wfm\_compose\_from\_json**](#function-wfm_compose_from_json) (const char \* json) <br>_Build a composer from a JSON spec string (for_  _from-file)._ |
|  const [**wfm\_segment\_t**](structwfm__segment__t.md) \* | [**wfm\_compose\_segments**](#function-wfm_compose_segments) (const [**wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-wfm_compose_state_t) \* state, size\_t \* n\_out, int \* repeat, int \* continuous) <br>_Borrow the composer's stored segment list (for_  _record / SigMF)._ |
|  char \* | [**wfm\_spec\_to\_json**](#function-wfm_spec_to_json) (const [**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n\_segs, int repeat, int continuous) <br>_Serialise a spec to a JSON string (for_  _record)._ |




























## Detailed Description


Sequences a list of segments — each one a `synth` configuration plus an on-time and a trailing off-time gap — into a single IQ stream, optionally repeating the whole sequence or running forever. The composer owns one `synth` at a time (the active segment) and reuses the Phase-A engine verbatim, so every waveform type / SNR mode / MLS behaviour is identical to the single-waveform path; a one-segment spec is byte-identical to calling `synth` directly.


Lifecycle: wfm\_compose\_create -&gt; wfm\_compose\_execute\* -&gt; wfm\_compose\_destroy



```C++
wfm_segment_t segs[2] = {
    {.type = 0, .fs = 1e6, .freq = 1e5, .snr = 100.0,
     .num_samples = 1000, .off_samples = 500},          // tone, then a gap
    {.type = 4, .fs = 1e6, .sps = 8, .snr = 9.0,
     .num_samples = 4096, .off_samples = 0},            // qpsk
};
wfm_compose_state_t *c = wfm_compose_create(segs, 2, 0, 0);
float complex buf[4096];
size_t n;
while ((n = wfm_compose_execute(c, buf, 4096)) > 0) { ... }
wfm_compose_destroy(c);
```
 


    
## Public Types Documentation




### typedef wfm\_compose\_state\_t 

```C++
typedef struct wfm_compose_state wfm_compose_state_t;
```



Opaque composer state. 


        

<hr>
## Public Functions Documentation




### function wfm\_compose\_create 

_Build a composer over a copy of_ `segs` _._
```C++
wfm_compose_state_t * wfm_compose_create (
    const wfm_segment_t * segs,
    size_t n_segs,
    int repeat,
    int continuous
) 
```





**Parameters:**


* `segs` Segment list (copied; caller keeps ownership). 
* `n_segs` Number of segments (&gt;= 1). 
* `repeat` Non-zero: loop the whole sequence after the last segment. 
* `continuous` Non-zero: never finish (implies repeat); execute always returns `max`. 



**Returns:**

Heap state, or NULL on bad args / allocation / synth failure. 




**Note:**

Caller must [**wfm\_compose\_destroy()**](wfm__compose_8h.md#function-wfm_compose_destroy) when done. 





        

<hr>



### function wfm\_compose\_destroy 

_Destroy a composer and its active synth._ 
```C++
void wfm_compose_destroy (
    wfm_compose_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function wfm\_compose\_execute 

_Emit up to_ `max` _samples of the composed stream._
```C++
size_t wfm_compose_execute (
    wfm_compose_state_t * state,
    float complex * out,
    size_t max
) 
```





**Returns:**

Number of samples written: &lt; `max` (or 0) signals the sequence finished (never, when `continuous`). 





        

<hr>



### function wfm\_compose\_from\_file 

_Build a composer from a JSON spec file._ 
```C++
wfm_compose_state_t * wfm_compose_from_file (
    const char * path
) 
```





**Returns:**

Composer state, or NULL on read/parse error. 





        

<hr>



### function wfm\_compose\_from\_json 

_Build a composer from a JSON spec string (for_  _from-file)._
```C++
wfm_compose_state_t * wfm_compose_from_json (
    const char * json
) 
```





**Returns:**

Composer state, or NULL on parse error / bad type / no segments. 





        

<hr>



### function wfm\_compose\_segments 

_Borrow the composer's stored segment list (for_  _record / SigMF)._
```C++
const wfm_segment_t * wfm_compose_segments (
    const wfm_compose_state_t * state,
    size_t * n_out,
    int * repeat,
    int * continuous
) 
```





**Parameters:**


* `state` the composer. 
* `n_out` receives the segment count. 
* `repeat` receives the repeat flag (may be NULL). 
* `continuous` receives the continuous flag (may be NULL). 



**Returns:**

Pointer to the internal segments (owned by the composer; valid until wfm\_compose\_destroy). 





        

<hr>



### function wfm\_spec\_to\_json 

_Serialise a spec to a JSON string (for_  _record)._
```C++
char * wfm_spec_to_json (
    const wfm_segment_t * segs,
    size_t n_segs,
    int repeat,
    int continuous
) 
```





**Returns:**

malloc'd JSON (caller frees), or NULL on allocation failure. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfmgen/wfm_compose.h`

