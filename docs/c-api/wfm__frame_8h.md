

# File wfm\_frame.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_frame.h**](wfm__frame_8h.md)

[Go to the source code of this file](wfm__frame_8h_source.md)

_A frame's BIT layout, described once and read from both ends._ [More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**wfm\_frame\_layout\_t**](structwfm__frame__layout__t.md) <br>_Where each field lands, in bits from the start of the frame._  |
| struct | [**wfm\_frame\_t**](structwfm__frame__t.md) <br>_A frame's bit layout:_ `[preamble × reps | sync | payload | crc]` _._ |
| struct | [**wfm\_seq\_t**](structwfm__seq__t.md) <br>_A run of bits, however it is produced._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**wfm\_seq\_kind\_t**](#enum-wfm_seq_kind_t)  <br>_Where a run of bits comes from._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**wfm\_frame\_bits**](#function-wfm_frame_bits) (const [**wfm\_frame\_t**](structwfm__frame__t.md) \* f, uint8\_t \* out, size\_t max\_out) <br>_Materialise the frame as one flat 0/1 bit array._  |
|  int | [**wfm\_frame\_crc\_ok**](#function-wfm_frame_crc_ok) (const [**wfm\_frame\_t**](structwfm__frame__t.md) \* f, const uint8\_t \* rx\_bits) <br>_Check a received frame's CRC in place._  |
|  int | [**wfm\_frame\_layout**](#function-wfm_frame_layout) (const [**wfm\_frame\_t**](structwfm__frame__t.md) \* f, [**wfm\_frame\_layout\_t**](structwfm__frame__layout__t.md) \* out) <br>_Fill_ `out` _with the field offsets._ |
|  size\_t | [**wfm\_frame\_nbits**](#function-wfm_frame_nbits) (const [**wfm\_frame\_t**](structwfm__frame__t.md) \* f) <br>_Total frame bits, or 0 if the geometry is empty._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**WFM\_FRAME\_CRC\_BITS**](wfm__frame_8h.md#define-wfm_frame_crc_bits)  `16u`<br>_Bits of CRC-16-CCITT, when a frame carries one._  |

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
## Public Functions Documentation




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

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

