

# File dp\_format.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_format.h**](dp__format_8h.md)

[Go to the source code of this file](dp__format_8h_source.md)

_Complex sample formats, named by their BLUE/Platinum codes._ [More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**dp\_sample\_type\_t**](#enum-dp_sample_type_t)  <br>_A complex sample format. The value IS the BLUE code._  |






















## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_format\_chars**](#function-dp_format_chars) ([**dp\_sample\_type\_t**](dp__format_8h.md#enum-dp_sample_type_t) type, char out) <br>_The two characters BLUE writes for_ `type` _(HCB bytes 52/53)._ |
|  int | [**dp\_format\_is\_valid**](#function-dp_format_is_valid) ([**dp\_sample\_type\_t**](dp__format_8h.md#enum-dp_sample_type_t) type) <br>_Non-zero when_ `type` _is a format doppler can send and decode._ |
|  size\_t | [**dp\_format\_size**](#function-dp_format_size) ([**dp\_sample\_type\_t**](dp__format_8h.md#enum-dp_sample_type_t) type) <br>_Bytes occupied by one complex sample of_ `type` _, or 0 if the code is not one doppler sends._ |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_FMT**](dp__format_8h.md#define-dp_fmt) (mode, type) `/* multi line expression */`<br>_Pack a BLUE two-character format code into a_ `uint16_t` _._ |

## Detailed Description


One vocabulary for the five interleaved-I/Q encodings doppler moves, used by both containers that carry them: the streaming wire header (`stream/stream.h`) and the BLUE file writer (`wfm_writer`). It lives in neither of those because it belongs to neither — the codes are Midas BLUE 1.1 Table 6's, and a transport that had to be linked in order to name a file's sample format would be the wrong dependency in the wrong direction.


There used to be three enumerations of these five types: the stream's `dp_sample_type_t`, `wfm_writer`'s `stype` in "wavegen order", and `wfm_sink.c`'s `WT_*`, plus a `FMTCH[]` table mapping one of them to BLUE and a `BPS[]` table repeating the sizes. They agreed on nothing and were reconciled by hand at every boundary. Naming a format by the code the file format already defines leaves one vocabulary and nothing to translate.


Everything here is a `static inline` over a switch, so a consumer needs the header and no link edge.



```C++
// The wire code and the file's HCB bytes 52/53 are the same two chars.
char code[2];
dp_format_chars (CF64, code);      // code = { 'C', 'D' }
size_t n = dp_format_size (CF64);  // 16 bytes per complex sample
```
 


    
## Public Types Documentation




### enum dp\_sample\_type\_t 

_A complex sample format. The value IS the BLUE code._ 
```C++
enum dp_sample_type_t {
    CI8 = DP_FMT ('C', 'B'),
    CI16 = DP_FMT ('C', 'I'),
    CI32 = DP_FMT ('C', 'L'),
    CF32 = DP_FMT ('C', 'F'),
    CF64 = DP_FMT ('C', 'D')
};
```



There is no code for a quad or extended float because BLUE defines none — which is the format agreeing with why doppler retired CF128: its representation differs between x86-64 and aarch64 at identical size, so a frame crossed an architecture boundary and decoded to nonsense. 


        

<hr>
## Public Static Functions Documentation




### function dp\_format\_chars 

_The two characters BLUE writes for_ `type` _(HCB bytes 52/53)._
```C++
static inline void dp_format_chars (
    dp_sample_type_t type,
    char out
) 
```



Unpacks rather than translates — the enum value IS the code — so a wire header and a file header cannot disagree about what they carry.




**Parameters:**


* `type` Sample format. 
* `out` Two characters, mode then element type. Not NUL-terminated. 




        

<hr>



### function dp\_format\_is\_valid 

_Non-zero when_ `type` _is a format doppler can send and decode._
```C++
static inline int dp_format_is_valid (
    dp_sample_type_t type
) 
```



Derived from dp\_format\_size(): a format with no size is not a format. Ask this rather than range-testing — the codes are two packed characters, so "between the first and the last" means nothing. 


        

<hr>



### function dp\_format\_size 

_Bytes occupied by one complex sample of_ `type` _, or 0 if the code is not one doppler sends._
```C++
static inline size_t dp_format_size (
    dp_sample_type_t type
) 
```



This switch is the single table: validity, element size and the wire layout all derive from it, so a format added here needs no second edit and a code that is not here is not a doppler format anywhere.




**Parameters:**


* `type` Sample format. 



**Returns:**

Bytes per complex sample, or 0. 





        

<hr>
## Macro Definition Documentation





### define DP\_FMT 

_Pack a BLUE two-character format code into a_ `uint16_t` _._
```C++
#define DP_FMT (
    mode,
    type
) `/* multi line expression */`
```



Mode in the low byte, element type in the high byte, so a little-endian hex dump of the wire field reads as the two characters in order. doppler uses mode `'C'` — complex, two components per element. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_format.h`

