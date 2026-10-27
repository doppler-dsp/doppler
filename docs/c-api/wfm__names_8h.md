

# File wfm\_names.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_names.h**](wfm__names_8h.md)

[Go to the source code of this file](wfm__names_8h_source.md)

_The one C home for the waveform enum name tables._ [More...](#detailed-description)
























## Public Static Attributes

| Type | Name |
| ---: | :--- |
|  const char \*const | [**BITMOD\_NAMES**](#variable-bitmod_names)   = `{ "none", "bpsk", "qpsk" }`<br> |
|  const char \*const | [**CRC\_NAMES**](#variable-crc_names)   = `{ "none", "crc16" }`<br> |
|  const char \*const | [**DOPPLER\_LIFETIME\_NAMES**](#variable-doppler_lifetime_names)   = `{ "per\_instance", "persist" }`<br> |
|  const char \*const | [**ENDIAN\_NAMES**](#variable-endian_names)   = `{ "le", "be" }`<br> |
|  const char \*const | [**FTYPE\_NAMES**](#variable-ftype_names)   = `{ "raw", "csv", "blue", "sigmf" }`<br> |
|  const char \*const | [**GAP\_NOISE\_NAMES**](#variable-gap_noise_names)   = `{ "auto", "off" }`<br> |
|  const char \*const | [**LFSR\_NAMES**](#variable-lfsr_names)   = `{ "galois", "fibonacci" }`<br> |
|  const char \*const | [**MODE\_NAMES**](#variable-mode_names)   = `{ "auto", "fs", "ebno", "esno" }`<br> |
|  const char \*const | [**PULSE\_NAMES**](#variable-pulse_names)   = `{ "rect", "rrc" }`<br> |
|  const char \*const | [**SEED\_ADVANCE\_NAMES**](#variable-seed_advance_names)   = `{ "none", "noise", "all" }`<br> |
|  const char \*const | [**SEQ\_KIND\_NAMES**](#variable-seq_kind_names)   = `{ "literal", "pn", "gold", "dotted", "data" }`<br> |
|  const char \*const | [**STAGE\_KIND\_NAMES**](#variable-stage_kind_names)   = `{ "crc16", "rs", "randomise", "conv", "interleave" }`<br> |
|  const char \*const | [**STYPE\_NAMES**](#variable-stype_names)   = `/* multi line expression */`<br> |
|  const char \*const | [**TYPE\_NAMES**](#variable-type_names)   = `/* multi line expression */`<br> |









































## Macros

| Type | Name |
| ---: | :--- |
| define  | [**N\_STAGE\_KINDS**](wfm__names_8h.md#define-n_stage_kinds)  `5`<br> |
| define  | [**N\_STYPES**](wfm__names_8h.md#define-n_stypes)  `10`<br> |
| define  | [**N\_TYPES**](wfm__names_8h.md#define-n_types)  `9`<br> |

## Detailed Description


One definition per enum: the `[[enum]]` manifest order IS the C int, append-only. Each table carries an `SSOT:` annotation naming the `[[enum]]` in just-makeit.toml that owns its contents, the `N_*` macro that sizes it, and the C enum that pins its indices where one exists. `make lint-wfm-enum-tables` reads those annotations and holds all four to each other, so the single-source-of-truth claim is a gate rather than a comment. A table added here without an annotation FAILS that gate.


These existed as per-file copies until the wfm\_writer one silently fell behind (8 entries, no "dsss"; a `< 7` label guard that also dropped "symbols"), and wfmgen.c and wfm\_json.c kept twelve and seven more (doppler#760) — one of which, the `--data` source list, had already drifted into the reverse order. That is the rot this header ends: list order IS the enum value, so a copy that drifts maps a flag to the wrong waveform rather than failing. 


    
## Public Static Attributes Documentation




### variable BITMOD\_NAMES 

```C++
const char* const BITMOD_NAMES[];
```




<hr>



### variable CRC\_NAMES 

```C++
const char* const CRC_NAMES[];
```




<hr>



### variable DOPPLER\_LIFETIME\_NAMES 

```C++
const char* const DOPPLER_LIFETIME_NAMES[];
```




<hr>



### variable ENDIAN\_NAMES 

```C++
const char* const ENDIAN_NAMES[];
```




<hr>



### variable FTYPE\_NAMES 

```C++
const char* const FTYPE_NAMES[];
```




<hr>



### variable GAP\_NOISE\_NAMES 

```C++
const char* const GAP_NOISE_NAMES[];
```




<hr>



### variable LFSR\_NAMES 

```C++
const char* const LFSR_NAMES[];
```




<hr>



### variable MODE\_NAMES 

```C++
const char* const MODE_NAMES[];
```




<hr>



### variable PULSE\_NAMES 

```C++
const char* const PULSE_NAMES[];
```




<hr>



### variable SEED\_ADVANCE\_NAMES 

```C++
const char* const SEED_ADVANCE_NAMES[];
```




<hr>



### variable SEQ\_KIND\_NAMES 

```C++
const char* const SEQ_KIND_NAMES[];
```




<hr>



### variable STAGE\_KIND\_NAMES 

```C++
const char* const STAGE_KIND_NAMES[];
```




<hr>



### variable STYPE\_NAMES 

```C++
const char* const STYPE_NAMES[];
```




<hr>



### variable TYPE\_NAMES 

```C++
const char* const TYPE_NAMES[];
```




<hr>
## Macro Definition Documentation





### define N\_STAGE\_KINDS 

```C++
#define N_STAGE_KINDS `5`
```



Entries in STAGE\_KIND\_NAMES. 


        

<hr>



### define N\_STYPES 

```C++
#define N_STYPES `10`
```



Entries in STYPE\_NAMES. 


        

<hr>



### define N\_TYPES 

```C++
#define N_TYPES `9`
```



Entries in TYPE\_NAMES. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_names.h`

