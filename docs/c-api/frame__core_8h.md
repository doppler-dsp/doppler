

# File frame\_core.h



[**FileList**](files.md) **>** [**frame**](dir_00858a83d5a24a6fcf61a222bafb8b7f.md) **>** [**frame\_core.h**](frame__core_8h.md)

[Go to the source code of this file](frame__core_8h_source.md)

_A frame's bit layout, held as an object so Python can describe one._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "pn/pn_core.h"`
* `#include "gold/gold_core.h"`
* `#include "wfm/wfm_frame.h"`
* `#include "conv/conv_core.h"`
* `#include "rs/rs_core.h"`
* `#include "cvt/cvt_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**frame\_check\_t**](structframe__check__t.md) <br>_What_ [_**frame\_check**_](frame__core_8h.md#function-frame_check) _found, summed across the stages it reversed._ |
| struct | [**frame\_state\_t**](structframe__state__t.md) <br>_Frame state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**frame\_add\_derived**](#function-frame_add_derived) ([**frame\_state\_t**](structframe__state__t.md) \* state, const char \* name, size\_t bits) <br>_Append a named field a stage will fill. Returns its index, or -1._  |
|  int | [**frame\_add\_field**](#function-frame_add_field) ([**frame\_state\_t**](structframe__state__t.md) \* state, const uint8\_t \* lit, size\_t lit\_len, int kind, size\_t gen\_len, size\_t reps, uint64\_t poly, uint64\_t seed, uint32\_t reg\_bits, int lfsr, uint64\_t taps\_a, uint64\_t seed\_a, uint64\_t taps\_b, uint64\_t seed\_b, uint32\_t derived\_by, size\_t derived\_bits) <br>_Append one field to a description._  |
|  int | [**frame\_add\_hex**](#function-frame_add_hex) ([**frame\_state\_t**](structframe__state__t.md) \* state, const char \* name, const char \* hex, size\_t reps) <br>_Append a named field from a hex literal. Returns its index, or -1._  |
|  int | [**frame\_add\_stage**](#function-frame_add_stage) ([**frame\_state\_t**](structframe__state__t.md) \* state, int kind, uint32\_t first\_field, uint32\_t n\_fields, uint32\_t depth, uint32\_t emit\_num, uint32\_t emit\_den, uint32\_t unit\_bits) <br>_Append one stage, and the span of fields it covers._  |
|  int | [**frame\_add\_stage\_over**](#function-frame_add_stage_over) ([**frame\_state\_t**](structframe__state__t.md) \* state, int kind, const char \* first, const char \* last, uint32\_t depth, uint32\_t unit\_bits) <br>_Append a stage covering_ `[first .. last]` _by name._ |
|  int | [**frame\_add\_value**](#function-frame_add_value) ([**frame\_state\_t**](structframe__state__t.md) \* state, const char \* name, uint64\_t value, uint32\_t bits, size\_t reps) <br>_Append a named field from an integer. Returns its index, or -1._  |
|  size\_t | [**frame\_bits**](#function-frame_bits) ([**frame\_state\_t**](structframe__state__t.md) \* state, size\_t n, uint8\_t \* out, size\_t max\_out) <br>_Materialise_ `n` _consecutive frames, one bit per byte._ |
|  size\_t | [**frame\_bits\_max\_out**](#function-frame_bits_max_out) ([**frame\_state\_t**](structframe__state__t.md) \* state, size\_t n) <br>_Bits_ [_**frame\_bits**_](frame__core_8h.md#function-frame_bits) _will write for_`n` _frames —_`n * nbits` _._ |
|  int | [**frame\_build**](#function-frame_build) ([**frame\_state\_t**](structframe__state__t.md) \* state) <br>_Lay out and materialise a described frame._  |
|  [**frame\_check\_t**](structframe__check__t.md) | [**frame\_check**](#function-frame_check) ([**frame\_state\_t**](structframe__state__t.md) \* state, const uint8\_t \* rx\_bits, size\_t rx\_bits\_len) <br>_Undo the description's stages over a received frame, and report._  |
|  int | [**frame\_crc\_ok**](#function-frame_crc_ok) ([**frame\_state\_t**](structframe__state__t.md) \* state, const uint8\_t \* rx\_bits, size\_t rx\_bits\_len) <br>_Check one received frame's CRC._  |
|  [**frame\_state\_t**](structframe__state__t.md) \* | [**frame\_create**](#function-frame_create) (int preamble\_kind, const uint8\_t \* preamble, size\_t preamble\_len, size\_t preamble\_nbits, size\_t preamble\_reps, uint64\_t preamble\_poly, uint64\_t preamble\_seed, uint32\_t preamble\_reg\_bits, int preamble\_lfsr, uint64\_t preamble\_taps\_a, uint64\_t preamble\_seed\_a, uint64\_t preamble\_taps\_b, uint64\_t preamble\_seed\_b, int sync\_kind, const uint8\_t \* sync, size\_t sync\_len, size\_t sync\_nbits, uint64\_t sync\_poly, uint64\_t sync\_seed, uint32\_t sync\_reg\_bits, int sync\_lfsr, uint64\_t sync\_taps\_a, uint64\_t sync\_seed\_a, uint64\_t sync\_taps\_b, uint64\_t sync\_seed\_b, int payload\_kind, const uint8\_t \* payload, size\_t payload\_len, size\_t payload\_nbits, uint64\_t payload\_poly, uint64\_t payload\_seed, uint32\_t payload\_reg\_bits, int payload\_lfsr, uint64\_t payload\_taps\_a, uint64\_t payload\_seed\_a, uint64\_t payload\_taps\_b, uint64\_t payload\_seed\_b, int crc) <br>_Create a frame instance._  |
|  [**frame\_state\_t**](structframe__state__t.md) \* | [**frame\_create\_desc**](#function-frame_create_desc) (int preamble\_kind, const uint8\_t \* preamble, size\_t preamble\_len, size\_t preamble\_nbits, size\_t preamble\_reps, uint64\_t preamble\_poly, uint64\_t preamble\_seed, uint32\_t preamble\_reg\_bits, int preamble\_lfsr, uint64\_t preamble\_taps\_a, uint64\_t preamble\_seed\_a, uint64\_t preamble\_taps\_b, uint64\_t preamble\_seed\_b, int sync\_kind, const uint8\_t \* sync, size\_t sync\_len, size\_t sync\_nbits, uint64\_t sync\_poly, uint64\_t sync\_seed, uint32\_t sync\_reg\_bits, int sync\_lfsr, uint64\_t sync\_taps\_a, uint64\_t sync\_seed\_a, uint64\_t sync\_taps\_b, uint64\_t sync\_seed\_b, int payload\_kind, const uint8\_t \* payload, size\_t payload\_len, size\_t payload\_nbits, uint64\_t payload\_poly, uint64\_t payload\_seed, uint32\_t payload\_reg\_bits, int payload\_lfsr, uint64\_t payload\_taps\_a, uint64\_t payload\_seed\_a, uint64\_t payload\_taps\_b, uint64\_t payload\_seed\_b, int crc) <br>_The same frame, DEFERRED — a description a caller can extend._  |
|  size\_t | [**frame\_deframe**](#function-frame_deframe) ([**frame\_state\_t**](structframe__state__t.md) \* state, const uint8\_t \* rx\_bits, size\_t rx\_bits\_len, uint8\_t \* out, size\_t max\_out) <br>_Undo this description's stages over a received frame — DEFRAME it._  |
|  size\_t | [**frame\_deframe\_max\_out**](#function-frame_deframe_max_out) ([**frame\_state\_t**](structframe__state__t.md) \* state, size\_t rx\_bits\_len) <br>_Max bits_ [_**frame\_deframe()**_](frame__core_8h.md#function-frame_deframe) _writes: the frame's own length._ |
|  void | [**frame\_destroy**](#function-frame_destroy) ([**frame\_state\_t**](structframe__state__t.md) \* state) <br>_Destroy a frame instance and release all memory._  |
|  size\_t | [**frame\_field\_bits**](#function-frame_field_bits) ([**frame\_state\_t**](structframe__state__t.md) \* state, size\_t i) <br>_Bits in field_ `i` _, or 0 if there is no such field._ |
|  int | [**frame\_field\_index**](#function-frame_field_index) ([**frame\_state\_t**](structframe__state__t.md) \* state, const char \* name) <br>_Index of the field called_ `name` _, or -1._ |
|  size\_t | [**frame\_field\_off**](#function-frame_field_off) ([**frame\_state\_t**](structframe__state__t.md) \* state, size\_t i) <br>_Bit offset of field_ `i` _, or 0 if there is no such field._ |
|  [**wfm\_frame\_layout\_t**](structwfm__frame__layout__t.md) | [**frame\_layout**](#function-frame_layout) ([**frame\_state\_t**](structframe__state__t.md) \* state) <br>_Where each field lands, in bits from the start of the frame._  |
|  size\_t | [**frame\_n\_fields**](#function-frame_n_fields) ([**frame\_state\_t**](structframe__state__t.md) \* state) <br>_Fields in the description._  |
|  size\_t | [**frame\_n\_stages**](#function-frame_n_stages) ([**frame\_state\_t**](structframe__state__t.md) \* state) <br>_Stages in the description._  |
|  int | [**frame\_name\_field**](#function-frame_name_field) ([**frame\_state\_t**](structframe__state__t.md) \* state, uint32\_t index, const char \* name) <br>_Give an already-appended field a name, or clear it with_ `""` _._ |
|  size\_t | [**frame\_stage\_bits**](#function-frame_stage_bits) ([**frame\_state\_t**](structframe__state__t.md) \* state, size\_t i) <br>_Bits stage_ `i` _covers; 0 for a stage that did not run._ |
|  size\_t | [**frame\_stage\_first**](#function-frame_stage_first) ([**frame\_state\_t**](structframe__state__t.md) \* state, size\_t i) <br>_First CADU bit stage_ `i` _covers; 0 for a stage that did not run._ |




























## Detailed Description


This is the RECEIVE half of the frame story. `wfm_frame_t` (`wfm/wfm_frame.h`) is what a generator builds a frame from and what `wfm_frame_crc_ok()` scores a received one against, and until now only C could hold one — so `ber`'s frame meter, which exists precisely to turn CRC outcomes into an exact error-rate interval, had no way to be fed from the language most captures are analysed in.


### It owns NO layout



Every decision — where the CRC sits, that it covers the payload alone and nothing else, that a repeated preamble repeats the SAME bits — stays in `wfm_frame.c`. This object is lifecycle and delegation: it copies the caller's literal arrays so the descriptor outlives the call that made it, materialises the frame once, and hands everything else to `wfm_frame_layout()` / `wfm_frame_bits()` / `wfm_frame_crc_ok()`. Re-deriving any of it here would rebuild exactly the TX/RX drift the descriptor was introduced to stop.



### Two lengths per field, and they are not the same length



A field is either a literal array or a handful of numbers a receiver can REGENERATE. So each of the three carries both: `preamble` is the literal and `preamble_len` is its extent, while `preamble_nbits` is how many bits a _generated_ kind should emit. `wfm_seq_t` already names this apart — `reg_bits` is a register width, `len` is an output length — and conflating them is the mistake that documentation exists to prevent.



### The frame is materialised at CREATE



`frame_create()` builds the bits immediately and returns NULL if the descriptor cannot produce them (a literal kind with no array, a PN with no register width, an empty geometry). A descriptor that cannot be materialised is not a frame, and finding that out at construction is what lets the binding raise something better than a failure three calls later.



```C++
// Barker-13 sync over a 16-bit literal payload, with a CRC-16 trailer.
static const uint8_t sync[13]  = {1,1,1,1,1,0,0,1,1,0,1,0,1};
static const uint8_t pay[16]   = {0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1};
frame_state_t *f = frame_create(
    0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // no preamble
    0, sync, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0,     // literal sync
    0, pay, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0,      // literal payload
    1);                                          // crc16
