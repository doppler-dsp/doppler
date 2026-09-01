

# File wfm\_frame.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_frame.h**](wfm__frame_8h.md)

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
| struct | [**wfm\_frame\_layout\_t**](structwfm__frame__layout__t.md) <br>_Where each field lands, in bits from the start of the frame._  |
| struct | [**wfm\_frame\_ops\_t**](structwfm__frame__ops__t.md) <br>_The kernels an assembly runs, and whatever state they carry._  |
| struct | [**wfm\_frame\_rx\_t**](structwfm__frame__rx__t.md) <br>_What_ [_**wfm\_frame\_check**_](wfm__frame_8h.md#function-wfm_frame_check) _found, stage by stage._ |
| struct | [**wfm\_frame\_span\_t**](structwfm__frame__span__t.md) <br>_A run of bits inside the assembled frame,_ `[first, first + n)` _._ |
| struct | [**wfm\_frame\_stage\_rx\_t**](structwfm__frame__stage__rx__t.md) <br>_What undoing one stage found._  |
| struct | [**wfm\_frame\_t**](structwfm__frame__t.md) <br>_A frame's bit layout:_ `[preamble × reps | sync | payload | crc]` _._ |
| struct | [**wfm\_seq\_t**](structwfm__seq__t.md) <br>_A run of bits, however it is produced._  |
| struct | [**wfm\_stage\_op\_t**](structwfm__stage__op__t.md) <br>_How one kind of stage actually transforms bits._  |
| struct | [**wfm\_stage\_t**](structwfm__stage__t.md) <br>_One transform, and — the whole point — the fields it covers._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**wfm\_\_frame\_8h\_1a385c44f6fb256e5716a2302a5b940388**](#enum-wfm__frame_8h_1a385c44f6fb256e5716a2302a5b940388)  <br>_Field indices_ [_**wfm\_frame\_describe**_](wfm__frame_8h.md#function-wfm_frame_describe) _writes, in wire order._ |
| enum  | [**wfm\_seq\_kind\_t**](#enum-wfm_seq_kind_t)  <br>_Where a run of bits comes from._  |
| enum  | [**wfm\_stage\_kind\_t**](#enum-wfm_stage_kind_t)  <br>_Stage kinds doppler itself names._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**wfm\_dsss\_desc\_chips**](#function-wfm_dsss_desc_chips) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const [**wfm\_frame\_ops\_t**](structwfm__frame__ops__t.md) \* ops, const uint8\_t \* acq\_code, size\_t acq\_len, size\_t acq\_reps, const uint8\_t \* data\_code, size\_t data\_len, uint8\_t \* out, size\_t max\_out) <br>_Build a two-code DSSS burst from a description: assemble, spread._  |
|  size\_t | [**wfm\_dsss\_desc\_nchips**](#function-wfm_dsss_desc_nchips) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, size\_t acq\_len, size\_t acq\_reps, size\_t data\_len) <br>_Chip count of a DSSS burst built from a description._  |
|  int | [**wfm\_frame\_add\_derived**](#function-wfm_frame_add_derived) ([**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const char \* name, size\_t bits) <br>_Append a named DERIVED field — one a stage will fill. Returns its index, or -1._  |
|  int | [**wfm\_frame\_add\_field**](#function-wfm_frame_add_field) ([**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const char \* name, const [**wfm\_seq\_t**](structwfm__seq__t.md) \* seq, size\_t reps) <br>_Append a named field. Returns its index, or -1._  |
|  int | [**wfm\_frame\_add\_stage**](#function-wfm_frame_add_stage) ([**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, uint32\_t kind, const char \* first, const char \* last) <br>_Append a stage covering_ `[first .. last]` _BY NAME. Returns its index, or -1._ |
|  size\_t | [**wfm\_frame\_assemble**](#function-wfm_frame_assemble) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const [**wfm\_frame\_ops\_t**](structwfm__frame__ops__t.md) \* ops, uint8\_t \* out, size\_t max\_out) <br>_Materialise a description: run every field, then every stage._  |
|  size\_t | [**wfm\_frame\_bits**](#function-wfm_frame_bits) (const [**wfm\_frame\_t**](structwfm__frame__t.md) \* f, uint8\_t \* out, size\_t max\_out) <br>_Materialise the frame as one flat 0/1 bit array._  |
|  int | [**wfm\_frame\_check**](#function-wfm_frame_check) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const [**wfm\_frame\_ops\_t**](structwfm__frame__ops__t.md) \* ops, uint8\_t \* bits, [**wfm\_frame\_rx\_t**](structwfm__frame__rx__t.md) \* rx) <br>_Undo a description's stages over a received frame, and report._  |
|  int | [**wfm\_frame\_crc\_ok**](#function-wfm_frame_crc_ok) (const [**wfm\_frame\_t**](structwfm__frame__t.md) \* f, const uint8\_t \* rx\_bits) <br>_Check a received frame's CRC in place._  |
|  int | [**wfm\_frame\_desc\_crc\_ok**](#function-wfm_frame_desc_crc_ok) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const uint8\_t \* rx\_bits) <br>_Check a received frame's CRC against any description that has one._  |
|  int | [**wfm\_frame\_desc\_layout**](#function-wfm_frame_desc_layout) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, [**wfm\_frame\_desc\_layout\_t**](structwfm__frame__desc__layout__t.md) \* out) <br>_Derive every field offset, every stage span and both lengths._  |
|  int | [**wfm\_frame\_describe**](#function-wfm_frame_describe) (const [**wfm\_frame\_t**](structwfm__frame__t.md) \* f, [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* out) <br>_Express a_ [_**wfm\_frame\_t**_](structwfm__frame__t.md) _as a_[_**wfm\_frame\_desc\_t**_](structwfm__frame__desc__t.md) _._ |
|  int | [**wfm\_frame\_field\_index**](#function-wfm_frame_field_index) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const char \* name) <br>_Index of the field called_ `name` _, or -1._ |
|  int | [**wfm\_frame\_layout**](#function-wfm_frame_layout) (const [**wfm\_frame\_t**](structwfm__frame__t.md) \* f, [**wfm\_frame\_layout\_t**](structwfm__frame__layout__t.md) \* out) <br>_Fill_ `out` _with the field offsets._ |
|  size\_t | [**wfm\_frame\_nbits**](#function-wfm_frame_nbits) (const [**wfm\_frame\_t**](structwfm__frame__t.md) \* f) <br>_Total frame bits, or 0 if the geometry is empty._  |
|  size\_t | [**wfm\_seq\_bits**](#function-wfm_seq_bits) (const [**wfm\_seq\_t**](structwfm__seq__t.md) \* s, uint8\_t \* out, size\_t max\_out) <br>_Write_ `s's` _bits, whatever produces them. Returns the count._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**WFM\_FRAME\_CRC\_BITS**](wfm__frame_8h.md#define-wfm_frame_crc_bits)  `16u`<br>_Bits of CRC-16-CCITT, when a frame carries one._  |
| define  | [**WFM\_FRAME\_MAX\_FIELDS**](wfm__frame_8h.md#define-wfm_frame_max_fields)  `16`<br>_Fields one description may carry._  |
| define  | [**WFM\_FRAME\_MAX\_STAGES**](wfm__frame_8h.md#define-wfm_frame_max_stages)  `8`<br>_Stages one description may carry._  |
| define  | [**WFM\_FRAME\_NAME\_MAX**](wfm__frame_8h.md#define-wfm_frame_name_max)  `16`<br>_Bytes a field's name may use, NUL included._  |

## Detailed Description


One struct saying what a frame contains, used by the generator that builds it and by the measurer that scores it. The DSSS assembler already stated the reason it must be shared — it is "assembled in one place so TX and RX can
never drift" — and this generalises that from one waveform to all of them: `wfm_frame_dsss_chips()` now builds these bits and spreads them, rather than carrying a second copy of the layout.


### It describes BITS



Not chips, not samples, not levels. Spreading, pulse shaping, oversampling, carrier and SNR layer above and stay `wfm_synth`'s job. That boundary is what lets one descriptor serve an unspread BPSK stream and a two-code DSSS burst alike.



### Every field is a sequence, and the generators already exist



The preamble, the sync word and the payload are all `wfm_seq_t`, so "a Gold
sync" is a configuration rather than a feature, and `pn_create()` / `gold_create()` stay the only implementations of those sequences.


**The generated kinds are the ones that matter.** A literal array is what a caller with real data has; a PN or Gold descriptor is a handful of numbers a receiver can REGENERATE, which is what makes a long-record BER practical — truth for a million-symbol run without a million-symbol array, and a capture reproducible from its metadata alone.



### The CRC is the one we already have



`dp_crc16_ccitt()`, over the payload only, MSB-first, carried as the same `int crc` flag `wfm_frame_dsss_chips()` already took. A second CRC would be a wire-format decision and nothing is asking for one.




**See also:** docs/design/rx-test.md section 7 




    
## Public Types Documentation




### enum wfm\_\_frame\_8h\_1a385c44f6fb256e5716a2302a5b940388 

_Field indices_ [_**wfm\_frame\_describe**_](wfm__frame_8h.md#function-wfm_frame_describe) _writes, in wire order._
```C++
enum wfm__frame_8h_1a385c44f6fb256e5716a2302a5b940388 {
    WFM_FRAME_FIELD_PREAMBLE = 0,
    WFM_FRAME_FIELD_SYNC = 1,
    WFM_FRAME_FIELD_PAYLOAD = 2,
    WFM_FRAME_FIELD_CRC = 3
};
```




<hr>



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




### function wfm\_dsss\_desc\_chips 

_Build a two-code DSSS burst from a description: assemble, spread._ 
```C++
size_t wfm_dsss_desc_chips (
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



 The general form of [**wfm\_frame\_dsss\_chips**](wfm__dsp_8h.md#function-wfm_frame_dsss_chips), and the only spreader — the four-field entry point is this one with the description filled in.


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
* `max_out` capacity of `out`; must be at least [**wfm\_dsss\_desc\_nchips**](wfm__frame_8h.md#function-wfm_dsss_desc_nchips). 



**Returns:**

chips written, or 0 if the geometry is refused, a stage has no kernel, or `max_out` is too small. 





        

<hr>



### function wfm\_dsss\_desc\_nchips 

_Chip count of a DSSS burst built from a description._ 
```C++
size_t wfm_dsss_desc_nchips (
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



### function wfm\_frame\_add\_derived 

_Append a named DERIVED field — one a stage will fill. Returns its index, or -1._ 
```C++
int wfm_frame_add_derived (
    wfm_frame_desc_t * d,
    const char * name,
    size_t bits
) 
```



A field with a declared length and no source: a CRC trailer, a block of R-S check symbols. Its producer is wired by [**wfm\_frame\_add\_stage**](wfm__frame_8h.md#function-wfm_frame_add_stage), not named here, because a stage does not exist yet when the field it derives is appended — fields are ordered by POSITION and stages by APPLICATION, and this is where those two orders meet.




**Parameters:**


* `d` the description. 
* `name` the field's name, or NULL/"" for anonymous. 
* `bits` its length, which its stage decides and the caller states. 



**Returns:**

the new field's index, or -1 on NULL, a full description, a zero `bits`, or a name already taken. 





        

<hr>



### function wfm\_frame\_add\_field 

_Append a named field. Returns its index, or -1._ 
```C++
int wfm_frame_add_field (
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



### function wfm\_frame\_add\_stage 

_Append a stage covering_ `[first .. last]` _BY NAME. Returns its index, or -1._
```C++
int wfm_frame_add_stage (
    wfm_frame_desc_t * d,
    uint32_t kind,
    const char * first,
    const char * last
) 
```



The cover is the whole point of the representation and this is the form that reads: `add_stage(d, WFM_STAGE_CRC16, "payload", "crc")` says what three integers used to.


**It wires a derived field's producer for you**, and that is applying an invariant rather than adding one: [**wfm\_frame\_desc\_layout**](wfm__frame_8h.md#function-wfm_frame_desc_layout) already refuses a description whose derived field is not the LAST of its producing stage's cover, so a field with a declared length and no source sitting at the end of this cover has exactly one possible producer. It is wired here so a caller cannot state it a second, different way.




**Parameters:**


* `d` the description. 
* `kind` a [**wfm\_stage\_kind\_t**](wfm__frame_8h.md#enum-wfm_stage_kind_t) value, or a caller's own from WFM\_STAGE\_USER up. 
* `first` name of the first field covered. 
* `last` name of the last field covered; may equal `first`. 



**Returns:**

the new stage's index, or -1 on NULL, a full description, a name neither field carries, or `last` before `first`. 





        

<hr>



### function wfm\_frame\_assemble 

_Materialise a description: run every field, then every stage._ 
```C++
size_t wfm_frame_assemble (
    const wfm_frame_desc_t * d,
    const wfm_frame_ops_t * ops,
    uint8_t * out,
    size_t max_out
) 
```



The general form of [**wfm\_frame\_bits**](wfm__frame_8h.md#function-wfm_frame_bits). Fields are written in wire order, then each stage is applied over the span [**wfm\_frame\_desc\_layout**](wfm__frame_8h.md#function-wfm_frame_desc_layout) gave it — over that span and no other, which is the whole content of the coverage table a standard's framing turns out to be.




**Parameters:**


* `d` the description. 
* `ops` kernels for the stage kinds beyond the built-in CRC; may be `NULL` when there are none. 
* `out` receives the unpacked output, one bit per byte. 
* `max_out` capacity of `out` in bits; must be at least the layout's `out_bits`. 



**Returns:**

The bits written, or 0 if the description is refused, a stage has no kernel, a field cannot be built, or `max_out` is too small — in which case `out` is untouched. 





        

<hr>



### function wfm\_frame\_bits 

_Materialise the frame as one flat 0/1 bit array._ 
```C++
size_t wfm_frame_bits (
    const wfm_frame_t * f,
    uint8_t * out,
    size_t max_out
) 
```



Generated fields are produced here, from the descriptor, so a receiver holding the same handful of numbers regenerates the identical bits.




**Parameters:**


* `f` the frame. 
* `out` output, one bit per byte. 
* `max_out` capacity of `out`. 



**Returns:**

bits written, or 0 if the geometry is empty, a field is unbuildable (a LITERAL with no array, a PN with no register width), or `max_out` is too small. 





        

<hr>



### function wfm\_frame\_check 

_Undo a description's stages over a received frame, and report._ 
```C++
int wfm_frame_check (
    const wfm_frame_desc_t * d,
    const wfm_frame_ops_t * ops,
    uint8_t * bits,
    wfm_frame_rx_t * rx
) 
```



The receive mirror of [**wfm\_frame\_assemble**](wfm__frame_8h.md#function-wfm_frame_assemble), reading the same description — so the two cannot disagree about which stage covered what, which is the failure the whole representation exists to prevent. Stages are reversed in the OPPOSITE order to the one they were applied in, each over the span the layout gives it.


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



### function wfm\_frame\_crc\_ok 

_Check a received frame's CRC in place._ 
```C++
int wfm_frame_crc_ok (
    const wfm_frame_t * f,
    const uint8_t * rx_bits
) 
```



**This is what makes a truth-free frame error rate possible.** It needs the layout and the received bits and no payload truth at all — so it works on a real capture, and unlike a self-referenced EVM or a blind M2M4 it still catches a false lock, because a rotated constellation fails the check rather than looking clean.




**Parameters:**


* `f` the frame the bits are laid out by. 
* `rx_bits` received bits, `wfm_frame_nbits(f)` of them. 



**Returns:**

1 pass, 0 fail, -1 if the frame carries no CRC (or on NULL). 





        

<hr>



### function wfm\_frame\_desc\_crc\_ok 

_Check a received frame's CRC against any description that has one._ 
```C++
int wfm_frame_desc_crc_ok (
    const wfm_frame_desc_t * d,
    const uint8_t * rx_bits
) 
```



The general form of [**wfm\_frame\_crc\_ok**](wfm__frame_8h.md#function-wfm_frame_crc_ok), and the same truth-free claim: it needs the description and the received bits and no payload truth at all. What the CRC protects is everything its stage covers except the trailer that stage derived — read back from the same rule the assembler writes by, so the two cannot disagree about where the trailer is.




**Parameters:**


* `d` the description the bits are laid out by. 
* `rx_bits` received bits, the layout's `frame_bits` of them. 



**Returns:**

1 pass, 0 fail, -1 if the description carries no CRC stage (or on NULL). The three are distinct on purpose: an FER that read "carries no check" as "the check failed" would count every unprotected frame as an error. 





        

<hr>



### function wfm\_frame\_desc\_layout 

_Derive every field offset, every stage span and both lengths._ 
```C++
int wfm_frame_desc_layout (
    const wfm_frame_desc_t * d,
    wfm_frame_desc_layout_t * out
) 
```



The one operation both shipped framers already have, widened: this is `wfm_frame_layout()`'s arithmetic and `ccsds_tm_frame_layout()`'s, with the field and stage lists supplied rather than fixed.


A derived field whose producing stage covers no caller-supplied bits is dropped to zero length — which is the general form of the rule [**wfm\_frame\_layout**](wfm__frame_8h.md#function-wfm_frame_layout) has always applied, that a CRC over an empty payload protects nothing and is not emitted.


An EMITTING stage (`emit_num` set) is refused unless it covers the whole frame, and a second one is refused outright. Refusing here is the point: such a description used to lay out perfectly and then be unassemblable for ever, because `out_bits` was computed from the cover while [**wfm\_frame\_assemble**](wfm__frame_8h.md#function-wfm_frame_assemble) hands the kernel the whole frame. The caller got a 0 from `assemble` and no way to learn that the geometry, not the data, was wrong. Geometry is decided here, so it is refused here.


A field that declares `bits` but supplies no sequence is DERIVED, and one that names no producing stage (`derived_by` zero) is refused for the same reason. It used to lay out at zero length: the frame came out short, the stage that should have filled the field ran over a cover whose tail no longer existed, and the caller got a record rather than an error. Every reader funnels through here, so refusing at this one point covers the scene JSON and the CLI as well as the builder — which cannot reach the state at all, since [**wfm\_frame\_add\_stage**](wfm__frame_8h.md#function-wfm_frame_add_stage) wires the producer from the cover it is given.




**Parameters:**


* `d` the description. 
* `out` receives the layout. 



**Returns:**

0, or -1 if `d` or `out` is NULL, a count or a cover runs past its array, a derived field names no producing stage, or an emitting stage covers less than the whole frame or is not the only one. 





        

<hr>



### function wfm\_frame\_describe 

_Express a_ [_**wfm\_frame\_t**_](structwfm__frame__t.md) _as a_[_**wfm\_frame\_desc\_t**_](structwfm__frame__desc__t.md) _._
```C++
int wfm_frame_describe (
    const wfm_frame_t * f,
    wfm_frame_desc_t * out
) 
```



The bridge that makes the closed struct a configuration rather than a rival: four fields in wire order, plus one CRC stage covering the payload and the trailer it derives. Exported because it is also the worked example — the shortest complete answer to "what does a description of my
frame look like".




**Parameters:**


* `f` the frame. 
* `out` receives the description. 



**Returns:**

0, or -1 if either argument is NULL. 





        

<hr>



### function wfm\_frame\_field\_index 

_Index of the field called_ `name` _, or -1._
```C++
int wfm_frame_field_index (
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



### function wfm\_frame\_layout 

_Fill_ `out` _with the field offsets._
```C++
int wfm_frame_layout (
    const wfm_frame_t * f,
    wfm_frame_layout_t * out
) 
```



The arithmetic both directions need, computed once. Today it is inline in `wfm_frame_dsss_nchips()`, and a receiver scoring a frame would have to recompute it — which is exactly how TX and RX drift apart.




**Returns:**

0, or -1 if `f` or `out` is NULL. 





        

<hr>



### function wfm\_frame\_nbits 

_Total frame bits, or 0 if the geometry is empty._ 
```C++
size_t wfm_frame_nbits (
    const wfm_frame_t * f
) 
```





**Parameters:**


* `f` the frame; must be non-NULL. 




        

<hr>



### function wfm\_seq\_bits 

_Write_ `s's` _bits, whatever produces them. Returns the count._
```C++
size_t wfm_seq_bits (
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
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

