

# File spectrogram\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**spectrogram**](dir_0e14824f67e83f19566ffb7fc1ca7a06.md) **>** [**spectrogram\_core.h**](spectrogram__core_8h.md)

[Go to the source code of this file](spectrogram__core_8h_source.md)

_Streaming spectrogram: a stream of any-size chunks in, rows of nfft-bin spectra out, one row every hop samples._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/f32_buffer/f32_buffer_core.h"`
* `#include "doppler/psd/psd_core.h"`
* `#include <stddef.h>`
* `#include <stdint.h>`
* `#include "doppler/dp_interrupt_guard/dp_interrupt_guard_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) <br>_Spectrogram state. Allocate with_ [_**dp\_spectrogram\_create()**_](spectrogram__core_8h.md#function-dp_spectrogram_create) _._ |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**spectrogram\_\_core\_8h\_1abed82baf7f470b522273a3e37c24c600**](#enum-spectrogram__core_8h_1abed82baf7f470b522273a3e37c24c600)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_spectrogram\_consumed**](#function-dp_spectrogram_consumed) (const [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s) <br>_Input samples the last_ [_**dp\_spectrogram\_push()**_](spectrogram__core_8h.md#function-dp_spectrogram_push) _took._ |
|  [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* | [**dp\_spectrogram\_create**](#function-dp_spectrogram_create) (size\_t nfft, size\_t hop, int window, float beta, int mode) <br>_Create a streaming spectrogram._  |
|  void | [**dp\_spectrogram\_destroy**](#function-dp_spectrogram_destroy) ([**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s) <br>_Release a spectrogram and everything it owns._  |
|  int64\_t | [**dp\_spectrogram\_flush**](#function-dp_spectrogram_flush) ([**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s, float \* row, size\_t row\_len) <br>_End the stream: write the one zero-padded row it still owes, if any._  |
|  void | [**dp\_spectrogram\_get\_state**](#function-dp_spectrogram_get_state) (const [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s, void \* blob) <br>_Serialize the configuration a row depends on, then the stream position, into_ `blob` _._ |
|  size\_t | [**dp\_spectrogram\_pending**](#function-dp_spectrogram_pending) (const [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s) <br>_Samples pushed that no written row has covered yet._  |
|  size\_t | [**dp\_spectrogram\_push**](#function-dp_spectrogram_push) ([**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s, const float \_Complex \* in, size\_t n\_in, float \* out, size\_t max\_out) <br>_Push input, write every whole row it completes that fits._  |
|  size\_t | [**dp\_spectrogram\_push\_max\_out**](#function-dp_spectrogram_push_max_out) (const [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s, size\_t n\_in) <br>_Floats one push of_ `n_in` _samples writes when_`out` _has room: the rows\_for(n\_in) count times nfft._ |
|  void | [**dp\_spectrogram\_reset**](#function-dp_spectrogram_reset) ([**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s) <br>_Forget the stream: drop the carry and restart at sample 0._  |
|  size\_t | [**dp\_spectrogram\_rows\_for**](#function-dp_spectrogram_rows_for) (const [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s, size\_t n\_in) <br>_Rows a push of_ `n_in` _more samples completes, given the carry: exact, not an estimate._ |
|  int | [**dp\_spectrogram\_set\_state**](#function-dp_spectrogram_set_state) ([**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s, const void \* blob) <br>_Restore the stream position from a blob of the same configuration, so the next push continues it bit for bit._  |
|  size\_t | [**dp\_spectrogram\_state\_bytes**](#function-dp_spectrogram_state_bytes) (const [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s) <br>_Bytes of the state blob, a function of nfft alone._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**SPECTROGRAM\_STATE\_MAGIC**](spectrogram__core_8h.md#define-spectrogram_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('S', 'P', 'G', 'M')`<br>_State-blob magic ('SPGM') and layout version._  |
| define  | [**SPECTROGRAM\_STATE\_VERSION**](spectrogram__core_8h.md#define-spectrogram_state_version)  `2u`<br> |

## Detailed Description


A row is the PSD of one frame, and nothing else: row k is PSD's per-frame kernel applied to stream samples [k\*hop, k\*hop + nfft)  [**dp\_psd\_frame\_linear()**](psd__core_8h.md#function-dp_psd_frame_linear) for power rows, the default, or [**dp\_psd\_frame\_db()**](psd__core_8h.md#function-dp_psd_frame_db) for dB rows, which a caller asks for by name. So the window, the FFT and the full-scale reference are PSD's own, and a full-scale tone on a bin reads 1.0 in power, 0 dB in dB. The rows are a function of the INPUT STREAM, not of how it was split into calls: pushing it in one call, a sample at a time, or in any other partition gives the same rows, bit for bit.


The object composes and re-implements none of its parts. The carry between calls is the ring's framed face (DECLARE\_DP\_BUFFER\_FRAMES): fewer than nfft samples are held once a push returns, so the state blob has a size that depends on nfft alone. The spectrum is PSD's per-frame kernel, held by value ([**dp\_psd\_kernel\_t**](structdp__psd__kernel__t.md)): the window, the transform and the full-scale reference, and none of PSD's averaging. It does not average rows (fold them with AccTrace), detect, display or decimate.


Lifecycle: [**dp\_spectrogram\_create()**](spectrogram__core_8h.md#function-dp_spectrogram_create), then any number of [**dp\_spectrogram\_push()**](spectrogram__core_8h.md#function-dp_spectrogram_push) calls, then [**dp\_spectrogram\_flush()**](spectrogram__core_8h.md#function-dp_spectrogram_flush) once to end the stream, then [**dp\_spectrogram\_destroy()**](spectrogram__core_8h.md#function-dp_spectrogram_destroy). A short output buffer never loses input: push stops at a whole row and [**dp\_spectrogram\_consumed()**](spectrogram__core_8h.md#function-dp_spectrogram_consumed) says where to resume.


Not thread-safe on one object (the kernel uses the object's scratch). Complex float32 input only. The design, its goals and what it costs are docs/design/spectrogram.md. The C examples are native/examples/ spectrogram\_demo.c, which the example gate runs; they are not kept in this header, because the Python docstrings are generated from it. 


    
## Public Types Documentation




### enum spectrogram\_\_core\_8h\_1abed82baf7f470b522273a3e37c24c600 

```C++
enum spectrogram__core_8h_1abed82baf7f470b522273a3e37c24c600 {
    DP_SPECTROGRAM_POWER = 0,
    DP_SPECTROGRAM_DB = 1
};
```




<hr>
## Public Functions Documentation




### function dp\_spectrogram\_consumed 

_Input samples the last_ [_**dp\_spectrogram\_push()**_](spectrogram__core_8h.md#function-dp_spectrogram_push) _took._
```C++
size_t dp_spectrogram_consumed (
    const dp_spectrogram_state_t * s
) 
```



Equal to its `n_in` unless `out` ran out of room; then the caller resumes at in + consumed. 0 after create, reset, flush and set\_state.




**Parameters:**


* `s` Must be non-NULL. 




        

<hr>



### function dp\_spectrogram\_create 

_Create a streaming spectrogram._ 
```C++
dp_spectrogram_state_t * dp_spectrogram_create (
    size_t nfft,
    size_t hop,
    int window,
    float beta,
    int mode
) 
```





**Parameters:**


* `nfft` Samples per frame and bins per row, which must be the same number: an nfft that would be zero-padded to a longer transform (anything but a power of two &gt;= 2) is refused rather than given rows wider than its frames. 
* `hop` Samples between row starts, 1 to nfft. 0 means nfft, which tiles the stream; the default. Less than nfft overlaps the frames. A hop above nfft would leave samples no row covers, so it is refused. 
* `window` The window: "hann", "kaiser", "blackman-harris" or "rect", as PSD takes them. It also refuses a window that sums to zero at this nfft (the symmetric Hann at nfft = 2). 
* `beta` Kaiser beta (ignored for the other windows). 
* `mode` Row units: "power" (linear rows, the default) or "db" (dBFS rows, asked for by name). Any other value is refused. 



**Returns:**

Heap-allocated state, or NULL on an invalid argument.


Every row is DC-centred exactly as PSD's kernel emits it: bin k at index nfft/2 + k, negative frequencies first. 

**Note:**

Call [**dp\_spectrogram\_destroy()**](spectrogram__core_8h.md#function-dp_spectrogram_destroy) when done. 





        

<hr>



### function dp\_spectrogram\_destroy 

_Release a spectrogram and everything it owns._ 
```C++
void dp_spectrogram_destroy (
    dp_spectrogram_state_t * s
) 
```





**Parameters:**


* `s` May be NULL (no-op). 




        

<hr>



### function dp\_spectrogram\_flush 

_End the stream: write the one zero-padded row it still owes, if any._ 
```C++
int64_t dp_spectrogram_flush (
    dp_spectrogram_state_t * s,
    float * row,
    size_t row_len
) 
```



The row sits on the hop grid: it starts at the next row start k\*hop, never at the first sample no row covered, so it is the row a one-shot push of the input zero-padded to that row's end would have written. It is written if and only if the stream holds a sample no earlier row covered. Either way the spectrogram then restarts at sample 0, as after reset, so a second flush writes nothing.




**Parameters:**


* `s` Must be non-NULL. 
* `row` Room for nfft floats. 
* `row_len` Floats of room in `row`. Must be exactly nfft: any other length is refused before anything moves, the owed frame stays owed, and -1 is returned (never a count). 



**Returns:**

Floats written: nfft, or 0 if no row was owed; -1 if the row is not exactly nfft floats long. 





        

<hr>



### function dp\_spectrogram\_get\_state 

_Serialize the configuration a row depends on, then the stream position, into_ `blob` _._
```C++
void dp_spectrogram_get_state (
    const dp_spectrogram_state_t * s,
    void * blob
) 
```





**Parameters:**


* `s` Must be non-NULL. 
* `blob` dp\_spectrogram\_state\_bytes(s) bytes, every one written. 




        

<hr>



### function dp\_spectrogram\_pending 

_Samples pushed that no written row has covered yet._ 
```C++
size_t dp_spectrogram_pending (
    const dp_spectrogram_state_t * s
) 
```



What [**dp\_spectrogram\_flush()**](spectrogram__core_8h.md#function-dp_spectrogram_flush) would turn into a row: 0 means the stream is complete as it stands.




**Parameters:**


* `s` Must be non-NULL. 




        

<hr>



### function dp\_spectrogram\_push 

_Push input, write every whole row it completes that fits._ 
```C++
size_t dp_spectrogram_push (
    dp_spectrogram_state_t * s,
    const float _Complex * in,
    size_t n_in,
    float * out,
    size_t max_out
) 
```



Takes the input in order and produces each row as soon as a frame of nfft samples is complete, at every hop. A partial frame is carried to the next call, so the rows do not depend on how the input was split into calls. Input that completes no row is always taken whole.


Only whole rows are written. A C caller bounds the write with `max_out` and takes the rows that fit; the consumed count reports the input those rows used, and the caller offers the rest again. The Python face has no short buffer: its binding sizes the output for the whole chunk (see the Python method doc).




**Parameters:**


* `s` Must be non-NULL. 
* `in` Complex baseband samples (cf32). 
* `n_in` Samples in `in`. 
* `out` Rows, row-major, nfft floats each. 
* `max_out` Floats `out` has room for. 



**Returns:**

Floats written, a multiple of nfft. From Python, the completed rows as a (rows, nfft) matrix, or None when none is completed. 





        

<hr>



### function dp\_spectrogram\_push\_max\_out 

_Floats one push of_ `n_in` _samples writes when_`out` _has room: the rows\_for(n\_in) count times nfft._
```C++
size_t dp_spectrogram_push_max_out (
    const dp_spectrogram_state_t * s,
    size_t n_in
) 
```



The capacity that makes a push take ALL of its input. Saturates at SIZE\_MAX rather than wrapping.




**Parameters:**


* `s` Must be non-NULL. 
* `n_in` Samples about to be pushed. 




        

<hr>



### function dp\_spectrogram\_reset 

_Forget the stream: drop the carry and restart at sample 0._ 
```C++
void dp_spectrogram_reset (
    dp_spectrogram_state_t * s
) 
```



The next row covers samples [0, nfft) of whatever is pushed next. The consumed count and the pending count read 0. Configuration is kept.




**Parameters:**


* `s` Must be non-NULL. 




        

<hr>



### function dp\_spectrogram\_rows\_for 

_Rows a push of_ `n_in` _more samples completes, given the carry: exact, not an estimate._
```C++
size_t dp_spectrogram_rows_for (
    const dp_spectrogram_state_t * s,
    size_t n_in
) 
```



The carry plus `n_in` samples, cut into frames of nfft at the hop. A push with room for this many rows writes exactly this many and takes all of `n_in`.




**Parameters:**


* `s` Must be non-NULL. 
* `n_in` Samples about to be pushed. 




        

<hr>



### function dp\_spectrogram\_set\_state 

_Restore the stream position from a blob of the same configuration, so the next push continues it bit for bit._ 
```C++
int dp_spectrogram_set_state (
    dp_spectrogram_state_t * s,
    const void * blob
) 
```



The blob is refused, with `s` unchanged, when its magic, version or size is wrong; when the configuration it carries differs from `s's`  another mode, nfft, hop or window, or under the Kaiser window another beta, since a checkpoint from another configuration would mean something else (#2102); or when its carry is corrupt. A beta under any other window changes no row, so it is not compared. A version-1 blob, which carried no configuration, is refused by its version.




**Parameters:**


* `s` A spectrogram. 
* `blob` From [**dp\_spectrogram\_get\_state()**](spectrogram__core_8h.md#function-dp_spectrogram_get_state). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID. 





        

<hr>



### function dp\_spectrogram\_state\_bytes 

_Bytes of the state blob, a function of nfft alone._ 
```C++
size_t dp_spectrogram_state_bytes (
    const dp_spectrogram_state_t * s
) 
```



The blob is the configuration a row depends on  mode, nfft, hop, window and beta  then the stream position: the framer's snapshot, which is the carry (fewer than nfft samples, padded to nfft - 1), its element size, the stream counts and the hop that frames them. Both sit inside the spectrogram's own envelope, version 2. Every field has a fixed size, so the length depends on nfft alone. It does not carry consumed(), which reads 0 after a restore. What a restore refuses is [**dp\_spectrogram\_set\_state()**](spectrogram__core_8h.md#function-dp_spectrogram_set_state)'s.




**Parameters:**


* `s` Must be non-NULL. 




        

<hr>
## Macro Definition Documentation





### define SPECTROGRAM\_STATE\_MAGIC 

_State-blob magic ('SPGM') and layout version._ 
```C++
#define SPECTROGRAM_STATE_MAGIC `DP_FOURCC ('S', 'P', 'G', 'M')`
```




<hr>



### define SPECTROGRAM\_STATE\_VERSION 

```C++
#define SPECTROGRAM_STATE_VERSION `2u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/spectrogram/spectrogram_core.h`

