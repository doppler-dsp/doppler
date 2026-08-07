

# File wfm\_keywords.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_keywords.h**](wfm__keywords_8h.md)

[Go to the source code of this file](wfm__keywords_8h_source.md)

_BLUE extended-header keywords — the X-Midas binary tag/value codec._ [More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**wfm\_keyword\_t**](structwfm__keyword__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**wfm\_kw\_check\_standard**](#function-wfm_kw_check_standard) (const char \* tag, char type, const void \* value, size\_t count) <br>_Advisory conformance check for the standard BLUE keywords._  |
|  int | [**wfm\_kw\_decode**](#function-wfm_kw_decode) (const uint8\_t \* p, size\_t avail, int be, [**wfm\_keyword\_t**](structwfm__keyword__t.md) \* out, size\_t \* consumed) <br>_Decode the keyword at_ `p` _, allocating its value._ |
|  size\_t | [**wfm\_kw\_elem\_size**](#function-wfm_kw_elem_size) (char type) <br>_Bytes per element for a keyword type code, or 0 if the code cannot appear in a keyword._  |
|  size\_t | [**wfm\_kw\_encode**](#function-wfm_kw_encode) (uint8\_t \* out, size\_t cap, const char \* tag, char type, const void \* value, size\_t count, int be) <br>_Encode one keyword into_ `out` _._ |
|  size\_t | [**wfm\_kw\_entry\_size**](#function-wfm_kw_entry_size) (size\_t ltag, size\_t vbytes) <br>_Total encoded size (_ `lkey` _) of a keyword, including padding._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**WFM\_KW\_MAX\_TAG**](wfm__keywords_8h.md#define-wfm_kw_max_tag)  `255`<br> |

## Detailed Description


The extended header is arbitrary metadata attached to a BLUE file as a packed sequence of tag/value pairs (Midas BLUE 1.1 §3.3.1, Table 26). One codec serves both directions: `wfm_writer` encodes with it, `wfm_reader` decodes with it, so the two can never disagree about the wire format.


Each keyword is an 8-byte header, then the value, then the tag, then padding to a multiple of eight bytes:



```C++
offset            field    bytes         note
0                 lkey     4 (int_4)     TOTAL entry length, incl. padding
4                 lext     2 (int_2)     NON-value length: 8 + ltag + pad
6                 ltag     1 (int_1)     tag character count
7                 type     1 (char)      element type (Table 6)
8                 value    lkey - lext   in the HEADER's byte order
8 + lkey - lext   tag      ltag          ASCII, no NUL
lkey - pad        pad      pad           zero fill to an 8-byte multiple
```



Note `lext` counts the 8-byte header too, so the value length is `lkey - lext` — not `lkey - 8 - ltag - pad` computed some other way. Readers advance by `lkey`, which is what lets a keyword of an unrecognised type be stepped over intact rather than aborting the parse (§3.3.1).


Values are stored in the byte order the HCB declares (`head_rep`), so the decoder swaps them to host order and the encoder swaps them back.



```C++
// Encode "F_C = 1.2345e9" into a buffer, then read it back.
double   fc = 1.2345e9;
uint8_t  buf[64];
size_t   n = wfm_kw_encode(buf, sizeof buf, "F_C", 'D', &fc, 1, 0);
wfm_keyword_t kw;
size_t   used;
wfm_kw_decode(buf, n, 0, &kw, &used);   // kw.tag = "F_C", kw.count = 1
double   got;
memcpy(&got, kw.value, sizeof got);     // 1.2345e9, host order
```
 


    
## Public Functions Documentation




### function wfm\_kw\_check\_standard 

_Advisory conformance check for the standard BLUE keywords._ 
```C++
int wfm_kw_check_standard (
    const char * tag,
    char type,
    const void * value,
    size_t count
) 
```



The keywords of Midas BLUE 1.1 3.4.2 have defined value formats  `ACQDATE` is `YY.DDD` or (Platinum-compatibility) `YYYYMMDD`, `ACQTIME` is `HH:MM:SS`, `COMMENT` and `TIMELINE` are free-form text  while `SUBREC_DEF`, `SUBREC_DESCRIP` and `T4INDEX` describe type-6000/4000 structures a type-1000 file does not have.


This is ADVISORY. 3.4.2 leaves the effect of these keywords to the consuming system, so nothing here refuses to write one; the check exists so a caller can find out before committing a capture.




**Returns:**

1 conforms, -1 does not, 0 the tag is not a standard keyword.



```C++
wfm_kw_check_standard("ACQTIME", 'A', "12:34:56", 8);  // 1
wfm_kw_check_standard("ACQTIME", 'A', "12:34", 5);     // -1
wfm_kw_check_standard("MY_TAG",  'A', "anything", 8);  // 0
```
 


        

<hr>



### function wfm\_kw\_decode 

_Decode the keyword at_ `p` _, allocating its value._
```C++
int wfm_kw_decode (
    const uint8_t * p,
    size_t avail,
    int be,
    wfm_keyword_t * out,
    size_t * consumed
) 
```





**Parameters:**


* `p` start of the entry. 
* `avail` bytes remaining in the extended header from `p`. 
* `be` the value's byte order (the HCB's `head_rep`). 
* `out` filled in on success; `out->value` is malloc'd and must be freed by the caller. 
* `consumed` always set to `lkey` when the entry header is intact, so the caller can step to the next keyword even when this one is skipped. 



**Return value:**


* `0` decoded; `out` is valid. 
* `1` well-formed but unsupported type — `out` is untouched, step by `consumed` and carry on (§3.3.1's skip-don't-abort rule). 
* `-1` malformed: the entry does not fit in `avail`, or its internal lengths are inconsistent. `consumed` is not meaningful; stop. 




        

<hr>



### function wfm\_kw\_elem\_size 

_Bytes per element for a keyword type code, or 0 if the code cannot appear in a keyword._ 
```C++
size_t wfm_kw_elem_size (
    char type
) 
```



Table 6's KW-legal set: `B` 1, `I` 2, `L` 4, `X` 8, `F` 4, `D` 8, `A` 1 (a variable-length string in keyword context — the eight-character implication of `A` does not apply here). `T` is a deprecated alias for a 32-bit integer and decodes as 4. `O` (offset byte), `P` (packed bits) and `N` (4-bit) are explicitly not permitted in keywords; `S` is reserved. 


        

<hr>



### function wfm\_kw\_encode 

_Encode one keyword into_ `out` _._
```C++
size_t wfm_kw_encode (
    uint8_t * out,
    size_t cap,
    const char * tag,
    char type,
    const void * value,
    size_t count,
    int be
) 
```





**Parameters:**


* `out` destination buffer. 
* `cap` bytes available at `out`. 
* `tag` NUL-terminated tag, 1..WFM\_KW\_MAX\_TAG characters. 
* `type` element type code (must be KW-legal, see wfm\_kw\_elem\_size). 
* `value` the elements to write, in HOST order (characters for an ASCII keyword). 
* `count` element count; must be non-zero. 
* `be` write the value big-endian (the HCB's `head_rep`). 



**Returns:**

bytes written, or 0 if the arguments are invalid or `cap` is too small (nothing is written in that case). 





        

<hr>



### function wfm\_kw\_entry\_size 

_Total encoded size (_ `lkey` _) of a keyword, including padding._
```C++
size_t wfm_kw_entry_size (
    size_t ltag,
    size_t vbytes
) 
```





**Parameters:**


* `ltag` tag length in characters (1..WFM\_KW\_MAX\_TAG). 
* `vbytes` value length in bytes. 



**Returns:**

the padded entry length, always a multiple of 8. 





        

<hr>
## Macro Definition Documentation





### define WFM\_KW\_MAX\_TAG 

```C++
#define WFM_KW_MAX_TAG `255`
```



Longest tag the format can express: `ltag` is a single byte. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_keywords.h`

