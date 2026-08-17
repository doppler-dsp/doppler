

# File ccsds\_tm\_frame.h



[**FileList**](files.md) **>** [**ccsds\_tm**](dir_c2a51186254da91e75ac1924b4969fdd.md) **>** [**ccsds\_tm\_frame.h**](ccsds__tm__frame_8h.md)

[Go to the source code of this file](ccsds__tm__frame_8h_source.md)

_The CCSDS frame assembler — where the ASM goes, and the one place the stages' disagreements about what they cover become visible._ [More...](#detailed-description)

* `#include "ccsds_tm/ccsds_tm.h"`
* `#include "ccsds_tm/ccsds_tm_rs.h"`
* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**ccsds\_tm\_frame\_cfg\_t**](structccsds__tm__frame__cfg__t.md) <br>_Which coding is applied to one Transfer Frame._  |
| struct | [**ccsds\_tm\_frame\_layout\_t**](structccsds__tm__frame__layout__t.md) <br>_The shape of one CADU, and what each stage covered._  |
| struct | [**ccsds\_tm\_frame\_rx\_t**](structccsds__tm__frame__rx__t.md) <br>_What_ [_**ccsds\_tm\_frame\_decode**_](ccsds__tm__frame_8h.md#function-ccsds_tm_frame_decode) _found on the way through._ |
| struct | [**ccsds\_tm\_frame\_span\_t**](structccsds__tm__frame__span__t.md) <br>_A run of CADU bits, as a half-open range_ `[first, first + n)` _._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**ccsds\_tm\_frame\_decode**](#function-ccsds_tm_frame_decode) (const [**ccsds\_tm\_frame\_cfg\_t**](structccsds__tm__frame__cfg__t.md) \* cfg, const uint8\_t \* cadu, size\_t n\_cadu, uint8\_t \* frame, size\_t max\_frame, [**ccsds\_tm\_frame\_rx\_t**](structccsds__tm__frame__rx__t.md) \* rx) <br>_Recover a Transfer Frame from the bits of one CADU._  |
|  size\_t | [**ccsds\_tm\_frame\_encode**](#function-ccsds_tm_frame_encode) (const [**ccsds\_tm\_frame\_cfg\_t**](structccsds__tm__frame__cfg__t.md) \* cfg, [**conv\_enc\_t**](structconv__enc__t.md) \* conv, const uint8\_t \* frame, size\_t frame\_len, uint8\_t \* out, size\_t max\_out) <br>_Encode one Transfer Frame into channel symbols._  |
|  size\_t | [**ccsds\_tm\_frame\_layout**](#function-ccsds_tm_frame_layout) (const [**ccsds\_tm\_frame\_cfg\_t**](structccsds__tm__frame__cfg__t.md) \* cfg, size\_t frame\_len, [**ccsds\_tm\_frame\_layout\_t**](structccsds__tm__frame__layout__t.md) \* out) <br>_Work out the CADU shape for a config, without encoding anything._  |




























## Detailed Description


The four kernels in `ccsds_tm/` are each separately falsifiable against a published value, and each of them is _right_ on its own. What none of them can be wrong about alone is the thing this file exists for: **the stages do not all cover the same bits.**



```C++
transfer frame                      223 * I octets
  -> R-S (255,223) E=16, depth I    4.3, 4.4.1  -> codeblock
  -> pseudo-randomiser              10.3.2      -> randomised codeblock
  -> ASM 0x1ACFFC1D prepended       9.4.1       -> CADU (table 9-1)
  -> convolutional K=7, r=1/2       3.2.1       -> channel symbols
```



Read as a pipeline that is four stages long and correct. Read as _what each stage covers_ it is not, because the marker enters third and one of the two stages after it reaches back over it:



|stage   |covers the ASM   |131.0-B-3    |
|-----|-----|-----|
|Reed-Solomon (outer)   |no   |9.5.1, 9.2.1.5    |
|pseudo-randomiser   |no   |10.3.2, 10.3.4 n.1    |
|convolutional (inner)   |**yes**   |3.2.1, 9.2.1.4   |






9.2.1.5 states both halves in one sentence — \*"the ASM shall be encoded by
the inner code but not by the outer code"\* — and 10.3.4's first NOTE states the third outright: \*"The ASM was not randomized and is not derandomized."\*


That is why [**ccsds\_tm\_frame\_layout\_t**](structccsds__tm__frame__layout__t.md) reports a **span per stage** rather than an order. An order is the representation that cannot express this: any chain of optional transforms applied to "the frame" is right at three stage boundaries and wrong at the fourth, and wrong in the direction that still encodes, still decodes against itself, and syncs to nothing. A span makes the disagreement a value a test can assert, which is what `test_ccsds_tm_frame` does against all three rows above.


### The packed/unpacked boundary lives here



`ccsds_tm_rs.h` takes **packed** symbols, because a Reed-Solomon symbol is a byte; `ccsds_tm.h` takes **unpacked** bits, one per byte, because a randomiser and a convolutional coder are bit machines. Both are right, and the conversion between them belongs to exactly one place rather than being hidden inside a kernel that then only works for one caller.


This is that place: [**ccsds\_tm\_frame\_encode**](ccsds__tm__frame_8h.md#function-ccsds_tm_frame_encode) takes a Transfer Frame as packed octets and returns unpacked channel symbols, the representation `wfm_frame_bits` and the spreader already pass around. Octets go on the wire **MSB-first** — figure 9-1 numbers the first transmitted bit of the ASM as the most significant bit of `0x1A`, and 4.3.9.2 orders an R-S symbol the same way.



### What is not here



Virtual fill (4.4.2's shortened codeblock) is not implemented, so a frame whose length is not exactly `223 * I` octets is **refused** rather than padded. Silently padding would produce a codeblock a receiver configured for the full length cannot parse, which is the failure this whole slice is built to avoid.




**See also:** [**ccsds\_tm.h**](ccsds__tm_8h.md) for the ASM pattern, the randomiser and the inner code. 


**See also:** [**ccsds\_tm\_rs.h**](ccsds__tm__rs_8h.md) for the outer code and the interleaver. 




    
## Public Functions Documentation




### function ccsds\_tm\_frame\_decode 

_Recover a Transfer Frame from the bits of one CADU._ 
```C++
size_t ccsds_tm_frame_decode (
    const ccsds_tm_frame_cfg_t * cfg,
    const uint8_t * cadu,
    size_t n_cadu,
    uint8_t * frame,
    size_t max_frame,
    ccsds_tm_frame_rx_t * rx
) 
```



The mirror of [**ccsds\_tm\_frame\_encode**](ccsds__tm__frame_8h.md#function-ccsds_tm_frame_encode), over the same spans and reading the same [**ccsds\_tm\_frame\_cfg\_t**](structccsds__tm__frame__cfg__t.md) — so the two cannot disagree about which stage covered what, which is the failure `ccsds_tm_frame.h` opens by describing.


### Where the inner code is, and why it is not here



This begins **after** the inner decode and after frame synchronisation: `cadu` is one marker-plus-codeblock, already Viterbi-decoded and already aligned by [**ccsds\_tm\_asm\_find**](ccsds__tm_8h.md#function-ccsds_tm_asm_find). That is not an omission, it is the only place the boundary can go. A Viterbi is streaming and emits its decisions `depth` bits late, so the bits of one CADU are not a function of that CADU's symbols alone; and the marker that says where a CADU _starts_ is only readable once the inner code has been undone. A function taking channel symbols would therefore have to own a decoder, a search window and a buffer — that is a streaming receiver object, and this is the pure per-frame chain it would call.


Consistent with the encoder, where [**conv\_enc\_t**](structconv__enc__t.md) belongs to the caller for the same reason: the inner code is continuous and the frame is not.



### What it undoes



The marker is skipped, the randomiser is re-applied over the block span (10.3.4 — it is involutive, so the same call serves both directions), the block is packed back to octets MSB-first, and with an outer code each of the `rs_depth` interleaved codewords is **decoded** — up to `E = 16` symbol errors per codeword repaired, in place, before anything reads the frame. The Transfer Frame is the information section, which 4.4.1 keeps in the order it entered.




**Parameters:**


* `cfg` The coding that was applied. Must match the transmitter. 
* `cadu` `n_cadu` unpacked CADU bits, one per byte. 
* `n_cadu` Number of CADU bits; must equal the layout's `cadu_bits` for this configuration. 
* `frame` Receives the recovered Transfer Frame, packed octets. 
* `max_frame` Capacity of `frame` in octets. 
* `rx` Receives what was found; may be `NULL`. 



**Returns:**

Transfer Frame octets written, or 0 if the configuration is refused, `n_cadu` is not the layout's CADU length, or `max_frame` is too small — in which case `frame` is untouched.



```C++
const ccsds_tm_frame_cfg_t cfg
    = { .rs_depth = 5, .randomise = 1, .attach_asm = 1,
        .convolutional = 1 };
ccsds_tm_frame_layout_t lay;
ccsds_tm_frame_layout (&cfg, 223 * 5, &lay);

uint8_t        frame[223 * 5];
ccsds_tm_frame_rx_t rx;
// `cadu` is lay.cadu_bits of Viterbi output, ASM-aligned.
const size_t n = ccsds_tm_frame_decode (&cfg, cadu, lay.cadu_bits, frame,
                                   sizeof frame, &rx);
printf ("%zu octets, R-S %u/%u ok, %u symbols repaired\n", n, rx.rs_ok,
        rx.rs_codewords, rx.rs_symbols);
```
 



        

<hr>



### function ccsds\_tm\_frame\_encode 

_Encode one Transfer Frame into channel symbols._ 
```C++
size_t ccsds_tm_frame_encode (
    const ccsds_tm_frame_cfg_t * cfg,
    conv_enc_t * conv,
    const uint8_t * frame,
    size_t frame_len,
    uint8_t * out,
    size_t max_out
) 
```



Runs whichever of the four stages `cfg` selects, each over the bits it covers and no others.


### The inner encoder belongs to the CALLER, because it is continuous



3.3.2 fixes the output as one uninterrupted symbol sequence with no per-frame flush, so the register carries from the last bit of one CADU into the first bit of the next. `conv` is where it lives. Pass the same one to every call in a stream; pass `NULL` for a frame encoded on its own.


The difference is small and it is exactly where it hurts: measured on depth 1, encoding two frames with `NULL` differs from the continuous stream in **6 of 8288 symbols**, all of them in the first 7 symbols of frame 2 — the `K - 1 = 6` bits of register memory, landing on the ASM a receiver is trying to correlate. A matched Viterbi absorbs it, which is what makes this the same class as the inversion on G2 and the dual basis: self-consistent, decodes against a receiver of one's own construction, and not what the standard says.




**Parameters:**


* `cfg` The coding to apply. 
* `conv` Inner-encoder state ([**conv\_enc\_t**](structconv__enc__t.md)) carried across frames, or `NULL` to start from the all-zero register. Ignored when `cfg->convolutional` is 0. 
* `frame` `frame_len` **packed** octets, MSB-first on the wire. 
* `frame_len` Transfer Frame length in octets. 
* `out` Receives the **unpacked** channel symbols, one per byte. 
* `max_out` Capacity of `out` in symbols. The CADU is assembled in the TAIL of this buffer, so a short one is not a truncated result but a write past the end — hence a capacity rather than a comment telling you to call [**ccsds\_tm\_frame\_layout**](ccsds__tm__frame_8h.md#function-ccsds_tm_frame_layout) first. 



**Returns:**

The number of symbols written, or 0 if the configuration is refused or `max_out` is too small — in which case `out` is untouched.



```C++
uint8_t frame[223 * 5];
uint8_t sym[(32 + 255 * 5 * 8) * 2];
const ccsds_tm_frame_cfg_t cfg
    = { .rs_depth = 5, .randomise = 1, .attach_asm = 1,
        .convolutional = 1 };
conv_enc_t conv;
conv_enc_init (&conv);
const size_t n
    = ccsds_tm_frame_encode (&cfg, &conv, frame, sizeof frame, sym,
                        sizeof sym);
```
 



        

<hr>



### function ccsds\_tm\_frame\_layout 

_Work out the CADU shape for a config, without encoding anything._ 
```C++
size_t ccsds_tm_frame_layout (
    const ccsds_tm_frame_cfg_t * cfg,
    size_t frame_len,
    ccsds_tm_frame_layout_t * out
) 
```



This is both the buffer-sizing call and the description of the coverage the encoder will apply, which is deliberate: a caller that sizes its buffer from one function and reasons about coverage from a comment is a caller whose two beliefs can drift apart.




**Parameters:**


* `cfg` The coding to apply. 
* `frame_len` Transfer Frame length in **octets**. 
* `out` Receives the layout; may be `NULL` to ask only for the output length. 



**Returns:**

The number of channel symbols the encode will write, or 0 if the configuration is refused — an interleaving depth outside 4.3.5.1's `{1, 2, 3, 4, 5, 8}`, an empty frame, or a frame that is not exactly `CCSDS_TM_RS_K * rs_depth` octets when the outer code is in use.



```C++
const ccsds_tm_frame_cfg_t cfg
    = { .rs_depth = 5, .randomise = 1, .attach_asm = 1,
        .convolutional = 1 };
const size_t n = ccsds_tm_frame_layout (&cfg, 223 * 5, NULL);
uint8_t *sym = malloc (n);          // n == (32 + 255 * 5 * 8) * 2
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ccsds_tm/ccsds_tm_frame.h`

