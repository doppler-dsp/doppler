

# File conv\_enc\_core.h



[**FileList**](files.md) **>** [**conv\_enc**](dir_b689baf1ac742b6ceba235289d5a286b.md) **>** [**conv\_enc\_core.h**](conv__enc__core_8h.md)

[Go to the source code of this file](conv__enc__core_8h_source.md)

_The convolutional encoder, as a stateful object over_ `conv` _._[More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "conv/conv_core.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**conv\_enc\_state\_t**](structconv__enc__state__t.md) <br>_A code and the register encoding it, together._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  const [**conv\_code\_t**](structconv__code__t.md) \* | [**conv\_enc\_code**](#function-conv_enc_code) (const [**conv\_enc\_state\_t**](structconv__enc__state__t.md) \* s) <br>_The code this encoder was built for._  |
|  [**conv\_enc\_state\_t**](structconv__enc__state__t.md) \* | [**conv\_enc\_create**](#function-conv_enc_create) (const uint32\_t \* poly, size\_t poly\_len, uint32\_t k, uint32\_t invert) <br>_Build an encoder for the code the polynomials describe._  |
|  [**conv\_enc\_state\_t**](structconv__enc__state__t.md) \* | [**conv\_enc\_create\_code**](#function-conv_enc_create_code) (const [**conv\_code\_t**](structconv__code__t.md) \* c) <br>_Build an encoder from a code already assembled._  |
|  void | [**conv\_enc\_destroy**](#function-conv_enc_destroy) ([**conv\_enc\_state\_t**](structconv__enc__state__t.md) \* state) <br>_Free an encoder. NULL is a no-op._  |
|  size\_t | [**conv\_enc\_encode**](#function-conv_enc_encode) ([**conv\_enc\_state\_t**](structconv__enc__state__t.md) \* state, const uint8\_t \* in, size\_t n\_in, uint8\_t \* out, size\_t max\_out) <br>_Encode information bits into channel symbols._  |
|  size\_t | [**conv\_enc\_encode\_max\_out**](#function-conv_enc_encode_max_out) (const [**conv\_enc\_state\_t**](structconv__enc__state__t.md) \* state, size\_t n\_in) <br>_Symbols_ [_**conv\_enc\_encode**_](conv__enc__core_8h.md#function-conv_enc_encode) _writes for_`n_in` _input bits._ |
|  void | [**conv\_enc\_get\_state**](#function-conv_enc_get_state) (const [**conv\_enc\_state\_t**](structconv__enc__state__t.md) \* s, void \* blob) <br>_Serialize the register into_ `blob` _._ |
|  void | [**conv\_enc\_reset**](#function-conv_enc_reset) ([**conv\_enc\_state\_t**](structconv__enc__state__t.md) \* state) <br>_Return the register to all-zero, keeping the code._  |
|  int | [**conv\_enc\_set\_state**](#function-conv_enc_set_state) ([**conv\_enc\_state\_t**](structconv__enc__state__t.md) \* s, const void \* blob) <br>_Restore a register from_ `blob` _._ |
|  size\_t | [**conv\_enc\_state\_bytes**](#function-conv_enc_state_bytes) (const [**conv\_enc\_state\_t**](structconv__enc__state__t.md) \* s) <br>_Bytes_ [_**conv\_enc\_get\_state**_](conv__enc__core_8h.md#function-conv_enc_get_state) _writes: envelope, code identity and the register._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CONV\_ENC\_STATE\_MAGIC**](conv__enc__core_8h.md#define-conv_enc_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('C', 'V', 'E', 'N')`<br>_Blob type tag: "CVEN"._  |
| define  | [**CONV\_ENC\_STATE\_VERSION**](conv__enc__core_8h.md#define-conv_enc_state_version)  `1u`<br>_Blob format version._  |

## Detailed Description


`conv` owns the CODE — the description, the trellis arithmetic, and the `conv_encode` kernel that turns bits into symbols. This owns the ENCODER built over one: a code and the shift register that must survive between calls, bound together so a caller cannot pair the wrong two.


**It is not a second implementation.** [**conv\_enc\_encode**](conv__enc__core_8h.md#function-conv_enc_encode) calls `conv_encode`, exactly as [**viterbi\_decode**](viterbi__core_8h.md#function-viterbi_decode)'s object calls its own kernel. Two encoders for one code family is how a rounding rule or an inversion comes to differ between them.


### Why this exists at all



`Viterbi` accepts any rate-1/n code, and until this the library could produce symbols for exactly one of them — CCSDS's, and only inside a `wfm_frame_desc_t`, whose stage kinds bind to `ccsds_tm_frame_ops` and carry a depth rather than a polynomial. Nothing in doppler exposed an `encode()` at all (doppler#900). A decoder whose matching encoder cannot be reached is a decoder that can only be tested against itself, which is the failure `conv`'s own tests are built to refuse.



### The register is the whole state, and it is load-bearing



3.3.2's shape, in the general case: the output is one uninterrupted symbol sequence, so encoding a long record in chunks must carry the `k-1` previous inputs across every boundary. An encoder that restarted per chunk emits `k-1` wrong symbols at each one — self-consistent, decodable by a receiver of one's own construction, and not what any standard says. That is why the register lives here rather than being passed in, and why this object serializes.


Bit convention follows `conv` and the rest of the coding chain: **unpacked** bits, one per byte in the LSB, in and out.




**See also:** [**conv/conv\_core.h**](conv__core_8h.md) for the code description and the kernel. 


**See also:** [**viterbi/viterbi\_core.h**](viterbi__core_8h.md) for the other direction.

```C++
const uint32_t poly[2] = { 0171u, 0133u };
conv_enc_state_t *e = conv_enc_create (poly, 2, 7u, 0x2u);  // CCSDS
uint8_t sym[2 * N];
const size_t n = conv_enc_encode (e, bits, N, sym, sizeof sym);
conv_enc_destroy (e);
```
 



    
## Public Functions Documentation




### function conv\_enc\_code 

_The code this encoder was built for._ 
```C++
const conv_code_t * conv_enc_code (
    const conv_enc_state_t * s
) 
```




<hr>



### function conv\_enc\_create 

_Build an encoder for the code the polynomials describe._ 
```C++
conv_enc_state_t * conv_enc_create (
    const uint32_t * poly,
    size_t poly_len,
    uint32_t k,
    uint32_t invert
) 
```



The array IS the code: its length gives the number of outputs per input bit, so `[0o171, 0o133]` is a rate-1/2 code and a three-element array is rate 1/3. `k` is the constraint length, which fixes the register width at `k - 1`.


`invert` is a mask over the outputs, and it is not decoration: CCSDS complements G2 and most codes complement nothing. An encoder built without it round-trips perfectly against a decoder built without it, and interoperates with nothing — which is why it is a parameter here rather than a property of any one standard's configuration.



```C++
>>> import numpy as np
>>> from doppler.coding import ConvEncoder
>>> e = ConvEncoder([0o171, 0o133], k=7, invert=0x2)
>>> e.encode(np.zeros(8, dtype=np.uint8)).size
16
```





**Parameters:**


* `poly` Generator polynomials, one per output. The array IS the code; `poly_len` gives `n`. 
* `poly_len` Number of polynomials, 1 to `CONV_N_MAX`. 
* `k` Constraint length, 2 to `CONV_K_MAX`. 
* `invert` Bit `j` complements output `j`. 



**Returns:**

Heap-allocated state, or NULL if the code is unusable. 




**Note:**

Caller must call [**conv\_enc\_destroy()**](conv__enc__core_8h.md#function-conv_enc_destroy) when done. 





        

<hr>



### function conv\_enc\_create\_code 

_Build an encoder from a code already assembled._ 
```C++
conv_enc_state_t * conv_enc_create_code (
    const conv_code_t * c
) 
```



The declared `conv_enc_create` takes the polynomials directly, because a struct pointer is not expressible in a manifest. Callers that already hold a [**conv\_code\_t**](structconv__code__t.md) — the CCSDS configuration, the validators — use this.




**Parameters:**


* `c` The code. Copied, so the caller's may be temporary. 



**Returns:**

The encoder, or NULL if `c` is invalid. 





        

<hr>



### function conv\_enc\_destroy 

_Free an encoder. NULL is a no-op._ 
```C++
void conv_enc_destroy (
    conv_enc_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function conv\_enc\_encode 

_Encode information bits into channel symbols._ 
```C++
size_t conv_enc_encode (
    conv_enc_state_t * state,
    const uint8_t * in,
    size_t n_in,
    uint8_t * out,
    size_t max_out
) 
```



The register carries across calls, so a long record may be fed in blocks and the symbol sequence is identical to one call — which is the property a standard fixes and a chunked encoder silently breaks.


Outputs are emitted in polynomial order per input bit: for `[G1, G2]`, `out[2i]` is `G1`'s symbol for input bit `i` and `out[2i+1]` is `G2`'s.




**Parameters:**


* `state` The encoder. 
* `in` `n_in` unpacked input bits, one per byte. 
* `n_in` Number of input bits. 
* `out` Receives `n_in * n` unpacked symbols, one per byte. 
* `max_out` Capacity of `out`. Short is a refusal, not a truncation: half a codeword is not a shorter codeword. 



**Returns:**

Symbols written, or 0 if `max_out` is too small — in which case `out` is untouched.



```C++
>>> import numpy as np
>>> from doppler.coding import ConvEncoder, Viterbi
>>> bits = np.array([1, 0, 1, 1, 0, 0, 1, 0] * 40, dtype=np.uint8)
>>> sym = ConvEncoder([0o171, 0o133], k=7).encode(bits)
>>> llr = np.where(sym, -8.0, 8.0).astype(np.float32)
>>> out = Viterbi([0o171, 0o133], k=7, depth=35).decode(llr)
>>> bool(np.array_equal(out, bits[: out.size]))
True
```
 


        

<hr>



### function conv\_enc\_encode\_max\_out 

_Symbols_ [_**conv\_enc\_encode**_](conv__enc__core_8h.md#function-conv_enc_encode) _writes for_`n_in` _input bits._
```C++
size_t conv_enc_encode_max_out (
    const conv_enc_state_t * state,
    size_t n_in
) 
```



Exactly `n_in * n` — a convolutional code has no fill and no latency on the encode side, which is the asymmetry with [**viterbi\_decode\_max\_out**](viterbi__core_8h.md#function-viterbi_decode_max_out), where the traceback still owes bits at the start of a stream.




**Parameters:**


* `state` The encoder. 
* `n_in` Number of input bits. 



**Returns:**

Symbols that call will write. 





        

<hr>



### function conv\_enc\_get\_state 

_Serialize the register into_ `blob` _._
```C++
void conv_enc_get_state (
    const conv_enc_state_t * s,
    void * blob
) 
```





**Parameters:**


* `s` The encoder. 
* `blob` At least [**conv\_enc\_state\_bytes**](conv__enc__core_8h.md#function-conv_enc_state_bytes) bytes. 




        

<hr>



### function conv\_enc\_reset 

_Return the register to all-zero, keeping the code._ 
```C++
void conv_enc_reset (
    conv_enc_state_t * state
) 
```



The boundary between two independent records, not a reconfiguration. The next encode starts from the same state a freshly created encoder is in, which is what makes a reset stream byte-identical to a fresh one.




**Parameters:**


* `state` Must be non-NULL.


```C++
>>> import numpy as np
>>> from doppler.coding import ConvEncoder
>>> e = ConvEncoder([0o171, 0o133], k=7)
>>> e.reset()
```
 


        

<hr>



### function conv\_enc\_set\_state 

_Restore a register from_ `blob` _._
```C++
int conv_enc_set_state (
    conv_enc_state_t * s,
    const void * blob
) 
```



The code identity travels in the blob and is CHECKED rather than restored: `create()` already fixed the code, and a blob from a different one describes a register that means something else. Refusing is the only answer that cannot silently produce a stream no decoder matches.




**Parameters:**


* `s` The encoder. 
* `blob` A blob from [**conv\_enc\_get\_state**](conv__enc__core_8h.md#function-conv_enc_get_state). 



**Returns:**

`DP_OK`, or `DP_ERR_INVALID` for a blob that is not this encoder's. 





        

<hr>



### function conv\_enc\_state\_bytes 

_Bytes_ [_**conv\_enc\_get\_state**_](conv__enc__core_8h.md#function-conv_enc_get_state) _writes: envelope, code identity and the register._
```C++
size_t conv_enc_state_bytes (
    const conv_enc_state_t * s
) 
```



A constant for this object, unlike the decoder's, whose ring is sized from the configuration. 


        

<hr>
## Macro Definition Documentation





### define CONV\_ENC\_STATE\_MAGIC 

_Blob type tag: "CVEN"._ 
```C++
#define CONV_ENC_STATE_MAGIC `DP_FOURCC ('C', 'V', 'E', 'N')`
```




<hr>



### define CONV\_ENC\_STATE\_VERSION 

_Blob format version._ 
```C++
#define CONV_ENC_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/conv_enc/conv_enc_core.h`

