

# Struct wfm\_synth\_state\_t



[**ClassList**](annotated.md) **>** [**wfm\_synth\_state\_t**](structwfm__synth__state__t.md)



_Synth state._ [More...](#detailed-description)

* `#include <wfm_synth_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**awgn\_state\_t**](structawgn__state__t.md) \* | [**awgn**](#variable-awgn)  <br> |
|  size\_t | [**bit\_idx**](#variable-bit_idx)  <br> |
|  int | [**bit\_mod**](#variable-bit_mod)  <br> |
|  uint8\_t \* | [**bits**](#variable-bits)  <br> |
|  double | [**chirp\_f0**](#variable-chirp_f0)  <br> |
|  double | [**chirp\_fend**](#variable-chirp_fend)  <br> |
|  double | [**chirp\_k**](#variable-chirp_k)  <br> |
|  size\_t | [**chirp\_n**](#variable-chirp_n)  <br> |
|  double | [**chirp\_ph**](#variable-chirp_ph)  <br> |
|  size\_t | [**chirp\_span**](#variable-chirp_span)  <br> |
|  float | [**cur\_im**](#variable-cur_im)  <br> |
|  float | [**cur\_re**](#variable-cur_re)  <br> |
|  [**fir\_state\_t**](structfir__state__t.md) \* | [**fir**](#variable-fir)  <br> |
|  [**lo\_state\_t**](structlo__state__t.md) \* | [**lo**](#variable-lo)  <br> |
|  size\_t | [**n\_bits**](#variable-n_bits)  <br> |
|  size\_t | [**n\_symbols**](#variable-n_symbols)  <br> |
|  int | [**nsps**](#variable-nsps)  <br> |
|  [**pn\_state\_t**](structpn__state__t.md) \* | [**pn**](#variable-pn)  <br> |
|  int | [**sym\_pos**](#variable-sym_pos)  <br> |
|  size\_t | [**sym\_read\_idx**](#variable-sym_read_idx)  <br> |
|  float \_Complex \* | [**symbols**](#variable-symbols)  <br> |
|  int | [**wtype**](#variable-wtype)  <br> |












































## Detailed Description


Allocate with [**wfm\_synth\_create()**](wfm__synth__core_8h.md#function-wfm_synth_create). 


    
## Public Attributes Documentation




### variable awgn 

```C++
awgn_state_t* wfm_synth_state_t::awgn;
```




<hr>



### variable bit\_idx 

```C++
size_t wfm_synth_state_t::bit_idx;
```




<hr>



### variable bit\_mod 

```C++
int wfm_synth_state_t::bit_mod;
```




<hr>



### variable bits 

```C++
uint8_t* wfm_synth_state_t::bits;
```




<hr>



### variable chirp\_f0 

```C++
double wfm_synth_state_t::chirp_f0;
```




<hr>



### variable chirp\_fend 

```C++
double wfm_synth_state_t::chirp_fend;
```




<hr>



### variable chirp\_k 

```C++
double wfm_synth_state_t::chirp_k;
```




<hr>



### variable chirp\_n 

```C++
size_t wfm_synth_state_t::chirp_n;
```




<hr>



### variable chirp\_ph 

```C++
double wfm_synth_state_t::chirp_ph;
```




<hr>



### variable chirp\_span 

```C++
size_t wfm_synth_state_t::chirp_span;
```




<hr>



### variable cur\_im 

```C++
float wfm_synth_state_t::cur_im;
```




<hr>



### variable cur\_re 

```C++
float wfm_synth_state_t::cur_re;
```




<hr>



### variable fir 

```C++
fir_state_t* wfm_synth_state_t::fir;
```




<hr>



### variable lo 

```C++
lo_state_t* wfm_synth_state_t::lo;
```




<hr>



### variable n\_bits 

```C++
size_t wfm_synth_state_t::n_bits;
```




<hr>



### variable n\_symbols 

```C++
size_t wfm_synth_state_t::n_symbols;
```




<hr>



### variable nsps 

```C++
int wfm_synth_state_t::nsps;
```




<hr>



### variable pn 

```C++
pn_state_t* wfm_synth_state_t::pn;
```




<hr>



### variable sym\_pos 

```C++
int wfm_synth_state_t::sym_pos;
```




<hr>



### variable sym\_read\_idx 

```C++
size_t wfm_synth_state_t::sym_read_idx;
```




<hr>



### variable symbols 

```C++
float _Complex* wfm_synth_state_t::symbols;
```




<hr>



### variable wtype 

```C++
int wfm_synth_state_t::wtype;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm_synth/wfm_synth_core.h`

