

# Struct wfm\_seq\_t



[**ClassList**](annotated.md) **>** [**wfm\_seq\_t**](structwfm__seq__t.md)



_A run of bits, however it is produced._ [More...](#detailed-description)

* `#include <wfm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  const uint8\_t \* | [**bits**](#variable-bits)  <br> |
|  [**wfm\_seq\_kind\_t**](wfm__frame_8h.md#enum-wfm_seq_kind_t) | [**kind**](#variable-kind)  <br> |
|  size\_t | [**len**](#variable-len)  <br> |
|  int | [**lfsr**](#variable-lfsr)  <br> |
|  uint64\_t | [**poly**](#variable-poly)  <br> |
|  uint32\_t | [**reg\_bits**](#variable-reg_bits)  <br> |
|  uint64\_t | [**seed**](#variable-seed)  <br> |
|  uint64\_t | [**seed\_a**](#variable-seed_a)  <br> |
|  uint64\_t | [**seed\_b**](#variable-seed_b)  <br> |
|  uint64\_t | [**taps\_a**](#variable-taps_a)  <br> |
|  uint64\_t | [**taps\_b**](#variable-taps_b)  <br> |












































## Detailed Description


`len` is always the OUTPUT length in bits. For the generated kinds it is independent of the register width — `pn_create()`'s `length` argument is the register width (period `2^n - 1`), while `pn_generate(state, n, …)` decides how many bits come out. Conflating the two is easy and costly, so they are named apart here: `reg_bits` against `len`. 


    
## Public Attributes Documentation




### variable bits 

```C++
const uint8_t* wfm_seq_t::bits;
```



LITERAL only; NULL otherwise 
 


        

<hr>



### variable kind 

```C++
wfm_seq_kind_t wfm_seq_t::kind;
```




<hr>



### variable len 

```C++
size_t wfm_seq_t::len;
```



output bits; 0 means the field is absent 
 


        

<hr>



### variable lfsr 

```C++
int wfm_seq_t::lfsr;
```



PN\_GALOIS (0) or PN\_FIBONACCI (1) 
 


        

<hr>



### variable poly 

```C++
uint64_t wfm_seq_t::poly;
```



0 selects `pn_mls_poly(reg_bits)` — the same "default" `wfm_synth`'s `--pn-poly` means. A literal 0 reaching [**pn\_create()**](pn__core_8h.md#function-pn_create) is a register with no feedback: it emits the seed and then zeros, which is a CONSTANT field that still looks like a field. 
 


        

<hr>



### variable reg\_bits 

```C++
uint32_t wfm_seq_t::reg_bits;
```



register width 1..64; period 2^reg\_bits - 1 
 


        

<hr>



### variable seed 

```C++
uint64_t wfm_seq_t::seed;
```



0 selects 1; an all-zero register is a fixed point 


        

<hr>



### variable seed\_a 

```C++
uint64_t wfm_seq_t::seed_a;
```




<hr>



### variable seed\_b 

```C++
uint64_t wfm_seq_t::seed_b;
```




<hr>



### variable taps\_a 

```C++
uint64_t wfm_seq_t::taps_a;
```




<hr>



### variable taps\_b 

```C++
uint64_t wfm_seq_t::taps_b;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

