

# Struct synth\_state\_t



[**ClassList**](annotated.md) **>** [**synth\_state\_t**](structsynth__state__t.md)



_Synth state._ [More...](#detailed-description)

* `#include <synth_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**awgn\_state\_t**](structawgn__state__t.md) \* | [**awgn**](#variable-awgn)  <br> |
|  float | [**cur\_im**](#variable-cur_im)  <br> |
|  float | [**cur\_re**](#variable-cur_re)  <br> |
|  [**lo\_state\_t**](structlo__state__t.md) \* | [**lo**](#variable-lo)  <br> |
|  int | [**nsps**](#variable-nsps)  <br> |
|  [**pn\_state\_t**](structpn__state__t.md) \* | [**pn**](#variable-pn)  <br> |
|  int | [**sym\_pos**](#variable-sym_pos)  <br> |
|  int | [**wtype**](#variable-wtype)  <br> |












































## Detailed Description


Allocate with [**synth\_create()**](synth__core_8h.md#function-synth_create). 


    
## Public Attributes Documentation




### variable awgn 

```C++
awgn_state_t* synth_state_t::awgn;
```




<hr>



### variable cur\_im 

```C++
float synth_state_t::cur_im;
```




<hr>



### variable cur\_re 

```C++
float synth_state_t::cur_re;
```




<hr>



### variable lo 

```C++
lo_state_t* synth_state_t::lo;
```




<hr>



### variable nsps 

```C++
int synth_state_t::nsps;
```




<hr>



### variable pn 

```C++
pn_state_t* synth_state_t::pn;
```




<hr>



### variable sym\_pos 

```C++
int synth_state_t::sym_pos;
```




<hr>



### variable wtype 

```C++
int synth_state_t::wtype;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/synth/synth_core.h`

