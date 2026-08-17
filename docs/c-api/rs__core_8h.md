

# File rs\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**rs**](dir_a447329db54f84e06767f7e282ab2567.md) **>** [**rs\_core.h**](rs__core_8h.md)

[Go to the source code of this file](rs__core_8h_source.md)

_Reed-Solomon codes: the code description, the encoder, the syndromes and the decoder that corrects — all reading the same description._ [More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**rs\_code\_t**](structrs__code__t.md) <br>_A Reed-Solomon code over_ `GF(2^J)` _._ |
| struct | [**rs\_t**](structrs__t.md) <br>_A code plus the tables derived from it._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**rs\_code\_valid**](#function-rs_code_valid) (const [**rs\_code\_t**](structrs__code__t.md) \* c) <br>_Is_ `c` _a code this file can represent and decode?_ |
|  int | [**rs\_codeword\_ok**](#function-rs_codeword_ok) (const [**rs\_t**](structrs__t.md) \* rs, const uint8\_t \* codeword) <br>_Is this a valid codeword? — every syndrome zero._  |
|  int | [**rs\_decode**](#function-rs_decode) (const [**rs\_t**](structrs__t.md) \* rs, uint8\_t \* codeword) <br>_Correct up to_ `E` _symbol errors, in place._ |
|  void | [**rs\_encode**](#function-rs_encode) (const [**rs\_t**](structrs__t.md) \* rs, const uint8\_t \* info, uint8\_t \* parity) <br>_Encode:_ `k` _information symbols in,_`nroots` _parity symbols out._ |
|  const uint8\_t \* | [**rs\_generator**](#function-rs_generator) (const [**rs\_t**](structrs__t.md) \* rs) <br>_The_ `nroots + 1` _coefficients of_`g(x)` _,_`gen[i]` _for_`x^i` _._ |
|  int | [**rs\_init**](#function-rs_init) ([**rs\_t**](structrs__t.md) \* rs, const [**rs\_code\_t**](structrs__code__t.md) \* c) <br>_Build the tables for_ `c` _into_`rs` _._ |
|  void | [**rs\_syndromes**](#function-rs_syndromes) (const [**rs\_t**](structrs__t.md) \* rs, const uint8\_t \* codeword, uint8\_t \* syn) <br>_The_ `nroots` _syndromes of_`codeword` _._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**RS\_NROOTS\_MAX**](rs__core_8h.md#define-rs_nroots_max)  `64`<br>_Largest number of parity symbols this can represent._  |
| define  | [**RS\_N\_MAX**](rs__core_8h.md#define-rs_n_max)  `255`<br>_Largest codeword,_ `2^RS_SYMBOL_BITS_MAX - 1` _._ |
| define  | [**RS\_SYMBOL\_BITS\_MAX**](rs__core_8h.md#define-rs_symbol_bits_max)  `8`<br>_Largest symbol width; symbols are held one per byte._  |

## Detailed Description


A Reed-Solomon code over `GF(2^J)` is five numbers — a symbol width, a field polynomial, a parity count, a first root and a root stride. This file holds that description once and derives everything from it, so an encoder, a checker and a decoder cannot disagree about what the code is.


### Nothing here is CCSDS



The CCSDS configuration lives in `ccsds_tm/ccsds_tm_rs.h` as `CCSDS_TM_RS`, beside the two things 131.0-B-3 adds that are _not_ properties of the code: the dual-basis symbol representation (4.3.9) and the interleaver (4.4.1). A standard picking a code is not the same fact as the code existing. Point this at RS(204,188) for DVB, at RS(15,11) to check something by hand, or at whatever a caller brings — the arithmetic is identical and only the table changes.



### Two fields that are validated rather than trusted



Both of these produce arithmetic that is entirely self-consistent, so a round trip against a matching encoder cannot see either:



* \*\*`field_poly` must be primitive.\*\* If `a = x` returns to 1 before `n` steps the polynomial generates a subgroup rather than the field, and [**rs\_init**](rs__core_8h.md#function-rs_init) refuses.
* \*\*`gcd(root_stride, n)` must be 1\*\*, or the `nroots` roots are not distinct and the code corrects fewer errors than its parity count claims. CCSDS 4.3.4 states this as a note about `a^11`; for a general implementation it is a condition to check.





### Conventions




* **Symbols are packed, one per byte** — a Reed-Solomon symbol _is_ a byte at `J = 8`, and at `J < 8` it is a byte with the top bits clear. This differs from the bit-oriented `conv` and `ccsds_tm` kernels, and the boundary between the two belongs to the frame assembler.
* **A codeword is `k` information symbols followed by `nroots` parity**, index 0 first on the wire, so index `i` carries `x^(n-1-i)`.
* **The conventional basis throughout.** A symbol representation is a transmission convention, not arithmetic; a caller whose standard uses another one transforms at its own boundary.






**See also:** docs/design/reed-solomon.md for the algebra, the two offsets a textbook omits, and what a decode refusal does and does not mean. 


**See also:** [**ccsds\_tm/ccsds\_tm\_rs.h**](ccsds__tm__rs_8h.md) for the CCSDS configuration. 




    
## Public Functions Documentation




### function rs\_code\_valid 

_Is_ `c` _a code this file can represent and decode?_
```C++
int rs_code_valid (
    const rs_code_t * c
) 
```



Checks the ranges, that `nroots` is even and leaves room for at least one information symbol, and that `gcd(root_stride, n) == 1`. It does **not** check that `field_poly` is primitive — that costs the table build, so [**rs\_init**](rs__core_8h.md#function-rs_init) reports it instead.




**Parameters:**


* `c` The code. 



**Returns:**

Non-zero if usable. 





        

<hr>



### function rs\_codeword\_ok 

_Is this a valid codeword? — every syndrome zero._ 
```C++
int rs_codeword_ok (
    const rs_t * rs,
    const uint8_t * codeword
) 
```





**Parameters:**


* `rs` An initialised code. 
* `codeword` `rs->n` symbols. 



**Returns:**

Non-zero when every syndrome is zero. 





        

<hr>



### function rs\_decode 

_Correct up to_ `E` _symbol errors, in place._
```C++
int rs_decode (
    const rs_t * rs,
    uint8_t * codeword
) 
```



Berlekamp-Massey for the error locator, Chien for the positions and Forney for the magnitudes — see `docs/design/reed-solomon.md` for the derivation, and in particular for the two factors a textbook omits when `first_root != 1` or `root_stride != 1`, both of which produce a decoder that decodes its own encoder perfectly and interoperates with nothing.


**It either refuses or returns a codeword.** When it corrects, the key equation has zeroed every syndrome by construction, so the result passes [**rs\_codeword\_ok**](rs__core_8h.md#function-rs_codeword_ok). There is no third outcome.


A refusal is not the same claim as "more than `E` errors": beyond `E` a bounded-distance decoder can land inside another codeword's sphere and miscorrect — a property of the code, not of this implementation. The protection is accounting at the frame level, which is why this reports a count rather than a verdict.




**Parameters:**


* `rs` An initialised code. 
* `codeword` `rs->n` symbols, corrected in place on success and left untouched on refusal. 



**Returns:**

Symbols corrected, 0 for an already-valid codeword, or -1 if the word could not be decoded.



```C++
const int fixed = rs_decode (&rs, word);
if (fixed < 0)
  ;  // too far from every codeword to name one
```
 


        

<hr>



### function rs\_encode 

_Encode:_ `k` _information symbols in,_`nroots` _parity symbols out._
```C++
void rs_encode (
    const rs_t * rs,
    const uint8_t * info,
    uint8_t * parity
) 
```



Systematic — the information symbols are not touched. The parity is the remainder of `info(x) * x^nroots` modulo `g(x)`, highest-order coefficient first, which is the order it is transmitted in.




**Parameters:**


* `rs` An initialised code. 
* `info` `rs->k` information symbols, in transmission order. 
* `parity` Receives `rs->code.nroots` parity symbols. 




        

<hr>



### function rs\_generator 

_The_ `nroots + 1` _coefficients of_`g(x)` _,_`gen[i]` _for_`x^i` _._
```C++
const uint8_t * rs_generator (
    const rs_t * rs
) 
```



Exposed because standards publish them — CCSDS 131.0-B-3 Annex G prints all 33 for `E = 16` — so a caller, or a test, can check an implementation against the standard rather than against itself.




**Parameters:**


* `rs` An initialised code. 



**Returns:**

Pointer into `rs`, valid as long as it is. 





        

<hr>



### function rs\_init 

_Build the tables for_ `c` _into_`rs` _._
```C++
int rs_init (
    rs_t * rs,
    const rs_code_t * c
) 
```





**Parameters:**


* `rs` Receives the code and its derived tables. 
* `c` The code; see [**rs\_code\_valid**](rs__core_8h.md#function-rs_code_valid). 



**Returns:**

Non-zero on success. Zero if `c` is not valid or if `field_poly` is not primitive, in which case `rs` is unusable and must not be passed to anything below.



```C++
rs_t rs;
const rs_code_t code = { .symbol_bits = 8, .field_poly = 0x1D,
                         .nroots = 16, .first_root = 1,
                         .root_stride = 1 };
if (!rs_init (&rs, &code))
  return 1;  // not a field, or not a code
```
 


        

<hr>



### function rs\_syndromes 

_The_ `nroots` _syndromes of_`codeword` _._
```C++
void rs_syndromes (
    const rs_t * rs,
    const uint8_t * codeword,
    uint8_t * syn
) 
```



`S_m = C(a^(s*(j0+m)))`, evaluated over the codeword as a polynomial with index `i` carrying `x^(n-1-i)`. All zero is the DEFINING property of the code — it needs no encoder and no decoder, which is what makes it usable as a test oracle and as a receiver's error detector.




**Parameters:**


* `rs` An initialised code. 
* `codeword` `rs->n` symbols: information then parity. 
* `syn` Receives `rs->code.nroots` syndromes. 




        

<hr>
## Macro Definition Documentation





### define RS\_NROOTS\_MAX 

_Largest number of parity symbols this can represent._ 
```C++
#define RS_NROOTS_MAX `64`
```




<hr>



### define RS\_N\_MAX 

_Largest codeword,_ `2^RS_SYMBOL_BITS_MAX - 1` _._
```C++
#define RS_N_MAX `255`
```




<hr>



### define RS\_SYMBOL\_BITS\_MAX 

_Largest symbol width; symbols are held one per byte._ 
```C++
#define RS_SYMBOL_BITS_MAX `8`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/rs/rs_core.h`

