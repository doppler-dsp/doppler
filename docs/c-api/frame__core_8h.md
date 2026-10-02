

# File frame\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**frame**](dir_f1fb4d4532bf7057e52e2ed064f78108.md) **>** [**frame\_core.h**](frame__core_8h.md)

[Go to the source code of this file](frame__core_8h_source.md)

_A frame's bit layout, held as an object so Python can describe one._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/pn/pn_core.h"`
* `#include "doppler/gold/gold_core.h"`
* `#include "doppler/wfm/wfm_frame.h"`
* `#include "doppler/conv/conv_core.h"`
* `#include "doppler/rs/rs_core.h"`
* `#include "doppler/cvt/cvt_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_frame\_state\_t**](structdp__frame__state__t.md) <br>_Frame state._  |
| struct | [**frame\_check\_t**](structframe__check__t.md) <br>_What_ [_**dp\_frame\_check**_](frame__core_8h.md#function-dp_frame_check) _found, summed across the stages it reversed._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_frame\_add\_data**](#function-dp_frame_add_data) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, const char \* name, size\_t len) <br>_Append a named DATA field:_ `len` _bits a data source fills, one chunk per frame. Returns its index; -1 in C,_`ValueError` _from Python._ |
|  int | [**dp\_frame\_add\_derived**](#function-dp_frame_add_derived) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, const char \* name, size\_t bits) <br>_Append a named field a stage will fill. Returns its index; -1 in C,_ `ValueError` _from Python._ |
|  int | [**dp\_frame\_add\_field**](#function-dp_frame_add_field) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, const char \* name, const uint8\_t \* bits, size\_t bits\_len) <br>_Append one named field to a description. Returns its index; -1 in C,_ `ValueError` _from Python._ |
|  int | [**dp\_frame\_add\_stage**](#function-dp_frame_add_stage) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, int kind, uint32\_t first\_field, uint32\_t n\_fields, uint32\_t depth, uint32\_t emit\_num, uint32\_t emit\_den, uint32\_t unit\_bits) <br>_Append one stage, and the span of fields it covers._  |
|  int | [**dp\_frame\_add\_stage\_over**](#function-dp_frame_add_stage_over) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, int kind, const char \* first, const char \* last, uint32\_t depth, uint32\_t unit\_bits) <br>_Append a stage covering_ `[first .. last]` _by name._ |
|  size\_t | [**dp\_frame\_bits**](#function-dp_frame_bits) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, size\_t n, uint8\_t \* out, size\_t max\_out) <br>_Materialise_ `n` _consecutive frames, one bit per byte._ |
|  size\_t | [**dp\_frame\_bits\_max\_out**](#function-dp_frame_bits_max_out) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, size\_t n) <br>_Bits_ [_**dp\_frame\_bits**_](frame__core_8h.md#function-dp_frame_bits) _will write for_`n` _frames —_`n * nbits` _._ |
|  int | [**dp\_frame\_build**](#function-dp_frame_build) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state) <br>_Lay out and materialise a described frame._  |
|  [**frame\_check\_t**](structframe__check__t.md) | [**dp\_frame\_check**](#function-dp_frame_check) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, const uint8\_t \* rx\_bits, size\_t rx\_bits\_len) <br>_Undo the description's stages over a received frame, and report._  |
|  int | [**dp\_frame\_crc\_ok**](#function-dp_frame_crc_ok) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, const uint8\_t \* rx\_bits, size\_t rx\_bits\_len) <br>_Check one received frame's CRC._  |
|  [**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* | [**dp\_frame\_create**](#function-dp_frame_create) (const uint8\_t \* preamble, size\_t preamble\_len, const uint8\_t \* sync, size\_t sync\_len, const uint8\_t \* payload, size\_t payload\_len, int crc) <br>_Create a frame instance._  |
|  [**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* | [**dp\_frame\_create\_desc**](#function-dp_frame_create_desc) (const uint8\_t \* preamble, size\_t preamble\_len, const uint8\_t \* sync, size\_t sync\_len, const uint8\_t \* payload, size\_t payload\_len, int crc) <br>_The same frame, DEFERRED — a description a caller can extend._  |
|  size\_t | [**dp\_frame\_deframe**](#function-dp_frame_deframe) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, const uint8\_t \* rx\_bits, size\_t rx\_bits\_len, uint8\_t \* out, size\_t max\_out) <br>_Undo this description's stages over a received frame — DEFRAME it._  |
|  size\_t | [**dp\_frame\_deframe\_max\_out**](#function-dp_frame_deframe_max_out) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, size\_t rx\_bits\_len) <br>_Max bits_ [_**dp\_frame\_deframe()**_](frame__core_8h.md#function-dp_frame_deframe) _writes: the frame's own length._ |
|  void | [**dp\_frame\_destroy**](#function-dp_frame_destroy) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state) <br>_Destroy a frame instance and release all memory._  |
|  size\_t | [**dp\_frame\_field\_bits**](#function-dp_frame_field_bits) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, size\_t i) <br>_Bits in field_ `i` _, or 0 if there is no such field._ |
|  int | [**dp\_frame\_field\_index**](#function-dp_frame_field_index) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, const char \* name) <br>_Index of the field called_ `name` _, or -1._ |
|  size\_t | [**dp\_frame\_field\_off**](#function-dp_frame_field_off) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, size\_t i) <br>_Bit offset of field_ `i` _, or 0 if there is no such field._ |
|  size\_t | [**dp\_frame\_n\_fields**](#function-dp_frame_n_fields) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state) <br>_Fields in the description._  |
|  size\_t | [**dp\_frame\_n\_stages**](#function-dp_frame_n_stages) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state) <br>_Stages in the description._  |
|  int | [**dp\_frame\_name\_field**](#function-dp_frame_name_field) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, uint32\_t index, const char \* name) <br>_Give an already-appended field a name, or clear it with_ `""` _._ |
|  size\_t | [**dp\_frame\_stage\_bits**](#function-dp_frame_stage_bits) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, size\_t i) <br>_Bits stage_ `i` _covers; 0 for a stage that did not run._ |
|  size\_t | [**dp\_frame\_stage\_first**](#function-dp_frame_stage_first) ([**dp\_frame\_state\_t**](structdp__frame__state__t.md) \* state, size\_t i) <br>_First CADU bit stage_ `i` _covers; 0 for a stage that did not run._ |




























## Detailed Description


This is the RECEIVE half of the frame story. A frame description (`wfm_frame_desc_t`, `wfm/wfm_frame.h`) is what a generator builds a frame from and what `dp_wfm_frame_desc_crc_ok()` scores a received one against, and until now only C could hold one — so `ber`'s frame meter, which exists precisely to turn CRC outcomes into an exact error-rate interval, had no way to be fed from the language most captures are analysed in.


### It owns NO layout



Every decision — where the CRC sits, that it covers the payload alone and nothing else, that a repeated preamble repeats the SAME bits — stays in `wfm_frame.c`. This object is lifecycle and delegation: it copies the caller's literal arrays so the description outlives the call that made it, describes them with `dp_wfm_frame_fixed()`, materialises the frame once, and hands everything else to `dp_wfm_frame_desc_layout()` / `dp_wfm_frame_assemble()` / `dp_wfm_frame_desc_crc_ok()`. Re-deriving any of it here would rebuild exactly the TX/RX drift the description was introduced to stop.



### It takes BITS



Each field is an unpacked bit array, one bit per byte, and nothing else: no kind, no generator parameters, no repetition count. Every other form reaches it through a helper that returns bits  `field_bits()` for the Field text form (`pn:1023:10`, `0x1ACFFC1D`, `*4`), `cvt`'s `hex_to_bin` and `bytes_to_bin` for hex and packed octets  so this object has one constructor shape and no dispatch (docs/design/frame-description.md §F.3). An element that is not 0 or 1 is REFUSED rather than masked: a byte of 101 is what a digit string becomes when it is passed where bits belong, and masking would make that mistake a valid-looking field.



### The frame is materialised at CREATE



`dp_frame_create()` builds the bits immediately and returns NULL if they cannot be built (an element that is not a bit, an empty geometry). A frame that cannot be materialised is not a frame, and finding that out at construction is what lets the binding raise something better than a failure three calls later.



```C++
// Barker-13 sync over a 16-bit literal payload, with a CRC-16 trailer.
static const uint8_t sync[13]  = {1,1,1,1,1,0,0,1,1,0,1,0,1};
static const uint8_t pay[16]   = {0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1};
dp_frame_state_t *f = dp_frame_create(NULL, 0,     // no preamble
                                      sync, 13,
                                      pay, 16,
                                      1);           // crc16
