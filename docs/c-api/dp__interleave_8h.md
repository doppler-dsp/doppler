

# File dp\_interleave.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_interleave.h**](dp__interleave_8h.md)

[Go to the source code of this file](dp__interleave_8h_source.md)

_Block interleaving — the permutation, and nothing else._ [More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`
* `#include <string.h>`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_deinterleave\_f32**](#function-dp_deinterleave_f32) (const float \* in, float \* out, size\_t rows, size\_t cols, size\_t unit) <br>_Undo dp\_interleave\_f32 — the soft-decision receive path._  |
|  size\_t | [**dp\_deinterleave\_index**](#function-dp_deinterleave_index) (size\_t o, size\_t rows, size\_t cols) <br>_Where output unit_ `o` _came from — the inverse permutation._ |
|  void | [**dp\_deinterleave\_u8**](#function-dp_deinterleave_u8) (const uint8\_t \* in, uint8\_t \* out, size\_t rows, size\_t cols, size\_t unit) <br>_Undo dp\_interleave\_u8 over a block of the same geometry._  |
|  size\_t | [**dp\_interleave\_block\_units**](#function-dp_interleave_block_units) (size\_t rows, size\_t cols) <br>_Units in one block —_ `rows * cols` _._ |
|  void | [**dp\_interleave\_f32**](#function-dp_interleave_f32) (const float \* in, float \* out, size\_t rows, size\_t cols, size\_t unit) <br>_Interleave one block of soft values._  |
|  size\_t | [**dp\_interleave\_index**](#function-dp_interleave_index) (size\_t i, size\_t rows, size\_t cols) <br>_Where input unit_ `i` _lands in the interleaved output._ |
|  void | [**dp\_interleave\_raw**](#function-dp_interleave_raw) (const void \* in, void \* out, size\_t rows, size\_t cols, size\_t unit\_bytes) <br>_Interleave one block of opaque fixed-size units._  |
|  void | [**dp\_interleave\_u8**](#function-dp_interleave_u8) (const uint8\_t \* in, uint8\_t \* out, size\_t rows, size\_t cols, size\_t unit) <br>_Interleave one block of unpacked bits or octets._  |


























## Detailed Description


A block interleaver writes its input by ROWS into a `rows` x `cols` matrix and reads it back by COLUMNS. That is the whole transform. It carries no state, adds no redundancy and detects nothing; what it buys is that a burst of errors on the wire arrives at the decoder spread out.


The two numbers are a link budget, not a tuning pair. Write one CODEWORD per row, `cols` units long, `rows` of them:



* a burst of up to `rows` consecutive OUTPUT units touches each codeword AT MOST ONCE;
* two originally adjacent INPUT units land `rows` apart on the wire.




So `rows` is the longest burst fully spread and `cols` is the codeword length. An outer code correcting `t` units per codeword survives a burst of `t` \* `rows`.


The invariant is one-per-CODEWORD, not a minimum index separation. Two burst positions either side of a column boundary can land as close as `cols` - 1 apart while still being in different codewords, which is what matters; a first draft of this header claimed the separation instead and `test_dp_interleave.c` refused it.


The UNIT is a parameter and not a detail. Interleaving octets is what spreads a burst across the codewords of a symbol-oriented code such as Reed-Solomon over GF(256); interleaving bits inside such a code spreads a burst WITHIN a symbol, which buys nothing, because the symbol is already wrong. Match the unit to the code the interleaver protects.


Not the CCSDS interleaver. `ccsds_tm/rs.c` also interleaves, and it is a different transform sharing a name: depth-I interleaving is intrinsic to the Reed-Solomon codeblock layout (131.0-B-6 4.4.1) and is fused into encode and decode, not a permutation applied afterwards. Neither can be written in terms of the other.


Not a convolutional (Forney) interleaver. That is a different structure with different latency and memory, and it is deliberately absent rather than pending — see doppler#1031.


Header-only, like `dp_crc16.h` and for the same reason: the frame stage kernel and the `Interleaver` object both need it, and neither should grow a link-line dependency for arithmetic. 


    
## Public Static Functions Documentation




### function dp\_deinterleave\_f32 

_Undo dp\_interleave\_f32 — the soft-decision receive path._ 
```C++
static inline void dp_deinterleave_f32 (
    const float * in,
    float * out,
    size_t rows,
    size_t cols,
    size_t unit
) 
```



This is the one a receiver actually needs. `dsss_burst_receiver`'s `llrs` span the whole frame, and an outer decoder wants them de-interleaved BEFORE it runs; hard-decision de-interleaving would throw away the confidence the soft output exists to carry.




**Parameters:**


* `in` `rows * cols * unit` floats of interleaved input. 
* `out` Where to write them; must not overlap `in`. 
* `rows` Interleaving depth, as given to the forward transform. 
* `cols` Block span, as given to the forward transform. 
* `unit` Floats per interleaved unit. 




        

<hr>



### function dp\_deinterleave\_index 

_Where output unit_ `o` _came from — the inverse permutation._
```C++
static inline size_t dp_deinterleave_index (
    size_t o,
    size_t rows,
    size_t cols
) 
```



Identical to dp\_interleave\_index with `rows` and `cols` exchanged, which is a fact about the transform rather than a coincidence: reading a `rows x cols` matrix by columns is writing a `cols x rows` one by rows. It is why every function below undoes itself by swapping two arguments, and why a SQUARE block is its own inverse.




**Parameters:**


* `o` Output unit index, below `rows * cols`. 
* `rows` Interleaving depth, as given to the forward transform. 
* `cols` Block span, as given to the forward transform. 



**Returns:**

The input unit index it came from. 





        

<hr>



### function dp\_deinterleave\_u8 

_Undo dp\_interleave\_u8 over a block of the same geometry._ 
```C++
static inline void dp_deinterleave_u8 (
    const uint8_t * in,
    uint8_t * out,
    size_t rows,
    size_t cols,
    size_t unit
) 
```





**Parameters:**


* `in` `rows * cols * unit` bytes of interleaved input. 
* `out` Where to write them; must not overlap `in`. 
* `rows` Interleaving depth, as given to the forward transform. 
* `cols` Block span, as given to the forward transform. 
* `unit` Bytes per interleaved unit. 




        

<hr>



### function dp\_interleave\_block\_units 

_Units in one block —_ `rows * cols` _._
```C++
static inline size_t dp_interleave_block_units (
    size_t rows,
    size_t cols
) 
```



The length every call below consumes and produces, in UNITS. Multiply by the unit size for elements.




**Parameters:**


* `rows` Interleaving depth. 
* `cols` Block span. 



**Returns:**

The block size in units. 





        

<hr>



### function dp\_interleave\_f32 

_Interleave one block of soft values._ 
```C++
static inline void dp_interleave_f32 (
    const float * in,
    float * out,
    size_t rows,
    size_t cols,
    size_t unit
) 
```





**Parameters:**


* `in` `rows * cols * unit` floats of input. 
* `out` Where to write them; must not overlap `in`. 
* `rows` Interleaving depth. 
* `cols` Block span. 
* `unit` Floats per interleaved unit. 




        

<hr>



### function dp\_interleave\_index 

_Where input unit_ `i` _lands in the interleaved output._
```C++
static inline size_t dp_interleave_index (
    size_t i,
    size_t rows,
    size_t cols
) 
```



Unit `i` sits at row `i / cols`, column `i % cols` of the write-by-rows matrix; reading by columns puts it at `(i % cols) * rows + (i / cols)`.




**Parameters:**


* `i` Input unit index, below `rows * cols`. 
* `rows` Interleaving depth. 
* `cols` Block span. 



**Returns:**

The output unit index. 





        

<hr>



### function dp\_interleave\_raw 

_Interleave one block of opaque fixed-size units._ 
```C++
static inline void dp_interleave_raw (
    const void * in,
    void * out,
    size_t rows,
    size_t cols,
    size_t unit_bytes
) 
```



The generic kernel the typed wrappers below call. `in` and `out` must not overlap: a block interleave is a transpose, so doing it in place needs cycle-following and is a different algorithm, not an option here.




**Parameters:**


* `in` `rows * cols * unit_bytes` bytes of input. 
* `out` Where to write the same number of bytes. 
* `rows` Interleaving depth. 
* `cols` Block span. 
* `unit_bytes` Bytes per interleaved unit; 0 writes nothing. 




        

<hr>



### function dp\_interleave\_u8 

_Interleave one block of unpacked bits or octets._ 
```C++
static inline void dp_interleave_u8 (
    const uint8_t * in,
    uint8_t * out,
    size_t rows,
    size_t cols,
    size_t unit
) 
```



The array form doppler's frame paths use: one bit per byte. `unit` is in BYTES of that array, so `unit == 1` interleaves bits and `unit == 8` interleaves octets of a bit-per-byte stream.




**Parameters:**


* `in` `rows * cols * unit` bytes of input. 
* `out` Where to write them; must not overlap `in`. 
* `rows` Interleaving depth. 
* `cols` Block span. 
* `unit` Bytes per interleaved unit. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_interleave.h`

