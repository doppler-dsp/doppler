

# File wfm\_frame.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_frame.h**](wfm__frame_8h.md)

[Go to the source code of this file](wfm__frame_8h_source.md)

_A frame's BIT layout, described once and read from both ends._ [More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**wfm\_field\_t**](structwfm__field__t.md) <br>_One field of a frame — a run of bits that appears on the wire._  |
| struct | [**wfm\_frame\_desc\_layout\_t**](structwfm__frame__desc__layout__t.md) <br>_Where every field and every stage landed._  |
| struct | [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) <br>_A frame as a description: what is on the wire, and what covers it._  |
| struct | [**wfm\_frame\_ops\_t**](structwfm__frame__ops__t.md) <br>_The kernels an assembly runs, and whatever state they carry._  |
| struct | [**wfm\_frame\_rx\_t**](structwfm__frame__rx__t.md) <br>_What_ [_**dp\_wfm\_frame\_check**_](wfm__frame_8h.md#function-dp_wfm_frame_check) _found, stage by stage._ |
| struct | [**wfm\_frame\_span\_t**](structwfm__frame__span__t.md) <br>_A run of bits inside the assembled frame,_ `[first, first + n)` _._ |
| struct | [**wfm\_frame\_stage\_rx\_t**](structwfm__frame__stage__rx__t.md) <br>_What undoing one stage found._  |
| struct | [**wfm\_seq\_t**](structwfm__seq__t.md) <br>_A run of bits, however it is produced._  |
| struct | [**wfm\_stage\_op\_t**](structwfm__stage__op__t.md) <br>_How one kind of stage actually transforms bits._  |
| struct | [**wfm\_stage\_t**](structwfm__stage__t.md) <br>_One transform, and — the whole point — the fields it covers._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**wfm\_seq\_kind\_t**](#enum-wfm_seq_kind_t)  <br>_Where a run of bits comes from._  |
| enum  | [**wfm\_stage\_kind\_t**](#enum-wfm_stage_kind_t)  <br>_Stage kinds doppler itself names._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_wfm\_dsss\_desc\_chips**](#function-dp_wfm_dsss_desc_chips) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const [**wfm\_frame\_ops\_t**](structwfm__frame__ops__t.md) \* ops, const uint8\_t \* acq\_code, size\_t acq\_len, size\_t acq\_reps, const uint8\_t \* data\_code, size\_t data\_len, uint8\_t \* out, size\_t max\_out) <br>_Build a two-code DSSS burst from a description: assemble, spread._  |
|  size\_t | [**dp\_wfm\_dsss\_desc\_nchips**](#function-dp_wfm_dsss_desc_nchips) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, size\_t acq\_len, size\_t acq\_reps, size\_t data\_len) <br>_Chip count of a DSSS burst built from a description._  |
|  size\_t | [**dp\_wfm\_field\_bits**](#function-dp_wfm_field_bits) (const char \* spec, uint8\_t \* out, size\_t max\_out, const char \*\* why) <br>_A Field's bits, straight from its text form. Returns the count._  |
|  size\_t | [**dp\_wfm\_field\_format**](#function-dp_wfm_field_format) (const [**wfm\_field\_t**](structwfm__field__t.md) \* field, char \* buf, size\_t cap) <br>_Write a field's canonical text form. Returns its length._  |
|  int | [**dp\_wfm\_field\_parse**](#function-dp_wfm_field_parse) (const char \* spec, [**wfm\_field\_t**](structwfm__field__t.md) \* field, uint8\_t \*\* owned, const char \*\* why) <br>_Read one Field from its text form._  |
|  size\_t | [**dp\_wfm\_field\_render**](#function-dp_wfm_field_render) (const [**wfm\_field\_t**](structwfm__field__t.md) \* f, uint8\_t \* out, size\_t max\_out) <br>_Write a field's bits: its sequence once, then repeated. Returns the count._  |
|  int | [**dp\_wfm\_frame\_add\_derived**](#function-dp_wfm_frame_add_derived) ([**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const char \* name, size\_t bits) <br>_Append a named DERIVED field — one a stage will fill. Returns its index, or -1._  |
|  int | [**dp\_wfm\_frame\_add\_field**](#function-dp_wfm_frame_add_field) ([**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const char \* name, const [**wfm\_seq\_t**](structwfm__seq__t.md) \* seq, size\_t reps) <br>_Append a named field. Returns its index, or -1._  |
|  int | [**dp\_wfm\_frame\_add\_stage**](#function-dp_wfm_frame_add_stage) ([**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, uint32\_t kind, const char \* first, const char \* last) <br>_Append a stage covering_ `[first .. last]` _BY NAME. Returns its index, or -1._ |
|  int | [**dp\_wfm\_frame\_add\_stage\_at**](#function-dp_wfm_frame_add_stage_at) ([**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, uint32\_t kind, unsigned first, unsigned n\_fields) <br>_Append a stage covering_ `n_fields` _fields from index_`first` _. Returns its index, or -1._ |
|  size\_t | [**dp\_wfm\_frame\_assemble**](#function-dp_wfm_frame_assemble) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const [**wfm\_frame\_ops\_t**](structwfm__frame__ops__t.md) \* ops, uint8\_t \* out, size\_t max\_out) <br>_Materialise a description: run every field, then every stage._  |
|  int | [**dp\_wfm\_frame\_check**](#function-dp_wfm_frame_check) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const [**wfm\_frame\_ops\_t**](structwfm__frame__ops__t.md) \* ops, uint8\_t \* bits, [**wfm\_frame\_rx\_t**](structwfm__frame__rx__t.md) \* rx) <br>_Undo a description's stages over a received frame, and report._  |
|  int | [**dp\_wfm\_frame\_desc\_crc\_ok**](#function-dp_wfm_frame_desc_crc_ok) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const uint8\_t \* rx\_bits) <br>_Check a received frame's CRC against any description that has one._  |
|  int | [**dp\_wfm\_frame\_desc\_layout**](#function-dp_wfm_frame_desc_layout) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, [**wfm\_frame\_desc\_layout\_t**](structwfm__frame__desc__layout__t.md) \* out) <br>_Derive every field offset, every stage span and both lengths._  |
|  int | [**dp\_wfm\_frame\_field\_index**](#function-dp_wfm_frame_field_index) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const char \* name) <br>_Index of the field called_ `name` _, or -1._ |
|  int | [**dp\_wfm\_frame\_fixed**](#function-dp_wfm_frame_fixed) ([**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const [**wfm\_seq\_t**](structwfm__seq__t.md) \* preamble, size\_t reps, const [**wfm\_seq\_t**](structwfm__seq__t.md) \* sync, const [**wfm\_seq\_t**](structwfm__seq__t.md) \* payload, int crc) <br>_Describe the common frame:_ `[preamble x reps | sync | payload | crc]` _._ |
|  int | [**dp\_wfm\_parse\_u64**](#function-dp_wfm_parse_u64) (const char \* p, size\_t n, uint64\_t \* v) <br>_Read an unsigned integer the way a Field's numbers are read._  |
|  size\_t | [**dp\_wfm\_seq\_bits**](#function-dp_wfm_seq_bits) (const [**wfm\_seq\_t**](structwfm__seq__t.md) \* s, uint8\_t \* out, size\_t max\_out) <br>_Write_ `s's` _bits, whatever produces them. Returns the count._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**WFM\_FIELD\_MAX\_BITS**](wfm__frame_8h.md#define-wfm_field_max_bits)  `261120`<br>_Bits one Field may hold,_ `LEN * REPS` _; the grammar refuses more._ |
| define  | [**WFM\_FRAME\_CRC\_BITS**](wfm__frame_8h.md#define-wfm_frame_crc_bits)  `16u`<br>_Bits of CRC-16-CCITT, when a frame carries one._  |
| define  | [**WFM\_FRAME\_MAX\_FIELDS**](wfm__frame_8h.md#define-wfm_frame_max_fields)  `16`<br>_Fields one description may carry._  |
| define  | [**WFM\_FRAME\_MAX\_STAGES**](wfm__frame_8h.md#define-wfm_frame_max_stages)  `8`<br>_Stages one description may carry._  |
| define  | [**WFM\_FRAME\_NAME\_MAX**](wfm__frame_8h.md#define-wfm_frame_name_max)  `16`<br>_Bytes a field's name may use, NUL included._  |

## Detailed Description


One struct saying what a frame contains, used by the generator that builds it and by the measurer that scores it. The DSSS assembler already stated the reason it must be shared — it is "assembled in one place so TX and RX can
never drift" — and this generalises that from one waveform to all of them: `dp_wfm_dsss_desc_chips()` assembles a description and spreads it, rather than carrying a second copy of the layout.


### It describes BITS



Not chips, not samples, not levels. Spreading, pulse shaping, oversampling, carrier and SNR layer above and stay `wfm_synth`'s job. That boundary is what lets one descriptor serve an unspread BPSK stream and a two-code DSSS burst alike.



### Every field is a sequence, and the generators already exist



The preamble, the sync word and the payload are all `wfm_seq_t`, so "a Gold
sync" is a configuration rather than a feature, and `dp_pn_create()` / `dp_gold_create()` stay the only implementations of those sequences.


**The generated kinds are the ones that matter.** A literal array is what a caller with real data has; a PN or Gold descriptor is a handful of numbers a receiver can REGENERATE, which is what makes a long-record BER practical — truth for a million-symbol run without a million-symbol array, and a capture reproducible from its metadata alone.



### The CRC is the one we already have



`dp_crc16_ccitt()`, over the payload only, MSB-first — the `WFM_STAGE_CRC16` stage, and the `crc` flag of `dp_wfm_frame_fixed()`. A second CRC would be a wire-format decision and nothing is asking for one.




**See also:** docs/design/rx-test.md section 7 




    
## Public Types Documentation




### enum wfm\_seq\_kind\_t 

_Where a run of bits comes from._ 
```C++
enum wfm_seq_kind_t {
    WFM_SEQ_LITERAL = 0,
    WFM_SEQ_PN = 1,
    WFM_SEQ_GOLD = 2,
    WFM_SEQ_DOTTED = 3
};
```




<hr>



### enum wfm\_stage\_kind\_t 

_Stage kinds doppler itself names._ 
```C++
enum wfm_stage_kind_t {
    WFM_STAGE_CRC16 = 0,
    WFM_STAGE_RS = 1,
    WFM_STAGE_RANDOMISE = 2,
    WFM_STAGE_CONV = 3,
    WFM_STAGE_INTERLEAVE = 4,
    WFM_STAGE_USER = 0x1000u
};
```



**A stage's kind is an open `uint32_t`, not this enumeration.** These are the values doppler has allocated; a caller allocates its own from WFM\_STAGE\_USER upward and supplies the kernel through [**wfm\_frame\_ops\_t**](structwfm__frame__ops__t.md). That is the difference between a description a caller can extend and a fixed menu — and a closed enum here would make "a mission that is not CCSDS" a pull request against this header rather than a configuration, which is the opposite of the point.


The value is only ever a lookup key. Nothing in this component switches on it exhaustively, so an unrecognised kind is not undefined behaviour: it finds no kernel and the assembly is REFUSED, which is the honest answer and is the same one a declared-but-unsupplied stage already gets. 


        

<hr>
## Public Functions Documentation




### function dp\_wfm\_dsss\_desc\_chips 

_Build a two-code DSSS burst from a description: assemble, spread._ 
```C++
size_t dp_wfm_dsss_desc_chips (
    const wfm_frame_desc_t * d,
    const wfm_frame_ops_t * ops,
    const uint8_t * acq_code,
    size_t acq_len,
    size_t acq_reps,
    const uint8_t * data_code,
    size_t data_len,
    uint8_t * out,
    size_t max_out
) 
```



 The only spreader: a common DSSS burst is this with a description from [**dp\_wfm\_frame\_fixed**](wfm__frame_8h.md#function-dp_wfm_frame_fixed), and a coded one is this with a coded one.


\*\*The preamble is not a field of `d`, by design.\*\* It is unmodulated, unspread and uncoded, because it is the coherent pull-in target a receiver correlates raw chips against; a stage covering "the whole
frame" therefore covers everything that is spread and not the preamble. That is the one place a DSSS burst's description differs from any other source's, and it is why this function takes the preamble separately.


A stage whose kernel `ops` does not supply makes the assembly fail and the burst is REFUSED — never transmitted with the stage quietly missing, which would produce a waveform that decodes against itself and syncs to nothing.




**Parameters:**


* `d` description of the spread frame. 
* `ops` kernels beyond the built-in CRC; may be NULL. 
* `acq_code` preamble chips (0/1); NULL when there is no preamble. 
* `acq_len` preamble length in chips. 
* `acq_reps` preamble repetitions. 
* `data_code` spreading code (0/1), length `data_len`. 
* `data_len` chips per frame bit. 
* `out` receives the burst, one chip per byte. 
* `max_out` capacity of `out`; must be at least [**dp\_wfm\_dsss\_desc\_nchips**](wfm__frame_8h.md#function-dp_wfm_dsss_desc_nchips). 



**Returns:**

chips written, or 0 if the geometry is refused, a stage has no kernel, or `max_out` is too small. 





        

<hr>



### function dp\_wfm\_dsss\_desc\_nchips 

_Chip count of a DSSS burst built from a description._ 
```C++
size_t dp_wfm_dsss_desc_nchips (
    const wfm_frame_desc_t * d,
    size_t acq_len,
    size_t acq_reps,
    size_t data_len
) 
```



`acq_len * acq_reps + out_bits * data_len`, where `out_bits` is what leaves the description's last emitting stage — so an inner code that doubles the frame doubles the burst, and nothing here restates the arithmetic the layout already did.




**Parameters:**


* `d` the description of everything that gets SPREAD. 
* `acq_len` preamble code length in chips (0 = no preamble). 
* `acq_reps` preamble repetitions. 
* `data_len` spreading-code length, i.e. chips per frame bit. 



**Returns:**

burst chips, or 0 if the description is refused, or it has bits and `data_len` is 0, or there is nothing to transmit. 





        

<hr>



### function dp\_wfm\_field\_bits 

_A Field's bits, straight from its text form. Returns the count._ 
```C++
size_t dp_wfm_field_bits (
    const char * spec,
    uint8_t * out,
    size_t max_out,
    const char ** why
) 
```



[**dp\_wfm\_field\_parse**](wfm__frame_8h.md#function-dp_wfm_field_parse), then [**dp\_wfm\_field\_render**](wfm__frame_8h.md#function-dp_wfm_field_render), with the literal's storage released before returning — the one door from the text a person writes to the bits every object takes. With `out` NULL it only SIZES: it returns how many bits the field is, which is what a caller allocates before the second call.




**Parameters:**


* `spec` NUL-terminated text, as [**dp\_wfm\_field\_parse**](wfm__frame_8h.md#function-dp_wfm_field_parse) reads it. 
* `out` receives the bits, one per byte; NULL to size. 
* `max_out` capacity of `out` in bits; ignored when `out` is NULL. 
* `why` optional; as [**dp\_wfm\_field\_parse**](wfm__frame_8h.md#function-dp_wfm_field_parse), plus the render's own refusals (a generator that rejects its parameters, a buffer too small). 



**Returns:**

the field's length in bits (repetitions included), or 0 on a refusal. A Field is never empty, so 0 is unambiguous. **A sizing call checks the grammar only**: a generator that rejects its own parameters (a Gold pair that is not a preferred pair) is found by the call that renders, which then returns 0.



```C++
uint8_t b[124];
size_t  n = dp_wfm_field_bits ("pn:31:5*4", NULL, 0, NULL);   // 124
dp_wfm_field_bits ("pn:31:5*4", b, n, NULL);
```
 


        

<hr>



### function dp\_wfm\_field\_format 

_Write a field's canonical text form. Returns its length._ 
```C++
size_t dp_wfm_field_format (
    const wfm_field_t * field,
    char * buf,
    size_t cap
) 
```



The ONLY writer of the grammar, and the inverse of [**dp\_wfm\_field\_parse**](wfm__frame_8h.md#function-dp_wfm_field_parse). Parsing what this writes gives back the same field, for every field it accepts. The form is canonical, so two equal fields print identically:



* a literal prints as `0x…` hex when its length is a multiple of 4, and as `0`/`1` digits otherwise;
* a generator prints only what differs from its defaults — a zero seed or poly is omitted unless a later number needs its position, and `galois` is never written;
* `*REPS` appears only when `reps > 1`.






**Parameters:**


* `field` the field. A derived field has no text form and is refused. 
* `buf` receives the text and a NUL; may be NULL to size it. 
* `cap` capacity of `buf` in bytes, NUL included. 



**Returns:**

the length WITHOUT the NUL, whether or not it fit — so a `buf == NULL` call sizes the buffer — or 0 for a field with no text form. Nothing is written unless all of it fits.



```C++
char s[64];
dp_wfm_field_format (&f, s, sizeof s);   // "pn:31:5*4"
```
 


        

<hr>



### function dp\_wfm\_field\_parse 

_Read one Field from its text form._ 
```C++
int dp_wfm_field_parse (
    const char * spec,
    wfm_field_t * field,
    uint8_t ** owned,
    const char ** why
) 
```



The ONLY reader of the grammar every text face shares — the CLI, a JSON scene and `field_bits()` in Python all call this, and none restates it (docs/design/frame-description.md §F.1):



```C++
field  := seq [ "*" REPS ]                       REPS >= 1
seq    := bin | hex | pn | gold | dotted
bin    := [01]+
hex    := "0x" [0-9A-Fa-f]+                       4 bits a digit, MSB first
pn     := "pn:" LEN ":" REG [":" SEED [":" POLY]] [":" LFSR]
gold   := "gold:" LEN ":" REG ":" TA ":" SA ":" TB ":" SB
dotted := "dotted:" LEN
LFSR   := "galois" | "fibonacci"                  default galois
```



A number is decimal, or hex after `0x`, and must be consumed WHOLE: `12abc`, `-1`, `5` and an empty field (`pn::10`) are refused, not read as far as they go. A leading `0` is decimal, never octal. `LEN` is the output length and must be &gt; 0; `REG` is the register width, 1..64. A `pn` with no `POLY` means the maximal-length polynomial for its register, so a register that has none (width 1) is refused unless a `POLY` is given. `LEN * REPS` is at most [**WFM\_FIELD\_MAX\_BITS**](wfm__frame_8h.md#define-wfm_field_max_bits), and the refusal names that number: a longer run is a stream, not a Field. A `0`/`1` string with any other character in it is refused rather than filtered, because a typo that quietly shortens a sync word syncs to nothing and fails nowhere.


`data:LEN` is part of the grammar but not yet of this parser: it names a payload drawn from a data source, which a `wfm_seq_t` cannot carry until that source exists. It is refused, by name.




**Parameters:**


* `spec` NUL-terminated text. 
* `field` receives the field: `name` empty, `derived_by` 0, `reps` as written (1 when absent). Untouched on refusal. 
* `owned` receives the allocated bit array of a LITERAL field, which `field->seq.bits` points into and the caller must `free()`; NULL for a generated kind. Untouched on refusal. 
* `why` optional; receives a STATIC sentence naming the cause of a refusal, NULL on success. Never freed. 



**Returns:**

[**DP\_OK**](clib__common_8h.md#define-dp_ok), or [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) for text outside the grammar. A literal's storage is at most four times the spec's length, so it is allocated with the abort-on-OOM helper.



```C++
wfm_field_t f;
uint8_t    *owned;
const char *why;
if (dp_wfm_field_parse ("pn:31:5*4", &f, &owned, &why) != DP_OK)
  fprintf (stderr, "error: %s\n", why);
// f.seq.kind == WFM_SEQ_PN, f.seq.len == 31, f.seq.reg_bits == 5,
// f.reps == 4, owned == NULL
```
 


        

<hr>



### function dp\_wfm\_field\_render 

_Write a field's bits: its sequence once, then repeated. Returns the count._ 
```C++
size_t dp_wfm_field_render (
    const wfm_field_t * f,
    uint8_t * out,
    size_t max_out
) 
```



The one place a field's REPETITION is expanded, used by [**dp\_wfm\_frame\_assemble**](wfm__frame_8h.md#function-dp_wfm_frame_assemble) for every caller-supplied field and by [**dp\_wfm\_field\_bits**](wfm__frame_8h.md#function-dp_wfm_field_bits) for a field given as text. A repetition is the same bits again, never fresh ones: a generated field that drew new bits per repetition would not be a periodic acquisition target, and coherent integration across the repetitions would be void.




**Parameters:**


* `f` the field; `reps == 0` means one. A DERIVED field has no source of its own and is refused. 
* `out` receives `f->seq.len * reps` bits, one per byte. 
* `max_out` capacity of `out` in bits. 



**Returns:**

bits written, or 0 if the field is derived, empty, larger than `max_out`, or its sequence cannot be built. On 0, `out` may have been partly written. 





        

<hr>



### function dp\_wfm\_frame\_add\_derived 

_Append a named DERIVED field — one a stage will fill. Returns its index, or -1._ 
```C++
int dp_wfm_frame_add_derived (
    wfm_frame_desc_t * d,
    const char * name,
    size_t bits
) 
```



A field with a declared length and no source: a CRC trailer, a block of R-S check symbols. Its producer is wired by [**dp\_wfm\_frame\_add\_stage**](wfm__frame_8h.md#function-dp_wfm_frame_add_stage), not named here, because a stage does not exist yet when the field it derives is appended — fields are ordered by POSITION and stages by APPLICATION, and this is where those two orders meet.




**Parameters:**


* `d` the description. 
* `name` the field's name, or NULL/"" for anonymous. 
* `bits` its length, which its stage decides and the caller states. 



**Returns:**

the new field's index, or -1 on NULL, a full description, a zero `bits`, or a name already taken. 





        

<hr>



### function dp\_wfm\_frame\_add\_field 

_Append a named field. Returns its index, or -1._ 
```C++
int dp_wfm_frame_add_field (
    wfm_frame_desc_t * d,
    const char * name,
    const wfm_seq_t * seq,
    size_t reps
) 
```



The building half of the description, and the reason a name is worth carrying: a caller says what a field IS rather than counting positions, and the stage that covers it says so by name too.




**Parameters:**


* `d` the description; appended in wire order. 
* `name` the field's name, or NULL/"" to leave it anonymous. 
* `seq` where the bits come from; copied by value, so the LITERAL kind still borrows the caller's array and the caller still owns it for as long as `d` is used. 
* `reps` repetitions of `seq`, verbatim; 0 means one. 



**Returns:**

the new field's index, or -1 if `d` or `seq` is NULL, the description is full, or `name` is already taken. 





        

<hr>



### function dp\_wfm\_frame\_add\_stage 

_Append a stage covering_ `[first .. last]` _BY NAME. Returns its index, or -1._
```C++
int dp_wfm_frame_add_stage (
    wfm_frame_desc_t * d,
    uint32_t kind,
    const char * first,
    const char * last
) 
```



The cover is the whole point of the representation and this is the form that reads: `add_stage(d, WFM_STAGE_CRC16, "payload", "crc")` says what three integers used to.


**It wires a derived field's producer for you**, and that is applying an invariant rather than adding one: [**dp\_wfm\_frame\_desc\_layout**](wfm__frame_8h.md#function-dp_wfm_frame_desc_layout) already refuses a description whose derived field is not the LAST of its producing stage's cover, so a field with a declared length and no source sitting at the end of this cover has exactly one possible producer. It is wired here so a caller cannot state it a second, different way.




**Parameters:**


* `d` the description. 
* `kind` a [**wfm\_stage\_kind\_t**](wfm__frame_8h.md#enum-wfm_stage_kind_t) value, or a caller's own from WFM\_STAGE\_USER up. 
* `first` name of the first field covered. 
* `last` name of the last field covered; may equal `first`. 



**Returns:**

the new stage's index, or -1 on NULL, a full description, a name neither field carries, or `last` before `first`. 





        

<hr>



### function dp\_wfm\_frame\_add\_stage\_at 

_Append a stage covering_ `n_fields` _fields from index_`first` _. Returns its index, or -1._
```C++
int dp_wfm_frame_add_stage_at (
    wfm_frame_desc_t * d,
    uint32_t kind,
    unsigned first,
    unsigned n_fields
) 
```



The index form of [**dp\_wfm\_frame\_add\_stage**](wfm__frame_8h.md#function-dp_wfm_frame_add_stage), which resolves its two names and calls this. Both wire a derived field's producer the same way, by the one rule stated there, so a caller counting fields and a caller naming them build the same description.




**Parameters:**


* `d` the description. 
* `kind` a [**wfm\_stage\_kind\_t**](wfm__frame_8h.md#enum-wfm_stage_kind_t) value, or a caller's own. 
* `first` index of the first field covered. 
* `n_fields` fields covered; 0 means the stage does not run. 



**Returns:**

the new stage's index, or -1 on NULL or a full description. A cover past the fields is not refused here: the layout judges it. 





        

<hr>



### function dp\_wfm\_frame\_assemble 

_Materialise a description: run every field, then every stage._ 
```C++
size_t dp_wfm_frame_assemble (
    const wfm_frame_desc_t * d,
    const wfm_frame_ops_t * ops,
    uint8_t * out,
    size_t max_out
) 
```



Fields are written in wire order, then each stage is applied over the span [**dp\_wfm\_frame\_desc\_layout**](wfm__frame_8h.md#function-dp_wfm_frame_desc_layout) gave it — over that span and no other, which is the whole content of the coverage table a standard's framing turns out to be.




**Parameters:**


* `d` the description. 
* `ops` kernels for the stage kinds beyond the built-in CRC; may be `NULL` when there are none. 
* `out` receives the unpacked output, one bit per byte. 
* `max_out` capacity of `out` in bits; must be at least the layout's `out_bits`. 



**Returns:**

The bits written, or 0 if the description is refused, a stage has no kernel, a field cannot be built, or `max_out` is too small — in which case `out` is untouched. 





        

<hr>



### function dp\_wfm\_frame\_check 

_Undo a description's stages over a received frame, and report._ 
```C++
int dp_wfm_frame_check (
    const wfm_frame_desc_t * d,
    const wfm_frame_ops_t * ops,
    uint8_t * bits,
    wfm_frame_rx_t * rx
) 
```



The receive mirror of [**dp\_wfm\_frame\_assemble**](wfm__frame_8h.md#function-dp_wfm_frame_assemble), reading the same description — so the two cannot disagree about which stage covered what, which is the failure the whole representation exists to prevent. Stages are reversed in the OPPOSITE order to the one they were applied in, each over the span the layout gives it.


**This is what makes a truth-free frame error rate possible on a coded link, and it is a strictly better detector than a CRC.** A CRC says one bit: right or wrong. An outer code says _how much repair it took_ — `ok == units` with a rising `symbols` is margin being spent, visible before it is lost. A caller wanting only good frames compares `ok` with `units`; one doing accounting reads the rest.


It begins AFTER the inner code and after frame synchronisation, for the reason `ccsds_tm_frame.h` gives at length: a Viterbi is streaming and emits its decisions `depth` bits late, so the bits of one frame are not a function of that frame's symbols alone, and the marker that says where a frame starts is only readable once the inner code is undone. A stage with no `undo` kernel is reported as **not checked**, never as passed.




**Parameters:**


* `d` the description the bits are laid out by. 
* `ops` kernels for the stage kinds beyond the built-in CRC; may be `NULL`. 
* `bits` the layout's `frame_bits` received bits, one per byte, CORRECTED IN PLACE by any stage that repairs. 
* `rx` receives the per-stage outcome; may be `NULL`. 



**Returns:**

1 when every stage that was checked came out good, 0 when one did not, or -1 if the description is refused. **A description with no checking stage at all returns -1**, not 1: "carries no check" and "the check passed" are different answers, and an FER that conflated them would score every unprotected frame as perfect. 





        

<hr>



### function dp\_wfm\_frame\_desc\_crc\_ok 

_Check a received frame's CRC against any description that has one._ 
```C++
int dp_wfm_frame_desc_crc_ok (
    const wfm_frame_desc_t * d,
    const uint8_t * rx_bits
) 
```



**This is what makes a truth-free frame error rate possible.** It needs the description and the received bits and no payload truth at all — so it works on a real capture, and unlike a self-referenced EVM or a blind M2M4 it still catches a false lock, because a rotated constellation fails the check rather than looking clean. What the CRC protects is everything its stage covers except the trailer that stage derived — read back from the same rule the assembler writes by, so the two cannot disagree about where the trailer is.




**Parameters:**


* `d` the description the bits are laid out by. 
* `rx_bits` received bits, the layout's `frame_bits` of them. 



**Returns:**

1 pass, 0 fail, -1 if the description carries no CRC stage (or on NULL). The three are distinct on purpose: an FER that read "carries no check" as "the check failed" would count every unprotected frame as an error. 





        

<hr>



### function dp\_wfm\_frame\_desc\_layout 

_Derive every field offset, every stage span and both lengths._ 
```C++
int dp_wfm_frame_desc_layout (
    const wfm_frame_desc_t * d,
    wfm_frame_desc_layout_t * out
) 
```



The one operation both shipped framers already have, widened: this is the common frame's arithmetic and `dp_ccsds_tm_frame_layout()`'s, with the field and stage lists supplied rather than fixed.


A derived field whose producing stage covers no caller-supplied bits is dropped to zero length — which is the general form of the rule the common frame has always applied, that a CRC over an empty payload protects nothing and is not emitted.


An EMITTING stage (`emit_num` set) is refused unless it covers the whole frame, and a second one is refused outright. Refusing here is the point: such a description used to lay out perfectly and then be unassemblable for ever, because `out_bits` was computed from the cover while [**dp\_wfm\_frame\_assemble**](wfm__frame_8h.md#function-dp_wfm_frame_assemble) hands the kernel the whole frame. The caller got a 0 from `assemble` and no way to learn that the geometry, not the data, was wrong. Geometry is decided here, so it is refused here.


A field that declares `bits` but supplies no sequence is DERIVED, and one that names no producing stage (`derived_by` zero) is refused for the same reason. It used to lay out at zero length: the frame came out short, the stage that should have filled the field ran over a cover whose tail no longer existed, and the caller got a record rather than an error. Every reader funnels through here, so refusing at this one point covers the scene JSON and the CLI as well as the builder — which cannot reach the state at all, since [**dp\_wfm\_frame\_add\_stage**](wfm__frame_8h.md#function-dp_wfm_frame_add_stage) wires the producer from the cover it is given.




**Parameters:**


* `d` the description. 
* `out` receives the layout. 



**Returns:**

0, or -1 if `d` or `out` is NULL, a count or a cover runs past its array, a derived field names no producing stage, or an emitting stage covers less than the whole frame or is not the only one. 





        

<hr>



### function dp\_wfm\_frame\_field\_index 

_Index of the field called_ `name` _, or -1._
```C++
int dp_wfm_frame_field_index (
    const wfm_frame_desc_t * d,
    const char * name
) 
```



The lookup the whole naming idea rests on, and it is deliberately the ONLY one: names resolve to indices here and nowhere else, so every existing index-taking entry point keeps working unchanged and there is one place a rename can be wrong.


An empty or NULL `name` finds nothing rather than matching the first unnamed field — an unnamed field is anonymous, not named `""`, and matching it would make an unnamed description answer questions about fields it does not have.




**Parameters:**


* `d` the description. 
* `name` the field name, NUL-terminated. 



**Returns:**

the field's index, or -1 if `d` or `name` is NULL, `name` is empty, or no field carries it. 





        

<hr>



### function dp\_wfm\_frame\_fixed 

_Describe the common frame:_ `[preamble x reps | sync | payload | crc]` _._
```C++
int dp_wfm_frame_fixed (
    wfm_frame_desc_t * d,
    const wfm_seq_t * preamble,
    size_t reps,
    const wfm_seq_t * sync,
    const wfm_seq_t * payload,
    int crc
) 
```



The one fixed layout every face reaches without writing a description of its own — `wfmgen`'s `--acq-code`, `--sync` and `--crc`, a scene's keys of the same names, the `Frame` object and the receiver harnesses. It is built through the general by-name builder, so what comes back is an ordinary description with fields called `"preamble"`, `"sync"`, `"payload"` and `"crc"`, and there is no second layout behind it.


A field is present when its sequence has a LENGTH, never merely a pointer, and the preamble additionally needs `reps:` a length with no bits reaches [**dp\_wfm\_frame\_assemble**](wfm__frame_8h.md#function-dp_wfm_frame_assemble) and is refused there, rather than being dropped here and assembling a frame quietly missing it. The payload is always a field, even an empty one, so that a CRC always has something to cover — and a CRC over an empty payload lays out as a stage that did not run, because a trailer over nothing protects nothing.


The sequences are BORROWED, as everywhere in a description: they must outlive `d`.




**Parameters:**


* `d` receives the description; overwritten. 
* `preamble` preamble sequence; NULL or zero-length for none. 
* `reps` preamble repetitions; 0 means no preamble. 
* `sync` sync-word sequence; NULL or zero-length for none. 
* `payload` payload sequence; NULL for an empty payload. 
* `crc` non-zero: a CRC-16-CCITT trailer over the payload. 



**Returns:**

0, or -1 if `d` is NULL.



```C++
// Barker-13 sync over a 16-bit payload, with a CRC-16 trailer.
static const uint8_t b13[13] = {1,1,1,1,1,0,0,1,1,0,1,0,1};
static const uint8_t pay[16] = {0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1};
wfm_seq_t sync = { .kind = WFM_SEQ_LITERAL, .bits = b13, .len = 13 };
wfm_seq_t data = { .kind = WFM_SEQ_LITERAL, .bits = pay, .len = 16 };
wfm_frame_desc_t d;
wfm_frame_desc_layout_t l;
dp_wfm_frame_fixed (&d, NULL, 0, &sync, &data, 1);
dp_wfm_frame_desc_layout (&d, &l);   // l.frame_bits == 13 + 16 + 16
```
 


        

<hr>



### function dp\_wfm\_parse\_u64 

_Read an unsigned integer the way a Field's numbers are read._ 
```C++
int dp_wfm_parse_u64 (
    const char * p,
    size_t n,
    uint64_t * v
) 
```



THE number reader of the Field grammar, public so that a caller reading integers from text  wfmgen's numeric flags  applies the same rule rather than a second one (doppler#1611): decimal, or hex after `0x` (`0X`), consumed WHOLE. A sign, a space, a trailing character, an empty token, a bare `0x` and a value past `UINT64_MAX` are refused, and a leading `0` is decimal, never octal.




**Parameters:**


* `p` the text; need not be NUL-terminated. 
* `n` its length in bytes. 
* `v` receives the value; untouched on refusal. 



**Returns:**

0, or -1 for text outside the rule.



```C++
uint64_t v;
int ok  = dp_wfm_parse_u64 ("0x10", 4, &v); // 0, v == 16
int dec = dp_wfm_parse_u64 ("010", 3, &v);  // 0, v == 10, not octal
int bad = dp_wfm_parse_u64 ("4x", 2, &v);   // -1, v unchanged
```
 


        

<hr>



### function dp\_wfm\_seq\_bits 

_Write_ `s's` _bits, whatever produces them. Returns the count._
```C++
size_t dp_wfm_seq_bits (
    const wfm_seq_t * s,
    uint8_t * out,
    size_t max_out
) 
```



The one place a `wfm_seq_t` becomes bits. A descriptor materialises its own fields through this, and a consumer that takes a RAW ARRAY rather than a description  the DSSS chip builder is the one in this tree  calls it to expand a generated sequence into a buffer first. Without that, `bits` is NULL for every generated kind and the array consumer reads through it.




**Parameters:**


* `s` the sequence; a LITERAL copies, the generated kinds run their generator. 
* `out` receives `s->len` bits, one per byte. 
* `max_out` capacity; 0 is returned if `s->len` exceeds it. 



**Returns:**

bits written, or 0 if the sequence is unbuildable (a LITERAL with no array, a length past `max_out`, a generator that refused its own parameters). 





        

<hr>
## Macro Definition Documentation





### define WFM\_FIELD\_MAX\_BITS 

_Bits one Field may hold,_ `LEN * REPS` _; the grammar refuses more._
```C++
#define WFM_FIELD_MAX_BITS `261120`
```



Derived, not picked. The longest frame a shipped stage accepts is the CCSDS Reed-Solomon codeblock at its deepest interleaving, 255 symbols x 8 bits x depth 8 = 16320 bits. The margin is 16, the smallest power of two that still admits a Field one whole period of the default CCSDS randomiser long (131071 bits; 8 x 16320 falls 511 short). A Field is a finite run of a frame, so a longer one is a STREAM, which is a data source's job, not a Field's. The bound is checked at parse, before any allocation sized by the caller's number. `ccsds_tm/desc.c` re-derives it from the R-S constants at compile time, so it cannot drift. 


        

<hr>



### define WFM\_FRAME\_CRC\_BITS 

_Bits of CRC-16-CCITT, when a frame carries one._ 
```C++
#define WFM_FRAME_CRC_BITS `16u`
```




<hr>



### define WFM\_FRAME\_MAX\_FIELDS 

_Fields one description may carry._ 
```C++
#define WFM_FRAME_MAX_FIELDS `16`
```



Raised from 8 against a measurement rather than a feeling: the deepest description doppler builds today is SIX fields (ASM, preamble, sync, payload, CRC, R-S parity) and FIVE stages, so 8 left room for two more fields — and a user frame that adds a header and a tail to that shape reaches the old ceiling exactly. The descriptor is a POD carried by value, so the cost is bytes on a stack frame: 1136 -&gt; 2152, which is still a comfortable local. 


        

<hr>



### define WFM\_FRAME\_MAX\_STAGES 

_Stages one description may carry._ 
```C++
#define WFM_FRAME_MAX_STAGES `8`
```




<hr>



### define WFM\_FRAME\_NAME\_MAX 

_Bytes a field's name may use, NUL included._ 
```C++
#define WFM_FRAME_NAME_MAX `16`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_frame.h`