uint8_t *b = malloc(dp_frame_bits_max_out(f, 1));
size_t   n = dp_frame_bits(f, 1, b, f->nbits);     // 13 + 16 + 16 == 45
dp_frame_crc_ok(f, b, n);                           // 1 -- its own truth
free(b);
dp_frame_destroy(f);
```





**See also:** docs/design/rx-test.md section 7 




    
## Public Functions Documentation




### function dp\_frame\_add\_data 

_Append a named DATA field:_ `len` _bits a data source fills, one chunk per frame. Returns its index; -1 in C,_`ValueError` _from Python._
```C++
int dp_frame_add_data (
    dp_frame_state_t * state,
    const char * name,
    size_t len
) 
```



The object spelling of the Field text `data:LEN`, and the same field: a WFM\_SEQ\_DATA sequence of length `len`, which is exactly what `dp_wfm_field_parse("data:LEN")` produces for a scene's or the CLI's frame. The description knows the field's length and never its bits: a transmitter draws them from its data source at each frame (a source's `data=`), and a CRC or outer code covering the field covers that frame's chunk. It is a method rather than Field text in [**dp\_frame\_add\_field**](frame__core_8h.md#function-dp_frame_add_field) because an object takes bits, and a data field has none (rx-frame-description.md, D5).


A frame draws from one data source, so a description carries at most one data field; a second is refused where geometry is judged, by the layout, as it is for every other face.




**Parameters:**


* `state` A frame from [**dp\_frame\_create\_desc**](frame__core_8h.md#function-dp_frame_create_desc). 
* `name` The field's name, or NULL/"" for anonymous; a name another field carries is refused. 
* `len` Bits per frame, `LEN` in `data:LEN`: from 1 to [**WFM\_FIELD\_MAX\_BITS**](wfm__frame_8h.md#define-wfm_field_max_bits), the bound the Field grammar puts on the text form. 



**Returns:**

The new field's index, or -1 if `len` is out of range, the description is full or already built, or the name is taken.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc, STAGE_CRC16, Segment, Composer
>>> d = FrameDesc()
>>> d.add_field("sync", np.array([1, 1, 1, 0, 0, 1, 0], np.uint8))
0
>>> d.add_data("payload", 8)          # 8 bits of the data source a frame
1
>>> d.add_derived("crc", 16)
2
>>> d.add_stage_over(STAGE_CRC16, "payload", "crc")
0
>>> seg = Segment(type="bits", sps=1, modulation="bpsk", frame=d,
...               data=np.unpackbits(np.array([0xA5, 0x3C], np.uint8)))
>>> len(Composer([seg]).compose())    # two frames of 7 + 8 + 16 bits
62
```
 


        

