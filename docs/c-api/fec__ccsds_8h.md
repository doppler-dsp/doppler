

# File fec\_ccsds.h



[**FileList**](files.md) **>** [**fec**](dir_df2a893a07d8c9ef377268dabdb4859f.md) **>** [**fec\_ccsds.h**](fec__ccsds_8h.md)

[Go to the source code of this file](fec__ccsds_8h_source.md)

_CCSDS TM channel coding — the transforms a transfer frame passes through on its way to symbols._ [More...](#detailed-description)

* `#include "conv/conv_core.h"`
* `#include <stddef.h>`
* `#include <stdint.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  const [**conv\_code\_t**](structconv__code__t.md) | [**FEC\_CCSDS\_CONV**](#variable-fec_ccsds_conv)  <br>_The CCSDS inner code, as a_ [_**conv\_code\_t**_](structconv__code__t.md) _._ |
















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**fec\_ccsds\_asm\_bits**](#function-fec_ccsds_asm_bits) (uint8\_t \* out) <br>_Write the ASM as_ [_**FEC\_CCSDS\_ASM\_BITS**_](fec__ccsds_8h.md#define-fec_ccsds_asm_bits) _unpacked bits._ |
|  void | [**fec\_ccsds\_rand\_seq**](#function-fec_ccsds_rand_seq) (uint8\_t \* out, size\_t n) <br>_Generate the first_ `n` _bits of the randomiser sequence._ |
|  void | [**fec\_ccsds\_randomise**](#function-fec_ccsds_randomise) (uint8\_t \* bits, size\_t n) <br>_Apply the CCSDS pseudo-randomiser to a bit run, in place._  |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**fec\_conv\_max\_out**](#function-fec_conv_max_out) (size\_t n) <br>_Symbols the CCSDS inner code writes for_ `n` _input bits._ |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**FEC\_CCSDS\_ASM**](fec__ccsds_8h.md#define-fec_ccsds_asm)  `0x1ACFFC1DuL`<br>_The CCSDS Attached Sync Marker,_ `0x1ACFFC1D` _._ |
| define  | [**FEC\_CCSDS\_ASM\_BITS**](fec__ccsds_8h.md#define-fec_ccsds_asm_bits)  `32`<br>_Length of_ [_**FEC\_CCSDS\_ASM**_](fec__ccsds_8h.md#define-fec_ccsds_asm) _in bits._ |
| define  | [**FEC\_CONV\_K**](fec__ccsds_8h.md#define-fec_conv_k)  `7`<br>_Constraint length of the inner code (3.3.1): 7._  |

## Detailed Description


This is the coding layer doppler did not have. Nothing in the tree encoded anything before it: no convolutional code, no Reed-Solomon, no interleaver and no randomiser, which is the gap between a test-vector generator and a link waveform.


### Normative references




* **CCSDS 131.0-B-5**, _TM Synchronization and Channel Coding_, Blue Book, September 2023 — the current issue, and what this implements.
* **CCSDS 130.1-G**, _TM Synchronization and Channel Coding—Summary of Concept and Rationale_, Green Book — the worked examples.




Section numbers below are cited from **131.0-B-3** (September 2017), which is the issue that could be read in full while writing this; it is marked HISTORICAL and superseded by B-5. The coding itself is unchanged between them — B-5's additions are a turbo channel interleaver and a reorganisation of slicing — but **a section number is not a value to trust across an issue.** Re-check any citation here against the issue in hand before relying on it.


CCSDS is the prototype because one frame exercises every element at once, and because its stages disagree about what they cover — which is the property a fixed pipeline cannot express:



```C++
transfer frame
  -> RS(255,223) E=16, interleave depth I   expands, interleaves  (4.3)
  -> pseudo-randomiser                      length-preserving     (10)
  -> ASM 0x1ACFFC1D prepended               NOT randomised        (9)
  -> convolutional K=7 r=1/2                expands               (4.1)
```



The ASM is the reason the assembler reports a span per stage ([**fec\_frame\_layout\_t**](structfec__frame__layout__t.md), in [**fec\_frame.h**](fec__frame_8h.md)) rather than a stage order. The randomiser is scoped to "the codeblock, codeword, or Transfer Frame" (10.4.2) and the ASM merely _precedes_ the codeblock (9.4.1), so it falls outside — stated outright in a NOTE: \*"The ASM was not randomized and is
not derandomized."\* A chain of optional stages applied to "the frame" is therefore wrong at exactly one stage boundary and right everywhere else.


That one boundary is worth the care: 8.2.2.2's NOTE, discussing the LDPC CSM, contains the phrase "the ASM is randomized", which reads as the opposite. It is loose wording about why a CSM and an ASM pattern do not collide at the codeblock synchronization level, and the two explicit NOTEs govern. A convention this easy to read backwards is exactly the kind a round-trip test agrees with and a published vector refuses.


**Every kernel here is falsified by a published vector, not by a round trip.** Encode-then-decode agrees with itself for a great many wrong implementations — a mis-ordered tap, a swapped generator polynomial, the wrong field representation — because the decoder inverts whatever the encoder did. The check that bites is the one the standard prints.


Bit convention: every function here takes and returns **unpacked** bits, one per byte in the LSB, which is what `wfm_frame_bits`, `dp_crc16_ccitt` and the spreader already pass around. Packed byte streams are a separate (and wanted) representation; conflating them silently is how MSB-first came to be hardcoded in three places that agree by luck.




**See also:** [**fec\_frame.h**](fec__frame_8h.md) for the assembler, which is where the four stages meet and where the packed/unpacked boundary is crossed. 




    
## Public Attributes Documentation




### variable FEC\_CCSDS\_CONV 

_The CCSDS inner code, as a_ [_**conv\_code\_t**_](structconv__code__t.md) _._
```C++
const conv_code_t FEC_CCSDS_CONV;
```



131.0-B-3 section 3.3.1: the non-systematic rate-1/2 K = 7 code with `G1 = 1111001` (171 octal), `G2 = 1011011` (133 octal), and — the part that is easy to miss — **symbol inversion on the output path of G2**, which is why `invert` is `0x2` and not `0`.


This is a **configuration, not an implementation**: `conv_encode` and `viterbi_decode` do the work and neither knows anything about CCSDS. A standard choosing a code is a different fact from the code existing, and keeping them apart is what stops the polynomials from being written down twice — once in an encoder and once in a decoder, where the inversion is exactly the detail that would drift.


`test_fec_ccsds_conv.c` holds it to the standard's printed impulse response: C1 must trace `G1`, and C2 the **complement** of `G2`.



```C++
conv_enc_t s;
conv_enc_init (&s);
conv_encode (&s, &FEC_CCSDS_CONV, bits, n, sym, sizeof sym);
```
 


        

<hr>
## Public Functions Documentation




### function fec\_ccsds\_asm\_bits 

_Write the ASM as_ [_**FEC\_CCSDS\_ASM\_BITS**_](fec__ccsds_8h.md#define-fec_ccsds_asm_bits) _unpacked bits._
```C++
void fec_ccsds_asm_bits (
    uint8_t * out
) 
```



Figure 9-1 numbers the first transmitted bit of the marker as the most significant bit of `0x1A`, so `out[0]` is that bit and `out[31]` is the least significant bit of `0x1D`.


It is a function rather than a table because the marker is wanted at both ends — the assembler prepends it, a receiver correlates against it — and an MSB-first expansion written out twice is a transcription that can disagree with itself. One expression, one direction.




**Parameters:**


* `out` Receives [**FEC\_CCSDS\_ASM\_BITS**](fec__ccsds_8h.md#define-fec_ccsds_asm_bits) bits, one per byte. 




        

<hr>



### function fec\_ccsds\_rand\_seq 

_Generate the first_ `n` _bits of the randomiser sequence._
```C++
void fec_ccsds_rand_seq (
    uint8_t * out,
    size_t n
) 
```



Exposed separately because the sequence itself is what CCSDS 131.0-B publishes (`FF 48 0E C0 9A ...`), so this is the surface a vector test can check directly rather than inferring it from a XOR.




**Parameters:**


* `out` Receives `n` unpacked bits. 
* `n` Number of bits to generate. 




        

<hr>



### function fec\_ccsds\_randomise 

_Apply the CCSDS pseudo-randomiser to a bit run, in place._ 
```C++
void fec_ccsds_randomise (
    uint8_t * bits,
    size_t n
) 
```



131.0-B-3 section 10.4.1: an 8-stage generator over `h(x) = x^8 + x^7 + x^5 + x^3 + 1`, XORed bit-for-bit onto the data. It is its own inverse, so the receive side calls the same function.


Two properties from 10.4.2 that a caller can get wrong: the generator is **initialised to all ones at the start of each** codeblock, codeword or Transfer Frame — not once per stream — and the sequence **repeats after 255 bits**. Both are handled here because this function owns a whole run; a caller that chunks its data and calls this per chunk would restart the sequence at every chunk boundary and produce a frame no receiver can derandomise.


Its ABSENCE is a measurement hazard rather than a missing feature: a PN payload is already maximally random, so a test built on one cannot tell a present randomiser from a missing one. A run of constant data can, which is why the test for this uses zeros.




**Parameters:**


* `bits` Unpacked bits (one per byte, LSB); modified in place. 
* `n` Number of bits.


```C++
uint8_t frame[1784] = { 0 };
fec_ccsds_randomise (frame, sizeof frame);   // now the published sequence
fec_ccsds_randomise (frame, sizeof frame);   // ...and back to zeros
```
 


        

<hr>
## Public Static Functions Documentation




### function fec\_conv\_max\_out 

_Symbols the CCSDS inner code writes for_ `n` _input bits._
```C++
static inline size_t fec_conv_max_out (
    size_t n
) 
```




<hr>
## Macro Definition Documentation





### define FEC\_CCSDS\_ASM 

_The CCSDS Attached Sync Marker,_ `0x1ACFFC1D` _._
```C++
#define FEC_CCSDS_ASM `0x1ACFFC1DuL`
```



32 bits, transmitted MSB-first, prepended AFTER randomisation. A receiver correlates against it to find the frame, which is precisely why it must not be randomised — it has to look the same in every frame. 


        

<hr>



### define FEC\_CCSDS\_ASM\_BITS 

_Length of_ [_**FEC\_CCSDS\_ASM**_](fec__ccsds_8h.md#define-fec_ccsds_asm) _in bits._
```C++
#define FEC_CCSDS_ASM_BITS `32`
```




<hr>



### define FEC\_CONV\_K 

_Constraint length of the inner code (3.3.1): 7._ 
```C++
#define FEC_CONV_K `7`
```



`K - 1` is the encoder's memory in bits, and that is the quantity a caller reasons with: it is how far into a frame a restarted register can still be wrong, and how much of a stream a decoder needs before its state is determined by the data rather than by where it started. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/fec/fec_ccsds.h`

