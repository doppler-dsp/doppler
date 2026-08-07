

# File wfm\_time.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_time.h**](wfm__time_8h.md)

[Go to the source code of this file](wfm__time_8h_source.md)



* `#include <stdint.h>`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  int | [**wfm\_j1950\_to\_unix\_ns**](#function-wfm_j1950_to_unix_ns) (double t\_j1950, uint64\_t \* out\_ns) <br> |
|  double | [**wfm\_j1950\_to\_unix\_sec**](#function-wfm_j1950_to_unix_sec) (double t\_j1950) <br> |
|  int | [**wfm\_timecode\_is\_set**](#function-wfm_timecode_is_set) (double timecode) <br> |
|  double | [**wfm\_unix\_to\_j1950\_sec**](#function-wfm_unix_to_j1950_sec) (double t\_unix) <br> |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**WFM\_J1950\_UNIX\_OFFSET\_SEC**](wfm__time_8h.md#define-wfm_j1950_unix_offset_sec)  `631152000.0`<br> |
| define  | [**WFM\_TIMECODE\_UNSET**](wfm__time_8h.md#define-wfm_timecode_unset)  `0.0`<br> |

## Public Static Functions Documentation




### function wfm\_j1950\_to\_unix\_ns 

```C++
static inline int wfm_j1950_to_unix_ns (
    double t_j1950,
    uint64_t * out_ns
) 
```



Convert a BLUE timecode to UNIX nanoseconds, the form `dp_sample_clock_track()` takes.


Fails rather than wrapping for the two cases a `uint64_t` cannot express: an unset timecode, and any instant before 1970. A 1950s capture is real BLUE data, so this returning nonzero is a normal answer, not an error to paper over — the caller has an epoch it simply cannot hand to the sample clock.




**Note:**

**Nanosecond precision is not available from BLUE.** The timecode is a `double` of seconds, so near the present its own resolution is about half a microsecond (`2^-52 * 2^31`). The nanosecond result is exact arithmetic on an input that is not itself nanosecond-accurate; treat the bottom three digits as padding.




**Parameters:**


* `t_j1950` Seconds since 1950-01-01T00:00:00Z. 
* `out_ns` Receives nanoseconds since the UNIX epoch; untouched on failure. 



**Returns:**

0 on success, -1 if the timecode is unset or predates 1970. 





        

<hr>



### function wfm\_j1950\_to\_unix\_sec 

```C++
static inline double wfm_j1950_to_unix_sec (
    double t_j1950
) 
```



Convert a BLUE timecode to seconds since the UNIX epoch.


No range check: the result is simply shifted, and a pre-1970 capture is a legitimate negative. Guard with wfm\_timecode\_is\_set first if a zero would be meaningless to the caller.




**Parameters:**


* `t_j1950` Seconds since 1950-01-01T00:00:00Z. 



**Returns:**

Seconds since 1970-01-01T00:00:00Z. 





        

<hr>



### function wfm\_timecode\_is\_set 

```C++
static inline int wfm_timecode_is_set (
    double timecode
) 
```



Nonzero if `timecode` carries a real capture time.




**Parameters:**


* `timecode` Raw BLUE header timecode, J1950 seconds. 




        

<hr>



### function wfm\_unix\_to\_j1950\_sec 

```C++
static inline double wfm_unix_to_j1950_sec (
    double t_unix
) 
```



Convert seconds since the UNIX epoch to a BLUE timecode.




**Parameters:**


* `t_unix` Seconds since 1970-01-01T00:00:00Z. 



**Returns:**

Seconds since 1950-01-01T00:00:00Z. 





        

<hr>
## Macro Definition Documentation





### define WFM\_J1950\_UNIX\_OFFSET\_SEC 

```C++
#define WFM_J1950_UNIX_OFFSET_SEC `631152000.0`
```



Seconds from the J1950 epoch to the UNIX epoch.


20 years, of which 1952/56/60/64/68 were leap: `20*365 + 5 = 7305` days, `7305 * 86400 = 631152000`. 


        

<hr>



### define WFM\_TIMECODE\_UNSET 

```C++
#define WFM_TIMECODE_UNSET `0.0`
```



The value a BLUE header carries when nothing set a capture time.


**A zero timecode is "unset", not 1950-01-01.** doppler's own writer leaves the field zero (`wfm_writer_core.c` zeroes bytes 54-96), and so do many producers, so converting a zero straight through would date every such capture to 1950 — confidently, and wrongly. Same ambiguity `wfm_fc_source_t` exists to resolve for centre frequency: ask whether it was set before trusting what it says. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_time.h`

