

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
| enum  | [**wfm\_\_frame\_8h\_1abc5c98fcc1211af2b80116dd6e0a035d**](#enum-wfm__frame_8h_1abc5c98fcc1211af2b80116dd6e0a035d)  <br>_Field indices_ [_**wfm\_frame\_describe**_](wfm__frame_8h.md#function-wfm_frame_describe) _writes, in wire order._ |
| enum  | [**wfm\_seq\_kind\_t**](#enum-wfm_seq_kind_t)  <br>_Where a run of bits comes from._  |
| enum  | [**wfm\_stage\_kind\_t**](#enum-wfm_stage_kind_t)  <br>_What a stage does to the fields it covers._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**wfm\_frame\_assemble**](#function-wfm_frame_assemble) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const [**wfm\_frame\_ops\_t**](structwfm__frame__ops__t.md) \* ops, uint8\_t \* out, size\_t max\_out) <br>_Materialise a description: run every field, then every stage._  |
|  size\_t | [**wfm\_frame\_bits**](#function-wfm_frame_bits) (const [**wfm\_frame\_t**](structwfm__frame__t.md) \* f, uint8\_t \* out, size\_t max\_out) <br>_Materialise the frame as one flat 0/1 bit array._  |
|  int | [**wfm\_frame\_check**](#function-wfm_frame_check) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const [**wfm\_frame\_ops\_t**](structwfm__frame__ops__t.md) \* ops, uint8\_t \* bits, [**wfm\_frame\_rx\_t**](structwfm__frame__rx__t.md) \* rx) <br>_Undo a description's stages over a received frame, and report._  |
|  int | [**wfm\_frame\_crc\_ok**](#function-wfm_frame_crc_ok) (const [**wfm\_frame\_t**](structwfm__frame__t.md) \* f, const uint8\_t \* rx\_bits) <br>_Check a received frame's CRC in place._  |
|  int | [**wfm\_frame\_desc\_crc\_ok**](#function-wfm_frame_desc_crc_ok) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const uint8\_t \* rx\_bits) <br>_Check a received frame's CRC against any description that has one._  |
|  int | [**wfm\_frame\_desc\_layout**](#function-wfm_frame_desc_layout) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, [**wfm\_frame\_desc\_layout\_t**](structwfm__frame__desc__layout__t.md) \* out) <br>_Derive every field offset, every stage span and both lengths._  |
|  int | [**wfm\_frame\_describe**](#function-wfm_frame_describe) (const [**wfm\_frame\_t**](structwfm__frame__t.md) \* f, [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* out) <br>_Express a_ [_**wfm\_frame\_t**_](structwfm__frame__t.md) _as a_[_**wfm\_frame\_desc\_t**_](structwfm__frame__desc__t.md) _._ |
|  int | [**wfm\_frame\_layout**](#function-wfm_frame_layout) (const [**wfm\_frame\_t**](structwfm__frame__t.md) \* f, [**wfm\_frame\_layout\_t**](structwfm__frame__layout__t.md) \* out) <br>_Fill_ `out` _with the field offsets._ |
|  size\_t | [**wfm\_frame\_nbits**](#function-wfm_frame_nbits) (const [**wfm\_frame\_t**](structwfm__frame__t.md) \* f) <br>_Total frame bits, or 0 if the geometry is empty._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**WFM\_FRAME\_CRC\_BITS**](wfm__frame_8h.md#define-wfm_frame_crc_bits)  `16u`<br>_Bits of CRC-16-CCITT, when a frame carries one._  |
| define  | [**WFM\_FRAME\_MAX\_FIELDS**](wfm__frame_8h.md#define-wfm_frame_max_fields)  `8`<br>_Fields one description may carry._  |
| define  | [**WFM\_FRAME\_MAX\_STAGES**](wfm__frame_8h.md#define-wfm_frame_max_stages)  `6`<br>_Stages one description may carry._  |

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




### enum wfm\_\_frame\_8h\_1abc5c98fcc1211af2b80116dd6e0a035d 

_Field indices_ [_**wfm\_frame\_describe**_](wfm__frame_8h.md#function-wfm_frame_describe) _writes, in wire order._
```C++
enum wfm__frame_8h_1abc5c98fcc1211af2b80116dd6e0a035d {
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

_What a stage does to the fields it covers._ 
```C++
enum wfm_stage_kind_t {
    WFM_STAGE_CRC16 = 0,
    WFM_STAGE_RS = 1,
    WFM_STAGE_RANDOMISE = 2,
    WFM_STAGE_CONV = 3
};
```




<hr>
## Public Functions Documentation




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




**Parameters:**


* `d` the description. 
* `out` receives the layout. 



**Returns:**

0, or -1 if `d` or `out` is NULL, or a count or a cover runs past its array. 





        

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
#define WFM_FRAME_MAX_FIELDS `8`
```




<hr>



### define WFM\_FRAME\_MAX\_STAGES 

_Stages one description may carry._ 
```C++
#define WFM_FRAME_MAX_STAGES `6`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

