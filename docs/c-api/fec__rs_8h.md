

# File fec\_rs.h



[**FileList**](files.md) **>** [**fec**](dir_df2a893a07d8c9ef377268dabdb4859f.md) **>** [**fec\_rs.h**](fec__rs_8h.md)

[Go to the source code of this file](fec__rs_8h_source.md)

_CCSDS Reed-Solomon (255,223) — the outer code, and the one with the conventions that only a published value catches._ [More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**fec\_rs\_codeword\_ok**](#function-fec_rs_codeword_ok) (const uint8\_t \* codeword) <br>_Is this a valid codeword? — all 32 syndromes zero._  |
|  uint8\_t | [**fec\_rs\_conv\_to\_dual**](#function-fec_rs_conv_to_dual) (uint8\_t u) <br>_Convert one symbol from the conventional basis to the dual basis._  |
|  uint8\_t | [**fec\_rs\_dual\_to\_conv**](#function-fec_rs_dual_to_conv) (uint8\_t z) <br>_Convert one symbol from the dual basis back to conventional._  |
|  void | [**fec\_rs\_encode**](#function-fec_rs_encode) (const uint8\_t \* info, uint8\_t \* parity) <br>_Encode one codeword: 223 information symbols in, 32 parity out._  |
|  size\_t | [**fec\_rs\_encode\_block**](#function-fec_rs_encode_block) (const uint8\_t \* info, unsigned depth, uint8\_t \* out) <br>_Encode an interleaved codeblock (4.3.5, 4.4.1)._  |
|  const uint8\_t \* | [**fec\_rs\_generator**](#function-fec_rs_generator) (void) <br>_The 33 coefficients of_ `g(x)` _, in conventional representation._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**FEC\_RS\_2E**](fec__rs_8h.md#define-fec_rs_2e)  `32`<br>_Parity symbols per codeword,_ `2E` _(4.3.2c)._ |
| define  | [**FEC\_RS\_K**](fec__rs_8h.md#define-fec_rs_k)  `223`<br>_Information symbols per codeword when_ `E = 16` _(4.3.2d)._ |
| define  | [**FEC\_RS\_MAX\_DEPTH**](fec__rs_8h.md#define-fec_rs_max_depth)  `8`<br>_Largest interleaving depth 4.3.5.1 allows._  |
| define  | [**FEC\_RS\_N**](fec__rs_8h.md#define-fec_rs_n)  `255`<br>_Symbols per codeword,_ `n = 2^J - 1` _(4.3.2b)._ |

## Detailed Description


CCSDS 131.0-B-3 section 4.3. `J = 8` bits per symbol, `E = 16` correctable symbols, so `n = 255`, `2E = 32` parity symbols and `k = 223`. Systematic.


Three things here are NOT the textbook Reed-Solomon a reader will expect, and each is invisible to an encode/decode round trip because a matched decoder inverts whatever the encoder did:



* **The field is not the usual one.** `F(x) = x^8 + x^7 + x^2 + x + 1` (4.3.3), where most implementations reach for `x^8 + x^4 + x^3 + x^2 + 1` out of habit.
* **The generator's roots are powers of `a^11`, not of `a`** — `g(x) = prod (x - a^(11j))` for `j = 128-E .. 127+E` (4.3.4). The standard notes `a^11` is itself primitive, which is what makes this a legitimate but unusual choice. Consecutive powers of `a` give a perfectly good (255,223) code that no CCSDS receiver can decode.
* **Symbols travel in the DUAL (Berlekamp) basis** — 4.3.9.1 says it _shall_ be used. A conventional-basis codeword is self-consistent and matches no spacecraft.




The oracle for the first two is Annex G, which prints every coefficient of `g(x)`; for the third it is the pair of matrices in 4.3.9.3, whose transcription is checked by requiring the two transforms to invert each other across all 256 symbols.


Bit convention follows the rest of `fec/`: **packed symbols**, one byte per R-S symbol, because a Reed-Solomon symbol IS a byte. That differs from the randomiser and the convolutional coder, which take unpacked bits — the boundary between the two is real and belongs to the frame assembler, not hidden inside a kernel.




**See also:** [**fec\_ccsds.h**](fec__ccsds_8h.md) for the randomiser, the ASM and the inner code. 



    
## Public Functions Documentation




### function fec\_rs\_codeword\_ok 

_Is this a valid codeword? — all 32 syndromes zero._ 
```C++
int fec_rs_codeword_ok (
    const uint8_t * codeword
) 
```



The DEFINING property of the code: a codeword polynomial evaluates to zero at every root of `g(x)`. Checking it needs no decoder and is not a round trip against the encoder's own logic, which is what makes it usable as a test oracle and, later, as a receiver's error detector.




**Parameters:**


* `codeword` 255 symbols in the dual basis: 223 information followed by 32 parity, exactly as transmitted. 



**Returns:**

Non-zero when every syndrome is zero. 





        

<hr>



### function fec\_rs\_conv\_to\_dual 

_Convert one symbol from the conventional basis to the dual basis._ 
```C++
uint8_t fec_rs_conv_to_dual (
    uint8_t u
) 
```



4.3.9.3, first equation: `[z0..z7] = [u7..u0] T`. The returned byte holds `z0` in its most significant bit, because 4.3.9.2 fixes `z0` as the first bit transmitted and this codebase writes MSB-first. 


        

<hr>



### function fec\_rs\_dual\_to\_conv 

_Convert one symbol from the dual basis back to conventional._ 
```C++
uint8_t fec_rs_dual_to_conv (
    uint8_t z
) 
```



4.3.9.3, second equation. Exact inverse of [**fec\_rs\_conv\_to\_dual**](fec__rs_8h.md#function-fec_rs_conv_to_dual), and the test asserts that across all 256 values — which is what catches a single mis-transcribed bit in either matrix. 


        

<hr>



### function fec\_rs\_encode 

_Encode one codeword: 223 information symbols in, 32 parity out._ 
```C++
void fec_rs_encode (
    const uint8_t * info,
    uint8_t * parity
) 
```



Both `info` and `parity` are in the **dual basis**, i.e. exactly what goes on the wire (4.3.9). The conventional-basis arithmetic and the pre/post transformation of figure F-1 happen inside.




**Parameters:**


* `info` 223 information symbols, in transmission order. 
* `parity` Receives 32 parity symbols, following the information. 




        

<hr>



### function fec\_rs\_encode\_block 

_Encode an interleaved codeblock (4.3.5, 4.4.1)._ 
```C++
size_t fec_rs_encode_block (
    const uint8_t * info,
    unsigned depth,
    uint8_t * out
) 
```



Depth `depth` means `depth` codewords are encoded in parallel, with switch S1 handing successive input symbols to successive encoders. Two consequences worth stating because they are what the tests assert:



* the information section comes out **unchanged** — 4.4.1 has S2 reassembling the information "in the same way as they entered", so only the check symbols are rearranged;
* `depth == 1` is the un-interleaved code, which 4.3.5.1 notes outright.




Interleaving is what makes the outer code burst-tolerant: a contiguous burst of `B` symbols lands as `ceil(B / depth)` errors in each codeword, so depth trades no rate at all for a `depth`-fold longer correctable burst.




**Parameters:**


* `info` `FEC_RS_K * depth` information symbols, dual basis. 
* `depth` Interleaving depth; 4.3.5.1 allows 1, 2, 3, 4, 5 and 8. 
* `out` Receives `FEC_RS_N * depth` symbols: the information verbatim, then `FEC_RS_2E * depth` interleaved check symbols. 



**Returns:**

The number of symbols written, or 0 if `depth` is not allowed. 





        

<hr>



### function fec\_rs\_generator 

_The 33 coefficients of_ `g(x)` _, in conventional representation._
```C++
const uint8_t * fec_rs_generator (
    void
) 
```



Exposed because Annex G publishes them, so a caller — or a test — can check this implementation against the standard rather than against itself. `g[i]` is the coefficient of `x^i`; the sequence is palindromic.




**Returns:**

Pointer to `FEC_RS_2E + 1` bytes, valid for the process lifetime. 





        

<hr>
## Macro Definition Documentation





### define FEC\_RS\_2E 

_Parity symbols per codeword,_ `2E` _(4.3.2c)._
```C++
#define FEC_RS_2E `32`
```




<hr>



### define FEC\_RS\_K 

_Information symbols per codeword when_ `E = 16` _(4.3.2d)._
```C++
#define FEC_RS_K `223`
```




<hr>



### define FEC\_RS\_MAX\_DEPTH 

_Largest interleaving depth 4.3.5.1 allows._ 
```C++
#define FEC_RS_MAX_DEPTH `8`
```




<hr>



### define FEC\_RS\_N 

_Symbols per codeword,_ `n = 2^J - 1` _(4.3.2b)._
```C++
#define FEC_RS_N `255`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/fec/fec_rs.h`

