

# File dp\_event\_log\_core.h



[**FileList**](files.md) **>** [**dp\_event\_log**](dir_f94295323d6f0149be6a261903cfcf6a.md) **>** [**dp\_event\_log\_core.h**](dp__event__log__core_8h.md)

[Go to the source code of this file](dp__event__log__core_8h_source.md)

_A run's events as SigMF annotations: appended live, finalized at close._ [More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`
* `#include "clib_common.h"`
* `#include "wfm_writer/wfm_writer_core.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef [**dp\_event\_log\_t**](dp__event__log__core_8h.md#typedef-dp_event_log_t) | [**dp\_event\_log\_state\_t**](#typedef-dp_event_log_state_t)  <br>_jm's spelling of_ [_**dp\_event\_log\_t**_](dp__event__log__core_8h.md#typedef-dp_event_log_t) _._ |
| typedef struct dp\_event\_log | [**dp\_event\_log\_t**](#typedef-dp_event_log_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_event\_log\_append**](#function-dp_event_log_append) ([**dp\_event\_log\_t**](dp__event__log__core_8h.md#typedef-dp_event_log_t) \* log, uint64\_t sample\_start, const char \* label, uint64\_t sample\_count, double freq\_hz, double bandwidth\_hz) <br>_Appends one event and consumes the staged fields._  |
|  int | [**dp\_event\_log\_close**](#function-dp_event_log_close) ([**dp\_event\_log\_t**](dp__event__log__core_8h.md#typedef-dp_event_log_t) \* log) <br>_Closes the flat file, keeping the object readable._  |
|  size\_t | [**dp\_event\_log\_count**](#function-dp_event_log_count) (const [**dp\_event\_log\_t**](dp__event__log__core_8h.md#typedef-dp_event_log_t) \* log) <br>_Events appended so far (successfully written)._  |
|  int | [**dp\_event\_log\_destroy**](#function-dp_event_log_destroy) ([**dp\_event\_log\_t**](dp__event__log__core_8h.md#typedef-dp_event_log_t) \* log) <br>_Closes if still open, then frees. NULL is a no-op._  |
|  int | [**dp\_event\_log\_field**](#function-dp_event_log_field) ([**dp\_event\_log\_t**](dp__event__log__core_8h.md#typedef-dp_event_log_t) \* log, const char \* name, double value) <br>_Stages a numeric field for the next event._  |
|  int | [**dp\_event\_log\_field\_str**](#function-dp_event_log_field_str) ([**dp\_event\_log\_t**](dp__event__log__core_8h.md#typedef-dp_event_log_t) \* log, const char \* name, const char \* value) <br>_Stages a string field for the next event._  |
|  int | [**dp\_event\_log\_finalize**](#function-dp_event_log_finalize) ([**dp\_event\_log\_t**](dp__event__log__core_8h.md#typedef-dp_event_log_t) \* log, const char \* meta\_path, int sample\_type, int endian, double fs, double t0\_unix\_sec) <br>_Writes the_ `.sigmf-meta` _sidecar for this log's events._ |
|  [**dp\_event\_log\_t**](dp__event__log__core_8h.md#typedef-dp_event_log_t) \* | [**dp\_event\_log\_open**](#function-dp_event_log_open) (const char \* path, double fc) <br>_Opens (truncating) the flat event file for a run._  |
|  int | [**dp\_event\_log\_set\_dataset**](#function-dp_event_log_set_dataset) ([**dp\_event\_log\_t**](dp__event__log__core_8h.md#typedef-dp_event_log_t) \* log, const char \* name) <br>_Names the sample file these events index._  |
|  int | [**dp\_event\_log\_set\_telemetry**](#function-dp_event_log_set_telemetry) ([**dp\_event\_log\_t**](dp__event__log__core_8h.md#typedef-dp_event_log_t) \* log, const char \* path) <br>_Names the_ `dp_tlm` _record file written for the same run._ |
|  int | [**dp\_event\_log\_write\_meta**](#function-dp_event_log_write_meta) (const char \* log\_path, const char \* meta\_path, int sample\_type, int endian, double fs, double fc, double t0\_unix\_sec, const char \* dataset, const char \* telemetry) <br>_Renders any flat event file into a_ `.sigmf-meta` _sidecar._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_EVENT\_LOG\_MAX\_FIELDS**](dp__event__log__core_8h.md#define-dp_event_log_max_fields)  `16`<br> |
| define  | [**DP\_EVENT\_LOG\_NAME\_MAX**](dp__event__log__core_8h.md#define-dp_event_log_name_max)  `32`<br> |
| define  | [**DP\_EVENT\_LOG\_STR\_MAX**](dp__event__log__core_8h.md#define-dp_event_log_str_max)  `64`<br> |

## Detailed Description


A pool of receivers produces _transitions_ — seeded, tracking, degraded, lost, released, a gap in the input stream. Each one is a fact about a span of the SAMPLE stream, which is the only index the DSP below has (docs/design/async-dsss-receiver.md §2.2): a record carries a stream position, never a time. SigMF says the same thing in a standard vocabulary — an annotation is `core:sample_start` plus `core:sample_count` — so the events of a run ARE a SigMF `annotations` array, with the receiver's own fields under a `doppler:` namespace.


## One shape cannot do both jobs



A `.sigmf-meta` is ONE JSON document: `global`, `captures`, `annotations`, closed braces and all. A run that emits events for hours cannot keep rewriting it, and a run that is killed has written nothing. So this object keeps the two jobs apart:



* **During the run** each event is appended to a flat file as one JSON object on one line (JSON Lines), flushed immediately. That file is tail-able while the run is live, and a crash costs at most the event being written, never the ones before it.
* **At finalize** the lines are collected into the `annotations` array of a proper `.sigmf-meta` sidecar — through the writer's existing SigMF emitter ([**wfm\_sigmf\_meta\_json\_ex()**](wfm__writer__core_8h.md#function-wfm_sigmf_meta_json_ex), [**wfm\_writer/wfm\_writer\_core.h**](wfm__writer__core_8h.md)), never a second one. `global` and `captures` therefore come out byte-for-byte the way every other doppler sidecar spells them, including the omit-when-unknown rules that document says at length.




Finalize reads the flat file rather than a memory copy, so the sidecar for a run that died can be written afterwards by [**dp\_event\_log\_write\_meta()**](dp__event__log__core_8h.md#function-dp_event_log_write_meta) with no live object at all.



## The fields of one event



The span and the label are the annotation. Everything the holder knows about the emitter — its id, its state, its C/N0, its Doppler — is staged first with [**dp\_event\_log\_field()**](dp__event__log__core_8h.md#function-dp_event_log_field) / [**dp\_event\_log\_field\_str()**](dp__event__log__core_8h.md#function-dp_event_log_field_str) and consumed by the next [**dp\_event\_log\_append()**](dp__event__log__core_8h.md#function-dp_event_log_append), which renders each one as `doppler:<name>` and clears the table. Staging rather than a parameter list is what keeps this object ignorant of any particular receiver's record: the fields are whatever the holder has, and the table is fixed-size, so nothing allocates per event (§11.5 — the pool runs for hours).



## Frequency edges are omitted, never guessed



`core:freq_lower_edge` / `core:freq_upper_edge` are ABSOLUTE frequencies, so they need the channel's centre — which a BLUE header carries and a NATS frame does not. An event states its offset from that centre and the width it occupies; the absolute edges are emitted only when the centre is known ([**dp\_event\_log\_open()**](dp__event__log__core_8h.md#function-dp_event_log_open)'s `fc`), and the offset and width are recorded as `doppler:freq_hz` / `doppler:bandwidth_hz` either way, so nothing the caller knew is lost when the centre is not known. 



    
## Public Types Documentation




### typedef dp\_event\_log\_state\_t 

_jm's spelling of_ [_**dp\_event\_log\_t**_](dp__event__log__core_8h.md#typedef-dp_event_log_t) _._
```C++
typedef dp_event_log_t dp_event_log_state_t;
```



The same one-line bridge [**dp\_tlm\_core.h**](dp__tlm__core_8h.md) and [**dp\_tlm\_capture\_core.h**](dp__tlm__capture__core_8h.md) carry, for the same reason: jm derives an object's state struct as `<component>_state_t` with no override (just-makeit#797), and a log is an opaque handle whose C name follows this library's own convention. The alias costs nothing at runtime and goes away when jm#797 lands `state_type`. 


        

<hr>



### typedef dp\_event\_log\_t 

```C++
typedef struct dp_event_log dp_event_log_t;
```



Opaque event log; see [**dp\_event\_log\_open()**](dp__event__log__core_8h.md#function-dp_event_log_open). 


        

<hr>
## Public Functions Documentation




### function dp\_event\_log\_append 

_Appends one event and consumes the staged fields._ 
```C++
int dp_event_log_append (
    dp_event_log_t * log,
    uint64_t sample_start,
    const char * label,
    uint64_t sample_count,
    double freq_hz,
    double bandwidth_hz
) 
```



Writes one JSON object on one line and flushes it, so a reader tailing the file sees the event as it happens and a crash cannot cost an earlier one. The staged fields are cleared whether or not the write succeeded — an event that failed to reach the disk must not leak its fields into the next one.




**Parameters:**


* `log` The log. 
* `sample_start` Stream position of the event → `core:sample_start`. 
* `label` `core:label` — `"seeded"`, `"tracking"`, `"lost"`, `"gap"`, whatever the holder calls it. It comes before the span, and has no default, because an unlabelled transition is a position with nothing said about it: the label is the event. 
* `sample_count` Span in samples → `core:sample_count`. 0 means an INSTANT and the key is omitted, which is the honest spelling: a transition happens at a sample, and a written `0` would claim a measured span of nothing. 
* `freq_hz` Offset from the channel centre (Hz), positive above. 
* `bandwidth_hz` Occupied width (Hz). &lt;= 0.0 means "no band stated", and then neither the edges nor `doppler:freq_hz` appear: an event like a stream gap has no frequency, and a 0 Hz offset written for it would read as an on-centre emitter. 



**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) on NULL or a closed log, or [**DP\_ERR\_SEND**](clib__common_8h.md#define-dp_err_send) if the line could not be written.



```C++
>>> import json, os, tempfile
>>> from doppler.telemetry import EventLog
>>> d = tempfile.mkdtemp()
>>> log = EventLog(os.path.join(d, "run.events"))
>>> log.append(48000, "seeded")            # an instant
>>> log.append(96000, "gap", sample_count=1024)   # a span
>>> log.close()
>>> rows = [json.loads(x) for x in
...         open(os.path.join(d, "run.events"))]
>>> "core:sample_count" in rows[0], rows[1]["core:sample_count"]
(False, 1024)
```
 


        

<hr>



### function dp\_event\_log\_close 

_Closes the flat file, keeping the object readable._ 
```C++
int dp_event_log_close (
    dp_event_log_t * log
) 
```



Idempotent — a second call is [**DP\_OK**](clib__common_8h.md#define-dp_ok) and does nothing. Separate from the destructor because a close can FAIL (the last buffered bytes meeting a full disk) and a caller is entitled to hear about it; the destructor cannot return.




**Parameters:**


* `log` The log. NULL is [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid). 



**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), or [**DP\_ERR\_SEND**](clib__common_8h.md#define-dp_err_send) if a write or the close itself failed at any point during the run (the error is sticky: an append that could not reach the disk is reported here even if later ones succeeded). 





        

<hr>



### function dp\_event\_log\_count 

_Events appended so far (successfully written)._ 
```C++
size_t dp_event_log_count (
    const dp_event_log_t * log
) 
```




<hr>



### function dp\_event\_log\_destroy 

_Closes if still open, then frees. NULL is a no-op._ 
```C++
int dp_event_log_destroy (
    dp_event_log_t * log
) 
```



Returns what [**dp\_event\_log\_close()**](dp__event__log__core_8h.md#function-dp_event_log_close) would have: a caller must not learn more from asking than from letting the object fall out of scope, and this is the same one condition reported twice rather than two verdicts free to drift.




**Parameters:**


* `log` The log, or NULL. 



**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), or [**DP\_ERR\_SEND**](clib__common_8h.md#define-dp_err_send) if any event failed to reach the disk. NULL is [**DP\_OK**](clib__common_8h.md#define-dp_ok) — freeing nothing cannot fail. 





        

<hr>



### function dp\_event\_log\_field 

_Stages a numeric field for the next event._ 
```C++
int dp_event_log_field (
    dp_event_log_t * log,
    const char * name,
    double value
) 
```



Rendered as `doppler:<name>` in the annotation, then cleared. Integral values print as integers (`3`, not `3.0`), which is what a reader expects of an emitter id.




**Parameters:**


* `log` The log. 
* `name` Field name, without the namespace — `"emitter"`, not `"doppler:emitter"`. Up to 31 bytes. 
* `value` The value. A non-finite value is refused rather than written: JSON has no NaN, and a sidecar that cannot be parsed is worse than a missing field. 



**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), or [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) on NULL, an over-long or empty name, a non-finite value, or a full staging table (16 fields).



```C++
>>> import os, tempfile
>>> from doppler.telemetry import EventLog
>>> d = tempfile.mkdtemp()
>>> log = EventLog(os.path.join(d, "run.events"))
>>> log.field("cn0_db_hz", 47.5)
>>> log.field("emitter", 3)          # integral: renders as 3
>>> log.append(1024, "tracking")
>>> log.count
1
>>> log.close()
```
 


        

<hr>



### function dp\_event\_log\_field\_str 

_Stages a string field for the next event._ 
```C++
int dp_event_log_field_str (
    dp_event_log_t * log,
    const char * name,
    const char * value
) 
```



The string face of [**dp\_event\_log\_field()**](dp__event__log__core_8h.md#function-dp_event_log_field), for the fields that are names rather than numbers — a state, a reason, a code.




**Parameters:**


* `log` The log. 
* `name` Field name, without the namespace. 
* `value` The value; copied, up to 63 bytes. 



**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), or [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) on NULL, an over-long or empty name, an over-long value, or a full staging table.



```C++
>>> import json, os, tempfile
>>> from doppler.telemetry import EventLog
>>> d = tempfile.mkdtemp()
>>> log = EventLog(os.path.join(d, "run.events"))
>>> log.field_str("state", "tracking")
>>> log.append(1024, "seeded")
>>> log.close()
>>> line = open(os.path.join(d, "run.events")).readline()
>>> json.loads(line)["doppler:state"]
'tracking'
```
 


        

<hr>



### function dp\_event\_log\_finalize 

_Writes the_ `.sigmf-meta` _sidecar for this log's events._
```C++
int dp_event_log_finalize (
    dp_event_log_t * log,
    const char * meta_path,
    int sample_type,
    int endian,
    double fs,
    double t0_unix_sec
) 
```



Flushes the flat file and renders it through [**dp\_event\_log\_write\_meta()**](dp__event__log__core_8h.md#function-dp_event_log_write_meta), with this log's own path and `fc`. The log stays open and usable afterwards: a long run can emit a sidecar per hour and keep going.




**Parameters:**


* `log` The log. 
* `meta_path` Sidecar to write, conventionally `<base>.sigmf-meta`. 
* `sample_type` Dataset wire type (wavegen order) → `core:datatype`. 
* `endian` 0 little, 1 big. 
* `fs` Sample rate (Hz), or 0.0 to leave `core:sample_rate` unstated. The dataset and the telemetry file come from [**dp\_event\_log\_set\_dataset()**](dp__event__log__core_8h.md#function-dp_event_log_set_dataset) / \_set\_telemetry(). 
* `t0_unix_sec` Capture start in UNIX seconds, or [**WFM\_TIMECODE\_UNSET**](wfm__time_8h.md#define-wfm_timecode_unset) (0.0) → `captures[0]."core:datetime"`, omitted when unset. 



**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) on NULL arguments, [**DP\_ERR\_SEND**](clib__common_8h.md#define-dp_err_send) on a read/write failure, or [**DP\_ERR\_MEMORY**](clib__common_8h.md#define-dp_err_memory).



```C++
>>> import json, os, tempfile
>>> from doppler.telemetry import EventLog
>>> d = tempfile.mkdtemp()
>>> log = EventLog(os.path.join(d, "run.events"), fc=2.4e9)
>>> log.append(48000, "seeded", bandwidth_hz=4.0e6)
>>> meta = os.path.join(d, "run.sigmf-meta")
>>> log.finalize(meta, fs=1.0e7)
>>> log.close()
>>> json.load(open(meta))["captures"][0]["core:frequency"]
2400000000
```
 


        

<hr>



### function dp\_event\_log\_open 

_Opens (truncating) the flat event file for a run._ 
```C++
dp_event_log_t * dp_event_log_open (
    const char * path,
    double fc
) 
```



Truncates, like [**dp\_tlm\_capture\_open()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_open): a log names one run, and appending to a previous run's file would merge two runs into one sidecar with no way for a reader to tell. To finalize a file this process did not write — the log of a run that crashed — use [**dp\_event\_log\_write\_meta()**](dp__event__log__core_8h.md#function-dp_event_log_write_meta), which needs no log object.




**Parameters:**


* `path` Flat event file, truncated if it exists. One JSON object per line, flushed per event, so it can be tailed live. 
* `fc` Channel centre frequency (Hz), or WFM\_FC\_NONE (0.0) when the input does not state one — a NATS stream, typically. It decides two things together, which is why it is one argument and not two: whether an event can carry absolute `core:freq_*_edge` keys, and what `captures[0]` reports as `core:frequency`. 



**Returns:**

New log, or NULL on a NULL/empty `path`, an unopenable `path`, or allocation failure.



```C++
>>> import json, os, tempfile
>>> from doppler.telemetry import EventLog
>>> d = tempfile.mkdtemp()
>>> log = EventLog(os.path.join(d, "run.events"), fc=2.4e9)
>>> log.field("emitter", 3)
>>> log.field_str("state", "tracking")
>>> log.append(48000, "seeded", bandwidth_hz=4.0e6)
>>> log.finalize(os.path.join(d, "run.sigmf-meta"), fs=10e6)
>>> m = json.load(open(os.path.join(d, "run.sigmf-meta")))
>>> m["annotations"][0]["core:sample_start"]
48000
>>> m["annotations"][0]["doppler:state"]
'tracking'
>>> log.close()
```
 


        

<hr>



### function dp\_event\_log\_set\_dataset 

_Names the sample file these events index._ 
```C++
int dp_event_log_set_dataset (
    dp_event_log_t * log,
    const char * name
) 
```



A property of the RUN, not of a sidecar, which is why it is set once here rather than passed to every [**dp\_event\_log\_finalize()**](dp__event__log__core_8h.md#function-dp_event_log_finalize) — a long run writes a sidecar an hour and the dataset does not change between them.


Unset (the default) means there is nothing on disk to point at — a live NATS stream that nobody recorded — and the sidecar then says `core:metadata_only`, which is SigMF's own word for it rather than an absent key a reader has to interpret.




**Parameters:**


* `log` The log. 
* `name` Dataset basename, copied. NULL or empty restores "none". 



**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), or [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) on a NULL log.



```C++
>>> import json, os, tempfile
>>> from doppler.telemetry import EventLog
>>> d = tempfile.mkdtemp()
>>> log = EventLog(os.path.join(d, "run.events"))
>>> log.set_dataset("capture.sigmf-data")
>>> log.append(0, "seeded")
>>> meta = os.path.join(d, "run.sigmf-meta")
>>> log.finalize(meta)
>>> log.close()
>>> json.load(open(meta))["global"]["core:dataset"]
'capture.sigmf-data'
```
 


        

<hr>



### function dp\_event\_log\_set\_telemetry 

_Names the_ `dp_tlm` _record file written for the same run._
```C++
int dp_event_log_set_telemetry (
    dp_event_log_t * log,
    const char * path
) 
```



Carried with its record dtype under a `doppler:telemetry` global, so one sidecar indexes all three products of a run — the dataset, the events, the telemetry — each in the format that suits its rate: annotations for transitions at a handful a minute, a flat record file for a time series at thousands a second (§8.1).




**Parameters:**


* `log` The log. 
* `path` Telemetry record file, copied. NULL or empty restores "none". 



**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), or [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) on a NULL log.



```C++
>>> import json, os, tempfile
>>> from doppler.telemetry import EventLog
>>> d = tempfile.mkdtemp()
>>> log = EventLog(os.path.join(d, "run.events"))
>>> log.set_telemetry("run.tlm")
>>> log.append(0, "seeded")
>>> meta = os.path.join(d, "run.sigmf-meta")
>>> log.finalize(meta)
>>> log.close()
>>> json.load(open(meta))["global"]["doppler:telemetry"]["path"]
'run.tlm'
```
 


        

<hr>



### function dp\_event\_log\_write\_meta 

_Renders any flat event file into a_ `.sigmf-meta` _sidecar._
```C++
int dp_event_log_write_meta (
    const char * log_path,
    const char * meta_path,
    int sample_type,
    int endian,
    double fs,
    double fc,
    double t0_unix_sec,
    const char * dataset,
    const char * telemetry
) 
```



The finalize step with no live log, which is what makes the flat file worth having: the sidecar for a run that was killed is written afterwards, from the file on disk, by a different process if need be. [**dp\_event\_log\_finalize()**](dp__event__log__core_8h.md#function-dp_event_log_finalize) is this function with the log's own path and centre frequency.


A line the parser rejects is SKIPPED, not fatal: a killed run leaves a truncated last line, and refusing to describe the hours before it would lose the whole run to its final millisecond.




**Parameters:**


* `log_path` Flat event file to read. 
* `meta_path` Sidecar to write. 
* `sample_type` Dataset wire type (wavegen order) → `core:datatype`. 
* `endian` 0 little, 1 big. 
* `fs` Sample rate (Hz), 0.0 leaves it unstated. 
* `fc` Channel centre (Hz), 0.0 leaves `core:frequency` unstated. The edges inside the annotations were decided when they were appended and are copied through as they stand. 
* `t0_unix_sec` Capture start in UNIX seconds, or 0.0. 
* `dataset` Dataset basename, or NULL for `core:metadata_only`. 
* `telemetry` `dp_tlm` record file for the same run, or NULL. 



**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) on NULL arguments, [**DP\_ERR\_SEND**](clib__common_8h.md#define-dp_err_send) if the log could not be read or the sidecar written, or [**DP\_ERR\_MEMORY**](clib__common_8h.md#define-dp_err_memory). 





        

<hr>
## Macro Definition Documentation





### define DP\_EVENT\_LOG\_MAX\_FIELDS 

```C++
#define DP_EVENT_LOG_MAX_FIELDS `16`
```



Fields stageable for one event. Staging a 17th fails rather than drops. 


        

<hr>



### define DP\_EVENT\_LOG\_NAME\_MAX 

```C++
#define DP_EVENT_LOG_NAME_MAX `32`
```



Maximum staged field name length, including the NUL terminator. 


        

<hr>



### define DP\_EVENT\_LOG\_STR\_MAX 

```C++
#define DP_EVENT_LOG_STR_MAX `64`
```



Maximum staged string VALUE length, including the NUL terminator. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_event_log/dp_event_log_core.h`

