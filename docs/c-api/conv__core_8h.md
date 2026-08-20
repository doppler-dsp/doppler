

# File conv\_core.h



[**FileList**](files.md) **>** [**conv**](dir_779d3467bbcde033259ac71c6a9863bb.md) **>** [**conv\_core.h**](conv__core_8h.md)

[Go to the source code of this file](conv__core_8h_source.md)

_Convolutional codes: the code description, the encoder, and the maximum-likelihood decoder that reads the same description._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
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
|  int | [**conv\_code\_valid**](#function-conv_code_valid) (const [**conv\_code\_t**](structconv__code__t.md) \* c) <br>_Is_ `c` _a code this file can represent?_ |
|  void | [**conv\_enc\_init**](#function-conv_enc_init) ([**conv\_enc\_t**](structconv__enc__t.md) \* s) <br>_Reset the encoder to the all-zero state._  |
|  size\_t | [**conv\_encode**](#function-conv_encode) ([**conv\_enc\_t**](structconv__enc__t.md) \* s, const [**conv\_code\_t**](structconv__code__t.md) \* c, const uint8\_t \* in, size\_t n\_in, uint8\_t \* out, size\_t max\_out) <br>_Encode_ `n_in` _bits, emitting_`n_in * c->n` _symbols._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) uint32\_t | [**conv\_next\_state**](#function-conv_next_state) (const [**conv\_code\_t**](structconv__code__t.md) \* c, uint32\_t state, unsigned bit) <br>_The state reached from_ `state` _on_`bit` _._ |
|  unsigned | [**conv\_outputs**](#function-conv_outputs) (const [**conv\_code\_t**](structconv__code__t.md) \* c, uint32\_t state, unsigned bit) <br>_The output word for one branch —_ **the** _expression of the code._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) uint32\_t | [**conv\_states**](#function-conv_states) (const [**conv\_code\_t**](structconv__code__t.md) \* c) <br>_Number of trellis states,_ `2^(k-1)` _._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CONV\_K\_MAX**](conv__core_8h.md#define-conv_k_max)  `9`<br>_Largest constraint length; 2^(k-1) states, so 256 at k = 9._  |
| define  | [**CONV\_N\_MAX**](conv__core_8h.md#define-conv_n_max)  `6`<br>_Largest number of outputs per input bit (rate 1/n)._  |

## Detailed Description


A rate-1/n convolutional code is four numbers — a constraint length, an output count, a generator polynomial per output, and which outputs are inverted. This file holds that description once and derives everything from it, so an encoder and a decoder cannot disagree about what the code is.


### The one expression



[**conv\_outputs**](conv__core_8h.md#function-conv_outputs) is what the family of codes emits, and it is the only place that says so. [**conv\_encode**](conv__core_8h.md#function-conv_encode) calls it to produce symbols; a Viterbi decoder calls it to build the trellis it searches. An encoder that computed the outputs and a decoder that computed them _again_ would be two implementations of one primitive, and the detail that drifts between them is never the arithmetic — it is a convention.


CCSDS is the worked example and the warning: 131.0-B-3 inverts the second output and most codes invert nothing. Omitting that inversion produces a code that decodes its own output perfectly and interoperates with nothing; measured on the CCSDS code, a decoder that omits it gets **39.2 % of bits wrong**. As a field of [**conv\_code\_t**](structconv__code__t.md) the mistake is a wrong argument. As a constant inside an encoder it is a wrong encoder, and the matching decoder hides it.



### Nothing here is CCSDS



The CCSDS configuration lives in `ccsds_tm/ccsds_tm.h` as `CCSDS_TM_CONV`, because a channel-coding standard picking a code is not the same fact as the code existing. Point this at the deep-space rate-1/6 code, at a K = 9 experiment, or at whatever a caller brings — the trellis is identical and only the table changes.



### Conventions




* **Bits are unpacked**, one per byte in the LSB, matching `wfm_frame_bits`, `dp_crc16_ccitt` and the `ccsds_tm` kernels.
* **The register holds the newest input in the high stage**: `reg = (reg >> 1) | (b << (k-1))`. A _state_ is the `k-1` bits that survive, so `state + bit -> reg = (bit << (k-1)) | state`, and the next state is `reg >> 1`. Deriving this the other way round yields a trellis that is perfectly self-consistent and decodes nothing a conforming encoder produced, which is why `test_conv_core.c` pins the two against each other rather than each against itself.
* **Polynomials are written as the standard writes them**, left to right with the newest input at the left: CCSDS's `G1 = 1111001` is `0171`.






**See also:** docs/design/viterbi.md for the decoder's design and its measurements. 




    
## Public Functions Documentation




### function conv\_code\_valid 

_Is_ `c` _a code this file can represent?_
```C++
int conv_code_valid (
    const conv_code_t * c
) 
```





**Parameters:**


* `c` The code. 



**Returns:**

Non-zero if usable: `k` in `[2, CONV_K_MAX]`, `n` in `[1, CONV_N_MAX]`, and every polynomial within `k` bits and non-zero. A zero polynomial is an output that carries no information, which is a typo rather than a code. 





        

<hr>



### function conv\_enc\_init 

_Reset the encoder to the all-zero state._ 
```C++
void conv_enc_init (
    conv_enc_t * s
) 
```




<hr>



### function conv\_encode 

_Encode_ `n_in` _bits, emitting_`n_in * c->n` _symbols._
```C++
size_t conv_encode (
    conv_enc_t * s,
    const conv_code_t * c,
    const uint8_t * in,
    size_t n_in,
    uint8_t * out,
    size_t max_out
) 
```





**Parameters:**


* `s` Encoder state, carried across calls. 
* `c` The code. 
* `in` `n_in` unpacked input bits. 
* `n_in` Number of input bits. 
* `out` Receives `n_in * c->n` unpacked symbols, outputs in polynomial order per input bit. 
* `max_out` Capacity of `out`. 



**Returns:**

Symbols written, or 0 if the code is invalid or `max_out` is too small — in which case `out` is untouched. 





        

<hr>



### function conv\_next\_state 

_The state reached from_ `state` _on_`bit` _._
```C++
JM_FORCEINLINE uint32_t conv_next_state (
    const conv_code_t * c,
    uint32_t state,
    unsigned bit
) 
```




<hr>



### function conv\_outputs 

_The output word for one branch —_ **the** _expression of the code._
```C++
unsigned conv_outputs (
    const conv_code_t * c,
    uint32_t state,
    unsigned bit
) 
```



Output `j` is bit `j` of the result, matching the order the polynomials are given in and the order [**conv\_encode**](conv__core_8h.md#function-conv_encode) emits them — so for CCSDS, bit 0 is C1 and bit 1 is C2.




**Parameters:**


* `c` The code. 
* `state` Trellis state: the `k-1` previous input bits. 
* `bit` The new input bit (0 or 1). 



**Returns:**

`n` bits, output `j` in bit `j`, inversion applied. 





        

<hr>



### function conv\_states 

_Number of trellis states,_ `2^(k-1)` _._
```C++
JM_FORCEINLINE uint32_t conv_states (
    const conv_code_t * c
) 
```




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
The documentation for this class was generated from the following file `native/inc/conv/conv_core.h`

