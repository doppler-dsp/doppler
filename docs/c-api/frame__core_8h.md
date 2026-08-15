

# File frame\_core.h



[**FileList**](files.md) **>** [**frame**](dir_00858a83d5a24a6fcf61a222bafb8b7f.md) **>** [**frame\_core.h**](frame__core_8h.md)

[Go to the source code of this file](frame__core_8h_source.md)

_A frame's bit layout, held as an object so Python can describe one._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "pn/pn_core.h"`
* `#include "gold/gold_core.h"`
* `#include "wfm/wfm_frame.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**frame\_state\_t**](structframe__state__t.md) <br>_Frame state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**frame\_bits**](#function-frame_bits) ([**frame\_state\_t**](structframe__state__t.md) \* state, size\_t n, uint8\_t \* out, size\_t max\_out) <br>_Materialise_ `n` _consecutive frames, one bit per byte._ |
|  size\_t | [**frame\_bits\_max\_out**](#function-frame_bits_max_out) ([**frame\_state\_t**](structframe__state__t.md) \* state, size\_t n) <br>_Bits_ [_**frame\_bits**_](frame__core_8h.md#function-frame_bits) _will write for_`n` _frames —_`n * nbits` _._ |
|  int | [**frame\_crc\_ok**](#function-frame_crc_ok) ([**frame\_state\_t**](structframe__state__t.md) \* state, const uint8\_t \* rx\_bits, size\_t rx\_bits\_len) <br>_Check one received frame's CRC._  |
|  [**frame\_state\_t**](structframe__state__t.md) \* | [**frame\_create**](#function-frame_create) (int preamble\_kind, const uint8\_t \* preamble, size\_t preamble\_len, size\_t preamble\_nbits, size\_t preamble\_reps, uint64\_t preamble\_poly, uint64\_t preamble\_seed, uint32\_t preamble\_reg\_bits, int preamble\_lfsr, uint64\_t preamble\_taps\_a, uint64\_t preamble\_seed\_a, uint64\_t preamble\_taps\_b, uint64\_t preamble\_seed\_b, int sync\_kind, const uint8\_t \* sync, size\_t sync\_len, size\_t sync\_nbits, uint64\_t sync\_poly, uint64\_t sync\_seed, uint32\_t sync\_reg\_bits, int sync\_lfsr, uint64\_t sync\_taps\_a, uint64\_t sync\_seed\_a, uint64\_t sync\_taps\_b, uint64\_t sync\_seed\_b, int payload\_kind, const uint8\_t \* payload, size\_t payload\_len, size\_t payload\_nbits, uint64\_t payload\_poly, uint64\_t payload\_seed, uint32\_t payload\_reg\_bits, int payload\_lfsr, uint64\_t payload\_taps\_a, uint64\_t payload\_seed\_a, uint64\_t payload\_taps\_b, uint64\_t payload\_seed\_b, int crc) <br>_Create a frame instance._  |
|  void | [**frame\_destroy**](#function-frame_destroy) ([**frame\_state\_t**](structframe__state__t.md) \* state) <br>_Destroy a frame instance and release all memory._  |
|  [**wfm\_frame\_layout\_t**](structwfm__frame__layout__t.md) | [**frame\_layout**](#function-frame_layout) ([**frame\_state\_t**](structframe__state__t.md) \* state) <br>_Where each field lands, in bits from the start of the frame._  |




























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




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/frame/frame_core.h`

