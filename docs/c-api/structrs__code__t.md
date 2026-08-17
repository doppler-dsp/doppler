

# Struct rs\_code\_t



[**ClassList**](annotated.md) **>** [**rs\_code\_t**](structrs__code__t.md)



_A Reed-Solomon code over_ `GF(2^J)` _._[More...](#detailed-description)

* `#include <rs_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint16\_t | [**field\_poly**](#variable-field_poly)  <br> |
|  unsigned | [**first\_root**](#variable-first_root)  <br> |
|  unsigned | [**nroots**](#variable-nroots)  <br> |
|  unsigned | [**root\_stride**](#variable-root_stride)  <br> |
|  unsigned | [**symbol\_bits**](#variable-symbol_bits)  <br> |












































## Detailed Description


`first_root` and `root_stride` together fix the generator's roots: `g(x) = prod (x - a^(root_stride * j))` for `j` in `[first_root, first_root + nroots)`. The textbook choice is a stride of 1; CCSDS 4.3.4 uses 11, which is a legitimate choice precisely because `a^11` is itself primitive, and a code no receiver expecting consecutive powers of `a` can decode. 


    
## Public Attributes Documentation




### variable field\_poly 

```C++
uint16_t rs_code_t::field_poly;
```



`F(x)` low `J` bits, `x^J` implicit 
 


        

<hr>



### variable first\_root 

```C++
unsigned rs_code_t::first_root;
```



`j0`: the first root is `a^(s*j0)` 
 


        

<hr>



### variable nroots 

```C++
unsigned rs_code_t::nroots;
```



parity symbols `2E`, 2..RS\_NROOTS\_MAX 


        

<hr>



### variable root\_stride 

```C++
unsigned rs_code_t::root_stride;
```



`s`, coprime with `n` 
 


        

<hr>



### variable symbol\_bits 

```C++
unsigned rs_code_t::symbol_bits;
```



`J`, 2..RS\_SYMBOL\_BITS\_MAX 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/rs/rs_core.h`

