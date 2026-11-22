

# File dp\_crc.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_crc.h**](dp__crc_8h.md)

[Go to the source code of this file](dp__crc_8h_source.md)

_One parametric CRC: the Rocksoft model over a bit stream._ [More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_crc\_t**](structdp__crc__t.md) <br>_A CRC model: width, polynomial, initial register, output reflection, output XOR and input reflection._  |
























## Public Static Functions

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**dp\_crc\_bits**](#function-dp_crc_bits) (const [**dp\_crc\_t**](structdp__crc__t.md) \* p, const uint8\_t \* bits, size\_t n) <br>_The CRC of a bit stream: first bit presented is shifted in first._  |
|  uint64\_t | [**dp\_crc\_bytes**](#function-dp_crc_bytes) (const [**dp\_crc\_t**](structdp__crc__t.md) \* p, const uint8\_t \* bytes, size\_t n) <br>_The CRC of octets, the model's_ `refin` _choosing each octet's bit order._ |
|  uint64\_t | [**dp\_crc\_finish**](#function-dp_crc_finish) (const [**dp\_crc\_t**](structdp__crc__t.md) \* p, uint64\_t reg) <br>_The register's result: reflected if the model says so, then XORed. The one place that happens._  |
|  uint64\_t | [**dp\_crc\_mask**](#function-dp_crc_mask) (unsigned width) <br> |
|  uint64\_t | [**dp\_crc\_reflect**](#function-dp_crc_reflect) (uint64\_t v, unsigned width) <br>_The register's bits, reversed within_ `width` _._ |
|  uint64\_t | [**dp\_crc\_step**](#function-dp_crc_step) (const [**dp\_crc\_t**](structdp__crc__t.md) \* p, uint64\_t reg, unsigned bit) <br>_Shift one bit into the register: the only place the model's arithmetic is written._  |
|  int | [**dp\_crc\_valid**](#function-dp_crc_valid) (const [**dp\_crc\_t**](structdp__crc__t.md) \* p) <br>_Is_ `p` _a model this header can compute?_ |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_CRC\_16\_IBM\_3740**](dp__crc_8h.md#define-dp_crc_16_ibm_3740)  `/* multi line expression */`<br>_CRC-16/IBM-3740, formerly CRC-16/CCITT-FALSE: the burst frame's trailer. Check 0x29B1,_ `refin` _false._ |
| define  | [**DP\_CRC\_16\_KERMIT**](dp__crc_8h.md#define-dp_crc_16_kermit)  `/* multi line expression */`<br>_CRC-16/KERMIT. Check 0x2189,_ `refin` _true._ |
| define  | [**DP\_CRC\_16\_XMODEM**](dp__crc_8h.md#define-dp_crc_16_xmodem)  `/* multi line expression */`<br>_CRC-16/XMODEM. Check 0x31C3,_ `refin` _false._ |
| define  | [**DP\_CRC\_32C**](dp__crc_8h.md#define-dp_crc_32c)  `/* multi line expression */`<br>_CRC-32C (Castagnoli, iSCSI). Check 0xE3069283,_ `refin` _true._ |
| define  | [**DP\_CRC\_32\_BZIP2**](dp__crc_8h.md#define-dp_crc_32_bzip2)  `/* multi line expression */`<br>_CRC-32/BZIP2: the same polynomial, not reflected. Check 0xFC891918,_ `refin` _false._ |
| define  | [**DP\_CRC\_32\_ISO\_HDLC**](dp__crc_8h.md#define-dp_crc_32_iso_hdlc)  `/* multi line expression */`<br>_CRC-32/ISO-HDLC (Ethernet, zlib). Check value 0xCBF43926. Its catalogue_ `refin` _is true: feed octets LSB-first, which dp\_crc\_bytes() does for it._ |
| define  | [**DP\_CRC\_64\_ECMA\_182**](dp__crc_8h.md#define-dp_crc_64_ecma_182)  `/* multi line expression */`<br>_CRC-64/ECMA-182. Check 0x6C40DF5F0B497347,_ `refin` _false._ |
| define  | [**DP\_CRC\_8\_ROHC**](dp__crc_8h.md#define-dp_crc_8_rohc)  `/* multi line expression */`<br>_CRC-8/ROHC. Check 0xD0,_ `refin` _true._ |
| define  | [**DP\_CRC\_8\_SMBUS**](dp__crc_8h.md#define-dp_crc_8_smbus)  `/* multi line expression */`<br>_CRC-8/SMBUS. Check 0xF4,_ `refin` _false._ |

## Detailed Description


A CRC is a model, not an algorithm: a width, a generator polynomial, an initial register, whether each input octet is reflected, whether the final register is, and a value XORed into the result. Every named CRC is those six numbers. This header holds the one loop that computes any of them and the named instances the tree uses, so a frame's FCS (CRC-32) and the burst frame's trailer (CRC-16-CCITT) are rows of data and cannot grow copies of the arithmetic. (The SIG fields' CRC-8 parameters are not a named row: they are the protocol's to state, see the last paragraph.)


Header-only, for the reason `dp_crc16.h` was: no component grows a link-line dependency for a ten-line kernel.


### The input is a bit stream; &lt;tt&gt;refin&lt;/tt&gt; is for the caller holding octets



The library moves bits unpacked, one per byte in the LSB (`conv`, the interleaver, a frame's stages). dp\_crc\_bits() takes exactly that and shifts the first bit presented into the register first, MSB-first. The catalogue's `refin` ("reflect each input byte") is a field of the model for a caller holding octets: dp\_crc\_bytes() feeds each octet LSB-first when `refin` is set, and calls the same bit loop. dp\_crc\_bits() never reads it: a stream already in bit order has nothing left to reflect, so for it `refin` is a property of the stream the caller builds. `refout` acts on the result, in both.



### Where the check values come from



The named instances below carry the catalogue's names and parameters (the CRC RevEng catalogue, [https://reveng.sourceforge.io/crc-catalogue/](https://reveng.sourceforge.io/crc-catalogue/)), and `test_dp_crc.c` pins each to the catalogue's check value, the CRC of the nine ASCII characters `123456789`. Those values define the named CRC. That is what the test holds: each named model's check value, nothing more.


**A named instance is not a claim that a standard uses it.** Which model a given protocol field takes, and in what bit order the value is transmitted, are facts in that protocol's text. Nothing here says so. 



    
## Public Static Functions Documentation




### function dp\_crc\_bits 

_The CRC of a bit stream: first bit presented is shifted in first._ 
```C++
static inline uint64_t dp_crc_bits (
    const dp_crc_t * p,
    const uint8_t * bits,
    size_t n
) 
```



Each input byte carries ONE bit (0/1) in its LSB, the form the frame paths pass around. The register shifts MSB-first, so presenting the bits in transmission order gives the usual CRC of a transmitted message, and an `refin = true` catalogue model needs each octet's bits presented LSB-first (see dp\_crc\_bytes()).


The value is returned right-aligned in `width` bits. How it is transmitted, MSB-first or otherwise, is the protocol's, not this function's.




**Parameters:**


* `p` A valid model (see dp\_crc\_valid(); not checked here). 
* `bits` `n` bytes, each 0 or 1 (only the LSB is used). 
* `n` Number of bits. 



**Returns:**

The CRC, in the low `p->width` bits. 





        

<hr>



### function dp\_crc\_bytes 

_The CRC of octets, the model's_ `refin` _choosing each octet's bit order._
```C++
static inline uint64_t dp_crc_bytes (
    const dp_crc_t * p,
    const uint8_t * bytes,
    size_t n
) 
```



Presents every octet's bits to the same step as dp\_crc\_bits(): LSB-first when `p->refin` is non-zero (the reflected CRCs: ISO-HDLC, CRC-32C, KERMIT, ROHC), MSB-first when it is zero.




**Parameters:**


* `p` A valid model. 
* `bytes` `n` octets. 
* `n` Number of octets. 



**Returns:**

The CRC, in the low `p->width` bits. 





        

<hr>



### function dp\_crc\_finish 

_The register's result: reflected if the model says so, then XORed. The one place that happens._ 
```C++
static inline uint64_t dp_crc_finish (
    const dp_crc_t * p,
    uint64_t reg
) 
```




<hr>



### function dp\_crc\_mask 

```C++
static inline uint64_t dp_crc_mask (
    unsigned width
) 
```




<hr>



### function dp\_crc\_reflect 

_The register's bits, reversed within_ `width` _._
```C++
static inline uint64_t dp_crc_reflect (
    uint64_t v,
    unsigned width
) 
```




<hr>



### function dp\_crc\_step 

_Shift one bit into the register: the only place the model's arithmetic is written._ 
```C++
static inline uint64_t dp_crc_step (
    const dp_crc_t * p,
    uint64_t reg,
    unsigned bit
) 
```



dp\_crc\_bits() and dp\_crc\_bytes() both call this, so there is one step and the two entry points cannot disagree about it.




**Parameters:**


* `p` A valid model. 
* `reg` The register, `width` bits. 
* `bit` The bit; only the LSB is used. 



**Returns:**

The register after the bit. 





        

<hr>



### function dp\_crc\_valid 

_Is_ `p` _a model this header can compute?_
```C++
static inline int dp_crc_valid (
    const dp_crc_t * p
) 
```





**Parameters:**


* `p` The model. 



**Returns:**

Non-zero if `width` is in `[1, 64]` and `poly`, `init` and `xorout` fit in `width` bits. A polynomial of zero is allowed (it is the degenerate CRC that shifts its register out). 





        

<hr>
## Macro Definition Documentation





### define DP\_CRC\_16\_IBM\_3740 

_CRC-16/IBM-3740, formerly CRC-16/CCITT-FALSE: the burst frame's trailer. Check 0x29B1,_ `refin` _false._
```C++
#define DP_CRC_16_IBM_3740 `/* multi line expression */`
```




<hr>



### define DP\_CRC\_16\_KERMIT 

_CRC-16/KERMIT. Check 0x2189,_ `refin` _true._
```C++
#define DP_CRC_16_KERMIT `/* multi line expression */`
```




<hr>



### define DP\_CRC\_16\_XMODEM 

_CRC-16/XMODEM. Check 0x31C3,_ `refin` _false._
```C++
#define DP_CRC_16_XMODEM `/* multi line expression */`
```




<hr>



### define DP\_CRC\_32C 

_CRC-32C (Castagnoli, iSCSI). Check 0xE3069283,_ `refin` _true._
```C++
#define DP_CRC_32C `/* multi line expression */`
```




<hr>



### define DP\_CRC\_32\_BZIP2 

_CRC-32/BZIP2: the same polynomial, not reflected. Check 0xFC891918,_ `refin` _false._
```C++
#define DP_CRC_32_BZIP2 `/* multi line expression */`
```




<hr>



### define DP\_CRC\_32\_ISO\_HDLC 

_CRC-32/ISO-HDLC (Ethernet, zlib). Check value 0xCBF43926. Its catalogue_ `refin` _is true: feed octets LSB-first, which dp\_crc\_bytes() does for it._
```C++
#define DP_CRC_32_ISO_HDLC `/* multi line expression */`
```




<hr>



### define DP\_CRC\_64\_ECMA\_182 

_CRC-64/ECMA-182. Check 0x6C40DF5F0B497347,_ `refin` _false._
```C++
#define DP_CRC_64_ECMA_182 `/* multi line expression */`
```




<hr>



### define DP\_CRC\_8\_ROHC 

_CRC-8/ROHC. Check 0xD0,_ `refin` _true._
```C++
#define DP_CRC_8_ROHC `/* multi line expression */`
```




<hr>



### define DP\_CRC\_8\_SMBUS 

_CRC-8/SMBUS. Check 0xF4,_ `refin` _false._
```C++
#define DP_CRC_8_SMBUS `/* multi line expression */`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dp_crc.h`

