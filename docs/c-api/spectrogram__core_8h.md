

# File spectrogram\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**spectrogram**](dir_0e14824f67e83f19566ffb7fc1ca7a06.md) **>** [**spectrogram\_core.h**](spectrogram__core_8h.md)

[Go to the source code of this file](spectrogram__core_8h_source.md)

_Streaming spectrogram: a stream of any-size chunks in, rows of nfft-bin spectra out, one row every hop samples._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/f32_buffer/f32_buffer_core.h"`
* `#include "doppler/psd/psd_core.h"`
* `#include <stddef.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) <br>_Spectrogram state. Allocate with_ [_**dp\_spectrogram\_create()**_](spectrogram__core_8h.md#function-dp_spectrogram_create) _._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_spectrogram\_consumed**](#function-dp_spectrogram_consumed) (const [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s) <br>_Input samples the last_ [_**dp\_spectrogram\_push()**_](spectrogram__core_8h.md#function-dp_spectrogram_push) _took._ |
|  [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* | [**dp\_spectrogram\_create**](#function-dp_spectrogram_create) (size\_t nfft, size\_t hop, int window, float beta, int mode) <br>_Create a streaming spectrogram._  |
|  void | [**dp\_spectrogram\_destroy**](#function-dp_spectrogram_destroy) ([**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s) <br>_Release a spectrogram and everything it owns._  |
|  size\_t | [**dp\_spectrogram\_flush**](#function-dp_spectrogram_flush) ([**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s, float \* row) <br>_End the stream: write the one zero-padded row it still owes, if any._  |
|  void | [**dp\_spectrogram\_get\_state**](#function-dp_spectrogram_get_state) (const [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s, void \* blob) <br>_Serialize the stream position into_ `blob` _._ |
|  size\_t | [**dp\_spectrogram\_pending**](#function-dp_spectrogram_pending) (const [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s) <br>_Samples pushed that no written row has covered yet._  |
|  size\_t | [**dp\_spectrogram\_push**](#function-dp_spectrogram_push) ([**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s, const float \_Complex \* in, size\_t n\_in, float \* out, size\_t max\_out) <br>_Push input, write every whole row it completes that fits._  |
|  size\_t | [**dp\_spectrogram\_push\_max\_out**](#function-dp_spectrogram_push_max_out) (const [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s, size\_t n\_in) <br>_Floats one push of_ `n_in` _samples writes when_`out` _has room: dp\_spectrogram\_rows\_for(s, n\_in) \* nfft._ |
|  void | [**dp\_spectrogram\_reset**](#function-dp_spectrogram_reset) ([**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s) <br>_Forget the stream: drop the carry and restart at sample 0._  |
|  size\_t | [**dp\_spectrogram\_rows\_for**](#function-dp_spectrogram_rows_for) (const [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s, size\_t n\_in) <br>_Rows a push of_ `n_in` _more samples completes, given the carry: exact, not an estimate._ |
|  int | [**dp\_spectrogram\_set\_state**](#function-dp_spectrogram_set_state) ([**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s, const void \* blob) <br>_Restore a stream position, so the next push continues it bit for bit._  |
|  size\_t | [**dp\_spectrogram\_state\_bytes**](#function-dp_spectrogram_state_bytes) (const [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md) \* s) <br>_Bytes of the state blob, a function of nfft alone._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_SPECTROGRAM\_DB**](spectrogram__core_8h.md#define-dp_spectrogram_db)  `0`<br>_Row units: dBFS, against the PSD's full-scale reference._  |
| define  | [**DP\_SPECTROGRAM\_POWER**](spectrogram__core_8h.md#define-dp_spectrogram_power)  `1`<br>_Row units: linear power. RESERVED:_ [_**dp\_spectrogram\_create()**_](spectrogram__core_8h.md#function-dp_spectrogram_create) _refuses it until PSD's normalised per-frame power is on main, so that a power row and a dB row share one reference._ |
| define  | [**SPECTROGRAM\_STATE\_MAGIC**](spectrogram__core_8h.md#define-spectrogram_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('S', 'P', 'G', 'M')`<br>_State-blob magic ('SPGM') and layout version._  |
| define  | [**SPECTROGRAM\_STATE\_VERSION**](spectrogram__core_8h.md#define-spectrogram_state_version)  `1u`<br> |

## Detailed Description


A row is the PSD of one frame, and nothing else: row k is [**dp\_psd\_frame\_db()**](psd__core_8h.md#function-dp_psd_frame_db) of stream samples [k\*hop, k\*hop + nfft), so the window, the FFT and the dBFS reference are PSD's own and a full-scale tone on a bin reads 0 dB. The rows are a function of the INPUT STREAM, not of how it was split into calls: pushing it in one call, a sample at a time, or in any other partition gives the same rows, bit for bit.


The object composes and re-implements none of its parts. The carry between calls is the ring's framed face (DECLARE\_DP\_BUFFER\_FRAMES): fewer than nfft samples are held once a push returns, so the state blob has a size that depends on nfft alone. The spectrum is PSD's per-frame kernel. It does not average rows (fold them with AccTrace), detect, display or decimate.


Lifecycle: [**dp\_spectrogram\_create()**](spectrogram__core_8h.md#function-dp_spectrogram_create), then any number of [**dp\_spectrogram\_push()**](spectrogram__core_8h.md#function-dp_spectrogram_push) calls, then [**dp\_spectrogram\_flush()**](spectrogram__core_8h.md#function-dp_spectrogram_flush) once to end the stream, then [**dp\_spectrogram\_destroy()**](spectrogram__core_8h.md#function-dp_spectrogram_destroy). A short output buffer never loses input: push stops at a whole row and [**dp\_spectrogram\_consumed()**](spectrogram__core_8h.md#function-dp_spectrogram_consumed) says where to resume.


Not thread-safe on one object (the kernel uses the object's scratch). Complex float32 input only. The design, its goals and what is still unmeasured are docs/design/spectrogram.md. 


    
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


* `nfft` Samples per frame and bins per row, which must be the same number: an nfft the PSD would zero-pad to a longer transform (anything but a power of two &gt;= 2) is refused rather than given rows wider than its frames. 
* `hop` Samples between row starts, 1 &lt;= hop &lt;= nfft. hop == nfft tiles the stream; hop &lt; nfft overlaps the frames. 
* `window` 0 = Hann, 1 = Kaiser, 2 = Blackman-Harris, 3 = rectangular, as [**dp\_psd\_create()**](psd__core_8h.md#function-dp_psd_create), which also refuses a window that sums to zero at this nfft (the symmetric Hann at nfft = 2). 
* `beta` Kaiser beta (ignored for the other windows). 
* `mode` DP\_SPECTROGRAM\_DB. DP\_SPECTROGRAM\_POWER is refused for now. 



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
size_t dp_spectrogram_flush (
    dp_spectrogram_state_t * s,
    float * row
) 
```



The row sits on the hop grid: it starts at the next row start k\*hop, never at the first sample no row covered, so it is the row a one-shot push of the input zero-padded to that row's end would have written. It is written if and only if the stream holds a sample no earlier row covered. Either way the spectrogram then restarts at sample 0, as after reset, so a second flush writes nothing.




**Parameters:**


* `s` Must be non-NULL. 
* `row` Room for nfft floats. 



**Returns:**

Floats written: nfft, or 0 if no row was owed. 





        

<hr>



### function dp\_spectrogram\_get\_state 

_Serialize the stream position into_ `blob` _._
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



Takes the input in order and writes row after row into `out`, each `nfft` floats, until either the input is used up or the next row is due and `out` has no room for it. Only whole rows are written: a `max_out` that is not a multiple of nfft uses floor(max\_out / nfft) rows of it, and an `out` too small for one row writes nothing.


A short `out` never loses input. A sample is taken unless taking it would complete a row `out` has no room for: the framer's feed contract (DECLARE\_DP\_BUFFER\_FRAMES) applied to rows. [**dp\_spectrogram\_consumed()**](spectrogram__core_8h.md#function-dp_spectrogram_consumed) reports how many were taken, and the caller offers the rest again. Taken input that does not yet complete a row is the carry, held inside: fewer than nfft samples once this returns. So input that completes no row is always taken whole, even with `max_out` 0.




**Parameters:**


* `s` Must be non-NULL. 
* `in` Complex baseband samples (cf32). 
* `n_in` Samples in `in`. 
* `out` Rows, row-major, nfft floats each. 
* `max_out` Floats `out` has room for. 



**Returns:**

Floats written: a multiple of nfft, at most dp\_spectrogram\_push\_max\_out(s, n\_in).



```C++
// nfft 8, hop 4, rectangular, dB: a unit tone on bin 2
dp_spectrogram_state_t *s = dp_spectrogram_create (8, 4, 3, 0.0f, 0);
float _Complex x[16];
for (int i = 0; i < 16; i++)
  x[i] = cexpf (I * 2.0f * 3.14159265f * 2.0f * (float)i / 8.0f);
float rows[3 * 8];
size_t got = dp_spectrogram_push (s, x, 16, rows, 3 * 8);
// 16 samples at hop 4 complete the rows starting at 0, 4 and 8
if (got != 3 * 8 || dp_spectrogram_consumed (s) != 16)
  return 1;
if (fabsf (rows[4 + 2]) > 1e-4f)   // bin 2 reads 0 dBFS, at nfft/2 + 2
  return 1;
dp_spectrogram_destroy (s);
```
 


        

<hr>



### function dp\_spectrogram\_push\_max\_out 

_Floats one push of_ `n_in` _samples writes when_`out` _has room: dp\_spectrogram\_rows\_for(s, n\_in) \* nfft._
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



The next row covers samples [0, nfft) of whatever is pushed next. [**dp\_spectrogram\_consumed()**](spectrogram__core_8h.md#function-dp_spectrogram_consumed) and [**dp\_spectrogram\_pending()**](spectrogram__core_8h.md#function-dp_spectrogram_pending) read 0. Configuration is kept.




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

_Restore a stream position, so the next push continues it bit for bit._ 
```C++
int dp_spectrogram_set_state (
    dp_spectrogram_state_t * s,
    const void * blob
) 
```



Precondition, the caller's to keep because the blob does not carry it: `s` was created with the same arguments (nfft, hop, window, beta, mode) as the spectrogram the blob came from. A different nfft or hop is refused (the size and the framer's stored hop tell); a different window or beta is NOT, and the rows that follow are that window's, not the original's.




**Parameters:**


* `s` A spectrogram created with the same arguments. 
* `blob` From [**dp\_spectrogram\_get\_state()**](spectrogram__core_8h.md#function-dp_spectrogram_get_state). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID (wrong magic, version, size or hop, or a corrupt carry), with `s` unchanged. 





        

<hr>



### function dp\_spectrogram\_state\_bytes 

_Bytes of the state blob, a function of nfft alone._ 
```C++
size_t dp_spectrogram_state_bytes (
    const dp_spectrogram_state_t * s
) 
```



The blob is the stream position  the framer's snapshot: the carry (fewer than nfft samples, padded to nfft - 1), its element size, the stream counts, and the hop that frames them  inside the spectrogram's own envelope. It does not carry consumed() or the PSD's configuration (window, beta, mode). So nfft and hop are checked on restore (the size and the stored hop), and window and beta are the caller's to keep the same  see [**dp\_spectrogram\_set\_state()**](spectrogram__core_8h.md#function-dp_spectrogram_set_state).




**Parameters:**


* `s` Must be non-NULL. 




        

<hr>
## Macro Definition Documentation





### define DP\_SPECTROGRAM\_DB 

_Row units: dBFS, against the PSD's full-scale reference._ 
```C++
#define DP_SPECTROGRAM_DB `0`
```




<hr>



### define DP\_SPECTROGRAM\_POWER 

_Row units: linear power. RESERVED:_ [_**dp\_spectrogram\_create()**_](spectrogram__core_8h.md#function-dp_spectrogram_create) _refuses it until PSD's normalised per-frame power is on main, so that a power row and a dB row share one reference._
```C++
#define DP_SPECTROGRAM_POWER `1`
```




<hr>



### define SPECTROGRAM\_STATE\_MAGIC 

_State-blob magic ('SPGM') and layout version._ 
```C++
#define SPECTROGRAM_STATE_MAGIC `DP_FOURCC ('S', 'P', 'G', 'M')`
```




<hr>



### define SPECTROGRAM\_STATE\_VERSION 

```C++
#define SPECTROGRAM_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/spectrogram/spectrogram_core.h`

