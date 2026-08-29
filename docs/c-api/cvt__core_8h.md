

# File cvt\_core.h



[**FileList**](files.md) **>** [**cvt**](dir_7aebb15fbd538257eeb7884581a8ab59.md) **>** [**cvt\_core.h**](cvt__core_8h.md)

[Go to the source code of this file](cvt__core_8h_source.md)

_Cvt module — public C API._ 

* `#include "clib_common.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**dp\_bitorder\_t**](#enum-dp_bitorder_t)  <br>_Bit order within a byte, for the bit/value conversions below._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**bin\_to\_hex**](#function-bin_to_hex) (const uint8\_t \* bits, size\_t bits\_len, uint8\_t \* out, size\_t out\_len, int bitorder) <br>_Render unpacked bits to hex digits_  _inverse of hex\_to\_bin._ |
|  uint64\_t | [**bin\_to\_int**](#function-bin_to_int) (const uint8\_t \* bits, size\_t bits\_len, int bitorder) <br>_Read unpacked bits back into an integer_  _inverse of int\_to\_bin._ |
|  size\_t | [**bin\_to\_nrz**](#function-bin_to_nrz) (const uint8\_t \* bits, size\_t bits\_len, float \* out, size\_t out\_len) <br>_Map unpacked bits to bipolar NRZ symbols: 0 -&gt; +1, 1 -&gt; -1._  |
|  size\_t | [**hex\_to\_bin**](#function-hex_to_bin) (const char \* hex, uint8\_t \* out, size\_t out\_len, int bitorder) <br>_Expand a hex string to unpacked bits, one per byte._  |
|  size\_t | [**int\_to\_bin**](#function-int_to_bin) (uint64\_t v, uint32\_t n\_bits, uint8\_t \* out, size\_t out\_len, int bitorder) <br>_Expand the low_ `n_bits` _of an integer to unpacked bits._ |
|  size\_t | [**nrz\_to\_bin**](#function-nrz_to_bin) (const float \* nrz, size\_t nrz\_len, uint8\_t \* out, size\_t out\_len) <br>_Hard-decide NRZ symbols back to bits_  _inverse of bin\_to\_nrz._ |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**cvt\_bit\_slot**](#function-cvt_bit_slot) (size\_t i, size\_t width, int bitorder) <br>_Where the i-th bit of a unit lands under_ `bitorder` _._ |
|  size\_t | [**cvt\_unit\_width**](#function-cvt_unit_width) (size\_t done, size\_t total) <br>_The unit both directions walk in: 8 bits, then whatever is left._  |


























## Public Types Documentation




### enum dp\_bitorder\_t 

_Bit order within a byte, for the bit/value conversions below._ 
```C++
enum dp_bitorder_t {
    DP_BITORDER_BIG = 0,
    DP_BITORDER_LITTLE = 1
};
```



The name and the values follow numpy's `packbits`/`unpackbits` `bitorder=` argument, because that is the convention anyone writing this conversion has already met. It is a DIFFERENT axis from the `endian` (`le`/`be`) the BLUE writer takes, which selects a file's BYTE order  the `"EEEI"` / `"IEEE"` field of a type-1000 header. A literal's digit order already fixes which byte comes first; what is left to choose is the order of bits inside one. Overloading one word for both would let a sync word and a sample stream disagree silently. 


        

<hr>
## Public Functions Documentation




### function bin\_to\_hex 

_Render unpacked bits to hex digits_  _inverse of hex\_to\_bin._
```C++
size_t bin_to_hex (
    const uint8_t * bits,
    size_t bits_len,
    uint8_t * out,
    size_t out_len,
    int bitorder
) 
```



The digits come back as ASCII BYTES rather than a string: jm has no string out-parameter, and `uint8_t` is the same type as the `unsigned char` a C caller would use anyway. A NUL is written after the digits.




**Parameters:**


* `bits` unpacked bits; any non-zero byte reads as 1. 
* `bits_len` number of bits; must be a multiple of 4. 
* `out` receives the digits plus a NUL. 
* `out_len` capacity of `out` in bytes, NUL included. 
* `bitorder` DP\_BITORDER\_BIG or DP\_BITORDER\_LITTLE. 



**Returns:**

digits written, NOT counting the NUL, or 0 on refusal.



```C++
>>> import numpy as np
>>> from doppler.cvt import hex_to_bin, bin_to_hex
>>> b = np.zeros(32, np.uint8)
>>> hex_to_bin("1acffc1d", b, 0)
32
>>> h = np.zeros(16, np.uint8)
>>> n = bin_to_hex(b, h, 0)
>>> bytes(h[:n]).decode()
'1acffc1d'
```
 


        

<hr>



### function bin\_to\_int 

_Read unpacked bits back into an integer_  _inverse of int\_to\_bin._
```C++
uint64_t bin_to_int (
    const uint8_t * bits,
    size_t bits_len,
    int bitorder
) 
```



Returns the value rather than a status, because that is the shape a binding can carry. 0 is therefore both "the value zero" and "refused", which is acceptable only because every refusal here is a programming error in the WIDTH the caller chose (0, or over 64) or the bit order it named  never a property of the data.




**Parameters:**


* `bits` 1..64 unpacked bits; any non-zero byte reads as 1. 
* `bits_len` number of bits. 
* `bitorder` DP\_BITORDER\_BIG or DP\_BITORDER\_LITTLE. 



**Returns:**

the value, or 0 on refusal.



```C++
>>> import numpy as np
>>> from doppler.cvt import bin_to_int
>>> bits = np.array([0, 0, 0, 1, 1, 0, 1, 0], np.uint8)
>>> hex(bin_to_int(bits, 0))
'0x1a'
```
 


        

<hr>



### function bin\_to\_nrz 

_Map unpacked bits to bipolar NRZ symbols: 0 -&gt; +1, 1 -&gt; -1._ 
```C++
size_t bin_to_nrz (
    const uint8_t * bits,
    size_t bits_len,
    float * out,
    size_t out_len
) 
```



That is `1 - 2*b`, and the convention's HOME is `mpsk_core.h`: BPSK is M-PSK at m = 2, where phi0 is 0, so label 0 lands at +1 and label 1 at -1. This states the same thing in the form a per-bit loop can afford, and `test_cvt_core` asserts the two agree rather than trusting them to. A mapper that disagreed with the receiver's would decode every bit INVERTED while looking perfectly locked  which a round-trip test cannot see.




**Parameters:**


* `bits` unpacked bits; any non-zero byte reads as 1. 
* `bits_len` number of bits. 
* `out` receives `bits_len` symbols, each +1.0f or -1.0f. 
* `out_len` capacity of `out` in symbols. 



**Returns:**

symbols written, or 0 on refusal.



```C++
>>> import numpy as np
>>> from doppler.cvt import bin_to_nrz
>>> bits = np.array([0, 1, 1, 0], np.uint8)
>>> sym = np.zeros(4, np.float32)
>>> bin_to_nrz(bits, sym)
4
>>> sym.tolist()
[1.0, -1.0, -1.0, 1.0]
```
 


        

<hr>



### function hex\_to\_bin 

_Expand a hex string to unpacked bits, one per byte._ 
```C++
size_t hex_to_bin (
    const char * hex,
    uint8_t * out,
    size_t out_len,
    int bitorder
) 
```



For what [**int\_to\_bin**](cvt__core_8h.md#function-int_to_bin) cannot serve: a literal wider than 64 bits, or one arriving as TEXT from a CLI flag or a JSON record. Each digit contributes 4 bits and digits read left to right, so an ODD number of digits is accepted and yields a 4-bit tail.


A bad digit is a REFUSAL, never a skipped one: a typo'd marker that silently shortens is the failure this exists to prevent, and it syncs to nothing rather than failing loudly.




**Parameters:**


* `hex` NUL-terminated `0-9a-fA-F`. No `0x`, no separators. 
* `out` receives `4 * strlen(hex)` bytes, each 0 or 1. 
* `out_len` capacity of `out` in bits. 
* `bitorder` DP\_BITORDER\_BIG or DP\_BITORDER\_LITTLE. 



**Returns:**

bits written, or 0 on refusal  `out` untouched.



```C++
>>> import numpy as np
>>> from doppler.cvt import hex_to_bin
>>> b = np.zeros(32, np.uint8)
>>> hex_to_bin("1ACFFC1D", b, 0)       # the CCSDS attached sync marker
32
>>> b[:8].tolist()
[0, 0, 0, 1, 1, 0, 1, 0]
```
 


        

<hr>



### function int\_to\_bin 

_Expand the low_ `n_bits` _of an integer to unpacked bits._
```C++
size_t int_to_bin (
    uint64_t v,
    uint32_t n_bits,
    uint8_t * out,
    size_t out_len,
    int bitorder
) 
```



The form a frame field literal usually wants, and the one to reach for first: exact, compiler-checked, with no failure mode a typo can reach. [**hex\_to\_bin**](cvt__core_8h.md#function-hex_to_bin) is for the two cases this cannot serve  a literal wider than 64 bits, and text arriving from outside.


Bit 0 out is the MOST significant of the `n_bits` requested under DP\_BITORDER\_BIG, which is what makes `int_to_bin(0x1A, 8, ...)` read `0,0,0,1,1,0,1,0`. Only the low `n_bits` are read, so a caller need not mask first.




**Parameters:**


* `v` the value. 
* `n_bits` 1..64. 
* `out` receives `n_bits` bytes, each 0 or 1. 
* `out_len` capacity of `out` in bits. 
* `bitorder` DP\_BITORDER\_BIG or DP\_BITORDER\_LITTLE. 



**Returns:**

`n_bits`, or 0 on refusal  `out` untouched.



```C++
>>> import numpy as np
>>> from doppler.cvt import int_to_bin
>>> b = np.zeros(8, np.uint8)
>>> int_to_bin(0x1A, 8, b, 0)          # 0 = big, MSB of each byte first
8
>>> b.tolist()
[0, 0, 0, 1, 1, 0, 1, 0]
```
 


        

<hr>



### function nrz\_to\_bin 

_Hard-decide NRZ symbols back to bits_  _inverse of bin\_to\_nrz._
```C++
size_t nrz_to_bin (
    const float * nrz,
    size_t nrz_len,
    uint8_t * out,
    size_t out_len
) 
```



Negative is a 1; zero and positive are a 0, matching `1 - 2*b`. Exactly zero decides to 0 rather than a coin toss, so the mapping is TOTAL and a round trip is exact. A caller that wants an erasure handled as an erasure wants a soft demapper, not this.




**Parameters:**


* `nrz` symbols. 
* `nrz_len` number of symbols. 
* `out` receives `nrz_len` bytes, each 0 or 1. 
* `out_len` capacity of `out` in bits. 



**Returns:**

bits written, or 0 on refusal.



```C++
>>> import numpy as np
>>> from doppler.cvt import nrz_to_bin
>>> sym = np.array([1.0, -1.0, -1.0, 1.0], np.float32)
>>> bits = np.zeros(4, np.uint8)
>>> nrz_to_bin(sym, bits)
4
>>> bits.tolist()
[0, 1, 1, 0]
```
 


        

<hr>
## Public Static Functions Documentation




### function cvt\_bit\_slot 

_Where the i-th bit of a unit lands under_ `bitorder` _._
```C++
static inline size_t cvt_bit_slot (
    size_t i,
    size_t width,
    int bitorder
) 
```




<hr>



### function cvt\_unit\_width 

_The unit both directions walk in: 8 bits, then whatever is left._ 
```C++
static inline size_t cvt_unit_width (
    size_t done,
    size_t total
) 
```



Written once so the value and the string forms cannot disagree about where a short final unit begins  the only place they could drift, and the place a marker would then be expanded two ways. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/cvt/cvt_core.h`

