

# File ccsds\_415\_frame.h



[**FileList**](files.md) **>** [**ccsds**](dir_c033bcb1c91e23f54b625bfe2cf0448c.md) **>** [**ccsds\_415\_frame.h**](ccsds__415__frame_8h.md)

[Go to the source code of this file](ccsds__415__frame_8h_source.md)

_One period of the CCSDS 415.1-B-1 point's stimulus as a frame description: the gap, the acquisition sequence, the start sequence and one CCSDS TC transfer frame (232.0-B-4), field by field._ [More...](#detailed-description)

* `#include "doppler/ccsds/ccsds_415.h"`
* `#include "doppler/wfm/wfm_frame.h"`
* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_ccsds\_415\_frame\_cfg\_t**](structdp__ccsds__415__frame__cfg__t.md) <br> |
| struct | [**dp\_ccsds\_415\_frame\_t**](structdp__ccsds__415__frame__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_ccsds\_415\_frame\_desc**](#function-dp_ccsds_415_frame_desc) ([**dp\_ccsds\_415\_frame\_t**](structdp__ccsds__415__frame__t.md) \* f, const [**dp\_ccsds\_415\_frame\_cfg\_t**](structdp__ccsds__415__frame__cfg__t.md) \* cfg, size\_t data\_octets, const char \*\* why) <br>_Describe one period of the 415.1-B-1 stimulus with a frame of_ `data_octets` _data octets._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_CCSDS\_415\_ACQ\_BITS**](ccsds__415__frame_8h.md#define-dp_ccsds_415_acq_bits)  `128u`<br> |
| define  | [**DP\_CCSDS\_415\_DATA\_MAX**](ccsds__415__frame_8h.md#define-dp_ccsds_415_data_max)  `1017u`<br> |
| define  | [**DP\_CCSDS\_415\_DATA\_MIN**](ccsds__415__frame_8h.md#define-dp_ccsds_415_data_min)  `1u`<br> |
| define  | [**DP\_CCSDS\_415\_FRAME\_CFG\_DEFAULTS**](ccsds__415__frame_8h.md#define-dp_ccsds_415_frame_cfg_defaults)  `{ 42u, 1u, 0u, [**DP\_CCSDS\_415\_CODE\_ONLY\_SYM**](ccsds__415_8h.md#define-dp_ccsds_415_code_only_sym) }`<br> |
| define  | [**DP\_CCSDS\_415\_HEADER\_BITS**](ccsds__415__frame_8h.md#define-dp_ccsds_415_header_bits)  `40u`<br> |
| define  | [**DP\_CCSDS\_415\_START\_BITS**](ccsds__415__frame_8h.md#define-dp_ccsds_415_start_bits)  `16u`<br> |
| define  | [**DP\_CCSDS\_415\_START\_SEQ**](ccsds__415__frame_8h.md#define-dp_ccsds_415_start_seq)  `0xEB90u`<br> |

## Detailed Description


docs/design/async-dsss-receiver.md §1.4 states the period; this is that table as DATA, a configuration of the general description in `wfm/wfm_frame.h` (which knows nothing about CCSDS), as [**dp\_ccsds\_tm\_frame\_describe()**](ccsds__tm__frame_8h.md#function-dp_ccsds_tm_frame_describe) is 131.0-B's. Every field is named, so a caller reads a field's place by name ([**dp\_wfm\_frame\_field\_index()**](wfm__frame_8h.md#function-dp_wfm_frame_field_index), [**dp\_wfm\_frame\_desc\_layout()**](wfm__frame_8h.md#function-dp_wfm_frame_desc_layout)) and checks the frame with [**dp\_wfm\_frame\_desc\_crc\_ok()**](wfm__frame_8h.md#function-dp_wfm_frame_desc_crc_ok) or [**dp\_wfm\_frame\_check()**](wfm__frame_8h.md#function-dp_wfm_frame_check).



|field   |bits   |content    |
|-----|-----|-----|
|`gap`   |`gap_sym`   |zeros: the code alone (415.1-B-1 §6.1)    |
|`acq`   |128   |1010...: the acquisition sequence (231.0-B-4 §7.2.2), WFM\_SEQ\_DOTTED    |
|`start`   |16   |EB90, bit 0 first (231.0-B-4 §5.2.2.2)    |
|`version`   |2   |`00` (232.0-B-4 §4.1.2.2.2)    |
|`bypass`   |1   |`1`, Type-B (§4.1.2.3.1)    |
|`ctrl_cmd`   |1   |`0`, Type-D (§4.1.2.3.2)    |
|`spare`   |2   |`00` (§4.1.2.4.2)    |
|`scid`   |10   |the spacecraft identifier (§4.1.2.5)    |
|`vcid`   |6   |the virtual channel identifier (§4.1.2.6)    |
|`length`   |10   |the frame's octets minus one (§4.1.2.7.2)    |
|`seq`   |8   |the frame sequence number (§4.1.2.8)    |
|`data`   |8 x octets   |the frame's data source (WFM\_SEQ\_DATA)    |
|`fecf`   |16   |CRC-16 over `version` .. `data` (§4.1.4)   |






The FECF is the description's own CRC stage, WFM\_STAGE\_CRC16: CRC-16 with G(X) = X^16 + X^12 + X^5 + 1, preset to all ones, no final inversion (dp\_crc16\_ccitt(), DP\_CRC\_16\_IBM\_3740), which is 232.0-B-4 §4.1.4.2's.


**Field 0 is the gap, not the start sequence**, so [**dp\_wfm\_frame\_desc\_rx()**](wfm__frame_8h.md#function-dp_wfm_frame_desc_rx), which reads a receiver's sync word from field 0, refuses this description (test\_ccsds\_415\_frame.c pins the refusal). A receiver finds the frame by searching for the start sequence itself: detection's SyncFinder on the `start` field's bits.


The description BORROWS the bit arrays in its [**dp\_ccsds\_415\_frame\_t**](structdp__ccsds__415__frame__t.md), so use it in place: a copy of the struct points into the original.



```C++
#include <doppler/ccsds/ccsds_415_frame.h>

dp_ccsds_415_frame_t        f;
dp_ccsds_415_frame_cfg_t    cfg = DP_CCSDS_415_FRAME_CFG_DEFAULTS;
wfm_frame_desc_layout_t     lay;
if (dp_ccsds_415_frame_desc (&f, &cfg, 17, NULL) != 0
    || dp_wfm_frame_desc_layout (&f.desc, &lay) != 0)
  return 1;
// lay.frame_bits == 275 + 128 + 16 + 40 + 17 * 8 + 16
```
 


    
## Public Functions Documentation




### function dp\_ccsds\_415\_frame\_desc 

_Describe one period of the 415.1-B-1 stimulus with a frame of_ `data_octets` _data octets._
```C++
int dp_ccsds_415_frame_desc (
    dp_ccsds_415_frame_t * f,
    const dp_ccsds_415_frame_cfg_t * cfg,
    size_t data_octets,
    const char ** why
) 
```



Fills `f` in place (see the file comment for the fields). The data field is a WFM\_SEQ\_DATA field: its bits come from the caller at assembly ([**dp\_wfm\_frame\_assemble\_data()**](wfm__frame_8h.md#function-dp_wfm_frame_assemble_data)). The header's `length` is written here, `5 + data_octets + 2 - 1`, since a description is one frame of one known length.




**Parameters:**


* `f` receives the description and the bits it borrows. 
* `cfg` the arbitrary values and the gap; NULL takes [**DP\_CCSDS\_415\_FRAME\_CFG\_DEFAULTS**](ccsds__415__frame_8h.md#define-dp_ccsds_415_frame_cfg_defaults). 
* `data_octets` the data field's octets, [**DP\_CCSDS\_415\_DATA\_MIN**](ccsds__415__frame_8h.md#define-dp_ccsds_415_data_min) to [**DP\_CCSDS\_415\_DATA\_MAX**](ccsds__415__frame_8h.md#define-dp_ccsds_415_data_max). 
* `why` on refusal, receives a static sentence naming the fix; may be NULL. 



**Returns:**

0, or -1 with `why` set: data octets out of range, a value that does not fit its field, a zero gap, or NULL `f`.



```C++
dp_ccsds_415_frame_t f;
const char          *why = NULL;
if (dp_ccsds_415_frame_desc (&f, NULL, 1018, &why) != 0)
  puts (why);   // the data field holds 1 to 1,017 octets ...
```
 


        

<hr>
## Macro Definition Documentation





### define DP\_CCSDS\_415\_ACQ\_BITS 

```C++
#define DP_CCSDS_415_ACQ_BITS `128u`
```



Bits of the acquisition sequence (231.0-B-4 §7.2.2's preferred minimum, 16 octets). 


        

<hr>



### define DP\_CCSDS\_415\_DATA\_MAX 

```C++
#define DP_CCSDS_415_DATA_MAX `1017u`
```



The most data octets a frame carries with its FECF present (232.0-B-4 §4.1.3.1.2): a 1,024-octet frame. 


        

<hr>



### define DP\_CCSDS\_415\_DATA\_MIN 

```C++
#define DP_CCSDS_415_DATA_MIN `1u`
```



The fewest data octets a frame carries (232.0-B-4 §4.1.3.1.2). 


        

<hr>



### define DP\_CCSDS\_415\_FRAME\_CFG\_DEFAULTS 

```C++
#define DP_CCSDS_415_FRAME_CFG_DEFAULTS `{ 42u, 1u, 0u, DP_CCSDS_415_CODE_ONLY_SYM }`
```



§1.4's values: SCID 42, VCID 1, sequence 0, and a gap of the point's code-only symbols ([**DP\_CCSDS\_415\_CODE\_ONLY\_SYM**](ccsds__415_8h.md#define-dp_ccsds_415_code_only_sym)). 


        

<hr>



### define DP\_CCSDS\_415\_HEADER\_BITS 

```C++
#define DP_CCSDS_415_HEADER_BITS `40u`
```



Bits of the transfer frame's primary header (232.0-B-4 §4.1.2). 


        

<hr>



### define DP\_CCSDS\_415\_START\_BITS 

```C++
#define DP_CCSDS_415_START_BITS `16u`
```



Bits of the start sequence. 


        

<hr>



### define DP\_CCSDS\_415\_START\_SEQ 

```C++
#define DP_CCSDS_415_START_SEQ `0xEB90u`
```



The start sequence, EB90, sent from its most significant bit. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ccsds/ccsds_415_frame.h`

