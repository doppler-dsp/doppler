

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
| define  | [**DP\_SPECTROGRAM\_DB**](spectrogram__core_8h.md#define-dp_spectrogram_db)  `1`<br>_Row units: dBFS, against the same reference. Asked for by name._  |
| define  | [**DP\_SPECTROGRAM\_POWER**](spectrogram__core_8h.md#define-dp_spectrogram_power)  `0`<br>_Row units: linear power against the PSD's full-scale reference. The DEFAULT: what every example, guide and binding uses unless it asks for dB by name._  |
| define  | [**SPECTROGRAM\_STATE\_MAGIC**](spectrogram__core_8h.md#define-spectrogram_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('S', 'P', 'G', 'M')`<br>_State-blob magic ('SPGM') and layout version._  |
| define  | [**SPECTROGRAM\_STATE\_VERSION**](spectrogram__core_8h.md#define-spectrogram_state_version)  `1u`<br> |

## Detailed Description


A row is the PSD of one frame, and nothing else: row k is PSD's per-frame kernel applied to stream samples [k\*hop, k\*hop + nfft)  [**dp\_psd\_frame\_linear()**](psd__core_8h.md#function-dp_psd_frame_linear) for power rows, the default, or [**dp\_psd\_frame\_db()**](psd__core_8h.md#function-dp_psd_frame_db) for dB rows, which a caller asks for by name. So the window, the FFT and the full-scale reference are PSD's own, and a full-scale tone on a bin reads 1.0 in power, 0 dB in dB. The rows are a function of the INPUT STREAM, not of how it was split into calls: pushing it in one call, a sample at a time, or in any other partition gives the same rows, bit for bit.


The object composes and re-implements none of its parts. The carry between calls is the ring's framed face (DECLARE\_DP\_BUFFER\_FRAMES): fewer than nfft samples are held once a push returns, so the state blob has a size that depends on nfft alone. The spectrum is PSD's per-frame kernel. It does not average rows (fold them with AccTrace), detect, display or decimate.


Lifecycle: [**dp\_spectrogram\_create()**](spectrogram__core_8h.md#function-dp_spectrogram_create), then any number of [**dp\_spectrogram\_push()**](spectrogram__core_8h.md#function-dp_spectrogram_push) calls, then [**dp\_spectrogram\_flush()**](spectrogram__core_8h.md#function-dp_spectrogram_flush) once to end the stream, then [**dp\_spectrogram\_destroy()**](spectrogram__core_8h.md#function-dp_spectrogram_destroy). A short output buffer never loses input: push stops at a whole row and [**dp\_spectrogram\_consumed()**](spectrogram__core_8h.md#function-dp_spectrogram_consumed) says where to resume.


Not thread-safe on one object (the kernel uses the object's scratch). Complex float32 input only. The design, its goals and what it costs are docs/design/spectrogram.md. 


    
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


```C++
dp_spectrogram_state_t *s
    = dp_spectrogram_create (8, 8, 3, 0.0f, DP_SPECTROGRAM_POWER);
float _Complex x[32] = { 0 };
float row[8];
// 32 samples make 4 rows, but out has room for 1: the push takes the 8
// that complete it and the 7 after them that complete nothing; sample 15
// would complete a row with no room, so it is not taken
if (dp_spectrogram_push (s, x, 32, row, 8) != 8
    || dp_spectrogram_consumed (s) != 15)
  return 1;
// resume at x + 15: sample 15 completes the next row
if (dp_spectrogram_push (s, x + 15, 32 - 15, row, 8) != 8
    || dp_spectrogram_consumed (s) != 8)
  return 1;
dp_spectrogram_destroy (s);
```
 


        

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
* `window` 0 = Hann, 1 = Kaiser, 2 = Blackman-Harris, 3 = rectangular, as [**dp\_psd\_create()**](psd__core_8h.md#function-dp_psd_create), in its periodic form, and refused as it refuses: a window that is not finite (a NaN Kaiser beta, or one past I0's overflow). Every window has gain at every nfft: Hann at nfft = 2 is `[0, 1]`. 
* `beta` Kaiser beta (ignored for the other windows). 
* `mode` DP\_SPECTROGRAM\_POWER (linear rows, the default, 0) or DP\_SPECTROGRAM\_DB (dBFS rows, 1); any other value is refused. Pass it by name: `make lint` refuses an integer literal here. 



**Returns:**

Heap-allocated state, or NULL on an invalid argument.


Every row is DC-centred exactly as PSD's kernel emits it: bin k at index nfft/2 + k, negative frequencies first. 

**Note:**

Call [**dp\_spectrogram\_destroy()**](spectrogram__core_8h.md#function-dp_spectrogram_destroy) when done.



```C++
// nfft 1024, a row every 256 samples (75% overlap), Blackman-Harris,
// power rows (the default), DC-centred
dp_spectrogram_state_t *s
    = dp_spectrogram_create (1024, 256, 2, 0.0f, DP_SPECTROGRAM_POWER);
// the same rows in dBFS, asked for by name
dp_spectrogram_state_t *d
    = dp_spectrogram_create (1024, 256, 2, 0.0f, DP_SPECTROGRAM_DB);
if (!s || !d)
  return 1;
// refused: 1000 is not a power of two, a hop may not exceed nfft, and
// a mode is one of the two
const int not_a_mode = DP_SPECTROGRAM_DB + 1;
if (dp_spectrogram_create (1000, 256, 2, 0.0f, DP_SPECTROGRAM_POWER)
    || dp_spectrogram_create (1024, 2048, 2, 0.0f, DP_SPECTROGRAM_POWER)
    || dp_spectrogram_create (1024, 256, 2, 0.0f, not_a_mode))
  return 1;
dp_spectrogram_destroy (d);
dp_spectrogram_destroy (s);
```
 


        

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


```C++
dp_spectrogram_state_t *s
    = dp_spectrogram_create (8, 4, 3, 0.0f, DP_SPECTROGRAM_POWER);
if (!s)
  return 1;
dp_spectrogram_destroy (s);    // its PSD, its ring and its carry go too
dp_spectrogram_destroy (NULL); // a no-op
```
 


        

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



```C++
dp_spectrogram_state_t *s
    = dp_spectrogram_create (8, 4, 3, 0.0f, DP_SPECTROGRAM_POWER);
dp_spectrogram_state_t *t
    = dp_spectrogram_create (8, 4, 3, 0.0f, DP_SPECTROGRAM_POWER);
if (!s || !t)
  return 1;
float _Complex x[10], tail[8] = { 0 };
for (int i = 0; i < 10; i++)
  x[i] = (float)(i + 1);
float row[8], last[8], want[8];
// 10 samples: one row, [0, 8); samples 8 and 9 no row has covered
dp_spectrogram_push (s, x, 10, row, 8);
if (dp_spectrogram_pending (s) != 2)
  return 1;
// the owed row starts on the hop grid, at 4 (not at 8): x[4..10), zeros
if (dp_spectrogram_flush (s, last) != 8)
  return 1;
memcpy (tail, x + 4, 6 * sizeof *x);      // the same frame, pushed whole
if (dp_spectrogram_push (t, tail, 8, want, 8) != 8
    || memcmp (last, want, sizeof want) != 0)
  return 1;
if (dp_spectrogram_flush (s, last) != 0)  // the stream is over
  return 1;
dp_spectrogram_destroy (t);
dp_spectrogram_destroy (s);
```
 


        

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


```C++
// every byte is written: two differently pre-filled blobs come out equal
// (a union, so each blob is aligned for the dp_state_hdr_t it opens with)
union { dp_state_hdr_t h; unsigned char b[256]; } b1, b2;
dp_spectrogram_state_t *s
    = dp_spectrogram_create (8, 4, 3, 0.0f, DP_SPECTROGRAM_POWER);
if (!s || dp_spectrogram_state_bytes (s) > sizeof b1.b)
  return 1;
size_t n = dp_spectrogram_state_bytes (s);
float _Complex x[5] = { 1, 2, 3, 4, 5 };
float out[8];
dp_spectrogram_push (s, x, 5, out, 8); // 5 samples held, no row yet
memset (b1.b, 0xAA, n);
memset (b2.b, 0x55, n);
dp_spectrogram_get_state (s, b1.b);
dp_spectrogram_get_state (s, b2.b);
if (memcmp (b1.b, b2.b, n) != 0)
  return 1;
dp_spectrogram_destroy (s);
```
 


        

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


```C++
dp_spectrogram_state_t *s
    = dp_spectrogram_create (8, 4, 3, 0.0f, DP_SPECTROGRAM_POWER);
float _Complex x[12] = { 0 };
float out[2 * 8];
dp_spectrogram_push (s, x, 12, out, 2 * 8);
if (dp_spectrogram_pending (s) != 0)   // rows at 0 and 4 cover all 12
  return 1;
dp_spectrogram_push (s, x, 3, out, 2 * 8);
if (dp_spectrogram_pending (s) != 3)
  return 1;
dp_spectrogram_destroy (s);
```
 


        

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
// nfft 8, hop 4, rectangular, power: a unit tone on bin 2
dp_spectrogram_state_t *s
    = dp_spectrogram_create (8, 4, 3, 0.0f, DP_SPECTROGRAM_POWER);
float _Complex x[16];
for (int i = 0; i < 16; i++)
  x[i] = cexpf (I * 2.0f * 3.14159265f * 2.0f * (float)i / 8.0f);
float rows[3 * 8];
size_t got = dp_spectrogram_push (s, x, 16, rows, 3 * 8);
// 16 samples at hop 4 complete the rows starting at 0, 4 and 8
if (got != 3 * 8 || dp_spectrogram_consumed (s) != 16)
  return 1;
if (fabsf (rows[4 + 2] - 1.0f) > 1e-4f) // bin 2 reads 1.0, at nfft/2 + 2
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


```C++
dp_spectrogram_state_t *s
    = dp_spectrogram_create (8, 4, 3, 0.0f, DP_SPECTROGRAM_POWER);
float _Complex x[16] = { 0 };
float out[2 * 8];
// 3 samples complete no row: no room is needed, and they are taken
if (dp_spectrogram_push_max_out (s, 3) != 0)
  return 1;
dp_spectrogram_push (s, x, 3, out, 0);
if (dp_spectrogram_consumed (s) != 3)
  return 1;
// 3 carried + 9 more = 12 samples: the rows starting at 0 and at 4
size_t room = dp_spectrogram_push_max_out (s, 9);
if (room != 2 * 8 || dp_spectrogram_push (s, x, 9, out, room) != room
    || dp_spectrogram_consumed (s) != 9)
  return 1;
dp_spectrogram_destroy (s);
```
 


        

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


```C++
dp_spectrogram_state_t *s
    = dp_spectrogram_create (8, 8, 3, 0.0f, DP_SPECTROGRAM_POWER);
float _Complex x[5] = { 0 };
float row[8];
dp_spectrogram_push (s, x, 5, row, 8);   // 5 samples of carry, no row
if (dp_spectrogram_pending (s) != 5)
  return 1;
dp_spectrogram_reset (s);                 // the carry is gone
if (dp_spectrogram_pending (s) != 0)
  return 1;
dp_spectrogram_destroy (s);
```
 


        

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


```C++
dp_spectrogram_state_t *s
    = dp_spectrogram_create (8, 2, 3, 0.0f, DP_SPECTROGRAM_POWER);
if (dp_spectrogram_rows_for (s, 7) != 0      // less than a frame
    || dp_spectrogram_rows_for (s, 8) != 1
    || dp_spectrogram_rows_for (s, 100) != 47) // (100 - 8) / 2 + 1
  return 1;
dp_spectrogram_destroy (s);
```
 


        

<hr>



### function dp\_spectrogram\_set\_state 

_Restore a stream position, so the next push continues it bit for bit._ 
```C++
int dp_spectrogram_set_state (
    dp_spectrogram_state_t * s,
    const void * blob
) 
```



Precondition, the caller's to keep because the blob does not carry it: `s` was created with the same arguments (nfft, hop, window, beta, mode) as the spectrogram the blob came from. A different nfft or hop is refused (the size and the framer's stored hop tell); a different window, beta or mode is NOT, and the rows that follow are the restoring object's, not the original's.




**Parameters:**


* `s` A spectrogram created with the same arguments. 
* `blob` From [**dp\_spectrogram\_get\_state()**](spectrogram__core_8h.md#function-dp_spectrogram_get_state). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID (wrong magic, version, size or hop, or a corrupt carry), with `s` unchanged.



```C++
// one stream, cut mid-frame, resumed in a FRESH object, rows unchanged
dp_spectrogram_state_t *a
    = dp_spectrogram_create (8, 4, 3, 0.0f, DP_SPECTROGRAM_POWER);
dp_spectrogram_state_t *b
    = dp_spectrogram_create (8, 4, 3, 0.0f, DP_SPECTROGRAM_POWER);
float _Complex x[20];
for (int i = 0; i < 20; i++)
  x[i] = (float)i;
float ra[4 * 8], rb[4 * 8];
size_t na = dp_spectrogram_push (a, x, 20, ra, 4 * 8);
size_t nb = dp_spectrogram_push (b, x, 11, rb, 4 * 8); // stop at 11
// a union: the blob opens with a dp_state_hdr_t, so it must be aligned
union { dp_state_hdr_t h; unsigned char b[256]; } blob;
if (dp_spectrogram_state_bytes (b) > sizeof blob.b)
  return 1;
dp_spectrogram_get_state (b, blob.b);
dp_spectrogram_destroy (b);
b = dp_spectrogram_create (8, 4, 3, 0.0f, DP_SPECTROGRAM_POWER);
if (dp_spectrogram_set_state (b, blob.b) != DP_OK)
  return 1;
nb += dp_spectrogram_push (b, x + 11, 9, rb + nb, 4 * 8 - nb);
if (nb != na || memcmp (ra, rb, na * sizeof *ra) != 0)
  return 1;
dp_spectrogram_destroy (a);
dp_spectrogram_destroy (b);
```
 


        

<hr>



### function dp\_spectrogram\_state\_bytes 

_Bytes of the state blob, a function of nfft alone._ 
```C++
size_t dp_spectrogram_state_bytes (
    const dp_spectrogram_state_t * s
) 
```



The blob is the stream position  the framer's snapshot: the carry (fewer than nfft samples, padded to nfft - 1), its element size, the stream counts, and the hop that frames them  inside the spectrogram's own envelope. It does not carry consumed(), the PSD's window or beta, or the spectrogram's own mode. So nfft and hop are checked on restore (the size and the stored hop), and window, beta and mode are the caller's to keep the same  see [**dp\_spectrogram\_set\_state()**](spectrogram__core_8h.md#function-dp_spectrogram_set_state).




**Parameters:**


* `s` Must be non-NULL.


```C++
// the same nfft, the same size, whatever the hop, window or beta
dp_spectrogram_state_t *a
    = dp_spectrogram_create (16, 4, 0, 0.0f, DP_SPECTROGRAM_POWER);
dp_spectrogram_state_t *b
    = dp_spectrogram_create (16, 16, 1, 8.0f, DP_SPECTROGRAM_POWER);
dp_spectrogram_state_t *c
    = dp_spectrogram_create (32, 4, 0, 0.0f, DP_SPECTROGRAM_POWER);
if (!a || !b || !c)
  return 1;
if (dp_spectrogram_state_bytes (a) != dp_spectrogram_state_bytes (b))
  return 1;
// a longer frame holds a longer carry
if (dp_spectrogram_state_bytes (c) <= dp_spectrogram_state_bytes (a))
  return 1;
dp_spectrogram_destroy (c);
dp_spectrogram_destroy (b);
dp_spectrogram_destroy (a);
```
 


        

<hr>
## Macro Definition Documentation





### define DP\_SPECTROGRAM\_DB 

_Row units: dBFS, against the same reference. Asked for by name._ 
```C++
#define DP_SPECTROGRAM_DB `1`
```



Row k is [**dp\_psd\_frame\_db()**](psd__core_8h.md#function-dp_psd_frame_db) of its frame, which is [**dp\_power\_to\_db\_f32()**](spectral__core_8h.md#function-dp_power_to_db_f32) of the power row, bit for bit: 10\*log10 within 0.01 dB (3.25e-4 measured over every float32), exact at every power of two. So a full-scale tone on a bin reads 0 dB, and a display that converts only the power bins it draws gets exactly the dB row's values. A bin reads no lower than -200 dB: the conversion's floor is 1e-20, so an all-zero frame and a frame below the floor write the same row. 


        

<hr>



### define DP\_SPECTROGRAM\_POWER 

_Row units: linear power against the PSD's full-scale reference. The DEFAULT: what every example, guide and binding uses unless it asks for dB by name._ 
```C++
#define DP_SPECTROGRAM_POWER `0`
```



Row k is [**dp\_psd\_frame\_linear()**](psd__core_8h.md#function-dp_psd_frame_linear) of its frame, so a full-scale tone on a bin reads 1.0 whatever the window, and an all-zero frame reads 0: the only floor is float32's. A power row is what an averaging consumer folds with AccTrace (the mean of dB rows is not the dB of the mean), and what a display converts to dB for only the bins it draws. It is the default because that conversion is most of a dB row's cost: 70-83% of it at nfft 256 to 65536 (docs/design/spectrogram-measurements.md section 5.8).


It is 0, so a zeroed or calloc'd configuration selects power, never dB: dB is only ever asked for. A binding that maps mode names to these values by position (jm's string\_enum does) lists them in this order: power, db. 


        

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

