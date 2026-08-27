

# File interleaver\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**interleaver**](dir_46ba54d679b7d3fa44b8264f360065a9.md) **>** [**interleaver\_core.h**](interleaver__core_8h.md)

[Go to the source code of this file](interleaver__core_8h_source.md)

_Block interleaving as an object — the geometry, held._ [More...](#detailed-description)

* `#include "dp_interleave.h"`
* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**interleaver\_state\_t**](structinterleaver__state__t.md) <br>_A block interleaver's geometry._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**interleaver\_state\_t**](structinterleaver__state__t.md) \* | [**interleaver\_create**](#function-interleaver_create) (size\_t rows, size\_t cols, size\_t unit\_bits) <br>_Build an interleaver over a_ `rows` _x_`cols` _block of_`unit_bits` _units._ |
|  [**interleaver\_state\_t**](structinterleaver__state__t.md) \* | [**interleaver\_create\_rx**](#function-interleaver_create_rx) (size\_t rows, size\_t cols, size\_t unit\_bits) <br>_The RECEIVE face of the same interleaver._  |
|  size\_t | [**interleaver\_deinterleave**](#function-interleaver_deinterleave) ([**interleaver\_state\_t**](structinterleaver__state__t.md) \* state, const uint8\_t \* in, size\_t n\_in, uint8\_t \* out, size\_t max\_out) <br>_Undo_ [_**interleaver\_interleave**_](interleaver__core_8h.md#function-interleaver_interleave) _over the same geometry._ |
|  size\_t | [**interleaver\_deinterleave\_max\_out**](#function-interleaver_deinterleave_max_out) (const [**interleaver\_state\_t**](structinterleaver__state__t.md) \* state, size\_t n\_in) <br>_Output bits for_ `n_in` _input bits — the same number._ |
|  size\_t | [**interleaver\_deinterleave\_soft**](#function-interleaver_deinterleave_soft) ([**interleaver\_state\_t**](structinterleaver__state__t.md) \* state, const float \* in, size\_t n\_in, float \* out, size\_t max\_out) <br>_Undo an interleave over SOFT values — the receive path that matters._  |
|  size\_t | [**interleaver\_deinterleave\_soft\_max\_out**](#function-interleaver_deinterleave_soft_max_out) (const [**interleaver\_state\_t**](structinterleaver__state__t.md) \* state, size\_t n\_in) <br>_Output values for_ `n_in` _soft input values — the same number._ |
|  void | [**interleaver\_destroy**](#function-interleaver_destroy) ([**interleaver\_state\_t**](structinterleaver__state__t.md) \* state) <br>_Release an interleaver._  |
|  size\_t | [**interleaver\_get\_block\_bits**](#function-interleaver_get_block_bits) (const [**interleaver\_state\_t**](structinterleaver__state__t.md) \* state) <br>_Bits in one block —_ `rows * cols * unit_bits` _._ |
|  size\_t | [**interleaver\_get\_burst\_len**](#function-interleaver_get_burst_len) (const [**interleaver\_state\_t**](structinterleaver__state__t.md) \* state) <br>_The longest burst this geometry fully spreads —_ `rows` _._ |
|  size\_t | [**interleaver\_get\_separation**](#function-interleaver_get_separation) (const [**interleaver\_state\_t**](structinterleaver__state__t.md) \* state) <br>_Units per codeword —_ `cols` _._ |
|  size\_t | [**interleaver\_interleave**](#function-interleaver_interleave) ([**interleaver\_state\_t**](structinterleaver__state__t.md) \* state, const uint8\_t \* in, size\_t n\_in, uint8\_t \* out, size\_t max\_out) <br>_Interleave a whole number of blocks._  |
|  size\_t | [**interleaver\_interleave\_max\_out**](#function-interleaver_interleave_max_out) (const [**interleaver\_state\_t**](structinterleaver__state__t.md) \* state, size\_t n\_in) <br>_Output bits for_ `n_in` _input bits — the same number._ |
|  void | [**interleaver\_reset**](#function-interleaver_reset) ([**interleaver\_state\_t**](structinterleaver__state__t.md) \* state) <br>_No-op; an interleaver carries nothing between calls._  |




























## Detailed Description


The transform itself is `dp_interleave.h`, header-only and stateless. **This is not a second implementation**: every method here calls that one. The split is `conv`/`conv_enc`'s — one owns the arithmetic, the other owns the configured thing a caller holds and reuses.


What holding it buys is that the geometry is DECLARED rather than inferred from whatever length arrives. A block interleaver only works if the transmitter and the receiver agree on the permutation, and deriving `cols` from the input length means a truncated frame silently produces a DIFFERENT permutation instead of an error.


Stateless, deliberately, and therefore not serializable. Interleaving is per-frame: carrying a partial block across frames would add frame-latency and break per-frame decoding, so a call takes a whole number of blocks or is refused. Nothing survives between calls, so there is nothing to checkpoint — the exemption `docs/design/state-serialization.md` grants a pure converter.



```C++
interleaver_state_t *il = interleaver_create (8, 32, 1);
uint8_t tx[256], rx[256];
interleaver_interleave (il, bits, 256, tx, sizeof tx);
interleaver_deinterleave (il, tx, 256, rx, sizeof rx);
interleaver_destroy (il);
```
 


    
## Public Functions Documentation




### function interleaver\_create 

_Build an interleaver over a_ `rows` _x_`cols` _block of_`unit_bits` _units._
```C++
interleaver_state_t * interleaver_create (
    size_t rows,
    size_t cols,
    size_t unit_bits
) 
```





**Parameters:**


* `rows` Interleaving depth; the longest burst fully spread. Must be non-zero. 
* `cols` Units per codeword. Must be non-zero. 
* `unit_bits` Bits per interleaved unit. 1 interleaves bits; 8 interleaves octets, which is what spreads a burst across the codewords of a symbol-oriented code such as Reed-Solomon over GF(256). Must be non-zero. 



**Returns:**

An interleaver, or NULL if any parameter is zero or the block would overflow.



```C++
>>> import numpy as np
>>> from doppler.coding import Interleaver
>>> il = Interleaver(rows=3, cols=4)
>>> il.block_bits, il.burst_len, il.separation
(12, 3, 4)
```
 


        

<hr>



### function interleaver\_create\_rx 

_The RECEIVE face of the same interleaver._ 
```C++
interleaver_state_t * interleaver_create_rx (
    size_t rows,
    size_t cols,
    size_t unit_bits
) 
```



Identical construction — it delegates to `interleaver_create` — and it exists because the two ends of a link are written by different people. Someone working the receive side reaches for a `Deinterleaver`, and a class that is only findable under the transmit name is a class they do not find.


The GEOMETRY is why this is a view over one core rather than a second object: `rows`, `cols` and `unit_bits` are exactly what the two ends must agree on, and a mismatch is not an error but a receiver de-interleaving into a different permutation and handing the decoder plausible garbage. One core means one definition of the geometry to get right.




**Parameters:**


* `rows` Interleaving depth, as the transmitter used. 
* `cols` Units per codeword, as the transmitter used. 
* `unit_bits` Bits per interleaved unit, as the transmitter used. 



**Returns:**

An interleaver, or NULL on the same refusals as `interleaver_create`.



```C++
>>> import numpy as np
>>> from doppler.coding import Interleaver, Deinterleaver
>>> tx = Interleaver(rows=3, cols=4)
>>> rx = Deinterleaver(rows=3, cols=4)
>>> bits = np.arange(12, dtype=np.uint8)
>>> wire = np.asarray(tx.interleave(bits))
>>> np.array_equal(np.asarray(rx.deinterleave(wire)), bits)
True
```
 


        

<hr>



### function interleaver\_deinterleave 

_Undo_ [_**interleaver\_interleave**_](interleaver__core_8h.md#function-interleaver_interleave) _over the same geometry._
```C++
size_t interleaver_deinterleave (
    interleaver_state_t * state,
    const uint8_t * in,
    size_t n_in,
    uint8_t * out,
    size_t max_out
) 
```





**Parameters:**


* `state` The interleaver. 
* `in` `n_in` interleaved bits, one bit per byte. 
* `n_in` Input length in bits; a whole number of blocks. 
* `out` Where to write `n_in` bits; must not overlap `in`. 
* `max_out` Room in `out`, in bits. 



**Returns:**

`n_in`, or 0 on a refusal.



```C++
>>> import numpy as np
>>> from doppler.coding import Interleaver
>>> il = Interleaver(rows=3, cols=4)
>>> x = np.arange(12, dtype=np.uint8)
>>> y = np.asarray(il.interleave(x))
>>> np.array_equal(np.asarray(il.deinterleave(y)), x)
True
```
 


        

<hr>



### function interleaver\_deinterleave\_max\_out 

_Output bits for_ `n_in` _input bits — the same number._
```C++
size_t interleaver_deinterleave_max_out (
    const interleaver_state_t * state,
    size_t n_in
) 
```



Identical to `interleaver_interleave_max_out`, and for the same reason: the inverse of a permutation is a permutation.




**Parameters:**


* `state` The interleaver. 
* `n_in` Input length in bits. 



**Returns:**

`n_in`.



```C++
>>> from doppler.coding import Interleaver
>>> Interleaver(rows=4, cols=8).deinterleave_max_out(32)
32
```
 


        

<hr>



### function interleaver\_deinterleave\_soft 

_Undo an interleave over SOFT values — the receive path that matters._ 
```C++
size_t interleaver_deinterleave_soft (
    interleaver_state_t * state,
    const float * in,
    size_t n_in,
    float * out,
    size_t max_out
) 
```



`dsss_burst_receiver`'s `llrs` span the whole frame, and an outer decoder wants them de-interleaved BEFORE it runs. Slicing to hard bits first and de-interleaving those throws away the confidence the soft output exists to carry, which is most of what an outer code is for.


There is no `interleave_soft`: a transmitter has bits, not LLRs.




**Parameters:**


* `state` The interleaver. 
* `in` `n_in` soft values, one per interleaved unit-bit. 
* `n_in` Input length in values; a whole number of blocks. 
* `out` Where to write `n_in` values; must not overlap `in`. 
* `max_out` Room in `out`, in values. 



**Returns:**

`n_in`, or 0 on a refusal.



```C++
>>> import numpy as np
>>> from doppler.coding import Interleaver
>>> il = Interleaver(rows=2, cols=3)
>>> llr = np.array([1., 2., 3., 4., 5., 6.], dtype=np.float32)
>>> np.asarray(il.deinterleave_soft(llr)).tolist()
[1.0, 3.0, 5.0, 2.0, 4.0, 6.0]
```
 


        

<hr>



### function interleaver\_deinterleave\_soft\_max\_out 

_Output values for_ `n_in` _soft input values — the same number._
```C++
size_t interleaver_deinterleave_soft_max_out (
    const interleaver_state_t * state,
    size_t n_in
) 
```





**Parameters:**


* `state` The interleaver. 
* `n_in` Input length in values. 



**Returns:**

`n_in`.



```C++
>>> from doppler.coding import Interleaver
>>> Interleaver(rows=4, cols=8).deinterleave_soft_max_out(32)
32
```
 


        

<hr>



### function interleaver\_destroy 

_Release an interleaver._ 
```C++
void interleaver_destroy (
    interleaver_state_t * state
) 
```



A no-op on NULL, like `free`. The object owns nothing but its three numbers, so this is one `free` and there is no buffer to drain first.




**Parameters:**


* `state` The interleaver, or NULL.


```C++
>>> from doppler.coding import Interleaver
>>> il = Interleaver(rows=2, cols=2)
>>> il.destroy()
```
 


        

<hr>



### function interleaver\_get\_block\_bits 

_Bits in one block —_ `rows * cols * unit_bits` _._
```C++
size_t interleaver_get_block_bits (
    const interleaver_state_t * state
) 
```





**Parameters:**


* `state` The interleaver. 



**Returns:**

The block size in bits.



```C++
>>> from doppler.coding import Interleaver
>>> Interleaver(rows=8, cols=32, unit_bits=8).block_bits
2048
```
 


        

<hr>



### function interleaver\_get\_burst\_len 

_The longest burst this geometry fully spreads —_ `rows` _._
```C++
size_t interleaver_get_burst_len (
    const interleaver_state_t * state
) 
```



A burst of up to this many consecutive units on the wire touches each codeword at most once, so an outer code correcting `t` units per codeword survives a burst of `t` times this.



```C++
>>> from doppler.coding import Interleaver
>>> Interleaver(rows=5, cols=51, unit_bits=8).burst_len
5
```
 


        

<hr>



### function interleaver\_get\_separation 

_Units per codeword —_ `cols` _._
```C++
size_t interleaver_get_separation (
    const interleaver_state_t * state
) 
```



The other half of the link budget: what [**interleaver\_get\_burst\_len**](interleaver__core_8h.md#function-interleaver_get_burst_len) spreads a burst ACROSS.



```C++
>>> from doppler.coding import Interleaver
>>> Interleaver(rows=5, cols=51, unit_bits=8).separation
51
```
 


        

<hr>



### function interleaver\_interleave 

_Interleave a whole number of blocks._ 
```C++
size_t interleaver_interleave (
    interleaver_state_t * state,
    const uint8_t * in,
    size_t n_in,
    uint8_t * out,
    size_t max_out
) 
```





**Parameters:**


* `state` The interleaver. 
* `in` `n_in` bits, one bit per byte. 
* `n_in` Input length in bits; must be a non-zero multiple of `interleaver_get_block_bits`. 
* `out` Where to write `n_in` bits; must not overlap `in`. 
* `max_out` Room in `out`, in bits. 



**Returns:**

`n_in` on success, 0 if the length is not a whole number of blocks or `out` is too small. A partial block is REFUSED rather than padded: padding changes the length, and a receiver that de-interleaved the padded block would recover different bits.



```C++
>>> import numpy as np
>>> from doppler.coding import Interleaver
>>> il = Interleaver(rows=3, cols=4)
>>> x = np.arange(12, dtype=np.uint8)
>>> np.asarray(il.interleave(x)).tolist()
[0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11]
```
 


        

<hr>



### function interleaver\_interleave\_max\_out 

_Output bits for_ `n_in` _input bits — the same number._
```C++
size_t interleaver_interleave_max_out (
    const interleaver_state_t * state,
    size_t n_in
) 
```



A permutation moves bits and does not add or remove any, so this is the identity. It exists because the binding asks a method how much room its output needs, and answering "the same" is not something a caller should have to know.




**Parameters:**


* `state` The interleaver. 
* `n_in` Input length in bits. 



**Returns:**

`n_in`.



```C++
>>> from doppler.coding import Interleaver
>>> Interleaver(rows=4, cols=8).interleave_max_out(32)
32
```
 


        

<hr>



### function interleaver\_reset 

_No-op; an interleaver carries nothing between calls._ 
```C++
void interleaver_reset (
    interleaver_state_t * state
) 
```



Present because the object surface has it, and honest about why it does nothing: a reset that pretended to clear something would suggest there was something to clear. The geometry is configuration, not state, so it survives — a reset that cleared THAT would leave every later call refusing.




**Parameters:**


* `state` The interleaver.


```C++
>>> import numpy as np
>>> from doppler.coding import Interleaver
>>> il = Interleaver(rows=2, cols=3)
>>> il.reset()
>>> il.block_bits
6
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/interleaver/interleaver_core.h`