uint8_t *b = malloc(frame_bits_max_out(f, 1));
size_t   n = frame_bits(f, 1, b, f->nbits);      // 13 + 16 + 16 == 45
frame_crc_ok(f, b, n);                           // 1 — it is its own truth
free(b);
frame_destroy(f);
```





**See also:** docs/design/rx-test.md section 7 




    
## Public Functions Documentation




### function frame\_add\_derived 

_Append a named field a stage will fill. Returns its index, or -1._ 
```C++
int frame_add_derived (
    frame_state_t * state,
    const char * name,
    size_t bits
) 
```



A field with a declared length and no source: a CRC trailer, a block of check symbols. Its producer is wired by [**frame\_add\_stage\_over**](frame__core_8h.md#function-frame_add_stage_over) rather than named here, because no stage exists yet when the field it derives is appended — fields are ordered by POSITION and stages by APPLICATION.




**Parameters:**


* `state` the frame. 
* `name` the field's name, or NULL for anonymous. 
* `bits` its length, which its stage decides and the caller states.


```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> e = np.empty(0, np.uint8)
>>> d = FrameDesc(e, e, e)
>>> d.add_field(np.array([1, 0, 1, 0], np.uint8))
0
>>> d.name_field(0, "payload")
0
>>> d.add_derived("crc", 16)          # a stage will fill it
1
```
 


        

<hr>



### function frame\_add\_field 

_Append one field to a description._ 
```C++
int frame_add_field (
    frame_state_t * state,
    const uint8_t * lit,
    size_t lit_len,
    int kind,
    size_t gen_len,
    size_t reps,
    uint64_t poly,
    uint64_t seed,
    uint32_t reg_bits,
    int lfsr,
    uint64_t taps_a,
    uint64_t seed_a,
    uint64_t taps_b,
    uint64_t seed_b,
    uint32_t derived_by,
    size_t derived_bits
) 
```



Either the caller supplies the bits (`lit`, or a generated kind) or a stage derives them (`derived_by` non-zero). Both are fields, because both are on the wire.




**Parameters:**


* `state` A frame from [**frame\_create\_desc**](frame__core_8h.md#function-frame_create_desc). 
* `lit` Literal bits, copied here so the description outlives the call; may be NULL. 
* `lit_len` Length of `lit` in bits. 
* `kind` [**wfm\_seq\_kind\_t**](wfm__frame_8h.md#enum-wfm_seq_kind_t) index; 0=literal…3=dotted. 
* `gen_len` Output bits for a GENERATED kind. 
* `reps` Repetitions of the field, verbatim; 0 means one. 
* `poly` PN feedback polynomial; 0 selects the maximal-length. 
* `seed` PN seed; 0 selects 1. 
* `reg_bits` PN/Gold register width. 
* `lfsr` 0=galois, 1=fibonacci. 
* `taps_a` Gold: first register's taps. 
* `seed_a` Gold: first register's seed. 
* `taps_b` Gold: second register's taps. 
* `seed_b` Gold: second register's seed. 
* `derived_by` 0 when the caller supplies this field; otherwise the index of the producing stage, PLUS ONE. 
* `derived_bits` Length of a derived field, in bits. 



**Returns:**

The new field's index, or -1 if the description is full, already built, or the literal could not be copied.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc, ccsds_asm_bits
>>> empty = np.empty(0, np.uint8)
>>> asm = ccsds_asm_bits()
>>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
...                   np.uint8)
>>> data = np.unpackbits(octets).astype(np.uint8)
>>> d = FrameDesc(empty, empty, empty)   # begin from nothing
>>> d.add_field(asm)                     # the attached sync marker
0
>>> d.add_field(data)                    # the transfer frame
1

A field the CALLER does not supply is still a field, because it is still
on the wire -- `derived_by` names the stage that fills it, PLUS ONE:

>>> d.add_field(empty, derived_by=1, derived_bits=32 * 8)
2
```
 


        

