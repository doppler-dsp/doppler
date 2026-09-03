

# File dp\_isotime.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_isotime.h**](dp__isotime_8h.md)

[Go to the source code of this file](dp__isotime_8h_source.md)

_ISO 8601 UTC timestamps in both spellings — filename-safe_ **basic** _for names doppler writes,_**extended** _for the wire formats that mandate it._[More...](#detailed-description)

* `#include <stdint.h>`
* `#include <stdio.h>`
* `#include <time.h>`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  int64\_t | [**dp\_isotime\_days\_from\_civil\_**](#function-dp_isotime_days_from_civil_) (int64\_t y, unsigned m, unsigned d) <br>_Days since 1970-01-01 for a proleptic-Gregorian civil date._  |
|  int | [**dp\_isotime\_digits\_**](#function-dp_isotime_digits_) (const char \*\* p, int n, int \* out) <br>_Reads_ `n` _decimal digits, advancing_`p` _. 0 on success._ |
|  int | [**dp\_isotime\_format**](#function-dp_isotime_format) (char \* buf, size\_t cap, int64\_t sec, uint32\_t nsec, unsigned frac) <br> |
|  int | [**dp\_isotime\_format\_as**](#function-dp_isotime_format_as) (char \* buf, size\_t cap, int64\_t sec, uint32\_t nsec, unsigned frac, int style) <br> |
|  int | [**dp\_isotime\_now**](#function-dp_isotime_now) (char \* buf, size\_t cap, unsigned frac) <br> |
|  int | [**dp\_isotime\_parse**](#function-dp_isotime_parse) (const char \* s, int64\_t \* sec, uint32\_t \* nsec) <br>_Parses an ISO 8601 UTC timestamp into UNIX seconds + nanoseconds._  |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_ISOTIME\_BASIC**](dp__isotime_8h.md#define-dp_isotime_basic)  `0`<br> |
| define  | [**DP\_ISOTIME\_EXTENDED**](dp__isotime_8h.md#define-dp_isotime_extended)  `1`<br> |
| define  | [**DP\_ISOTIME\_MAX**](dp__isotime_8h.md#define-dp_isotime_max)  `32`<br> |
| define  | [**DP\_ISOTIME\_MSEC**](dp__isotime_8h.md#define-dp_isotime_msec)  `3u`<br> |
| define  | [**DP\_ISOTIME\_NSEC**](dp__isotime_8h.md#define-dp_isotime_nsec)  `9u`<br> |
| define  | [**DP\_ISOTIME\_SEC**](dp__isotime_8h.md#define-dp_isotime_sec)  `0u`<br> |
| define  | [**DP\_ISOTIME\_USEC**](dp__isotime_8h.md#define-dp_isotime_usec)  `6u`<br> |

## Detailed Description


Extended ISO 8601 (`2026-08-05T04:15:30Z`) is what a human reads, what doppler's CLI logs print, and what SigMF's `core:datetime` requires. It is also illegal in a filename on Windows and FAT, because of the colons, and awkward to quote in a shell. The basic form drops the separators:  Both come out of dp\_isotime\_format\_as, one calendar computation and one truncation rule rendered two ways, because the only thing that differs is whether `strftime` writes the separators. A second formatter would be a second place for the truncation rule below to be got wrong.


**The basic format is not defined here.** It is `just-bashit`'s `iso-8601-basic` (`src/just_bashit/datetime.sh`), whose stated contract is "path and file-name-friendly characters only". doppler formats it in C rather than shelling out — a library shipped as a wheel cannot put its file naming behind a runtime `bash` + `date`/`gdate` lookup on `PATH`, and `clock_gettime` hands back the nanoseconds the sub-second field needs anyway. Code cannot be shared between a bash library and a C one, so the agreement is held by the golden vectors in `native/tests/test_dp_isotime.c` rather than asserted.


**The fraction truncates; it never rounds.** `.999888777` at millisecond precision is `.999`, matching the shell helper. Rounding would carry `.9996` to `1.000` and step the seconds field, emitting a timestamp one second in the future that disagrees with every name written beside it. The integer division below is what makes that structural rather than a convention someone has to remember.


Header-only, like `dp_crc16.h`, so no component grows a link-line dependency for a formatter.




**Note:**

`CLOCK_REALTIME` steps under NTP. These names are unique and human-readable, **not** a chronological sort key. 





    
## Public Static Functions Documentation




### function dp\_isotime\_days\_from\_civil\_ 

_Days since 1970-01-01 for a proleptic-Gregorian civil date._ 
```C++
static inline int64_t dp_isotime_days_from_civil_ (
    int64_t y,
    unsigned m,
    unsigned d
) 
```



Howard Hinnant's `days_from_civil`, which is exact for every date and needs no timezone database. Written out rather than reached through `timegm`: that function is neither C nor POSIX, and the one thing this parser must never do is consult the ambient `TZ`. 


        

<hr>



### function dp\_isotime\_digits\_ 

_Reads_ `n` _decimal digits, advancing_`p` _. 0 on success._
```C++
static inline int dp_isotime_digits_ (
    const char ** p,
    int n,
    int * out
) 
```




<hr>



### function dp\_isotime\_format 

```C++
static inline int dp_isotime_format (
    char * buf,
    size_t cap,
    int64_t sec,
    uint32_t nsec,
    unsigned frac
) 
```



Format one instant as a filename-safe basic-format UTC timestamp.


The default spelling: this is the one that goes in a name doppler writes. dp\_isotime\_format\_as with [**DP\_ISOTIME\_EXTENDED**](dp__isotime_8h.md#define-dp_isotime_extended) is for the wire formats that mandate separators.




**Parameters:**


* `buf` Destination; receives a NUL-terminated string. 
* `cap` Size of `buf`; [**DP\_ISOTIME\_MAX**](dp__isotime_8h.md#define-dp_isotime_max) is always enough. 
* `sec` Seconds since the UNIX epoch (UTC). 
* `nsec` Nanoseconds within that second, `[0, 999999999]`. 
* `frac` Fractional digits, as dp\_isotime\_format\_as. 



**Returns:**

As dp\_isotime\_format\_as. 





        

<hr>



### function dp\_isotime\_format\_as 

```C++
static inline int dp_isotime_format_as (
    char * buf,
    size_t cap,
    int64_t sec,
    uint32_t nsec,
    unsigned frac,
    int style
) 
```



Format one instant as a UTC timestamp in either separator style.




**Parameters:**


* `buf` Destination; receives a NUL-terminated string. 
* `cap` Size of `buf`; [**DP\_ISOTIME\_MAX**](dp__isotime_8h.md#define-dp_isotime_max) is always enough. 
* `sec` Seconds since the UNIX epoch (UTC). 
* `nsec` Nanoseconds within that second, `[0, 999999999]`. 
* `frac` Fractional digits: 0, 3, 6 or 9 (the [**DP\_ISOTIME\_MSEC**](dp__isotime_8h.md#define-dp_isotime_msec) family). Any other value is rejected. 
* `style` [**DP\_ISOTIME\_BASIC**](dp__isotime_8h.md#define-dp_isotime_basic) or [**DP\_ISOTIME\_EXTENDED**](dp__isotime_8h.md#define-dp_isotime_extended). 



**Returns:**

Characters written (excluding the NUL), or -1 if `frac` is not one of the four, `style` is neither, `nsec` is out of range, `buf` is too small, or the instant is not representable as a UTC calendar time. 





        

<hr>



### function dp\_isotime\_now 

```C++
static inline int dp_isotime_now (
    char * buf,
    size_t cap,
    unsigned frac
) 
```



Format the current wall clock, the way a capture filename wants it.


Reads `CLOCK_REALTIME` directly: no `PATH` lookup, no subprocess, and the nanoseconds arrive already split from the seconds.




**Parameters:**


* `buf` Destination; receives a NUL-terminated string. 
* `cap` Size of `buf`; [**DP\_ISOTIME\_MAX**](dp__isotime_8h.md#define-dp_isotime_max) is always enough. 
* `frac` Fractional digits, as dp\_isotime\_format. **Prefer [**DP\_ISOTIME\_MSEC**](dp__isotime_8h.md#define-dp_isotime_msec) or finer when the stamp is being used to keep filenames apart** — two captures written in the same second collide at seconds resolution. 



**Returns:**

As dp\_isotime\_format, or -1 if the clock read fails. 





        

<hr>



### function dp\_isotime\_parse 

_Parses an ISO 8601 UTC timestamp into UNIX seconds + nanoseconds._ 
```C++
static inline int dp_isotime_parse (
    const char * s,
    int64_t * sec,
    uint32_t * nsec
) 
```



Accepts both spellings this file writes — extended (`2026-08-05T04:15:30.123456Z`) and basic (`20260805T041530Z`) — with an optional fraction of one to nine digits, and either `Z` or an explicit `+hh:mm` / `-hhmm` offset, which is applied.


**A timestamp with NO zone is REFUSED.** It is the one input where guessing costs hours rather than nothing: read as UTC, a local-time stamp dates a capture wrong and looks authoritative doing it. A caller that cannot parse a start time has ways to say so (the reader reports WFM\_T0\_NONE); a caller holding a wrong one does not.




**Parameters:**


* `s` The timestamp. 
* `sec` Receives UNIX seconds (may be negative, before 1970). 
* `nsec` Receives the nanoseconds part, 0 when no fraction is given. 



**Returns:**

0 on success, -1 on a NULL argument or any malformed field.



```C++
int64_t  sec;
uint32_t nsec;
dp_isotime_parse ("1970-01-01T00:00:01Z", &sec, &nsec);  // sec == 1
```
 


        

<hr>
## Macro Definition Documentation





### define DP\_ISOTIME\_BASIC 

```C++
#define DP_ISOTIME_BASIC `0`
```



Separator style: `20260805T041530Z`, safe in a filename. 


        

<hr>



### define DP\_ISOTIME\_EXTENDED 

```C++
#define DP_ISOTIME_EXTENDED `1`
```



Separator style: `2026-08-05T04:15:30Z`, what SigMF and humans want. 


        

<hr>



### define DP\_ISOTIME\_MAX 

```C++
#define DP_ISOTIME_MAX `32`
```



Bytes needed for the longest form (nanoseconds) plus the NUL. 


        

<hr>



### define DP\_ISOTIME\_MSEC 

```C++
#define DP_ISOTIME_MSEC `3u`
```




<hr>



### define DP\_ISOTIME\_NSEC 

```C++
#define DP_ISOTIME_NSEC `9u`
```




<hr>



### define DP\_ISOTIME\_SEC 

```C++
#define DP_ISOTIME_SEC `0u`
```



Fractional digits `iso-8601-basic` offers: none, `-m`, `-u`, `-n`. 


        

<hr>



### define DP\_ISOTIME\_USEC 

```C++
#define DP_ISOTIME_USEC `6u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_isotime.h`

