

# File viterbi\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**viterbi**](dir_abfb52fd33d2d22e092a3b80738d1015.md) **>** [**viterbi\_core.h**](viterbi__core_8h.md)

[Go to the source code of this file](viterbi__core_8h_source.md)

_Soft-decision Viterbi decoding of convolutional codes._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "conv/conv_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**node\_sync\_t**](structnode__sync__t.md) <br>_What one alignment hypothesis scored, and what the runner-up did._  |
| struct | [**viterbi\_state\_t**](structviterbi__state__t.md) <br>_A streaming maximum-likelihood (Viterbi) decoder._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**node\_sync\_scan**](#function-node_sync_scan) ([**viterbi\_state\_t**](structviterbi__state__t.md) \* v, const float \* llr, size\_t n\_llr, [**node\_sync\_t**](structnode__sync__t.md) \* out) <br>_Try every branch alignment and report which one the stream is on._  |
|  size\_t | [**node\_sync\_score**](#function-node_sync_score) ([**viterbi\_state\_t**](structviterbi__state__t.md) \* v, const float \* llr, size\_t n\_llr) <br>_Score the alignment as given: decode, re-encode, count disagreements against the received hard decisions._  |
|  size\_t | [**node\_sync\_scored\_symbols**](#function-node_sync_scored_symbols) (const [**viterbi\_state\_t**](structviterbi__state__t.md) \* v, size\_t n\_llr) <br>_Symbols_ [_**node\_sync\_score**_](viterbi__core_8h.md#function-node_sync_score) _will actually score for a window of_`n_llr` _, which is fewer than_`n_llr` _._ |
|  const [**conv\_code\_t**](structconv__code__t.md) \* | [**viterbi\_code**](#function-viterbi_code) (const [**viterbi\_state\_t**](structviterbi__state__t.md) \* s) <br>_The code this decoder was built for._  |
|  [**viterbi\_state\_t**](structviterbi__state__t.md) \* | [**viterbi\_create**](#function-viterbi_create) (const uint32\_t \* poly, size\_t poly\_len, uint32\_t k, uint32\_t invert, size\_t depth) <br>_Build a decoder for the code the polynomials describe._  |
|  [**viterbi\_state\_t**](structviterbi__state__t.md) \* | [**viterbi\_create\_code**](#function-viterbi_create_code) (const [**conv\_code\_t**](structconv__code__t.md) \* c, size\_t depth) <br>_Build a decoder from a code already assembled._  |
|  size\_t | [**viterbi\_decode**](#function-viterbi_decode) ([**viterbi\_state\_t**](structviterbi__state__t.md) \* state, const float \* in, size\_t n\_in, uint8\_t \* out, size\_t max\_out) <br>_Decode soft channel symbols into information bits._  |
|  size\_t | [**viterbi\_decode\_max\_out**](#function-viterbi_decode_max_out) (const [**viterbi\_state\_t**](structviterbi__state__t.md) \* state, size\_t n\_in) <br>_Bits_ [_**viterbi\_decode**_](viterbi__core_8h.md#function-viterbi_decode) _will emit for_`n_in` _soft symbols._ |
|  size\_t | [**viterbi\_depth**](#function-viterbi_depth) (const [**viterbi\_state\_t**](structviterbi__state__t.md) \* s) <br>_Its traceback depth, in input bits._  |
|  void | [**viterbi\_destroy**](#function-viterbi_destroy) ([**viterbi\_state\_t**](structviterbi__state__t.md) \* state) <br>_Free a decoder and everything it allocated. NULL is a no-op._  |
|  void | [**viterbi\_get\_state**](#function-viterbi_get_state) (const [**viterbi\_state\_t**](structviterbi__state__t.md) \* s, void \* blob) <br>_Serialize_ `s` _into_`blob` _, which must hold_[_**viterbi\_state\_bytes**_](viterbi__core_8h.md#function-viterbi_state_bytes) _bytes._ |
|  void | [**viterbi\_reset**](#function-viterbi_reset) ([**viterbi\_state\_t**](structviterbi__state__t.md) \* state) <br>_Return to the all-zero start state, discarding the traceback._  |
|  int | [**viterbi\_set\_state**](#function-viterbi_set_state) ([**viterbi\_state\_t**](structviterbi__state__t.md) \* s, const void \* blob) <br>_Restore_ `s` _from_`blob` _._ |
|  size\_t | [**viterbi\_state\_bytes**](#function-viterbi_state_bytes) (const [**viterbi\_state\_t**](structviterbi__state__t.md) \* s) <br>_Bytes_ [_**viterbi\_get\_state**_](viterbi__core_8h.md#function-viterbi_get_state) _writes: envelope, code identity, ring cursor, the path metrics and the traceback ring._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**VITERBI\_STATE\_MAGIC**](viterbi__core_8h.md#define-viterbi_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('V', 'T', 'R', 'B')`<br>_Blob type tag: "VTRB"._  |
| define  | [**VITERBI\_STATE\_VERSION**](viterbi__core_8h.md#define-viterbi_state_version)  `1u`<br>_Blob format version._  |

## Detailed Description


`conv` owns the CODE — polynomials, the encoder, the trellis arithmetic — and this owns the DECODER built over one. A caller names the generator polynomials and gets a decoder for them; nothing here knows about CCSDS, which is a configuration of the same code family (see `ccsds_tm`).


Soft in, hard out: `decode` takes log-likelihood ratios, one per channel symbol, and returns decoded information bits. A hard-decision decoder throws away most of the gain the code exists to provide, which is why the input is LLRs rather than bits.


Lifecycle: `create -> [decode / reset]* -> destroy`.



```C++
>>> import numpy as np
>>> from doppler.coding import Viterbi
>>> v = Viterbi([0o171, 0o133], k=7, depth=35)
>>> llr = np.array([2.0, -2.0] * 64, dtype=np.float32)
>>> bits = v.decode(llr)
>>> bits.dtype, len(bits) > 0
(dtype('uint8'), True)
```
 


    
## Public Functions Documentation




### function node\_sync\_scan 

_Try every branch alignment and report which one the stream is on._ 
```C++
int node_sync_scan (
    viterbi_state_t * v,
    const float * llr,
    size_t n_llr,
    node_sync_t * out
) 
```



`c->n` hypotheses for a rate-1/n code — the offsets `0 .. n-1` — each scored by [**node\_sync\_score**](viterbi__core_8h.md#function-node_sync_score) over the same window.


**Re-runnable, and it has to be.** A symbol slip moves the stream by an odd number of symbols and the alignment changes mid-capture; measured through a real receiver at Es/N0 = 0 dB, that happened three times in forty-six frame slots (`docs/design/fec-receive.md` §8). A one-shot at start of stream would decode noise from the first slip onward, so this takes its window as an argument and holds no state between calls.




**Parameters:**


* `v` A decoder for the code; reset per hypothesis. 
* `llr` Soft symbols. 
* `n_llr` Window length. It buys the separation: the counts differ by about `0.5 - SER` per symbol, so a window of a few hundred symbols decides at any Es/N0 a coded link runs at. 
* `out` Receives the outcome; may be `NULL`. 



**Returns:**

Non-zero when a hypothesis was scored. Zero — with `out` untouched — when the window is too short.



```C++
node_sync_t ns;
if (node_sync_scan (v, llr, 1000, &ns) && ns.margin > 100)
  {
    viterbi_reset (v);
    viterbi_decode (v, llr + ns.phase, n - ns.phase, bits, cap);
  }
```
 


        

<hr>



### function node\_sync\_score 

_Score the alignment as given: decode, re-encode, count disagreements against the received hard decisions._ 
```C++
size_t node_sync_score (
    viterbi_state_t * v,
    const float * llr,
    size_t n_llr
) 
```



The **re-encoding metric**. It needs no truth, no marker and no training sequence — it compares the decoder's own output against the decoder's own input — so it works on a live capture, which is what makes it the statistic a receiver can carry. `docs/design/viterbi.md` §9 derives what it reads in and out of sync, and why a marker correlation is the wrong tool for this even when a marker exists.


**It is blind to polarity, and that is correct.** A transparent code (every generator of odd weight, which CCSDS's are) decodes an inverted stream to the complement of the bits, which re-encodes to the inverted symbols — so the disagreement count is identical. Polarity is resolved downstream by something that knows what the bits mean; this resolves only which symbol starts a branch.


The first `k - 1` decoded bits are excluded from the count: the encoder used for the comparison starts from a zero register while the real one was mid-stream, so those bits are re-encoded from the wrong state and would bias every hypothesis by a few symbols.




**Parameters:**


* `v` A decoder for the code being synchronized. It is RESET, and left holding this scoring run's state — a caller decoding with it afterwards must reset it again. 
* `llr` Soft symbols, `mpsk_soft_demap`'s convention. 
* `n_llr` Number of symbols; the tail beyond a whole number of branches is ignored. 



**Returns:**

Disagreements, or 0 if the window is too short to decode anything past the traceback and the encoder fill. 





        

<hr>



### function node\_sync\_scored\_symbols 

_Symbols_ [_**node\_sync\_score**_](viterbi__core_8h.md#function-node_sync_score) _will actually score for a window of_`n_llr` _, which is fewer than_`n_llr` _._
```C++
size_t node_sync_scored_symbols (
    const viterbi_state_t * v,
    size_t n_llr
) 
```



The head of a window is skipped: the decoder starts from its own all-zero prior, which is wrong whenever the window opens mid-capture, and the comparison encoder starts from a zero register while the transmitter's was mid-stream. A caller reading `errors / symbols` as a channel symbol error rate wants this denominator rather than the window length. 


        

<hr>



### function viterbi\_code 

_The code this decoder was built for._ 
```C++
const conv_code_t * viterbi_code (
    const viterbi_state_t * s
) 
```




<hr>



### function viterbi\_create 

_Build a decoder for the code the polynomials describe._ 
```C++
viterbi_state_t * viterbi_create (
    const uint32_t * poly,
    size_t poly_len,
    uint32_t k,
    uint32_t invert,
    size_t depth
) 
```



The array IS the code: its length gives the number of outputs per input bit, so `[0o171, 0o133]` is a rate-1/2 code and a three-element array is rate 1/3. `k` is the constraint length, which sets the trellis to `2^(k-1)` states — the dominant term in what a decode costs.


`depth` is the traceback depth in information bits. The conventional rule of thumb is `5*(k-1)` or more; a longer depth is safer at low Es/N0 and costs only the traceback walk, not the add-compare-selects (measured in `native/benchmarks/bench_viterbi_core.c`).



```C++
>>> import numpy as np
>>> from doppler.coding import Viterbi
>>> v = Viterbi([0o171, 0o133], k=7, depth=35)
>>> v.decode(np.zeros(8, dtype=np.float32)).dtype
dtype('uint8')
```





**Parameters:**


* `poly` Generator polynomials, one per output. The array IS the code; `poly_len` gives `n`. 
* `poly_len` Number of polynomials, 1 to `CONV_N_MAX`. 
* `k` k (default: 7). 
* `invert` invert (default: 0). 
* `depth` depth (default: 35). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**viterbi\_destroy()**](viterbi__core_8h.md#function-viterbi_destroy) when done. 





        

<hr>



### function viterbi\_create\_code 

_Build a decoder from a code already assembled._ 
```C++
viterbi_state_t * viterbi_create_code (
    const conv_code_t * c,
    size_t depth
) 
```



The declared `viterbi_create` takes the polynomials directly, because a struct pointer is not expressible in a manifest. Callers that already hold a [**conv\_code\_t**](structconv__code__t.md) — the CCSDS configuration, the validators — use this.




**Parameters:**


* `c` The code. Copied, so the caller's may be temporary. 
* `depth` Traceback depth in input bits. A decision is emitted only after `depth - 1` further bits have been seen, which is the decoder's latency and the dominant term in its memory. **60 is the measured choice for CCSDS's K = 7 rate-1/2 code** — `5*K = 35`, the textbook number, sits 33 % above the achievable BER (docs/design/viterbi.md section 4). It is a default for other codes, not a law. 



**Returns:**

The decoder, or NULL if `c` is invalid, `depth` is 0, or allocation failed. 





        

<hr>



### function viterbi\_decode 

_Decode soft channel symbols into information bits._ 
```C++
size_t viterbi_decode (
    viterbi_state_t * state,
    const float * in,
    size_t n_in,
    uint8_t * out,
    size_t max_out
) 
```



The input carries one value per channel symbol, in the convention `mpsk_soft_demap` produces: `L = log(P(0)/P(1))`, so **positive means symbol 0**. The branch metric for an expected symbol `e` is `+L` when `e == 0` and `-L` otherwise, and the survivor maximises the sum — which makes the decoder agree with `mpsk_demap` on hard decisions by construction rather than by a second convention.


A maximum-likelihood path cannot move when every metric is scaled by a positive constant, so **the LLRs need no accurate scaling** — a caller with no SNR estimate may pass unscaled values.


Streaming: state carries across calls, so a long capture may be fed in blocks and the bits come out continuously. The first `depth - 1` branches of a stream produce no output — the traceback walks `depth - 1` steps back, so a decision needs that many branches BEHIND it — and thereafter one bit is emitted per `n` symbols consumed. [**viterbi\_decode\_max\_out**](viterbi__core_8h.md#function-viterbi_decode_max_out) is the same statement as arithmetic, and is what a caller should size a buffer with rather than repeating this sentence: they disagreed by one until a test asserted the count against a literal.




**Parameters:**


* `state` The decoder. 
* `in` Log-likelihood ratios, one per channel symbol. `n_in` must be a multiple of the code's `n`. 
* `n_in` Number of LLRs in `in`. 
* `out` Receives the decoded information bits, one per byte. 
* `max_out` Capacity of `out`; see [**viterbi\_decode\_max\_out**](viterbi__core_8h.md#function-viterbi_decode_max_out). 



**Returns:**

Bits written, which may be 0 while the traceback fills.



```C++
>>> import numpy as np
>>> from doppler.coding import Viterbi
>>> v = Viterbi([0o171, 0o133], k=7, depth=35)
>>> llr = np.array([2.0, -2.0] * 128, dtype=np.float32)
>>> bits = v.decode(llr)
>>> set(np.unique(bits)) <= {0, 1}
True
```
 


        

<hr>



### function viterbi\_decode\_max\_out 

_Bits_ [_**viterbi\_decode**_](viterbi__core_8h.md#function-viterbi_decode) _will emit for_`n_in` _soft symbols._
```C++
size_t viterbi_decode_max_out (
    const viterbi_state_t * state,
    size_t n_in
) 
```



Accounts for the fill still owed at the start of a stream, so a caller can size a buffer exactly rather than conservatively.




**Parameters:**


* `state` The decoder. 
* `n_in` Number of soft symbols the next call would be given. 



**Returns:**

Bits that call would write. 





        

<hr>



### function viterbi\_depth 

_Its traceback depth, in input bits._ 
```C++
size_t viterbi_depth (
    const viterbi_state_t * s
) 
```




<hr>



### function viterbi\_destroy 

_Free a decoder and everything it allocated. NULL is a no-op._ 
```C++
void viterbi_destroy (
    viterbi_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function viterbi\_get\_state 

_Serialize_ `s` _into_`blob` _, which must hold_[_**viterbi\_state\_bytes**_](viterbi__core_8h.md#function-viterbi_state_bytes) _bytes._
```C++
void viterbi_get_state (
    const viterbi_state_t * s,
    void * blob
) 
```



The ring travels in its stored order with the cursor beside it rather than rotated into a canonical one — the rotation would cost a pass and buy nothing, since only [**viterbi\_set\_state**](viterbi__core_8h.md#function-viterbi_set_state) reads it back. 


        

<hr>



### function viterbi\_reset 

_Return to the all-zero start state, discarding the traceback._ 
```C++
void viterbi_reset (
    viterbi_state_t * state
) 
```



The code and the depth are unchanged — this is the boundary between two independent captures, not a reconfiguration. The next decode refills the traceback before it emits, exactly as after create, and the all-zero state is given the winning metric, matching an encoder that starts from a reset register.




**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.coding import Viterbi
>>> v = Viterbi([0o171, 0o133], k=7, depth=35)
>>> v.reset()
```
 


        

<hr>



### function viterbi\_set\_state 

_Restore_ `s` _from_`blob` _._
```C++
int viterbi_set_state (
    viterbi_state_t * s,
    const void * blob
) 
```



The code and the depth are configuration, restored by [**viterbi\_create**](viterbi__core_8h.md#function-viterbi_create) rather than carried in the payload — but they are _stamped_ in it and checked here, because a size match is not a configuration match: two codes with the same `k` and `n` differing only in a polynomial or in `invert` produce blobs of identical length, and reinterpreting one as the other yields a decoder that is confidently wrong rather than one that refuses.




**Returns:**

`DP_OK`, or `DP_ERR_INVALID` if the envelope, the code, the depth, or the ring cursor does not match this decoder — in which case `s` is untouched. 





        

<hr>



### function viterbi\_state\_bytes 

_Bytes_ [_**viterbi\_get\_state**_](viterbi__core_8h.md#function-viterbi_get_state) _writes: envelope, code identity, ring cursor, the path metrics and the traceback ring._
```C++
size_t viterbi_state_bytes (
    const viterbi_state_t * s
) 
```



Depends on the configuration (`2^(k-1)` metrics and a `depth x 2^(k-1)` ring), so it is not a constant across decoders. 


        

<hr>
## Macro Definition Documentation





### define VITERBI\_STATE\_MAGIC 

_Blob type tag: "VTRB"._ 
```C++
#define VITERBI_STATE_MAGIC `DP_FOURCC ('V', 'T', 'R', 'B')`
```




<hr>



### define VITERBI\_STATE\_VERSION 

_Blob format version._ 
```C++
#define VITERBI_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/viterbi/viterbi_core.h`