<hr>



### function frame\_add\_hex 

_Append a named field from a hex literal. Returns its index, or -1._ 
```C++
int frame_add_hex (
    frame_state_t * state,
    const char * name,
    const char * hex,
    size_t reps
) 
```



Four bits per digit, MSB-first, so an odd number of digits gives a 4-bit tail. The expansion is `cvt`'s `hex_to_bin` rather than a second parser here, so a bad digit is a refusal there and the two cannot disagree about what a marker expands to.




**Parameters:**


* `state` the frame. 
* `name` the field's name, or NULL for anonymous. 
* `hex` NUL-terminated hex digits; no `0x`, no separators. 
* `reps` repetitions; 0 means one.


```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> e = np.empty(0, np.uint8)
>>> d = FrameDesc(e, e, e)
>>> d.add_hex("asm", "1ACFFC1D")     # the CCSDS marker, 4 bits a digit
0
>>> d.build()
>>> d.nbits
32
```
 


        

<hr>



### function frame\_add\_stage 

_Append one stage, and the span of fields it covers._ 
```C++
int frame_add_stage (
    frame_state_t * state,
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


* `state` A frame from [**frame\_create\_desc**](frame__core_8h.md#function-frame_create_desc). 
* `kind` stage kind: a [**wfm\_stage\_kind\_t**](wfm__frame_8h.md#enum-wfm_stage_kind_t) value (0=crc16…4=interleave), or a caller's own from `WFM_STAGE_USER` (0x1000) up, whose kernel then has to reach the assembler through its ops table. 
* `first_field` First field covered. 
* `n_fields` Fields covered; 0 = the stage does not run. 
* `depth` Interleaving depth, for an outer code. 
* `emit_num` Expansion numerator for a stage that emits a NEW stream; 0 when the stage stays inside the frame. 
* `emit_den` Expansion denominator. 
* `unit_bits` INTERLEAVE only: bits per interleaved unit; 0 reads as 1. Match it to the outer code's symbol — permuting octets is what spreads a burst across the codewords of a code over GF(256), and permuting bits inside one spreads a burst within a symbol that is already wrong. 



**Returns:**

The new stage's index, or -1 if the description is full or already built.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc, ccsds_asm_bits
>>> empty = np.empty(0, np.uint8)
>>> asm = ccsds_asm_bits()
>>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
...                   np.uint8)
>>> data = np.unpackbits(octets).astype(np.uint8)
>>> d = FrameDesc(empty, empty, empty)
>>> _ = d.add_field(asm), d.add_field(data)
>>> _ = d.add_field(empty, derived_by=1, derived_bits=32 * 8)
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



### function frame\_add\_stage\_over 

_Append a stage covering_ `[first .. last]` _by name._
```C++
int frame_add_stage_over (
    frame_state_t * state,
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
* `kind` a `wfm_stage_kind_t` index, or a caller's own kind. 
* `first` name of the first field covered. 
* `last` name of the last field covered; may equal `first`. 
* `depth` RS / interleave depth; 0 when unused. 
* `unit_bits` interleave unit; 0 reads as 1. 



**Returns:**

the new stage's index, or -1 on NULL, a full description, a name neither field carries, `last` before `first`, or once built.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> e = np.empty(0, np.uint8)
>>> d = FrameDesc(e, e, e)
>>> d.add_field(np.array([0, 1, 1, 0, 1, 0, 0, 1], np.uint8))
0
>>> d.name_field(0, "payload")
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



### function frame\_add\_value 

_Append a named field from an integer. Returns its index, or -1._ 
```C++
int frame_add_value (
    frame_state_t * state,
    const char * name,
    uint64_t value,
    uint32_t bits,
    size_t reps
) 
```



The form to reach for when a literal fits in 64 bits: exact, and with no failure mode a typo can reach. Wider ones want [**frame\_add\_hex**](frame__core_8h.md#function-frame_add_hex).




**Parameters:**


* `state` the frame. 
* `name` the field's name, or NULL for anonymous. 
* `value` the value; only the low `bits` are read. 
* `bits` 1..64, MSB first. 
* `reps` repetitions; 0 means one.


```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> e = np.empty(0, np.uint8)
>>> d = FrameDesc(e, e, e)
>>> d.add_value("marker", 0x1A, 8)
0
>>> d.build()
>>> d.bits().tolist()                 # MSB first
[0, 0, 0, 1, 1, 0, 1, 0]
```
 


        

<hr>



### function frame\_bits 

_Materialise_ `n` _consecutive frames, one bit per byte._
```C++
size_t frame_bits (
    frame_state_t * state,
    size_t n,
    uint8_t * out,
    size_t max_out
) 
```



`n` counts FRAMES, not bits: a descriptor describes one frame, and a capture holds many. Repeating here rather than making the caller tile it is what matches the generator, whose framed source cycles the same frame to fill whatever length was asked for — so a stream compared against this lines up with the one that was transmitted.




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
>>> empty = np.empty(0, np.uint8)
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(empty, sync, payload, crc="crc16")
>>> d.build()
>>> len(d.bits())        # one frame: 13 + 16 + 16
45
>>> len(d.bits(2))       # n counts FRAMES, tiled the way a capture is
90
```
 


        

<hr>



### function frame\_bits\_max\_out 

_Bits_ [_**frame\_bits**_](frame__core_8h.md#function-frame_bits) _will write for_`n` _frames —_`n * nbits` _._
```C++
size_t frame_bits_max_out (
    frame_state_t * state,
    size_t n
) 
```





**Parameters:**


* `state` The frame. 
* `n` Frame repetitions. 




        

<hr>



### function frame\_build 

_Lay out and materialise a described frame._ 
```C++
int frame_build (
    frame_state_t * state
) 
```



The point at which a description is checked, which for [**frame\_create**](frame__core_8h.md#function-frame_create) happens inside the constructor: a description that cannot produce its own bits is not a frame. It is separate here only because the description arrives over several calls and there is no earlier moment at which it is complete.


The CRC, the outer code, the randomiser and the inner code are all runnable: `ccsds_tm` has no Python binding and is not getting one, so this object is where a caller meets them. A stage naming a kernel nothing here carries is refused rather than skipped, because a stage that quietly did not run produces a frame that still assembles and syncs to nothing.


The inner encoder starts from the all-zero register on every build: a description describes ONE frame. A stream of CADUs sharing one register is a transmitter's job and lives in `ccsds_tm_frame_encode`.




**Parameters:**


* `state` A frame from [**frame\_create\_desc**](frame__core_8h.md#function-frame_create_desc). 



**Returns:**

0 on success, -1 if the description is empty, unbuildable, names a stage with no kernel here, or was already built.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> empty = np.empty(0, np.uint8)
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(empty, sync, payload, crc="crc16")
>>> d.build()
>>> d.nbits                     # 13 + 16 + 16, laid out by build()
45

A description that cannot produce bits is not a frame, and is refused
rather than half-built:

>>> FrameDesc(empty, empty, empty).build()
Traceback (most recent call last):
    ...
ValueError: build failed (rc=-1)
```
 


        

<hr>



### function frame\_check 

_Undo the description's stages over a received frame, and report._ 
```C++
frame_check_t frame_check (
    frame_state_t * state,
    const uint8_t * rx_bits,
    size_t rx_bits_len
) 
```



The receive mirror of [**frame\_bits**](frame__core_8h.md#function-frame_bits), reading the same description — so a transmitter and a receiver holding the same `Frame` cannot disagree about which stage covered what.


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
>>> empty = np.empty(0, np.uint8)
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(empty, sync, payload, crc="crc16")
>>> d.build()
>>> r = d.check(d.bits(1))
>>> r.passed, r.ok, r.units
(1, 1, 1)

Flip a bit the CRC covers and the verdict turns over:

>>> rx = np.asarray(d.bits(1)).copy()
>>> rx[d.field_off(2)] ^= 1
>>> d.check(rx).passed
0

Carrying no check is NOT passing one -- both are reported, separately:

>>> n = FrameDesc(empty, sync, payload, crc="none")
>>> n.build()
>>> c = n.check(n.bits(1))
>>> c.passed, c.checked
(0, 0)
```
 


        

<hr>



### function frame\_crc\_ok 

_Check one received frame's CRC._ 
```C++
int frame_crc_ok (
    frame_state_t * state,
    const uint8_t * rx_bits,
    size_t rx_bits_len
) 
```



**This is what makes a truth-free frame error rate possible.** It needs no payload truth at all, so it works on a real capture, and unlike a self-referenced EVM or a blind M2M4 it still catches a false lock — a rotated constellation fails the check rather than looking clean.




**Parameters:**


* `state` The frame the bits are laid out by. 
* `rx_bits` Received bits, one per byte. 
* `rx_bits_len` How many; must be at least [**frame\_state\_t::nbits**](structframe__state__t.md#variable-nbits). 



**Returns:**

1 pass, 0 fail, -1 if the frame carries no CRC or `rx_bits` is shorter than one frame.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> empty = np.empty(0, np.uint8)
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(empty, sync, payload, crc="crc16")
>>> d.build()
>>> d.crc_ok(d.bits())           # its own bits are its own truth
1
>>> rx = np.asarray(d.bits()).copy()
>>> rx[d.field_off(2)] ^= 1      # flip one payload bit
>>> d.crc_ok(rx)
0
```
 


        

<hr>



### function frame\_create 

_Create a frame instance._ 
```C++
frame_state_t * frame_create (
    int preamble_kind,
    const uint8_t * preamble,
    size_t preamble_len,
    size_t preamble_nbits,
    size_t preamble_reps,
    uint64_t preamble_poly,
    uint64_t preamble_seed,
    uint32_t preamble_reg_bits,
    int preamble_lfsr,
    uint64_t preamble_taps_a,
    uint64_t preamble_seed_a,
    uint64_t preamble_taps_b,
    uint64_t preamble_seed_b,
    int sync_kind,
    const uint8_t * sync,
    size_t sync_len,
    size_t sync_nbits,
    uint64_t sync_poly,
    uint64_t sync_seed,
    uint32_t sync_reg_bits,
    int sync_lfsr,
    uint64_t sync_taps_a,
    uint64_t sync_seed_a,
    uint64_t sync_taps_b,
    uint64_t sync_seed_b,
    int payload_kind,
    const uint8_t * payload,
    size_t payload_len,
    size_t payload_nbits,
    uint64_t payload_poly,
    uint64_t payload_seed,
    uint32_t payload_reg_bits,
    int payload_lfsr,
    uint64_t payload_taps_a,
    uint64_t payload_seed_a,
    uint64_t payload_taps_b,
    uint64_t payload_seed_b,
    int crc
) 
```



Each of the three fields takes the same twelve arguments: a kind, a literal array with its length, a generated output length, and the PN/Gold generator parameters. Only the ones the kind uses are read.




**Parameters:**


* `preamble_kind` Enum index; 0=literal…3=dotted. 
* `preamble` Input uint8\_t array (length passed as preamble\_len). 
* `preamble_len` Literal preamble length in bits. 
* `preamble_nbits` Output bits for a GENERATED preamble kind (default: 0). 
* `preamble_reps` Repetitions of the preamble; 0 = no preamble (default: 0). 
* `preamble_poly` PN feedback polynomial; 0 selects the maximal-length one (default: 0). 
* `preamble_seed` PN seed; 0 selects 1, since an all-zero register is a fixed point (default: 0). 
* `preamble_reg_bits` PN/Gold register width, 1..64 (default: 0). 
* `preamble_lfsr` Enum index; 0=galois…1=fibonacci. 
* `preamble_taps_a` Gold: first register's taps (default: 0). 
* `preamble_seed_a` Gold: first register's seed (default: 0). 
* `preamble_taps_b` Gold: second register's taps (default: 0). 
* `preamble_seed_b` Gold: second register's seed (default: 0). 
* `sync_kind` Enum index; 0=literal…3=dotted. 
* `sync` Input uint8\_t array (length passed as sync\_len). 
* `sync_len` Literal sync-word length in bits. 
* `sync_nbits` Output bits for a GENERATED sync kind (default: 0). 
* `sync_poly` PN feedback polynomial; 0 selects the maximal-length one (default: 0). 
* `sync_seed` PN seed; 0 selects 1 (default: 0). 
* `sync_reg_bits` PN/Gold register width, 1..64 (default: 0). 
* `sync_lfsr` Enum index; 0=galois…1=fibonacci. 
* `sync_taps_a` Gold: first register's taps (default: 0). 
* `sync_seed_a` Gold: first register's seed (default: 0). 
* `sync_taps_b` Gold: second register's taps (default: 0). 
* `sync_seed_b` Gold: second register's seed (default: 0). 
* `payload_kind` Enum index; 0=literal…3=dotted. 
* `payload` Input uint8\_t array (length passed as payload\_len). 
* `payload_len` Literal payload length in bits. 
* `payload_nbits` Output bits for a GENERATED payload kind (default: 0). 
* `payload_poly` PN feedback polynomial; 0 selects the maximal-length one (default: 0). 
* `payload_seed` PN seed; 0 selects 1 (default: 0). 
* `payload_reg_bits` PN/Gold register width, 1..64 (default: 0). 
* `payload_lfsr` Enum index; 0=galois…1=fibonacci. 
* `payload_taps_a` Gold: first register's taps (default: 0). 
* `payload_seed_a` Gold: first register's seed (default: 0). 
* `payload_taps_b` Gold: second register's taps (default: 0). 
* `payload_seed_b` Gold: second register's seed (default: 0). 
* `crc` Enum index; 0=none…1=crc16. 



**Returns:**

Heap-allocated state, or NULL if the geometry is empty or a field cannot be built (a literal with no array, a PN with no register width) — the descriptor is refused rather than half-honoured. 




**Note:**

Caller must call [**frame\_destroy()**](frame__core_8h.md#function-frame_destroy) when done.



```C++
>>> import numpy as np
>>> from doppler.wfm import Frame
>>> empty = np.empty(0, np.uint8)                    # an absent field
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)   # Barker-13
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> f = Frame(empty, sync, payload, crc="crc16")
>>> f.nbits                                          # 13 + 16 + 16
45
>>> f.layout().payload_off
13
>>> f.crc_ok(f.bits())        # its own bits are its own truth
1

A payload a receiver can REGENERATE, rather than one it must be handed:

>>> g = Frame(empty, sync, empty, payload_kind="pn",
...           payload_nbits=1024, payload_reg_bits=10, crc="crc16")
>>> g.nbits
1053
```
 


        

<hr>



### function frame\_create\_desc 

_The same frame, DEFERRED — a description a caller can extend._ 
```C++
frame_state_t * frame_create_desc (
    int preamble_kind,
    const uint8_t * preamble,
    size_t preamble_len,
    size_t preamble_nbits,
    size_t preamble_reps,
    uint64_t preamble_poly,
    uint64_t preamble_seed,
    uint32_t preamble_reg_bits,
    int preamble_lfsr,
    uint64_t preamble_taps_a,
    uint64_t preamble_seed_a,
    uint64_t preamble_taps_b,
    uint64_t preamble_seed_b,
    int sync_kind,
    const uint8_t * sync,
    size_t sync_len,
    size_t sync_nbits,
    uint64_t sync_poly,
    uint64_t sync_seed,
    uint32_t sync_reg_bits,
    int sync_lfsr,
    uint64_t sync_taps_a,
    uint64_t sync_seed_a,
    uint64_t sync_taps_b,
    uint64_t sync_seed_b,
    int payload_kind,
    const uint8_t * payload,
    size_t payload_len,
    size_t payload_nbits,
    uint64_t payload_poly,
    uint64_t payload_seed,
    uint32_t payload_reg_bits,
    int payload_lfsr,
    uint64_t payload_taps_a,
    uint64_t payload_seed_a,
    uint64_t payload_taps_b,
    uint64_t payload_seed_b,
    int crc
) 
```



Every argument [**frame\_create**](frame__core_8h.md#function-frame_create) takes, and the flavor is what it does with them: this one stops before materialising, so the four fields are a STARTING POINT rather than a finished frame. Append with [**frame\_add\_field**](frame__core_8h.md#function-frame_add_field) and [**frame\_add\_stage**](frame__core_8h.md#function-frame_add_stage), then [**frame\_build**](frame__core_8h.md#function-frame_build). Pass empty arrays for all three to begin from nothing.


That is what makes a frame doppler has never heard of describable — a CCSDS CADU among them — without a constructor argument per field of a fixed list. The thirty-odd arguments both constructors take exist because a field count baked into a prototype forces every field's every parameter into it; appending is how a fifth field is added without a signature change.


It is also what makes the CCSDS coding reachable from Python at all. `ccsds_tm` has no binding and is not getting one, so a caller meets the outer code, the randomiser and the inner code by DESCRIBING a CADU rather than through a CCSDS entry point added here.


An empty description is legal here and refused by [**frame\_create**](frame__core_8h.md#function-frame_create), and the difference is where completeness can be judged: that constructor's description is complete when it returns, and this one is not complete until [**frame\_build**](frame__core_8h.md#function-frame_build) is called.


[**frame\_layout**](frame__core_8h.md#function-frame_layout)'s NAMED view reports nothing for a description, on purpose — it would go stale the moment a fifth field is appended, and a stale offset is worse than an absent one. Read a description through [**frame\_field\_off**](frame__core_8h.md#function-frame_field_off) and its siblings.




**Returns:**

An unbuilt description, or NULL on allocation failure or a field that cannot be copied.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> empty = np.empty(0, np.uint8)
>>> d = FrameDesc(empty, empty, empty)          # begin from nothing
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)  # Barker-13
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d.add_field(sync)                           # returns its index
0
>>> d.add_field(payload)
1
>>> d.add_field(empty, derived_by=1, derived_bits=16)  # stage 0, PLUS ONE
2
>>> d.add_stage(kind=0, first_field=1, n_fields=2)   # crc16 over 1..2
0
>>> d.build()
>>> d.nbits                                     # 13 + 16 + 16
45
>>> d.crc_ok(d.bits())        # its own bits are its own truth
1
```
 


        

<hr>



### function frame\_deframe 

_Undo this description's stages over a received frame — DEFRAME it._ 
```C++
size_t frame_deframe (
    frame_state_t * state,
    const uint8_t * rx_bits,
    size_t rx_bits_len,
    uint8_t * out,
    size_t max_out
) 
```



The receive counterpart of building one, and the layer a receiver stops short of: `DsssBurstReceiver` and friends hand back hard and soft decisions for a frame's symbols and make no claim about what they mean, because knowing that needs a description — this one (doppler#1022).


Returns the frame with every reversible stage undone, in place order: a randomiser XORed back, an outer code's repairs APPLIED, a CRC checked. The payload is then a slice, at [**frame\_field\_off**](frame__core_8h.md#function-frame_field_off) of the payload field — which is the caller's arithmetic because a description does not privilege one field over another.


The verdict comes back as read-backs (`ok`, `units`, `checked`, `symbols`), not as a return value, since the return is the bits. Read them exactly as [**frame\_check\_t**](structframe__check__t.md)'s, including the distinction that matters most: `checked == 0` says the description carries no reversible stage at all, which is a different fact from a check that failed.


A stage with no `undo` kernel — a convolutional inner code, which a receiver cannot even frame-sync through — is reported as not checked rather than as passed.




**Parameters:**


* `state` The frame. 
* `rx_bits` Received bits, `frame_bits` of them; treated as a capture and never modified. 
* `rx_bits_len` How many were supplied. 
* `out` Receives the corrected frame. 
* `max_out` Capacity of `out`; see [**frame\_deframe\_max\_out()**](frame__core_8h.md#function-frame_deframe_max_out). 



**Returns:**

Bits written — the frame's length — or 0 if the description is empty or either buffer is too small. 
```C++
>>> import numpy as np
>>> from doppler.wfm import Frame
>>> empty = np.zeros(0, dtype=np.uint8)
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], dtype=np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], dtype=np.uint8)
>>> f = Frame(empty, sync, payload, crc="crc16")
>>> rx = np.asarray(f.bits())          # a clean capture of its own frame
>>> got = np.asarray(f.deframe(rx))
>>> f.rx_ok, f.rx_units, f.rx_checked  # one CRC, and it passed
(1, 1, 1)
>>> off = f.layout().payload_off       # the payload is a SLICE
>>> bool(np.array_equal(got[off:off + 16], payload))
True
>>> rx[off] ^= 1                       # one bit flipped in flight
>>> _ = f.deframe(rx)
>>> f.rx_ok, f.rx_units                # the check notices
(0, 1)
```
 





        

<hr>



### function frame\_deframe\_max\_out 

_Max bits_ [_**frame\_deframe()**_](frame__core_8h.md#function-frame_deframe) _writes: the frame's own length._
```C++
size_t frame_deframe_max_out (
    frame_state_t * state,
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



### function frame\_destroy 

_Destroy a frame instance and release all memory._ 
```C++
void frame_destroy (
    frame_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function frame\_field\_bits 

_Bits in field_ `i` _, or 0 if there is no such field._
```C++
size_t frame_field_bits (
    frame_state_t * state,
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
>>> empty = np.empty(0, np.uint8)
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(empty, sync, payload, crc="crc16")
>>> d.build()
>>> d.field_bits(1), d.field_bits(2), d.field_bits(3)
(13, 16, 16)
```
 


        

<hr>



### function frame\_field\_index 

_Index of the field called_ `name` _, or -1._
```C++
int frame_field_index (
    frame_state_t * state,
    const char * name
) 
```



The one lookup that resolves a name, so every index-taking entry point keeps working unchanged and a rename can only be wrong once. An unnamed field is ANONYMOUS rather than named `""`, so the empty name matches nothing — including a field that has no name.




**Parameters:**


* `state` the frame. 
* `name` the field name. 



**Returns:**

the index, or -1 on NULL or a name no field carries.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> e = np.empty(0, np.uint8)
>>> d = FrameDesc(e, e, e)
>>> d.add_value("sync", 0xABC, 12)
0
>>> d.field_index("sync")
0
>>> d.field_index("absent")
-1
```
 


        

<hr>



### function frame\_field\_off 

_Bit offset of field_ `i` _, or 0 if there is no such field._
```C++
size_t frame_field_off (
    frame_state_t * state,
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
>>> empty = np.empty(0, np.uint8)
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(empty, sync, payload, crc="crc16")
>>> d.build()
>>> d.field_off(1), d.field_off(2), d.field_off(3)
(0, 13, 29)

Field 0 is the absent preamble: an empty field still HAS an index, so the
indices a caller passed to `add_field` keep meaning what they meant.

>>> d.field_off(0), d.field_bits(0)
(0, 0)
```
 


        

<hr>



### function frame\_layout 

_Where each field lands, in bits from the start of the frame._ 
```C++
wfm_frame_layout_t frame_layout (
    frame_state_t * state
) 
```



The offsets a receiver needs to slice a capture, computed by the same code the generator laid the frame out with.




**Parameters:**


* `state` The frame. 



**Returns:**

Where each named field lands.



```C++
>>> import numpy as np
>>> from doppler.wfm import Frame
>>> empty = np.empty(0, np.uint8)
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> lay = Frame(empty, sync, payload, crc="crc16").layout()
>>> lay.sync_off, lay.payload_off, lay.crc_off
(0, 13, 29)
>>> lay.total_bits
45

This is the NAMED view, so it reports the four fields a `Frame` is built
from. A description assembled with `add_field` reports zeros here and is
read with `field_off()` / `field_bits()` instead.
```
 


        

<hr>



### function frame\_n\_fields 

_Fields in the description._ 
```C++
size_t frame_n_fields (
    frame_state_t * state
) 
```





**Parameters:**


* `state` The frame. 



**Returns:**

How many fields the description carries.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> empty = np.empty(0, np.uint8)
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(empty, sync, payload, crc="crc16")
>>> d.n_fields()          # the four named fields, absent ones included
4
```
 


        

<hr>



### function frame\_n\_stages 

_Stages in the description._ 
```C++
size_t frame_n_stages (
    frame_state_t * state
) 
```





**Parameters:**


* `state` The frame. 



**Returns:**

How many stages the description carries.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> empty = np.empty(0, np.uint8)
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(empty, sync, payload, crc="crc16")
>>> d.build()
>>> d.n_stages()         # the CRC is a stage like any other
1
```
 


        

<hr>



### function frame\_name\_field 

_Give an already-appended field a name, or clear it with_ `""` _._
```C++
int frame_name_field (
    frame_state_t * state,
    uint32_t index,
    const char * name
) 
```





**Parameters:**


* `state` the frame. 
* `index` the field to name. 
* `name` the new name; truncated at `WFM_FRAME_NAME_MAX - 1`. 



**Returns:**

0, or -1 on NULL, an out-of-range `index`, a name another field already carries, or once the frame is built.



```C++
>>> import numpy as np
>>> from doppler.wfm import FrameDesc
>>> e = np.empty(0, np.uint8)
>>> d = FrameDesc(e, e, e)
>>> d.add_field(np.array([1, 0, 1, 0], np.uint8))
0
>>> d.name_field(0, "payload")
0
>>> d.field_index("payload")
0
```
 


        

<hr>



### function frame\_stage\_bits 

_Bits stage_ `i` _covers; 0 for a stage that did not run._
```C++
size_t frame_stage_bits (
    frame_state_t * state,
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
>>> empty = np.empty(0, np.uint8)
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(empty, sync, payload, crc="crc16")
>>> d.build()
>>> d.stage_bits(0)      # payload+CRC: what crc16 covered
32
```
 


        

<hr>



### function frame\_stage\_first 

_First CADU bit stage_ `i` _covers; 0 for a stage that did not run._
```C++
size_t frame_stage_first (
    frame_state_t * state,
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
>>> empty = np.empty(0, np.uint8)
>>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
>>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
>>> d = FrameDesc(empty, sync, payload, crc="crc16")
>>> d.build()
>>> d.stage_first(0)     # the CRC starts at the payload, not at bit 0
13
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/frame/frame_core.h`

