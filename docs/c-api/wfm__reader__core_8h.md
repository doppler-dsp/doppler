

# File wfm\_reader\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm\_reader**](dir_01018a3d11538c9aca2db4daa45a442f.md) **>** [**wfm\_reader\_core.h**](wfm__reader__core_8h.md)

[Go to the source code of this file](wfm__reader__core_8h_source.md)

_Input file types for generated IQ — the dual of wfm\_writer._ [More...](#detailed-description)

* `#include <complex.h>`
* `#include <stddef.h>`
* `#include "wfm/wfm_keywords.h"`
* `#include "wfm_writer/wfm_writer_core.h"`
* `#include "dp_interrupt_guard/dp_interrupt_guard_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**wfm\_reader\_info\_t**](structwfm__reader__info__t.md) <br> |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**wfm\_fc\_source\_t**](#enum-wfm_fc_source_t)  <br> |
| enum  | [**wfm\_follow\_end\_t**](#enum-wfm_follow_end_t)  <br> |
| enum  | [**wfm\_fs\_source\_t**](#enum-wfm_fs_source_t)  <br> |
| enum  | [**wfm\_mode\_t**](#enum-wfm_mode_t)  <br> |
| typedef struct wfm\_reader\_state | [**wfm\_reader\_state\_t**](#typedef-wfm_reader_state_t)  <br> |
| enum  | [**wfm\_t0\_source\_t**](#enum-wfm_t0_source_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* | [**wfm\_reader\_create**](#function-wfm_reader_create) (const char \* path, int sample\_type, int endian) <br>_Open a capture, auto-detecting its file type from its content._  |
|  void | [**wfm\_reader\_destroy**](#function-wfm_reader_destroy) ([**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br>_Close the file, free the reader and its decoded keywords._  |
|  const [**wfm\_keyword\_t**](structwfm__keyword__t.md) \* | [**wfm\_reader\_find\_header\_field**](#function-wfm_reader_find_header_field) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state, const char \* name) <br>_Look up one HCB field by name, or NULL if absent._  |
|  const [**wfm\_keyword\_t**](structwfm__keyword__t.md) \* | [**wfm\_reader\_find\_keyword**](#function-wfm_reader_find_keyword) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* r, const char \* tag) <br> |
|  int | [**wfm\_reader\_get\_endian**](#function-wfm_reader_get_endian) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br> |
|  int | [**wfm\_reader\_get\_ending**](#function-wfm_reader_get_ending) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br> |
|  double | [**wfm\_reader\_get\_fc**](#function-wfm_reader_get_fc) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br> |
|  int | [**wfm\_reader\_get\_fc\_source**](#function-wfm_reader_get_fc_source) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br>_Which keyword_ [_**wfm\_reader\_get\_fc**_](wfm__reader__core_8h.md#function-wfm_reader_get_fc) _read the centre frequency from._ |
|  int | [**wfm\_reader\_get\_file\_type**](#function-wfm_reader_get_file_type) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br> |
|  uint32\_t | [**wfm\_reader\_get\_follow\_grace\_ms**](#function-wfm_reader_get_follow_grace_ms) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br> |
|  uint32\_t | [**wfm\_reader\_get\_follow\_timeout\_ms**](#function-wfm_reader_get_follow_timeout_ms) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br> |
|  double | [**wfm\_reader\_get\_fs**](#function-wfm_reader_get_fs) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br> |
|  int | [**wfm\_reader\_get\_fs\_source**](#function-wfm_reader_get_fs_source) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br>_Which metadata_ [_**wfm\_reader\_get\_fs**_](wfm__reader__core_8h.md#function-wfm_reader_get_fs) _read the sample rate from._ |
|  int | [**wfm\_reader\_get\_mode**](#function-wfm_reader_get_mode) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br> |
|  size\_t | [**wfm\_reader\_get\_num\_samples**](#function-wfm_reader_get_num_samples) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br> |
|  int | [**wfm\_reader\_get\_sample\_type**](#function-wfm_reader_get_sample_type) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br> |
|  double | [**wfm\_reader\_get\_t0**](#function-wfm_reader_get_t0) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br>_Capture start time in seconds since the UNIX epoch, or 0.0._  |
|  int | [**wfm\_reader\_get\_t0\_source**](#function-wfm_reader_get_t0_source) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br>_Where_ [_**wfm\_reader\_get\_t0**_](wfm__reader__core_8h.md#function-wfm_reader_get_t0) _read the capture start time from._ |
|  size\_t | [**wfm\_reader\_get\_trailing\_bytes**](#function-wfm_reader_get_trailing_bytes) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br>_Payload bytes left over after the last whole sample._  |
|  const [**wfm\_keyword\_t**](structwfm__keyword__t.md) \* | [**wfm\_reader\_header\_field**](#function-wfm_reader_header_field) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state, size\_t i) <br>_The i-th decoded HCB field, or NULL if_ `i` _is out of range._ |
|  const char \* | [**wfm\_reader\_header\_tag**](#function-wfm_reader_header_tag) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state, size\_t i) <br>_The i-th HCB field's name, for the_ `.header` _dict binding._ |
|  void | [**wfm\_reader\_info**](#function-wfm_reader_info) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* r, [**wfm\_reader\_info\_t**](structwfm__reader__info__t.md) \* info) <br>_Copy the resolved capture metadata into_ `info` _._ |
|  const [**wfm\_keyword\_t**](structwfm__keyword__t.md) \* | [**wfm\_reader\_keyword**](#function-wfm_reader_keyword) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* r, size\_t i) <br>_The_ `i'th` _keyword in file order, or NULL if_`i` _is out of range._ |
|  const char \* | [**wfm\_reader\_keyword\_tag**](#function-wfm_reader_keyword_tag) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state, size\_t i) <br>_The tag of the_ `i'th` _keyword (key\_fn for the_`.keywords` _dict)._ |
|  size\_t | [**wfm\_reader\_num\_header\_fields**](#function-wfm_reader_num_header_fields) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br>_The first keyword whose tag equals_ `tag` _, or NULL if absent._ |
|  size\_t | [**wfm\_reader\_num\_keywords**](#function-wfm_reader_num_keywords) (const [**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br>_Number of extended-header keywords recovered from the capture._  |
|  size\_t | [**wfm\_reader\_read**](#function-wfm_reader_read) ([**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state, size\_t n, float complex \* out, size\_t max\_out) <br>_Read up to_ `count` _samples, returning them as_`complex64` _._ |
|  size\_t | [**wfm\_reader\_read\_follow**](#function-wfm_reader_read_follow) ([**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state, size\_t n, float complex \* out, size\_t max\_out) <br>_Read from a capture that is still being written._  |
|  size\_t | [**wfm\_reader\_read\_follow\_max\_out**](#function-wfm_reader_read_follow_max_out) ([**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state, size\_t n) <br> |
|  size\_t | [**wfm\_reader\_read\_max\_out**](#function-wfm_reader_read_max_out) ([**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state, size\_t n) <br>_Maximum samples one read(n) yields: n (fewer at EOF)._  |
|  void | [**wfm\_reader\_reset**](#function-wfm_reader_reset) ([**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state) <br>_Rewind to the first sample of the capture._  |
|  void | [**wfm\_reader\_set\_follow\_grace\_ms**](#function-wfm_reader_set_follow_grace_ms) ([**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state, uint32\_t val) <br> |
|  void | [**wfm\_reader\_set\_follow\_timeout\_ms**](#function-wfm_reader_set_follow_timeout_ms) ([**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state, uint32\_t val) <br> |
|  void | [**wfm\_reader\_set\_stop\_fn**](#function-wfm_reader_set_stop_fn) ([**wfm\_reader\_state\_t**](wfm__reader__core_8h.md#typedef-wfm_reader_state_t) \* state, int(\*)(void) fn) <br>_Tell a following read how to learn that a stop was requested._  |




























## Detailed Description


Reads back what wfm\_writer wrote: raw interleaved I/Q, CSV, BLUE type-1000 (attached or detached, `format` mode `S` or `C`), and SigMF. A BLUE file in any other mode is rejected at open — see [**wfm\_mode\_t**](wfm__reader__core_8h.md#enum-wfm_mode_t).


The file type is **auto-detected from the file's CONTENT**, not its name: the BLUE magic at byte 0, a first line that parses as `I,Q` for CSV, a `.sigmf-meta` sidecar alongside. The extension only breaks a tie the content cannot (a `.det` payload, which is headerless by construction). So a CSV called `capture.dat` reads as CSV, and a BLUE file called `capture.csv` reads as BLUE — misnaming a capture costs nothing.


Self-describing file types (BLUE, SigMF) recover the sample type, byte order, sample rate and centre frequency from their metadata. Headerless file types (raw, CSV) take the sample type / byte order as hints, and there is no way to check a hint against the file — see [**wfm\_reader\_get\_trailing\_bytes**](wfm__reader__core_8h.md#function-wfm_reader_get_trailing_bytes) for the one tell that is available.


Samples come out as `float _Complex` at unit scale: float wire types are reinterpreted, integer wire types are rescaled by their full-scale (the exact inverse of the writer's quantiser).



```C++
wfm_reader_state_t *r = wfm_reader_create("cap.sigmf-data", 0, 0);
wfm_reader_info_t info;
wfm_reader_info(r, &info);                 // info.fs, info.sample_type, ...
float _Complex buf[4096];
size_t n;
while ((n = wfm_reader_read(r, 4096, buf, 4096)) > 0)   // (state, count, out)
  consume(buf, n);
wfm_reader_destroy(r);
```
 


    
## Public Types Documentation




### enum wfm\_fc\_source\_t 

```C++
enum wfm_fc_source_t {
    WFM_FC_NONE = 0,
    WFM_FC_FREQ,
    WFM_FC_RF_FREQ,
    WFM_FC_CENTER_FREQ,
    WFM_FC_F_C,
    WFM_FC_SIGMF
};
```



Where a capture's centre frequency came from.


BLUE type-1000 has **no HCB field for centre frequency** — the adjunct carries `xstart`/`xdelta`/`xunits`, which describe the abscissa (time), not the RF the capture was taken at. So an RF capture conveys it as a keyword, and which tag it uses is X-Midas convention rather than anything BLUE 1.1 mandates: 3.1.2.6.4.4 defines `FREQ`, but only as a type-6000 _column_ name, under a heading stating those names "are not keyword
names". `FREQ` in the HCB keyword area is nonetheless what real captures carry, so it is what this reader looks for first.


Reporting the tag matters because 0.0 is a legitimate answer: a genuine baseband capture and a capture whose frequency this library failed to find are otherwise indistinguishable. WFM\_FC\_NONE says "not found", and only then is `fc == 0.0` a guess rather than a reading. 


        

<hr>



### enum wfm\_follow\_end\_t 

```C++
enum wfm_follow_end_t {
    WFM_FOLLOW_NONE = 0,
    WFM_FOLLOW_EOF,
    WFM_FOLLOW_TIMEOUT,
    WFM_FOLLOW_INTERRUPTED
};
```



Why a following read came back empty.


Indices, because that is what the `ending` property decodes to a string. Each corresponds 1:1 to the doppler return code a C caller would expect from any other transport  WFM\_FOLLOW\_EOF is [**DP\_ERR\_EOF**](clib__common_8h.md#define-dp_err_eof), and so on  so the two faces name the same four states even though the property carries the index. 


        

<hr>



### enum wfm\_fs\_source\_t 

```C++
enum wfm_fs_source_t {
    WFM_FS_NONE = 0,
    WFM_FS_BLUE_XDELTA,
    WFM_FS_SIGMF
};
```



Where `fs` came from, for the same reason [**wfm\_fc\_source\_t**](wfm__reader__core_8h.md#enum-wfm_fc_source_t) exists.


`fs == 0.0` is not a rate anyone captured at, so it is less ambiguous than `fc == 0.0` — but "which metadata said so" is still the difference between a capture that declares its rate and one this library had to give up on, and a caller about to build a timeline on it wants to know which it has. 


        

<hr>



### enum wfm\_mode\_t 

```C++
enum wfm_mode_t {
    WFM_MODE_COMPLEX = 0,
    WFM_MODE_SCALAR = 1
};
```



Components per sample — the BLUE `format` field's _mode_ designator (HCB byte 52). Only these two are supported; every other Midas mode (V/Q/M/T/…, 3..10 components) is rejected at open rather than misinterpreted as interleaved I/Q. Non-BLUE file types are complex. 


        

<hr>



### typedef wfm\_reader\_state\_t 

```C++
typedef struct wfm_reader_state wfm_reader_state_t;
```



Opaque reader handle. Opaque reader state; the layout is private to wfm\_reader\_core.c. 


        

<hr>



### enum wfm\_t0\_source\_t 

```C++
enum wfm_t0_source_t {
    WFM_T0_NONE = 0,
    WFM_T0_BLUE_TIMECODE
};
```



Where the capture's absolute start time came from.


This is the `t0` of `t = t0 + n/fs` — the epoch belonging to the DATA, which is what makes a replayed capture's telemetry line up with the recording rather than with the machine replaying it. Hand it to `dp_sample_clock_track()`; the sample clock owns the arithmetic.


WFM\_T0\_NONE is the common case and has to stay visible: doppler's own BLUE writer leaves the timecode field zero, so a zero there means "unset", never 1950-01-01 (see `wfm/wfm_time.h`). A caller that cannot tell those apart will confidently date every doppler-written capture to
* 




        

<hr>
## Public Functions Documentation




### function wfm\_reader\_create 

_Open a capture, auto-detecting its file type from its content._ 
```C++
wfm_reader_state_t * wfm_reader_create (
    const char * path,
    int sample_type,
    int endian
) 
```



Detection order, first match wins: the BLUE magic at byte 0; a first line that scans as `I,Q`; otherwise headerless raw. Two suffixes are decided by name instead, because neither has content that identifies it — `.det`, a detached payload described by its header sibling, and `.sigmf-data`, half of a pair whose other half carries the datatype.


Nothing is refused for looking unfamiliar: an unrecognised file opens as raw at the caller's `sample_type`, because a truncated or partial recording is a real thing and a reader that rejects it is useless. What you get instead of a refusal is [**wfm\_reader\_get\_trailing\_bytes**](wfm__reader__core_8h.md#function-wfm_reader_get_trailing_bytes).




**Parameters:**


* `path` file to read  a `str` or any `os.PathLike` from Python. For a DETACHED BLUE capture this is normally the HEADER file  `<base>.tmp` or `<base>.prm` per BLUE 3.1.1.4 (this library's own writer emits `<base>.hdr`)  whose HCB `detached` field points at the collocated `<base>.det` payload; the extension does not decide, `detached` does. Passing the `<base>.det` directly also works (its header sibling is resolved). A SigMF `.sigmf-data` file resolves its `.sigmf-meta` sidecar the same way. 
* `sample_type` the wire sample type, used only as a HINT for the headerless file types (raw, CSV)  BLUE and SigMF carry their own and ignore it. `"cf32"`, `"cf64"`, `"ci32"`, `"ci16"` or `"ci8"` from Python; the matching 0..4 from C. A wrong hint does not fail; see [**wfm\_reader\_get\_trailing\_bytes**](wfm__reader__core_8h.md#function-wfm_reader_get_trailing_bytes). 
* `endian` byte order, likewise a hint that only headerless raw uses; `"le"` or `"be"` from Python, 0 or 1 from C. 



**Returns:**

a reader, or NULL on open/parse failure.



```C++
>>> import pathlib, tempfile
>>> from doppler.wfm import Composer, Reader, Segment, Writer
>>> tmp = tempfile.TemporaryDirectory()
>>> p = pathlib.Path(tmp.name) / "capture.blue"
>>> x = Composer([Segment("qpsk", sps=8, num_samples=1024)]).compose()
>>> w = Writer(p, file_type="blue", sample_type="ci16", fs=2.4e6)
>>> w.add_keyword("NAME", "A", "demo")   # tag the header
>>> _ = w.write(x)
>>> w.close()
>>> r = Reader(p)                         # file type auto-detected
>>> r.file_type, r.sample_type, r.fs
('blue', 'ci16', 2400000.0)
>>> r.keywords["NAME"]                    # keyword round-trips
'demo'
>>> total = 0
>>> while len(block := r.read(256)):      # read returns 0 at EOF
...     total += len(block)
>>> total == r.num_samples == 1024
True
>>> r.close()
>>> tmp.cleanup()
```
 


        

<hr>



### function wfm\_reader\_destroy 

_Close the file, free the reader and its decoded keywords._ 
```C++
void wfm_reader_destroy (
    wfm_reader_state_t * state
) 
```




<hr>



### function wfm\_reader\_find\_header\_field 

_Look up one HCB field by name, or NULL if absent._ 
```C++
const wfm_keyword_t * wfm_reader_find_header_field (
    const wfm_reader_state_t * state,
    const char * name
) 
```




<hr>



### function wfm\_reader\_find\_keyword 

```C++
const wfm_keyword_t * wfm_reader_find_keyword (
    const wfm_reader_state_t * r,
    const char * tag
) 
```




<hr>



### function wfm\_reader\_get\_endian 

```C++
int wfm_reader_get_endian (
    const wfm_reader_state_t * state
) 
```




<hr>



### function wfm\_reader\_get\_ending 

```C++
int wfm_reader_get_ending (
    const wfm_reader_state_t * state
) 
```




<hr>



### function wfm\_reader\_get\_fc 

```C++
double wfm_reader_get_fc (
    const wfm_reader_state_t * state
) 
```




<hr>



### function wfm\_reader\_get\_fc\_source 

_Which keyword_ [_**wfm\_reader\_get\_fc**_](wfm__reader__core_8h.md#function-wfm_reader_get_fc) _read the centre frequency from._
```C++
int wfm_reader_get_fc_source (
    const wfm_reader_state_t * state
) 
```



A [**wfm\_fc\_source\_t**](wfm__reader__core_8h.md#enum-wfm_fc_source_t). WFM\_FC\_NONE means nothing was found, which is the only way to tell a baseband capture (`fc` genuinely 0 Hz) from one whose frequency this library could not locate — both report `fc == 0.0`. 


        

<hr>



### function wfm\_reader\_get\_file\_type 

```C++
int wfm_reader_get_file_type (
    const wfm_reader_state_t * state
) 
```




<hr>



### function wfm\_reader\_get\_follow\_grace\_ms 

```C++
uint32_t wfm_reader_get_follow_grace_ms (
    const wfm_reader_state_t * state
) 
```




<hr>



### function wfm\_reader\_get\_follow\_timeout\_ms 

```C++
uint32_t wfm_reader_get_follow_timeout_ms (
    const wfm_reader_state_t * state
) 
```




<hr>



### function wfm\_reader\_get\_fs 

```C++
double wfm_reader_get_fs (
    const wfm_reader_state_t * state
) 
```




<hr>



### function wfm\_reader\_get\_fs\_source 

_Which metadata_ [_**wfm\_reader\_get\_fs**_](wfm__reader__core_8h.md#function-wfm_reader_get_fs) _read the sample rate from._
```C++
int wfm_reader_get_fs_source (
    const wfm_reader_state_t * state
) 
```



A [**wfm\_fs\_source\_t**](wfm__reader__core_8h.md#enum-wfm_fs_source_t). WFM\_FS\_NONE means nothing carried a rate — raw and CSV always, and any BLUE header whose `xdelta` is zero. 


        

<hr>



### function wfm\_reader\_get\_mode 

```C++
int wfm_reader_get_mode (
    const wfm_reader_state_t * state
) 
```




<hr>



### function wfm\_reader\_get\_num\_samples 

```C++
size_t wfm_reader_get_num_samples (
    const wfm_reader_state_t * state
) 
```




<hr>



### function wfm\_reader\_get\_sample\_type 

```C++
int wfm_reader_get_sample_type (
    const wfm_reader_state_t * state
) 
```




<hr>



### function wfm\_reader\_get\_t0 

_Capture start time in seconds since the UNIX epoch, or 0.0._ 
```C++
double wfm_reader_get_t0 (
    const wfm_reader_state_t * state
) 
```



The `t0` of `t = t0 + n/fs`, belonging to the capture rather than to whatever is reading it — hand it to `dp_sample_clock_track()` and a replayed recording's timeline lands where the samples were taken, not where they were played back.


**0.0 does not mean 1970.** Check [**wfm\_reader\_get\_t0\_source**](wfm__reader__core_8h.md#function-wfm_reader_get_t0_source) first: WFM\_T0\_NONE is "not found", which is the usual answer, including for every capture doppler itself writes. 


        

<hr>



### function wfm\_reader\_get\_t0\_source 

_Where_ [_**wfm\_reader\_get\_t0**_](wfm__reader__core_8h.md#function-wfm_reader_get_t0) _read the capture start time from._
```C++
int wfm_reader_get_t0_source (
    const wfm_reader_state_t * state
) 
```



A [**wfm\_t0\_source\_t**](wfm__reader__core_8h.md#enum-wfm_t0_source_t). WFM\_T0\_NONE is the common case and the one that matters: a zero BLUE timecode means the field was never set, not 1950-01-01, so a caller that skips this check dates every such capture to 1950. 


        

<hr>



### function wfm\_reader\_get\_trailing\_bytes 

_Payload bytes left over after the last whole sample._ 
```C++
size_t wfm_reader_get_trailing_bytes (
    const wfm_reader_state_t * state
) 
```



A capture is a whole number of samples, so this is 0 for every file whose declared sample type and mode match its content. Non-zero means one of two things, and the reader cannot tell them apart:



* the `sample_type`/`endian` hint is wrong for a headerless file type (reading a `ci16` file as `cf32` leaves a remainder unless the length happens to divide), or
* the capture is truncated — a recording that was cut mid-sample.




Either way the leftover bytes are dropped: [**wfm\_reader\_read**](wfm__reader__core_8h.md#function-wfm_reader_read) stops at the last complete sample. This exists because there is otherwise no signal at all. A wrong hint on a headerless file does not fail, it returns plausible garbage at the wrong stride, and nothing in the samples themselves says so.


Always 0 for CSV, which is delimited rather than strided. 


        

<hr>



### function wfm\_reader\_header\_field 

_The i-th decoded HCB field, or NULL if_ `i` _is out of range._
```C++
const wfm_keyword_t * wfm_reader_header_field (
    const wfm_reader_state_t * state,
    size_t i
) 
```



Every field of the 512-byte header control block is carried as a `wfm_keyword_t`, under the name the format itself uses  `data_start`, `ext_size`, `xdelta` and so on (Midas BLUE 1.1 3.1.1). Reusing the keyword struct means the header and the keywords share one tag/value codec, so a double or an ASCII field can never be turned into a Python object two different ways. 


        

<hr>



### function wfm\_reader\_header\_tag 

_The i-th HCB field's name, for the_ `.header` _dict binding._
```C++
const char * wfm_reader_header_tag (
    const wfm_reader_state_t * state,
    size_t i
) 
```




<hr>



### function wfm\_reader\_info 

_Copy the resolved capture metadata into_ `info` _._
```C++
void wfm_reader_info (
    const wfm_reader_state_t * r,
    wfm_reader_info_t * info
) 
```




<hr>



### function wfm\_reader\_keyword 

_The_ `i'th` _keyword in file order, or NULL if_`i` _is out of range._
```C++
const wfm_keyword_t * wfm_reader_keyword (
    const wfm_reader_state_t * r,
    size_t i
) 
```



The returned pointer (and its `value` buffer) is owned by the reader and is freed by [**wfm\_reader\_destroy()**](wfm__reader__core_8h.md#function-wfm_reader_destroy). 


        

<hr>



### function wfm\_reader\_keyword\_tag 

_The tag of the_ `i'th` _keyword (key\_fn for the_`.keywords` _dict)._
```C++
const char * wfm_reader_keyword_tag (
    const wfm_reader_state_t * state,
    size_t i
) 
```



jm's generated dict loop (gh-543) calls this for every index in [0, [**wfm\_reader\_num\_keywords()**](wfm__reader__core_8h.md#function-wfm_reader_num_keywords)), so `i` is always in range. The returned pointer is owned by the reader. 


        

<hr>



### function wfm\_reader\_num\_header\_fields 

_The first keyword whose tag equals_ `tag` _, or NULL if absent._
```C++
size_t wfm_reader_num_header_fields (
    const wfm_reader_state_t * state
) 
```



Tags are not required to be unique; this returns the earliest match.


Number of decoded HCB fields (0 for a non-BLUE file type). 


        

<hr>



### function wfm\_reader\_num\_keywords 

_Number of extended-header keywords recovered from the capture._ 
```C++
size_t wfm_reader_num_keywords (
    const wfm_reader_state_t * state
) 
```



BLUE only, and 0 unless the file carries an extended header. Keywords of a type this library cannot decode are skipped during the walk (BLUE §3.3.1) and are not counted; a truncated or malformed keyword region yields whatever decoded cleanly before it, since metadata must never cost you the samples. For a detached capture the keywords come from the HEADER file, not the `.det`. 


        

<hr>



### function wfm\_reader\_read 

_Read up to_ `count` _samples, returning them as_`complex64` _._
```C++
size_t wfm_reader_read (
    wfm_reader_state_t * state,
    size_t n,
    float complex * out,
    size_t max_out
) 
```



Samples come out at unit scale whatever the wire type was: a float type is reinterpreted, an integer type is divided by its full scale. Returns fewer than asked at the end of the capture, and 0 once it is exhausted, so a `while` over the result terminates. Never returns more than the file's declared payload — trailing bytes past `data_size` (an extended header, X-Midas slack) are not samples.




**Parameters:**


* `state` the reader. 
* `n` how many samples to read (`count` in the Python binding, which also accepts an optional pre-allocated `out=` array to avoid an allocation per block in a streaming loop). 
* `out` destination, at least `max_out` samples. 
* `max_out` capacity of `out`; emission stops there.


```C++
>>> import pathlib, tempfile
>>> from doppler.wfm import Composer, Reader, Segment, Writer
>>> tmp = tempfile.TemporaryDirectory()
>>> p = pathlib.Path(tmp.name) / "capture.blue"
>>> x = Composer([Segment("qpsk", sps=8, num_samples=1024)]).compose()
>>> with Writer(p, file_type="blue", sample_type="ci16",
...             fs=2.4e6, fc=1.2e9) as w:
...     _ = w.write(x)
>>> r = Reader(p)
>>> r.file_type, r.sample_type, r.endian
('blue', 'ci16', 'le')
>>> r.fs, r.fc, r.fc_source
(2400000.0, 1200000000.0, 'FREQ')
>>> total = 0
>>> while len(block := r.read(256)):
...     total += len(block)
>>> total
1024
>>> r.close()
>>> tmp.cleanup()   # directory and contents removed
```
 


        

<hr>



### function wfm\_reader\_read\_follow 

_Read from a capture that is still being written._ 
```C++
size_t wfm_reader_read_follow (
    wfm_reader_state_t * state,
    size_t n,
    float complex * out,
    size_t max_out
) 
```



Blocks until whole samples arrive. A short or empty result does not mean end-of-file the way [**wfm\_reader\_read**](wfm__reader__core_8h.md#function-wfm_reader_read)'s does  the reader waits. **Zero means the capture ENDED**, because with the default unbounded budgets the call does not come back for "not yet"; [**wfm\_reader\_get\_ending**](wfm__reader__core_8h.md#function-wfm_reader_get_ending) says which way it ended.



```C++
>>> import pathlib, tempfile
>>> import numpy as np
>>> from doppler.wfm import Reader, Writer
>>> tmp = tempfile.TemporaryDirectory()
>>> p = pathlib.Path(tmp.name) / "capture.blue"
>>> x = np.zeros(8, dtype=np.complex64)
>>> with Writer(p, file_type="blue", sample_type="ci16", fs=2.4e6) as w:
...     _ = w.write(x)
>>> r = Reader(p)
>>> total = 0
>>> while len(block := r.read_follow(4)):   # 0 only when the capture ends
...     total += len(block)
>>> total, r.ending
(8, 'eof')
>>> r.close()
>>> tmp.cleanup()
```
 


        

<hr>



### function wfm\_reader\_read\_follow\_max\_out 

```C++
size_t wfm_reader_read_follow_max_out (
    wfm_reader_state_t * state,
    size_t n
) 
```




<hr>



### function wfm\_reader\_read\_max\_out 

_Maximum samples one read(n) yields: n (fewer at EOF)._ 
```C++
size_t wfm_reader_read_max_out (
    wfm_reader_state_t * state,
    size_t n
) 
```



A reader streams, so a read of n produces at most n samples; the binding sizes its buffer to this per-call bound (gh-607) and resizes down to the actual count, never pre-allocating the whole capture. 


        

<hr>



### function wfm\_reader\_reset 

_Rewind to the first sample of the capture._ 
```C++
void wfm_reader_reset (
    wfm_reader_state_t * state
) 
```



Seeks back to where the payload starts — 512 bytes into an attached BLUE file, byte 0 of a `.det` or a raw/SigMF payload — and restores the remaining-sample count, so the capture reads again from the top. The file's metadata and decoded keywords are unaffected: they came from the header and do not change. 


        

<hr>



### function wfm\_reader\_set\_follow\_grace\_ms 

```C++
void wfm_reader_set_follow_grace_ms (
    wfm_reader_state_t * state,
    uint32_t val
) 
```




<hr>



### function wfm\_reader\_set\_follow\_timeout\_ms 

```C++
void wfm_reader_set_follow_timeout_ms (
    wfm_reader_state_t * state,
    uint32_t val
) 
```




<hr>



### function wfm\_reader\_set\_stop\_fn 

_Tell a following read how to learn that a stop was requested._ 
```C++
void wfm_reader_set_stop_fn (
    wfm_reader_state_t * state,
    int(*)(void) fn
) 
```



`read_follow()` blocks until data arrives; `fn` is what lets it stop for a reason other than the capture ending. It is INJECTED rather than hard-wired because a capture reader has no business depending on the process interrupt primitive: doing so would put `dp_interrupt.c` on the link line of every consumer of `wfm_reader_core`, and the policy is the caller's anyway. doppler passes `dp_interrupted`; a test passes its own.


NULL (the default) means the follow read never stops early  only the capture's end or a bounded budget finishes it.



```C++
wfm_reader_set_stop_fn (r, dp_interrupted);
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm_reader/wfm_reader_core.h`

