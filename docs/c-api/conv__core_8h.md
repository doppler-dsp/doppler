

# File conv\_core.h



[**FileList**](files.md) **>** [**conv**](dir_dadcb1d47e07452fac6fef665f736671.md) **>** [**conv\_core.h**](conv__core_8h.md)

[Go to the source code of this file](conv__core_8h_source.md)

_Convolutional codes: the code description, the encoder, and the maximum-likelihood decoder that reads the same description._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**conv\_code\_t**](structconv__code__t.md) <br>_A rate-1/n convolutional code._  |
| struct | [**conv\_enc\_t**](structconv__enc__t.md) <br>_Encoder state: the shift register, and nothing else._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) uint32\_t | [**conv\_next\_state**](#function-conv_next_state) (const [**conv\_code\_t**](structconv__code__t.md) \* c, uint32\_t state, unsigned bit) <br>_The state reached from_ `state` _on_`bit` _._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) uint32\_t | [**conv\_states**](#function-conv_states) (const [**conv\_code\_t**](structconv__code__t.md) \* c) <br>_Number of trellis states,_ `2^(k-1)` _._ |
|  int | [**dp\_conv\_code\_valid**](#function-dp_conv_code_valid) (const [**conv\_code\_t**](structconv__code__t.md) \* c) <br>_Is_ `c` _a code this file can represent?_ |
|  size\_t | [**dp\_conv\_coded\_len**](#function-dp_conv_coded_len) (const [**conv\_code\_t**](structconv__code__t.md) \* c, unsigned phase, size\_t n\_in) <br>_Symbols transmitted for_ `n_in` _input bits starting at_`phase` _._ |
|  void | [**dp\_conv\_enc\_init**](#function-dp_conv_enc_init) ([**conv\_enc\_t**](structconv__enc__t.md) \* s) <br>_Reset the encoder to the all-zero state._  |
|  size\_t | [**dp\_conv\_encode**](#function-dp_conv_encode) ([**conv\_enc\_t**](structconv__enc__t.md) \* s, const [**conv\_code\_t**](structconv__code__t.md) \* c, const uint8\_t \* in, size\_t n\_in, uint8\_t \* out, size\_t max\_out) <br>_Encode_ `n_in` _bits, emitting_`n_in * c->n` _symbols, fewer when the code is punctured._ |
|  unsigned | [**dp\_conv\_kept**](#function-dp_conv_kept) (const [**conv\_code\_t**](structconv__code__t.md) \* c, unsigned phase) <br>_Symbols transmitted for the input bit at pattern position_ `phase` _._ |
|  unsigned | [**dp\_conv\_outputs**](#function-dp_conv_outputs) (const [**conv\_code\_t**](structconv__code__t.md) \* c, uint32\_t state, unsigned bit) <br>_The output word for one branch —_ **the** _expression of the code._ |
|  unsigned | [**dp\_conv\_phase\_after**](#function-dp_conv_phase_after) (const [**conv\_code\_t**](structconv__code__t.md) \* c, unsigned phase, size\_t n\_in) <br>_Pattern position after_ `n_in` _more input bits._ |
|  int | [**dp\_conv\_punctured**](#function-dp_conv_punctured) (const [**conv\_code\_t**](structconv__code__t.md) \* c) <br>_Does the code delete any of the mother code's symbols?_  |
|  int | [**dp\_conv\_steps\_for**](#function-dp_conv_steps_for) (const [**conv\_code\_t**](structconv__code__t.md) \* c, unsigned phase, size\_t n\_syms, size\_t \* steps) <br>_How many input bits_ `n_syms` _received symbols hold, from_`phase` _._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CONV\_K\_MAX**](conv__core_8h.md#define-conv_k_max)  `9`<br>_Largest constraint length; 2^(k-1) states, so 256 at k = 9._  |
| define  | [**CONV\_N\_MAX**](conv__core_8h.md#define-conv_n_max)  `6`<br>_Largest number of outputs per input bit (rate 1/n)._  |

## Detailed Description


A rate-1/n convolutional code is four numbers — a constraint length, an output count, a generator polynomial per output, and which outputs are inverted. This file holds that description once and derives everything from it, so an encoder and a decoder cannot disagree about what the code is. A **puncturing pattern** is the fifth part of the description: it deletes symbols from the mother code's output to raise the rate, and the decoder puts them back as erasures. It lives here for the same reason the polynomials do, so the two directions read one pattern.


### The one expression



[**dp\_conv\_outputs**](conv__core_8h.md#function-dp_conv_outputs) is what the family of codes emits, and it is the only place that says so. [**dp\_conv\_encode**](conv__core_8h.md#function-dp_conv_encode) calls it to produce symbols; a Viterbi decoder calls it to build the trellis it searches. An encoder that computed the outputs and a decoder that computed them _again_ would be two implementations of one primitive, and the detail that drifts between them is never the arithmetic — it is a convention.


CCSDS is the worked example and the warning: 131.0-B-3 inverts the second output and most codes invert nothing. Omitting that inversion produces a code that decodes its own output perfectly and interoperates with nothing; measured on the CCSDS code, a decoder that omits it gets **39.2 % of bits wrong**. As a field of [**conv\_code\_t**](structconv__code__t.md) the mistake is a wrong argument. As a constant inside an encoder it is a wrong encoder, and the matching decoder hides it.



### Nothing here is CCSDS



The CCSDS configuration lives in `ccsds_tm/ccsds_tm.h` as `dp_CCSDS_TM_CONV`, because a channel-coding standard picking a code is not the same fact as the code existing. Point this at the deep-space rate-1/6 code, at a K = 9 experiment, or at whatever a caller brings — the trellis is identical and only the table changes.



### Conventions




* **Bits are unpacked**, one per byte in the LSB, matching `dp_wfm_frame_assemble`, `dp_crc16_ccitt` and the `ccsds_tm` kernels.
* **The register holds the newest input in the high stage**: `reg = (reg >> 1) | (b << (k-1))`. A _state_ is the `k-1` bits that survive, so `state + bit -> reg = (bit << (k-1)) | state`, and the next state is `reg >> 1`. Deriving this the other way round yields a trellis that is perfectly self-consistent and decodes nothing a conforming encoder produced, which is why `test_conv_core.c` pins the two against each other rather than each against itself.
* **Polynomials are written as the standard writes them**, left to right with the newest input at the left: CCSDS's `G1 = 1111001` is `0171`.






**See also:** docs/design/viterbi.md for the decoder's design and its measurements. 




    
## Public Functions Documentation




### function conv\_next\_state 

_The state reached from_ `state` _on_`bit` _._
```C++
JM_FORCEINLINE uint32_t conv_next_state (
    const conv_code_t * c,
    uint32_t state,
    unsigned bit
) 
```



Shifts `bit` in at the high stage (bit `k-2` of the result) and drops the oldest input from bit 0.




**Parameters:**


* `c` The code (only `k` is read). 
* `state` Current trellis state: the `k-1` previous input bits. 
* `bit` The new input bit; only bit 0 is used. 



**Returns:**

The next state, in `[0, 2^(k-1))`. 





        

<hr>



### function conv\_states 

_Number of trellis states,_ `2^(k-1)` _._
```C++
JM_FORCEINLINE uint32_t conv_states (
    const conv_code_t * c
) 
```





**Parameters:**


* `c` The code; `k` must already be valid (see [**dp\_conv\_code\_valid()**](conv__core_8h.md#function-dp_conv_code_valid)), it is not checked here. 



**Returns:**

`1 << (k-1)`. 





        

<hr>



### function dp\_conv\_code\_valid 

_Is_ `c` _a code this file can represent?_
```C++
int dp_conv_code_valid (
    const conv_code_t * c
) 
```





**Parameters:**


* `c` The code. 



**Returns:**

Non-zero if usable: `k` in `[2, CONV_K_MAX]`, `n` in `[1, CONV_N_MAX]`, and every polynomial within `k` bits and non-zero. A zero polynomial is an output that carries no information, which is a typo rather than a code. A puncturing pattern must be both-zero (none) or satisfy the rules on [**conv\_code\_t**](structconv__code__t.md). 





        

<hr>



### function dp\_conv\_coded\_len 

_Symbols transmitted for_ `n_in` _input bits starting at_`phase` _._
```C++
size_t dp_conv_coded_len (
    const conv_code_t * c,
    unsigned phase,
    size_t n_in
) 
```



The exact count the encoder writes, so a caller sizes its buffer with this and not with `n_in * n`.




**Parameters:**


* `c` A valid code. 
* `phase` Pattern position of the first input bit. 
* `n_in` Input bits. 



**Returns:**

Symbols transmitted. 





        

<hr>



### function dp\_conv\_enc\_init 

_Reset the encoder to the all-zero state._ 
```C++
void dp_conv_enc_init (
    conv_enc_t * s
) 
```





**Parameters:**


* `s` Encoder state to clear. Must be non-NULL. 




        

<hr>



### function dp\_conv\_encode 

_Encode_ `n_in` _bits, emitting_`n_in * c->n` _symbols, fewer when the code is punctured._
```C++
size_t dp_conv_encode (
    conv_enc_t * s,
    const conv_code_t * c,
    const uint8_t * in,
    size_t n_in,
    uint8_t * out,
    size_t max_out
) 
```



A punctured code emits [**dp\_conv\_coded\_len**](conv__core_8h.md#function-dp_conv_coded_len) symbols: each input bit's outputs in polynomial order, less the ones the pattern deletes.




**Parameters:**


* `s` Encoder state, carried across calls. 
* `c` The code. 
* `in` `n_in` unpacked input bits. 
* `n_in` Number of input bits. 
* `out` Receives the symbols, one per byte, outputs in polynomial order per input bit. 
* `max_out` Capacity of `out`. 



**Returns:**

Symbols written, or 0 if the code is invalid or `max_out` is smaller than [**dp\_conv\_coded\_len**](conv__core_8h.md#function-dp_conv_coded_len) — in which case `out` is untouched. 





        

<hr>



### function dp\_conv\_kept 

_Symbols transmitted for the input bit at pattern position_ `phase` _._
```C++
unsigned dp_conv_kept (
    const conv_code_t * c,
    unsigned phase
) 
```





**Parameters:**


* `c` A valid code. 
* `phase` Position within the period, `[0, punct_period)`; ignored for an unpunctured code. 



**Returns:**

`n` unpunctured, otherwise the mask bits set for that input. 





        

<hr>



### function dp\_conv\_outputs 

_The output word for one branch —_ **the** _expression of the code._
```C++
unsigned dp_conv_outputs (
    const conv_code_t * c,
    uint32_t state,
    unsigned bit
) 
```



Output `j` is bit `j` of the result, matching the order the polynomials are given in and the order [**dp\_conv\_encode**](conv__core_8h.md#function-dp_conv_encode) emits them — so for CCSDS, bit 0 is C1 and bit 1 is C2.




**Parameters:**


* `c` The code. 
* `state` Trellis state: the `k-1` previous input bits. 
* `bit` The new input bit (0 or 1). 



**Returns:**

`n` bits, output `j` in bit `j`, inversion applied. 





        

<hr>



### function dp\_conv\_phase\_after 

_Pattern position after_ `n_in` _more input bits._
```C++
unsigned dp_conv_phase_after (
    const conv_code_t * c,
    unsigned phase,
    size_t n_in
) 
```





**Parameters:**


* `c` A valid code. 
* `phase` Position now. 
* `n_in` Input bits consumed. 



**Returns:**

`(phase + n_in) mod punct_period`, or 0 unpunctured. 





        

<hr>



### function dp\_conv\_punctured 

_Does the code delete any of the mother code's symbols?_ 
```C++
int dp_conv_punctured (
    const conv_code_t * c
) 
```





**Parameters:**


* `c` A valid code. 



**Returns:**

Non-zero if `punct_period` is set and the mask leaves at least one symbol out. An all-ones mask is a pattern that punctures nothing, and is treated as none. 





        

<hr>



### function dp\_conv\_steps\_for 

_How many input bits_ `n_syms` _received symbols hold, from_`phase` _._
```C++
int dp_conv_steps_for (
    const conv_code_t * c,
    unsigned phase,
    size_t n_syms,
    size_t * steps
) 
```



The inverse of [**dp\_conv\_coded\_len**](conv__core_8h.md#function-dp_conv_coded_len). Every input bit keeps at least one symbol, so the answer is unique when it exists.




**Parameters:**


* `c` A valid code. 
* `phase` Pattern position of the first input bit. 
* `n_syms` Received symbols. 
* `steps` Input bits those symbols cover. 



**Returns:**

Non-zero if `n_syms` is exactly a whole number of input bits' symbols, zero if it ends part way through one. 





        

<hr>
## Macro Definition Documentation





### define CONV\_K\_MAX 

_Largest constraint length; 2^(k-1) states, so 256 at k = 9._ 
```C++
#define CONV_K_MAX `9`
```




<hr>



### define CONV\_N\_MAX 

_Largest number of outputs per input bit (rate 1/n)._ 
```C++
#define CONV_N_MAX `6`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/conv/conv_core.h`

