

# Group wire



[**Modules**](modules.md) **>** [**wire**](group__wire.md)





































































## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_FLAG\_CHUNKED**](group__wire.md#define-dp_flag_chunked)  `0x0001u`<br> |
| define  | [**DP\_FLAG\_KNOWN**](group__wire.md#define-dp_flag_known)  `([**DP\_FLAG\_CHUNKED**](group__wire.md#define-dp_flag_chunked))`<br> |
| define  | [**DP\_REP\_BE**](group__wire.md#define-dp_rep_be)  `"IEEE"`<br> |
| define  | [**DP\_REP\_LE**](group__wire.md#define-dp_rep_le)  `"EEEI"`<br> |
| define  | [**DP\_STREAM\_MAGIC**](group__wire.md#define-dp_stream_magic)  `0x4D41455254535044ULL /\* "DPSTREAM" little-endian \*/`<br> |
| define  | [**DP\_WIRE\_VERSION**](group__wire.md#define-dp_wire_version)  `2u`<br> |

## Macro Definition Documentation





### define DP\_FLAG\_CHUNKED 

```
#define DP_FLAG_CHUNKED `0x0001u`
```



Frame flags. A 24-byte chunk block follows the header; see §4 of \ docs/design/streaming.md. 


        

<hr>



### define DP\_FLAG\_KNOWN 

```
#define DP_FLAG_KNOWN `( DP_FLAG_CHUNKED )`
```



Every flag bit this build understands.


A receiver REJECTS a frame carrying a bit outside this mask rather than guessing. That is what makes a later additive change safe: a frame with a new optional block cannot be mistaken for one without it, because the block changes where the payload starts. 


        

<hr>



### define DP\_REP\_BE 

```
#define DP_REP_BE `"IEEE"`
```



Byte-order tag: IEEE big-endian. 


        

<hr>



### define DP\_REP\_LE 

```
#define DP_REP_LE `"EEEI"`
```



Byte-order tag, BLUE's own token (HCB `data_rep`): IEEE little-endian. Written as four ASCII characters so a hex dump says which order the numbers are in without decoding anything else. 


        

<hr>



### define DP\_STREAM\_MAGIC 

```
#define DP_STREAM_MAGIC `0x4D41455254535044ULL /* "DPSTREAM" little-endian */`
```



Magic: the eight ASCII bytes `DPSTREAM`, as one `uint64_t`.


An integer rather than an eight-character array on purpose: the header is written in host byte order with no conversion, so a peer of the opposite endianness reads this field byte-swapped and it no longer matches. The magic is therefore the endianness probe as well as the format tag, and costs nothing to be both. (An array of characters would read identically either way and detect nothing.) 


        

<hr>



### define DP\_WIRE\_VERSION 

```
#define DP_WIRE_VERSION `2u`
```



Wire revision. A receiver rejects a frame whose major differs: within a major, changes are additive and announced by an unrecognised flag bit, which is also rejected (see [**DP\_FLAG\_KNOWN**](group__wire.md#define-dp_flag_known)). 


        

<hr>

------------------------------


