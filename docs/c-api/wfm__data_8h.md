

# File wfm\_data.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_data.h**](wfm__data_8h.md)

[Go to the source code of this file](wfm__data_8h_source.md)

_A frame's data source: where a_ `data:LEN` _payload's bits come from._[More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`
* `#include "doppler/wfm/wfm_frame.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**wfm\_data\_stats\_t**](structwfm__data__stats__t.md) <br>_What a source has done so far: the truth a record carries._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**wfm\_data\_pacing\_t**](#enum-wfm_data_pacing_t)  <br>_Whether frames are due at a time (_ `--realtime` _) or on demand._ |
| typedef struct wfm\_data\_src | [**wfm\_data\_src\_t**](#typedef-wfm_data_src_t)  <br>_Opaque; see_ [_**dp\_wfm\_data\_create**_](wfm__data_8h.md#function-dp_wfm_data_create) _._ |
| enum  | [**wfm\_data\_status\_t**](#enum-wfm_data_status_t)  <br>_What a request for the next frame's data produced._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**wfm\_data\_src\_t**](wfm__data_8h.md#typedef-wfm_data_src_t) \* | [**dp\_wfm\_data\_create**](#function-dp_wfm_data_create) (const char \* data, const char \* path, size\_t len, const char \* fill, const char \*\* why) <br>_Build a data source from a Field or from a path. NULL on refusal._  |
|  [**wfm\_data\_src\_t**](wfm__data_8h.md#typedef-wfm_data_src_t) \* | [**dp\_wfm\_data\_create\_fd**](#function-dp_wfm_data_create_fd) (int fd, size\_t len, const char \* fill, const char \*\* why) <br>_Build a data source over an open file descriptor. NULL on refusal._  |
|  [**wfm\_data\_src\_t**](wfm__data_8h.md#typedef-wfm_data_src_t) \* | [**dp\_wfm\_data\_create\_seq**](#function-dp_wfm_data_create_seq) (const [**wfm\_seq\_t**](structwfm__seq__t.md) \* data, const char \* path, size\_t len, const [**wfm\_seq\_t**](structwfm__seq__t.md) \* fill, const char \*\* why) <br>_Build a data source from a source's own members. NULL on refusal._  |
|  void | [**dp\_wfm\_data\_destroy**](#function-dp_wfm_data_destroy) ([**wfm\_data\_src\_t**](wfm__data_8h.md#typedef-wfm_data_src_t) \* s) <br>_Free a source; closes a file it opened itself. NULL is a no-op._  |
|  [**wfm\_data\_status\_t**](wfm__data_8h.md#enum-wfm_data_status_t) | [**dp\_wfm\_data\_frame**](#function-dp_wfm_data_frame) ([**wfm\_data\_src\_t**](wfm__data_8h.md#typedef-wfm_data_src_t) \* s, [**wfm\_data\_pacing\_t**](wfm__data_8h.md#enum-wfm_data_pacing_t) pacing, size\_t reps, uint8\_t \* out, size\_t max\_out) <br>_The next frame's data field under a pacing: THE rule for idle frames._  |
|  [**wfm\_data\_status\_t**](wfm__data_8h.md#enum-wfm_data_status_t) | [**dp\_wfm\_data\_idle**](#function-dp_wfm_data_idle) ([**wfm\_data\_src\_t**](wfm__data_8h.md#typedef-wfm_data_src_t) \* s, size\_t reps, uint8\_t \* out, size\_t max\_out) <br>_Write an idle frame's data field: all fill,_ `reps` _times._ |
|  uint64\_t | [**dp\_wfm\_data\_length\_bits**](#function-dp_wfm_data_length_bits) (const char \* data, const char \* path) <br>_A source's length in bits, known before it is built; 0 for a stream._  |
|  [**wfm\_data\_status\_t**](wfm__data_8h.md#enum-wfm_data_status_t) | [**dp\_wfm\_data\_next**](#function-dp_wfm_data_next) ([**wfm\_data\_src\_t**](wfm__data_8h.md#typedef-wfm_data_src_t) \* s, size\_t reps, uint8\_t \* out, size\_t max\_out, int timeout\_ms) <br>_Write the next frame's data field: ONE chunk of_ `LEN` _bits, written_`reps` _times._ |
|  void | [**dp\_wfm\_data\_stats**](#function-dp_wfm_data_stats) (const [**wfm\_data\_src\_t**](wfm__data_8h.md#typedef-wfm_data_src_t) \* s, [**wfm\_data\_stats\_t**](structwfm__data__stats__t.md) \* out) <br>_What the source has done so far._  |




























## Detailed Description


A frame declares how many bits its payload carries, `data:LEN`, and a data source supplies them, `LEN` at a time, one chunk per frame (docs/design/payload-data-source.md). The source is one of:



|built from   |is a   |frames    |
|-----|-----|-----|
|a literal Field (`0x…`, `0101`)   |finite   |`ceil(bits / LEN)`    |
|a generated Field with `LEN > 0`   |finite   |the same    |
|`pn:0:REG[:SEED[:POLY]]`   |stream   |never ends, never pauses    |
|a regular file   |finite   |`ceil(8 * bytes / LEN)`    |
|stdin, a pipe (any other fd)   |stream   |until the input ends   |






`--data none` is not a source: it means no data at all (continuous DSSS sends its code alone), so a face that reads `none` builds no source, and [**dp\_wfm\_data\_create**](wfm__data_8h.md#function-dp_wfm_data_create) refuses the word as text outside the grammar.


A file and a pipe carry **packed** octets, unpacked MSB first by `dp_bytes_to_bin`. `LEN` need not be a multiple of 8: an octet can straddle two frames, and the source keeps the leftover bits for the next.


**Every refusal that can be decided before the first sample is decided at create** (§4.4): a finite source whose length is not a multiple of `LEN` needs a fill, a pipe always needs one (its length is unknowable up front), and an empty finite source is refused. The last frame of a finite source is padded with the fill, tiled from its first bit.


**Each chunk has one of three outcomes** (§4.5): a frame of data, nothing yet, or the end. _Nothing yet_ happens only when a caller asks with a timeout and a pipe has not delivered; the bits already read are kept for the next frame, and it is the paced caller that decides to send an idle frame instead, all fill (§4.1). An unpaced caller asks with no timeout and waits, as `cat` does.


A source read from an fd hashes every octet it reads with `dp_hash64()`, so a record can identify the file without reading it twice (§4.8).


Not in this object yet: the state triplet (a stream's resume position), which lands with the faces that serialize it. 


    
## Public Types Documentation




### enum wfm\_data\_pacing\_t 

_Whether frames are due at a time (_ `--realtime` _) or on demand._
```C++
enum wfm_data_pacing_t {
    WFM_DATA_UNPACED = 0,
    WFM_DATA_PACED = 1
};
```




<hr>



### typedef wfm\_data\_src\_t 

_Opaque; see_ [_**dp\_wfm\_data\_create**_](wfm__data_8h.md#function-dp_wfm_data_create) _._
```C++
typedef struct wfm_data_src wfm_data_src_t;
```




<hr>



### enum wfm\_data\_status\_t 

_What a request for the next frame's data produced._ 
```C++
enum wfm_data_status_t {
    WFM_DATA_FRAME = 0,
    WFM_DATA_NOT_YET = 1,
    WFM_DATA_END = 2,
    WFM_DATA_ERROR = 3,
    WFM_DATA_IDLE = 4
};
```




<hr>
## Public Functions Documentation




### function dp\_wfm\_data\_create 

_Build a data source from a Field or from a path. NULL on refusal._ 
```C++
wfm_data_src_t * dp_wfm_data_create (
    const char * data,
    const char * path,
    size_t len,
    const char * fill,
    const char ** why
) 
```



Exactly one of `data` and `path` is given: `--data` and `--data-from-file` are one exclusive pair (§4.9), and both is refused.




**Parameters:**


* `data` a Field (`0x…`, `0101`, `pn:N:…`, or `pn:0:REG[:SEED[:POLY]]` for an endless seeded stream); NULL when `path` is given. `data:LEN` and `*REPS` on a stream are refused. 
* `path` a file, or `-` for stdin; NULL when `data` is given. 
* `len` bits per frame, `LEN` of the frame's `data:LEN`; &gt; 0. 
* `fill` a Field whose bits pad the last frame and fill an idle one, or NULL for none. `data:LEN` is refused as a fill. 
* `why` optional; receives a STATIC sentence naming the cause of a refusal (a Field's is the parser's own), untouched on success. A remainder is stated in bits by the caller, from [**dp\_wfm\_data\_length\_bits**](wfm__data_8h.md#function-dp_wfm_data_length_bits). 



**Returns:**

the source, or NULL: text outside the grammar, a file that cannot be opened, an empty finite source, a finite source that does not divide into `LEN`-bit frames with no fill, or a pipe with no fill.



```C++
const char     *why;
wfm_data_src_t *s = dp_wfm_data_create ("0xABCD", NULL, 8, NULL, &why);
uint8_t         b[8];
dp_wfm_data_next (s, 1, b, sizeof b, -1); // WFM_DATA_FRAME: 1010 1011
dp_wfm_data_next (s, 1, b, sizeof b, -1); // WFM_DATA_FRAME: 1100 1101
dp_wfm_data_next (s, 1, b, sizeof b, -1); // WFM_DATA_END
dp_wfm_data_destroy (s);
```
 


        

<hr>



### function dp\_wfm\_data\_create\_fd 

_Build a data source over an open file descriptor. NULL on refusal._ 
```C++
wfm_data_src_t * dp_wfm_data_create_fd (
    int fd,
    size_t len,
    const char * fill,
    const char ** why
) 
```



What [**dp\_wfm\_data\_create**](wfm__data_8h.md#function-dp_wfm_data_create) does for a path, over an fd the caller already holds: a regular file is finite and its length comes from `fstat`; anything else (a pipe, a terminal, a socket) is a stream and needs `fill`. The source never closes `fd`. 


        

<hr>



### function dp\_wfm\_data\_create\_seq 

_Build a data source from a source's own members. NULL on refusal._ 
```C++
wfm_data_src_t * dp_wfm_data_create_seq (
    const wfm_seq_t * data,
    const char * path,
    size_t len,
    const wfm_seq_t * fill,
    const char ** why
) 
```



What [**dp\_wfm\_data\_create**](wfm__data_8h.md#function-dp_wfm_data_create) does from text, from the representation a `wfm_source_t` carries: `data` is a `wfm_seq_t`  a literal (a bit array from Python, a parsed Field from the CLI or a scene) or a finite generated sequence  and `fill` likewise, rendered once through `dp_wfm_seq_bits()`. Exactly one of `data` (with `len > 0`) and `path`; the refusals are [**dp\_wfm\_data\_create**](wfm__data_8h.md#function-dp_wfm_data_create)'s.




**Parameters:**


* `data` the data sequence, or NULL / `len == 0` with `path`. 
* `path` a file, or `-` for stdin; NULL with `data`. 
* `len` bits per frame; &gt; 0. 
* `fill` the fill sequence, or NULL / `len == 0` for none. 
* `why` optional; receives a STATIC reason for a refusal.


```C++
static const uint8_t bits[12] = { 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 0 };
const wfm_seq_t      data = { .kind = WFM_SEQ_LITERAL, .bits = bits,
                              .len = 12 };
const wfm_seq_t      fill = { .kind = WFM_SEQ_DOTTED, .len = 2 };
const char          *why;
wfm_data_src_t *s = dp_wfm_data_create_seq (&data, NULL, 8, &fill, &why);
if (!s)
  return 1;
wfm_data_stats_t st;
dp_wfm_data_stats (s, &st);
if (st.total_bits != 12) // two 8-bit frames, the second padded
  return 1;
dp_wfm_data_destroy (s);
```
 


        

<hr>



### function dp\_wfm\_data\_destroy 

_Free a source; closes a file it opened itself. NULL is a no-op._ 
```C++
void dp_wfm_data_destroy (
    wfm_data_src_t * s
) 
```




<hr>



### function dp\_wfm\_data\_frame 

_The next frame's data field under a pacing: THE rule for idle frames._ 
```C++
wfm_data_status_t dp_wfm_data_frame (
    wfm_data_src_t * s,
    wfm_data_pacing_t pacing,
    size_t reps,
    uint8_t * out,
    size_t max_out
) 
```



§4.5 in one place: only a paced caller gets idle frames. Paced, the source is asked without waiting, and _nothing yet_ becomes an all-fill idle frame ([**dp\_wfm\_data\_idle**](wfm__data_8h.md#function-dp_wfm_data_idle)), so the carrier and the frame timing never break. Unpaced, it waits for as long as the data takes and never sends an idle frame. Every frame-pulling caller goes through this, so no caller has to remember which timeout means which.




**Returns:**

WFM\_DATA\_FRAME, WFM\_DATA\_IDLE (paced only), WFM\_DATA\_END or WFM\_DATA\_ERROR.



```C++
const char     *why;
wfm_data_src_t *s = dp_wfm_data_create ("0xABCD", NULL, 16, "10", &why);
uint8_t         b[16];
// A Field never pauses, so this is a data frame: 1010 1011 1100 1101.
// A paced pipe with nothing yet would give WFM_DATA_IDLE, b all fill.
if (dp_wfm_data_frame (s, WFM_DATA_PACED, 1, b, sizeof b)
        != WFM_DATA_FRAME
    || b[0] != 1 || b[1] != 0)
  return 1;
if (dp_wfm_data_frame (s, WFM_DATA_PACED, 1, b, sizeof b) != WFM_DATA_END)
  return 1; // 16 bits in 16-bit frames: one frame, then the end
dp_wfm_data_destroy (s);
```
 


        

<hr>



### function dp\_wfm\_data\_idle 

_Write an idle frame's data field: all fill,_ `reps` _times._
```C++
wfm_data_status_t dp_wfm_data_idle (
    wfm_data_src_t * s,
    size_t reps,
    uint8_t * out,
    size_t max_out
) 
```



What a paced caller sends when [**dp\_wfm\_data\_next**](wfm__data_8h.md#function-dp_wfm_data_next) says WFM\_DATA\_NOT\_YET, so the carrier and the frame timing never break. It consumes no source bits (a partial chunk already read waits for the next data frame) and is counted in `idle_frames`.




**Returns:**

WFM\_DATA\_FRAME, or WFM\_DATA\_ERROR when the source has no fill or `out` is too small. 





        

<hr>



### function dp\_wfm\_data\_length\_bits 

_A source's length in bits, known before it is built; 0 for a stream._ 
```C++
uint64_t dp_wfm_data_length_bits (
    const char * data,
    const char * path
) 
```



What a caller needs before the first sample: how many `LEN`-bit frames a finite source makes, `ceil(bits / LEN)`, and  when [**dp\_wfm\_data\_create**](wfm__data_8h.md#function-dp_wfm_data_create) refuses a source that leaves a remainder with no fill  the numbers to state that remainder in. Exactly one of `data` and `path`, as [**dp\_wfm\_data\_create**](wfm__data_8h.md#function-dp_wfm_data_create) takes them.




**Returns:**

the bits of a literal or generated Field, or of a regular file; 0 for a stream (`pn:0:…`, `-`, a pipe), for text outside the grammar, and for a file that cannot be opened. A 0 does not say which, so call this after a successful [**dp\_wfm\_data\_create**](wfm__data_8h.md#function-dp_wfm_data_create), where the source is known to exist and its stats say whether it is a stream.



```C++
const uint64_t n = dp_wfm_data_length_bits ("0xABC", NULL);   // 12
// in 8-bit frames: the last holds n % 8 = 4 and is 8 - 4 = 4 short
```
 


        

<hr>



### function dp\_wfm\_data\_next 

_Write the next frame's data field: ONE chunk of_ `LEN` _bits, written_`reps` _times._
```C++
wfm_data_status_t dp_wfm_data_next (
    wfm_data_src_t * s,
    size_t reps,
    uint8_t * out,
    size_t max_out,
    int timeout_ms
) 
```



A repeated data field (`data:LEN*REPS`) is one draw sent again, never fresh draws (frame-description.md §F.1), so the source advances by `LEN` bits per frame whatever `reps` is. The last chunk of a finite source, or of a pipe that closes mid-chunk, is padded with the fill.




**Parameters:**


* `s` the source. 
* `reps` the field's repetitions; 0 means one. 
* `out` receives `LEN * reps` bits, one per byte. 
* `max_out` capacity of `out` in bits. 
* `timeout_ms` how long to wait for a pipe: -1 waits for as long as it takes (unpaced), 0 does not wait at all. 



**Returns:**

WFM\_DATA\_FRAME, or WFM\_DATA\_NOT\_YET (nothing written; the bits already read are kept), WFM\_DATA\_END (nothing written), or WFM\_DATA\_ERROR (a read failed, or `out` is smaller than `LEN * reps`). 





        

<hr>



### function dp\_wfm\_data\_stats 

_What the source has done so far._ 
```C++
void dp_wfm_data_stats (
    const wfm_data_src_t * s,
    wfm_data_stats_t * out
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_data.h`