<hr>



### function dp\_frame\_add\_derived 

_Append a named field a stage will fill. Returns its index; -1 in C,_ `ValueError` _from Python._
```C++
int dp_frame_add_derived (
    dp_frame_state_t * state,
    const char * name,
    size_t bits
) 
```



A field with a declared length and no source: a CRC trailer, a block of check symbols. Its producer is wired by [**dp\_frame\_add\_stage\_over**](frame__core_8h.md#function-dp_frame_add_stage_over) rather than named here, because no stage exists yet when the field it derives is appended — fields are ordered by POSITION and stages by APPLICATION.




**Parameters:**


* `state` the frame. 
* `name` the field's name, or NULL for anonymous. 
* `bits` its length, which its stage decides and the caller states.


```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> d = FrameDesc()
>>> d.add_field("payload", np.array([1, 0, 1, 0], np.uint8))
0
>>> d.add_derived("crc", 16)          # a stage will fill it
1
```
 


        

<hr>



### function dp\_frame\_add\_field 

_Append one named field to a description. Returns its index; -1 in C,_ `ValueError` _from Python._
```C++
int dp_frame_add_field (
    dp_frame_state_t * state,
    const char * name,
    const uint8_t * bits,
    size_t bits_len
) 
```



The field is bits and nothing else, copied here so the description outlives the call. A field a STAGE fills is appended with [**dp\_frame\_add\_derived**](frame__core_8h.md#function-dp_frame_add_derived) instead, because the caller has no bits for it.




**Parameters:**


* `state` A frame from [**dp\_frame\_create\_desc**](frame__core_8h.md#function-dp_frame_create_desc). 
* `name` The field's name, or NULL/"" for anonymous; a name another field carries is refused. 
* `bits` The bits, one per element, each 0 or 1. 
* `bits_len` How many; 0 is refused (an empty field is no field). 



**Returns:**

The new field's index, or -1 if the description is full or already built, the name is taken, or an element is not a bit.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> from doppler.ccsds import asm_bits
>>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
...                   np.uint8)
>>> d = FrameDesc()                      # begin from nothing
>>> d.add_field("asm", asm_bits())       # the attached sync marker
0
>>> d.add_field("data", np.unpackbits(octets))   # the transfer frame
1
>>> d.field_index("data")
1
```
 


        

<hr>



### function dp\_frame\_add\_stage 

_Append one stage, and the span of fields it covers._ 
```C++
int dp_frame_add_stage (
    dp_frame_state_t * state,
    int kind,
    uint32_t first_field,
    uint32_t n_fields,
    uint32_t depth,
    uint32_t emit_num,
    uint32_t emit_den,
    uint32_t unit_bits
) 
```



`n_fields` is the load-bearing part and 0 means the stage does not run. A stage that inherited "everything before me" instead of declaring its cover is the representation that cannot express a CCSDS CADU — see `wfm/wfm_frame.h`.




**Parameters:**


* `state` A frame from [**dp\_frame\_create\_desc**](frame__core_8h.md#function-dp_frame_create_desc). 
* `kind` stage kind: a [**wfm\_stage\_kind\_t**](wfm__frame_8h.md#enum-wfm_stage_kind_t) value (0=crc16…4=interleave), or a caller's own from `WFM_STAGE_USER` (0x1000) up, whose kernel then has to reach the assembler through its ops table. 
* `first_field` First field covered. 
* `n_fields` Fields covered; 0 = the stage does not run. 
* `depth` Interleaving depth, for an outer code. 
* `emit_num` Expansion numerator for a stage that emits a NEW stream; 0 when the stage stays inside the frame. 
* `emit_den` Expansion denominator. 
* `unit_bits` INTERLEAVE only: bits per interleaved unit; 0 reads as 1. Match it to the outer code's symbol — permuting octets is what spreads a burst across the codewords of a code over GF(256), and permuting bits inside one spreads a burst within a symbol that is already wrong. 



**Returns:**

The new stage's index, or -1 if the description is full or already built. The Python binding raises `ValueError` rather than handing back the -1.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> from doppler.ccsds import asm_bits
>>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
...                   np.uint8)
>>> d = FrameDesc()
>>> _ = d.add_field("asm", asm_bits())
>>> _ = d.add_field("data", np.unpackbits(octets))
>>> _ = d.add_derived("parity", 32 * 8)   # the outer code fills it
>>> d.add_stage(1, first_field=1, n_fields=2, depth=1)   # RS(255,223)
0
>>> d.add_stage(2, first_field=1, n_fields=2)            # randomiser
1

Both start at field 1, so both skip the marker -- the cover is DECLARED,
which is the whole reason a CADU is describable here:

>>> d.build()
>>> d.stage_first(0), d.stage_bits(0)
(32, 2040)
```
 


        

<hr>



### function dp\_frame\_add\_stage\_over 

_Append a stage covering_ `[first .. last]` _by name._
```C++
int dp_frame_add_stage_over (
    dp_frame_state_t * state,
    int kind,
    const char * first,
    const char * last,
    uint32_t depth,
    uint32_t unit_bits
) 
```



The cover is the load-bearing part of the representation and this is the form that reads. It wires a derived field's producer for you, which applies the invariant the layout already enforces rather than adding one.




**Parameters:**


* `state` the frame. 
* `kind` a stage kind — `doppler.wfm.STAGE_CRC16` and its siblings, or a caller's own from `STAGE_USER` up. 
* `first` name of the first field covered. 
* `last` name of the last field covered; may equal `first`. 
* `depth` RS / interleave depth; 0 when unused. 
* `unit_bits` interleave unit; 0 reads as 1. 



**Returns:**

the new stage's index, or -1 on NULL, a full description, a name neither field carries, `last` before `first`, or once built. The Python binding raises `ValueError` rather than handing back the -1.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> d = FrameDesc()
>>> d.add_field("payload", np.array([0, 1, 1, 0, 1, 0, 0, 1], np.uint8))
0
>>> d.add_derived("crc", 16)
1
>>> d.add_stage_over(0, "payload", "crc")   # 0 = crc16
0
>>> d.build()
>>> d.crc_ok(d.bits())                # its own bits are its own truth
1
```
 


        

<hr>



### function dp\_frame\_bits 

_Materialise_ `n` _consecutive frames, one bit per byte._
```C++
size_t dp_frame_bits (
    dp_frame_state_t * state,
    size_t n,
    uint8_t * out,
    size_t max_out
) 
```



`n` counts FRAMES, not bits: a descriptor describes one frame, and a capture holds many. It is the truth for a transmitter that sends the same frame n times  a data source of n copies of the payload, one chunk a frame  so a stream compared against this lines up with the one that was transmitted.




**Parameters:**


* `state` The frame. 
* `n` Frame repetitions. 
* `out` Output, one bit per byte. 
* `max_out` Capacity of `out`; the write is truncated to whole frames that fit rather than overrunning. 



**Returns:**

Bits written.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
>>> d.build()
>>> len(d.bits())        # one frame: 13 + 16 + 16
45
>>> len(d.bits(2))       # n counts FRAMES, tiled the way a capture is
90
```
 


        

<hr>



### function dp\_frame\_bits\_max\_out 

_Bits_ [_**dp\_frame\_bits**_](frame__core_8h.md#function-dp_frame_bits) _will write for_`n` _frames —_`n * nbits` _._
```C++
size_t dp_frame_bits_max_out (
    dp_frame_state_t * state,
    size_t n
) 
```





**Parameters:**


* `state` The frame. 
* `n` Frame repetitions. 




        

<hr>



### function dp\_frame\_build 

_Lay out and materialise a described frame._ 
```C++
int dp_frame_build (
    dp_frame_state_t * state
) 
```



The point at which a description is checked, which for [**dp\_frame\_create**](frame__core_8h.md#function-dp_frame_create) happens inside the constructor: a description that cannot produce its own bits is not a frame. It is separate here only because the description arrives over several calls and there is no earlier moment at which it is complete.


The CRC, the outer code, the randomiser and the inner code are all runnable: `ccsds_tm` has no Python binding and is not getting one, so this object is where a caller meets them. A stage naming a kernel nothing here carries is refused rather than skipped, because a stage that quietly did not run produces a frame that still assembles and syncs to nothing.


The inner encoder starts from the all-zero register on every build: a description describes ONE frame. A stream of CADUs sharing one register is a transmitter's job and lives in `dp_ccsds_tm_frame_encode`.




**Parameters:**


* `state` A frame from [**dp\_frame\_create\_desc**](frame__core_8h.md#function-dp_frame_create_desc). 



**Returns:**

0 on success, -1 if the description is empty, unbuildable, names a stage with no kernel here, or was already built. The Python binding raises `ValueError` and returns nothing.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
>>> d.build()
>>> d.nbits                     # 13 + 16 + 16, laid out by build()
45

A description that cannot produce bits is not a frame, and is refused
rather than half-built:

>>> FrameDesc().build()
Traceback (most recent call last):
    ...
ValueError: cannot build: the description is empty, unbuildable, ...
```
 


        

<hr>



### function dp\_frame\_check 

_Undo the description's stages over a received frame, and report._ 
```C++
frame_check_t dp_frame_check (
    dp_frame_state_t * state,
    const uint8_t * rx_bits,
    size_t rx_bits_len
) 
```



The receive mirror of [**dp\_frame\_bits**](frame__core_8h.md#function-dp_frame_bits), reading the same description — so a transmitter and a receiver holding the same `Frame` cannot disagree about which stage covered what.


**This is the truth-free frame error rate on a coded link.** It needs the description and the received bits and no payload truth at all, so it works on a real capture, and unlike a self-referenced EVM it still catches a false lock.


`checked` is smaller than `stages` when the description names a stage the receiver does not reverse here — the inner code is the case, since it is undone before frame synchronisation and a frame checker never sees channel symbols. Such a stage is reported as not checked, never as passed.




**Parameters:**


* `state` The frame the bits are laid out by. 
* `rx_bits` Received bits, one per byte. Copied, not modified. 
* `rx_bits_len` How many; must be at least one frame. 



**Returns:**

The outcome. `passed` is 0 and `checked` is 0 when the description carries no reversible stage at all — "carries no check" is not "the
        check passed", and an FER conflating them would score every unprotected frame as perfect.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
>>> d.build()
>>> r = d.check(d.bits(1))
>>> r.passed, r.ok, r.units
(1, 1, 1)

Flip a bit the CRC covers and the verdict turns over:

>>> rx = np.asarray(d.bits(1)).copy()
>>> rx[d.field_off(d.field_index("payload"))] ^= 1
>>> d.check(rx).passed
0

Carrying no check is NOT passing one -- both are reported, separately:

>>> n = FrameDesc(sync=sync, payload=payload, crc="none")
>>> n.build()
>>> c = n.check(n.bits(1))
>>> c.passed, c.checked
(0, 0)
```
 


        

<hr>



### function dp\_frame\_crc\_ok 

_Check one received frame's CRC._ 
```C++
int dp_frame_crc_ok (
    dp_frame_state_t * state,
    const uint8_t * rx_bits,
    size_t rx_bits_len
) 
```



**This is what makes a truth-free frame error rate possible.** It needs no payload truth at all, so it works on a real capture, and unlike a self-referenced EVM or a blind M2M4 it still catches a false lock — a rotated constellation fails the check rather than looking clean.




**Parameters:**


* `state` The frame the bits are laid out by. 
* `rx_bits` Received bits, one per byte. 
* `rx_bits_len` How many; must be at least [**dp\_frame\_state\_t::nbits**](structdp__frame__state__t.md#variable-nbits). 



**Returns:**

1 pass, 0 fail, -1 if the frame carries no CRC or `rx_bits` is shorter than one frame.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
>>> d.build()
>>> d.crc_ok(d.bits())           # its own bits are its own truth
1
>>> rx = np.asarray(d.bits()).copy()
>>> rx[d.field_off(d.field_index("payload"))] ^= 1   # one payload bit
>>> d.crc_ok(rx)
0
```
 


        

<hr>



### function dp\_frame\_create 

_Create a frame instance._ 
```C++
dp_frame_state_t * dp_frame_create (
    const uint8_t * preamble,
    size_t preamble_len,
    const uint8_t * sync,
    size_t sync_len,
    const uint8_t * payload,
    size_t payload_len,
    int crc
) 
```



Three fields as bits, each optional, and the CRC. An omitted field is absent, which is `wfm_seq_t`'s own spelling of absence (a zero length).




**Parameters:**


* `preamble` Preamble bits, one per element, each 0 or 1; may be empty. A repeated preamble is repeated in its bits. 
* `preamble_len` Its length in bits. 
* `sync` Sync-word bits; may be empty. 
* `sync_len` Its length in bits. 
* `payload` Payload bits; may be empty. 
* `payload_len` Its length in bits. 
* `crc` Enum index; 0=none, 1=crc16 over the payload. 



**Returns:**

Heap-allocated state, or NULL if the geometry is empty or an element is not a bit  the frame is refused rather than half-honoured. 




**Note:**

Caller must call [**dp\_frame\_destroy()**](frame__core_8h.md#function-dp_frame_destroy) when done.



```C++
>>> import numpy as np
>>> from doppler.wfm import Frame
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)   # Barker-13
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> f = Frame(sync=sync, payload=payload, crc="crc16")
>>> f.nbits                                          # 13 + 16 + 16
45
>>> f.field_off(f.field_index("payload"))
13
>>> f.crc_ok(f.bits())        # its own bits are its own truth
1
```
 


        

<hr>



### function dp\_frame\_create\_desc 

_The same frame, DEFERRED — a description a caller can extend._ 
```C++
dp_frame_state_t * dp_frame_create_desc (
    const uint8_t * preamble,
    size_t preamble_len,
    const uint8_t * sync,
    size_t sync_len,
    const uint8_t * payload,
    size_t payload_len,
    int crc
) 
```



Every argument [**dp\_frame\_create**](frame__core_8h.md#function-dp_frame_create) takes, and the flavor is what it does with them: this one stops before materialising, so the fields are a STARTING POINT rather than a finished frame. Append with [**dp\_frame\_add\_field**](frame__core_8h.md#function-dp_frame_add_field), [**dp\_frame\_add\_derived**](frame__core_8h.md#function-dp_frame_add_derived) and [**dp\_frame\_add\_stage\_over**](frame__core_8h.md#function-dp_frame_add_stage_over), then [**dp\_frame\_build**](frame__core_8h.md#function-dp_frame_build). Omit all three fields to begin from nothing.


That is what makes a frame doppler has never heard of describable — a CCSDS CADU among them — without a constructor argument per field of a fixed list: appending is how a fifth field is added without a signature change.


It is also what makes the CCSDS coding reachable from Python at all. `ccsds_tm` has no binding and is not getting one, so a caller meets the outer code, the randomiser and the inner code by DESCRIBING a CADU rather than through a CCSDS entry point added here.


An empty description is legal here and refused by [**dp\_frame\_create**](frame__core_8h.md#function-dp_frame_create), and the difference is where completeness can be judged: that constructor's description is complete when it returns, and this one is not complete until [**dp\_frame\_build**](frame__core_8h.md#function-dp_frame_build) is called.


Read either one through [**dp\_frame\_field\_index**](frame__core_8h.md#function-dp_frame_field_index) and the indexed accessors beside it, [**dp\_frame\_field\_off**](frame__core_8h.md#function-dp_frame_field_off) and its siblings.




**Returns:**

An unbuilt description, or NULL if an element is not a bit.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc, STAGE_CRC16
>>> d = FrameDesc()                             # begin from nothing
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)  # Barker-13
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d.add_field("sync", sync)                   # returns its index
0
>>> d.add_field("payload", payload)
1
>>> d.add_derived("crc", 16)                    # a stage will fill it
2
>>> d.add_stage_over(STAGE_CRC16, "payload", "crc")
0
>>> d.build()
>>> d.nbits                                     # 13 + 16 + 16
45
>>> d.crc_ok(d.bits())        # its own bits are its own truth
1
```
 


        

<hr>



### function dp\_frame\_deframe 

_Undo this description's stages over a received frame — DEFRAME it._ 
```C++
size_t dp_frame_deframe (
    dp_frame_state_t * state,
    const uint8_t * rx_bits,
    size_t rx_bits_len,
    uint8_t * out,
    size_t max_out
) 
```



The receive counterpart of building one, and the layer a receiver stops short of: `DsssBurstReceiver` and friends hand back hard and soft decisions for a frame's symbols and make no claim about what they mean, because knowing that needs a description — this one (doppler#1022).


Returns the frame with every reversible stage undone, in place order: a randomiser XORed back, an outer code's repairs APPLIED, a CRC checked. The payload is then a slice, at [**dp\_frame\_field\_off**](frame__core_8h.md#function-dp_frame_field_off) of the payload field — which is the caller's arithmetic because a description does not privilege one field over another.


The verdict comes back as read-backs (`ok`, `units`, `checked`, `symbols`), not as a return value, since the return is the bits. Read them exactly as [**frame\_check\_t**](structframe__check__t.md)'s, including the distinction that matters most: `checked == 0` says the description carries no reversible stage at all, which is a different fact from a check that failed.


A stage with no `undo` kernel — a convolutional inner code, which a receiver cannot even frame-sync through — is reported as not checked rather than as passed.




**Parameters:**


* `state` The frame. 
* `rx_bits` Received bits, `frame_bits` of them; treated as a capture and never modified. 
* `rx_bits_len` How many were supplied. 
* `out` Receives the corrected frame. 
* `max_out` Capacity of `out`; see [**dp\_frame\_deframe\_max\_out()**](frame__core_8h.md#function-dp_frame_deframe_max_out). 



**Returns:**

Bits written — the frame's length — or 0 if the description is empty or either buffer is too small. 
```C++
>>> import numpy as np
>>> from doppler.wfm import Frame
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], dtype=np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], dtype=np.uint8)
>>> f = Frame(sync=sync, payload=payload, crc="crc16")
>>> rx = np.asarray(f.bits())          # a clean capture of its own frame
>>> got = np.asarray(f.deframe(rx))
>>> f.rx_ok, f.rx_units, f.rx_checked  # one CRC, and it passed
(1, 1, 1)
>>> off = f.field_off(f.field_index("payload"))   # a SLICE
>>> bool(np.array_equal(got[off:off + 16], payload))
True
>>> rx[off] ^= 1                       # one bit flipped in flight
>>> _ = f.deframe(rx)
>>> f.rx_ok, f.rx_units                # the check notices
(0, 1)
```
 





        

<hr>



### function dp\_frame\_deframe\_max\_out 

_Max bits_ [_**dp\_frame\_deframe()**_](frame__core_8h.md#function-dp_frame_deframe) _writes: the frame's own length._
```C++
size_t dp_frame_deframe_max_out (
    dp_frame_state_t * state,
    size_t rx_bits_len
) 
```



Size a `deframe()` buffer with this. The bound is the DESCRIPTION's, not the input's: a frame is as long as its fields say, so how many bits were received does not change how many come back.




**Parameters:**


* `state` The frame. 
* `rx_bits_len` How many bits are on offer. Ignored, for the reason above; it is in the signature because the binding's capacity call passes the input's length. 



**Returns:**

The frame's length in bits, or 0 for an empty description. 





        

<hr>



### function dp\_frame\_destroy 

_Destroy a frame instance and release all memory._ 
```C++
void dp_frame_destroy (
    dp_frame_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_frame\_field\_bits 

_Bits in field_ `i` _, or 0 if there is no such field._
```C++
size_t dp_frame_field_bits (
    dp_frame_state_t * state,
    size_t i
) 
```





**Parameters:**


* `state` The frame. 
* `i` Field index. 



**Returns:**

The field's length in bits.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
>>> d.build()
>>> d.field_bits(0), d.field_bits(1), d.field_bits(2)
(13, 16, 16)
```
 


        

<hr>



### function dp\_frame\_field\_index 

_Index of the field called_ `name` _, or -1._
```C++
int dp_frame_field_index (
    dp_frame_state_t * state,
    const char * name
) 
```



The one lookup that resolves a name, so every index-taking entry point keeps working unchanged and a rename can only be wrong once. An unnamed field is ANONYMOUS rather than named `""`, so the empty name matches nothing — including a field that has no name.




**Parameters:**


* `state` the frame. 
* `name` the field name. 



**Returns:**

the index, or -1 on NULL or a name no field carries. This is the one verb whose -1 survives into Python: a name that matches nothing is an ANSWER, not a refusal, so there is nothing to raise about.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> d = FrameDesc()
>>> d.add_field("sync", np.array([1,0,1,0,1,0,1,1,1,1,0,0], np.uint8))
0
>>> d.field_index("sync")
0
>>> d.field_index("absent")
-1
```
 


        

<hr>



### function dp\_frame\_field\_off 

_Bit offset of field_ `i` _, or 0 if there is no such field._
```C++
size_t dp_frame_field_off (
    dp_frame_state_t * state,
    size_t i
) 
```





**Parameters:**


* `state` The frame. 
* `i` Field index. 



**Returns:**

Bits from the start of the frame.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
>>> d.build()
>>> d.field_off(0), d.field_off(1), d.field_off(2)
(0, 13, 29)

An absent field has no index: no preamble was given, so field 0 is the
sync word. Ask for a field by name rather than by position, and an index
past the end is 0.

>>> d.field_off(d.field_index("crc")), d.field_off(7)
(29, 0)
```
 


        

<hr>



### function dp\_frame\_n\_fields 

_Fields in the description._ 
```C++
size_t dp_frame_n_fields (
    dp_frame_state_t * state
) 
```





**Parameters:**


* `state` The frame. 



**Returns:**

How many fields the description carries.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
>>> d.n_fields()          # sync, payload, crc -- no preamble was given
3
```
 


        

<hr>



### function dp\_frame\_n\_stages 

_Stages in the description._ 
```C++
size_t dp_frame_n_stages (
    dp_frame_state_t * state
) 
```





**Parameters:**


* `state` The frame. 



**Returns:**

How many stages the description carries.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
>>> d.build()
>>> d.n_stages()         # the CRC is a stage like any other
1
```
 


        

<hr>



### function dp\_frame\_name\_field 

_Give an already-appended field a name, or clear it with_ `""` _._
```C++
int dp_frame_name_field (
    dp_frame_state_t * state,
    uint32_t index,
    const char * name
) 
```





**Parameters:**


* `state` the frame. 
* `index` the field to name. 
* `name` the new name; truncated at `WFM_FRAME_NAME_MAX - 1`. 



**Returns:**

0, or -1 on NULL, an out-of-range `index`, a name another field already carries, or once the frame is built. It is a command rather than a query, so the Python binding raises `ValueError` on the -1 and returns nothing on the 0.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> d = FrameDesc()
>>> d.add_field("", np.array([1, 0, 1, 0], np.uint8))   # anonymous
0
>>> d.name_field(0, "payload")
>>> d.field_index("payload")
0
```
 


        

<hr>



### function dp\_frame\_stage\_bits 

_Bits stage_ `i` _covers; 0 for a stage that did not run._
```C++
size_t dp_frame_stage_bits (
    dp_frame_state_t * state,
    size_t i
) 
```





**Parameters:**


* `state` The frame. 
* `i` Stage index. 



**Returns:**

The covered span, in bits.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
>>> d.build()
>>> d.stage_bits(0)      # payload+CRC: what crc16 covered
32
```
 


        

<hr>



### function dp\_frame\_stage\_first 

_First CADU bit stage_ `i` _covers; 0 for a stage that did not run._
```C++
size_t dp_frame_stage_first (
    dp_frame_state_t * state,
    size_t i
) 
```





**Parameters:**


* `state` The frame. 
* `i` Stage index. 



**Returns:**

Bits from the start of the frame.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
>>> d.build()
>>> d.stage_first(0)     # the CRC starts at the payload, not at bit 0
13
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/frame/frame_core.h`

