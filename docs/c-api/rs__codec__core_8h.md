

# File rs\_codec\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**rs\_codec**](dir_3e7cbca72be4a95038c1797bf5803786.md) **>** [**rs\_codec\_core.h**](rs__codec__core_8h.md)

[Go to the source code of this file](rs__codec__core_8h_source.md)

_The Reed-Solomon codec, as an object over_ `rs` _._[More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "rs/rs_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**rs\_codec\_state\_t**](structrs__codec__state__t.md) <br>_A code and the tables derived from it._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**rs\_codec\_codeword\_ok**](#function-rs_codec_codeword_ok) ([**rs\_codec\_state\_t**](structrs__codec__state__t.md) \* state, const uint8\_t \* codeword, size\_t codeword\_len) <br>_Is this a valid codeword? — every syndrome zero._  |
|  [**rs\_codec\_state\_t**](structrs__codec__state__t.md) \* | [**rs\_codec\_create**](#function-rs_codec_create) (uint32\_t nroots, uint32\_t symbol\_bits, uint32\_t field\_poly, uint32\_t first\_root, uint32\_t root\_stride) <br>_Create a codec for the code named by the five arguments._  |
|  int | [**rs\_codec\_decode**](#function-rs_codec_decode) ([**rs\_codec\_state\_t**](structrs__codec__state__t.md) \* state, uint8\_t \* codeword, size\_t codeword\_len) <br>_Correct up to_ `E` _symbol errors, IN PLACE._ |
|  void | [**rs\_codec\_destroy**](#function-rs_codec_destroy) ([**rs\_codec\_state\_t**](structrs__codec__state__t.md) \* state) <br>_Destroy a codec and release all memory._  |
|  size\_t | [**rs\_codec\_encode**](#function-rs_codec_encode) ([**rs\_codec\_state\_t**](structrs__codec__state__t.md) \* state, const uint8\_t \* in, size\_t n\_in, uint8\_t \* out, size\_t max\_out) <br>_Encode_ `k` _information symbols into a whole_`n` _-symbol codeword._ |
|  size\_t | [**rs\_codec\_encode\_max\_out**](#function-rs_codec_encode_max_out) ([**rs\_codec\_state\_t**](structrs__codec__state__t.md) \* state, size\_t n\_in) <br>_Symbols_ [_**rs\_codec\_encode**_](rs__codec__core_8h.md#function-rs_codec_encode) _writes for_`n_in` _information symbols: a whole codeword,_`n` _._ |
|  size\_t | [**rs\_codec\_generator**](#function-rs_codec_generator) ([**rs\_codec\_state\_t**](structrs__codec__state__t.md) \* state, uint8\_t \* out, size\_t out\_len) <br>_The_ `nroots + 1` _coefficients of_`g(x)` _,_`out[i]` _for_`x^i` _._ |
|  size\_t | [**rs\_codec\_get\_e**](#function-rs_codec_get_e) (const [**rs\_codec\_state\_t**](structrs__codec__state__t.md) \* state) <br>_Correctable symbols per codeword,_ `nroots / 2` _._ |
|  size\_t | [**rs\_codec\_get\_k**](#function-rs_codec_get_k) (const [**rs\_codec\_state\_t**](structrs__codec__state__t.md) \* state) <br>_Information symbols per codeword,_ `n - nroots` _._ |
|  size\_t | [**rs\_codec\_get\_n**](#function-rs_codec_get_n) (const [**rs\_codec\_state\_t**](structrs__codec__state__t.md) \* state) <br>_Symbols per codeword,_ `2^J - 1` _._ |
|  size\_t | [**rs\_codec\_get\_nroots**](#function-rs_codec_get_nroots) (const [**rs\_codec\_state\_t**](structrs__codec__state__t.md) \* state) <br>_Parity symbols per codeword,_ `2E` _._ |
|  size\_t | [**rs\_codec\_get\_symbol\_bits**](#function-rs_codec_get_symbol_bits) (const [**rs\_codec\_state\_t**](structrs__codec__state__t.md) \* state) <br>_Symbol width_ `J` _, in bits._ |
|  size\_t | [**rs\_codec\_syndromes**](#function-rs_codec_syndromes) ([**rs\_codec\_state\_t**](structrs__codec__state__t.md) \* state, const uint8\_t \* in, size\_t n\_in, uint8\_t \* out, size\_t max\_out) <br>_The_ `nroots` _syndromes of an_`n` _-symbol word._ |
|  size\_t | [**rs\_codec\_syndromes\_max\_out**](#function-rs_codec_syndromes_max_out) ([**rs\_codec\_state\_t**](structrs__codec__state__t.md) \* state, size\_t n\_in) <br>_Syndromes_ [_**rs\_codec\_syndromes**_](rs__codec__core_8h.md#function-rs_codec_syndromes) _writes:_`nroots` _._ |




























## Detailed Description


`rs` owns the CODE — the field, the derived tables, the systematic encoder, the syndromes and the Berlekamp-Massey / Chien / Forney decoder, for any RS code over `GF(2^J)`. This owns the OBJECT built over one: the five numbers that name a code, bound to the tables derived from them, so a caller cannot pair the wrong two.


**It is not a second implementation.** Every function here calls the matching `rs_*` kernel. Two Reed-Solomon implementations for one code family is how a root offset or a basis convention comes to differ between them — and both of those are invisible to a round trip, because a matched decoder inverts whatever the encoder did.


### Why this exists at all



`rs_encode`, `rs_syndromes` and `rs_codeword_ok` were reachable from Python only through `wfm_frame_desc_t`'s Reed-Solomon stage, which binds to `ccsds_tm_frame_ops` and carries an interleaving depth rather than a code. So Python could run exactly ONE Reed-Solomon code — CCSDS's — and only inside a frame (doppler#900).



### Matching the algebra is not matching the wire



The five numbers here are the CODE. A standard adds conventions that are not properties of it, and CCSDS adds two: symbols travel in the **dual (Berlekamp) basis** (131.0-B 4.3.9) and codewords are **interleaved** (4.4.1). `ccsds_tm/ccsds_tm_rs.h` holds both. Construct this with CCSDS's five numbers and the arithmetic is right and the wire format is not; a conventional-basis codeword is self-consistent and matches no spacecraft.



### Conventions, inherited from &lt;tt&gt;rs&lt;/tt&gt;




* **Symbols are packed, one per byte** — an RS symbol _is_ a byte at `J = 8`. This differs from `conv_enc` and the randomiser, which take unpacked bits; the boundary belongs to the frame assembler.
* **A codeword is `k` information symbols then `nroots` parity**, index 0 first on the wire.
* `n` is `2^J - 1` by construction, so a SHORTENED code is not expressible — DVB's RS(204,188) and CCSDS 4.4.2's shortened codeblock are the full codes with leading zeros the sender never transmits, and that virtual fill is [gh-813](https://github.com/doppler-dsp/doppler/issues/813).




Lifecycle: `create -> [encode / decode / syndromes / codeword_ok]* -> destroy`.




**See also:** [**rs/rs\_core.h**](rs__core_8h.md) for the code description and every kernel. 


**See also:** [**ccsds\_tm/ccsds\_tm\_rs.h**](ccsds__tm__rs_8h.md) for CCSDS's configuration and its two conventions. 


**See also:** docs/design/reed-solomon.md for the algebra. 




    
## Public Functions Documentation




### function rs\_codec\_codeword\_ok 

_Is this a valid codeword? — every syndrome zero._ 
```C++
int rs_codec_codeword_ok (
    rs_codec_state_t * state,
    const uint8_t * codeword,
    size_t codeword_len
) 
```





**Parameters:**


* `state` The codec. 
* `codeword` `n` symbols. 
* `codeword_len` Number of symbols in `codeword`. 



**Returns:**

1 when every syndrome is zero, 0 otherwise — including when `codeword_len` is not `n`, since a word of the wrong length is not a codeword of this code.



```C++
>>> import numpy as np
>>> from doppler.coding import ReedSolomon
>>> rs = ReedSolomon(nroots=32)
>>> rs.codeword_ok(np.zeros(rs.n, np.uint8))   # all-zero IS a codeword
1
>>> rs.codeword_ok(np.zeros(rs.n - 1, np.uint8))   # at the right size
0
```
 


        

<hr>



### function rs\_codec\_create 

_Create a codec for the code named by the five arguments._ 
```C++
rs_codec_state_t * rs_codec_create (
    uint32_t nroots,
    uint32_t symbol_bits,
    uint32_t field_poly,
    uint32_t first_root,
    uint32_t root_stride
) 
```



Two of them are VALIDATED rather than trusted, because both produce arithmetic that is entirely self-consistent — a round trip against a matching encoder cannot see either: `field_poly` must be primitive, and `root_stride` must be coprime with `n`, or the `nroots` roots are not distinct and the code corrects fewer errors than its parity count claims.




**Parameters:**


* `nroots` Parity symbols `2E`; even, &gt;= 2, leaving `k >= 1`. 
* `symbol_bits` `J`, 2..8. 
* `field_poly` `F(x)`, low `J` bits, `x^J` implicit; PRIMITIVE. 
* `first_root` `j0`: the first root is `a^(root_stride * j0)`. 
* `root_stride` `s`; coprime with `n`. 



**Returns:**

Heap-allocated state, or NULL if the five do not name a usable code. 




**Note:**

Caller must call [**rs\_codec\_destroy()**](rs__codec__core_8h.md#function-rs_codec_destroy) when done.



```C++
>>> from doppler.coding import ReedSolomon
>>> rs = ReedSolomon(nroots=32)      # RS(255,223) over the usual GF(256)
>>> rs.n, rs.k, rs.e
(255, 223, 16)
>>> ReedSolomon(nroots=4, symbol_bits=4, field_poly=0b0011).n
15
```
 


        

<hr>



### function rs\_codec\_decode 

_Correct up to_ `E` _symbol errors, IN PLACE._
```C++
int rs_codec_decode (
    rs_codec_state_t * state,
    uint8_t * codeword,
    size_t codeword_len
) 
```



`rs_decode`, over the caller's own buffer: the corrected symbols land in `codeword` itself, which is why the binding demands a writable array rather than quietly working on a copy the caller would then discard.


**It either refuses or leaves a codeword.** On success the key equation has zeroed every syndrome by construction, so the result passes [**rs\_codec\_codeword\_ok**](rs__codec__core_8h.md#function-rs_codec_codeword_ok). On refusal `codeword` is untouched.


A refusal is not the same claim as "more than `E` errors". Beyond `E` a bounded-distance decoder can land inside another codeword's sphere and miscorrect — a property of the code, not of this implementation — which is why this reports a COUNT rather than a verdict, and why frame-level accounting is the protection.




**Parameters:**


* `state` The codec. 
* `codeword` `n` symbols, corrected in place. 
* `codeword_len` Number of symbols in `codeword`. 



**Returns:**

Symbols corrected, 0 for an already-valid codeword, **-1** when the word is too far from every codeword to name one, or **-2** when `codeword_len` is not `n`. Two negative codes rather than one because they are different kinds of fact: -1 is the channel's answer and -2 is the caller's mistake.



```C++
>>> import numpy as np
>>> from doppler.coding import ReedSolomon
>>> rs = ReedSolomon(nroots=32)
>>> word = rs.encode(np.arange(rs.k, dtype=np.uint8))
>>> word[3] ^= 0xFF          # one symbol, however many bits it moved
>>> word[40] ^= 0x01
>>> rs.decode(word)          # corrected in place
2
>>> bool(np.array_equal(word[: rs.k], np.arange(rs.k, dtype=np.uint8)))
True
```
 


        

<hr>



### function rs\_codec\_destroy 

_Destroy a codec and release all memory._ 
```C++
void rs_codec_destroy (
    rs_codec_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function rs\_codec\_encode 

_Encode_ `k` _information symbols into a whole_`n` _-symbol codeword._
```C++
size_t rs_codec_encode (
    rs_codec_state_t * state,
    const uint8_t * in,
    size_t n_in,
    uint8_t * out,
    size_t max_out
) 
```



Systematic: the information symbols are copied through untouched and the `nroots` parity symbols follow them, which is the order they are transmitted in. `rs_encode` computes the parity; this places it.


The WHOLE codeword rather than the parity alone, because that is the unit every other method here takes — [**rs\_codec\_decode**](rs__codec__core_8h.md#function-rs_codec_decode), [**rs\_codec\_syndromes**](rs__codec__core_8h.md#function-rs_codec_syndromes) and [**rs\_codec\_codeword\_ok**](rs__codec__core_8h.md#function-rs_codec_codeword_ok) all read `n` symbols, and a caller who wants the parity by itself can take the last `nroots` of the answer. (`rs_encode` is the other split, and is still there for a frame assembler that has already placed the information.)


`out` may alias `in` — `rs_codec_encode (rs, buf, k, buf, n)` appends the parity to a buffer that already holds the information, which is the call a frame assembler makes and the one `rs_encode` exists for.




**Parameters:**


* `state` The codec. 
* `in` Exactly `k` information symbols. 
* `n_in` Number of symbols in `in`. 
* `out` Receives `n` symbols; may be `in`. 
* `max_out` Capacity of `out`. 



**Returns:**

`n` on success, or 0 if `n_in` is not exactly `k` or `out` is too small — refusing rather than truncating, since a short codeword is not a codeword.



```C++
>>> import numpy as np
>>> from doppler.coding import ReedSolomon
>>> rs = ReedSolomon(nroots=32)
>>> info = np.arange(rs.k, dtype=np.uint8)
>>> word = rs.encode(info)
>>> word.size, bool(np.array_equal(word[: rs.k], info))
(255, True)
>>> rs.codeword_ok(word)
1
```
 


        

<hr>



### function rs\_codec\_encode\_max\_out 

_Symbols_ [_**rs\_codec\_encode**_](rs__codec__core_8h.md#function-rs_codec_encode) _writes for_`n_in` _information symbols: a whole codeword,_`n` _._
```C++
size_t rs_codec_encode_max_out (
    rs_codec_state_t * state,
    size_t n_in
) 
```




<hr>



### function rs\_codec\_generator 

_The_ `nroots + 1` _coefficients of_`g(x)` _,_`out[i]` _for_`x^i` _._
```C++
size_t rs_codec_generator (
    rs_codec_state_t * state,
    uint8_t * out,
    size_t out_len
) 
```



Exposed because standards PUBLISH them — CCSDS 131.0-B Annex G prints all 33 for `E = 16` — so a caller who has just configured a code from a document can check that they read the five numbers correctly, against the document rather than against this implementation.


The caller supplies the buffer rather than being handed one, because the length is a property of the CODE and not of the call: `g(x)` has exactly `nroots + 1` coefficients and there is no other number a caller could ask for. A self-sizing method would carry a `count` parameter that means nothing, which is a worse trade than one line of allocation.




**Parameters:**


* `state` The codec. 
* `out` Receives `nroots + 1` coefficients; `out[i]` is the coefficient of `x^i`, so `out[nroots]` is 1. 
* `out_len` Length of `out`; fewer than `nroots + 1` writes nothing. 



**Returns:**

`nroots + 1`, or 0 if `out` is too small.



```C++
>>> import numpy as np
>>> from doppler.coding import ReedSolomon
>>> rs = ReedSolomon(nroots=32, field_poly=0x87, first_root=112,
...                  root_stride=11)          # CCSDS 131.0-B 4.3
>>> g = np.empty(rs.nroots + 1, np.uint8)
>>> rs.generator(g)                  # Annex G prints all 33
33
>>> int(g[0]), int(g[-1])
(1, 1)
```
 


        

<hr>



### function rs\_codec\_get\_e 

_Correctable symbols per codeword,_ `nroots / 2` _._
```C++
size_t rs_codec_get_e (
    const rs_codec_state_t * state
) 
```




<hr>



### function rs\_codec\_get\_k 

_Information symbols per codeword,_ `n - nroots` _._
```C++
size_t rs_codec_get_k (
    const rs_codec_state_t * state
) 
```




<hr>



### function rs\_codec\_get\_n 

_Symbols per codeword,_ `2^J - 1` _._
```C++
size_t rs_codec_get_n (
    const rs_codec_state_t * state
) 
```




<hr>



### function rs\_codec\_get\_nroots 

_Parity symbols per codeword,_ `2E` _._
```C++
size_t rs_codec_get_nroots (
    const rs_codec_state_t * state
) 
```




<hr>



### function rs\_codec\_get\_symbol\_bits 

_Symbol width_ `J` _, in bits._
```C++
size_t rs_codec_get_symbol_bits (
    const rs_codec_state_t * state
) 
```




<hr>



### function rs\_codec\_syndromes 

_The_ `nroots` _syndromes of an_`n` _-symbol word._
```C++
size_t rs_codec_syndromes (
    rs_codec_state_t * state,
    const uint8_t * in,
    size_t n_in,
    uint8_t * out,
    size_t max_out
) 
```



All zero is the DEFINING property of the code: it needs no encoder and no decoder to check, which is what makes it usable both as a test oracle and as a receiver's error detector. [**rs\_codec\_codeword\_ok**](rs__codec__core_8h.md#function-rs_codec_codeword_ok) is this reduced to the one bit most callers want.




**Parameters:**


* `state` The codec. 
* `in` `n` symbols. 
* `n_in` Number of symbols in `in`. 
* `out` Receives `nroots` syndromes. 
* `max_out` Capacity of `out`. 



**Returns:**

`nroots`, or 0 if `n_in` is not `n` or `out` is too small.



```C++
>>> import numpy as np
>>> from doppler.coding import ReedSolomon
>>> rs = ReedSolomon(nroots=32)
>>> word = rs.encode(np.zeros(rs.k, dtype=np.uint8))
>>> bool(rs.syndromes(word).any())      # a codeword has none
False
>>> word[7] ^= 0x20
>>> bool(rs.syndromes(word).any())
True
```
 


        

<hr>



### function rs\_codec\_syndromes\_max\_out 

_Syndromes_ [_**rs\_codec\_syndromes**_](rs__codec__core_8h.md#function-rs_codec_syndromes) _writes:_`nroots` _._
```C++
size_t rs_codec_syndromes_max_out (
    rs_codec_state_t * state,
    size_t n_in
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/rs_codec/rs_codec_core.h`

