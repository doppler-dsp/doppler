

# Struct rs\_t



[**ClassList**](annotated.md) **>** [**rs\_t**](structrs__t.md)



_A code plus the tables derived from it._ [More...](#detailed-description)

* `#include <rs_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**rs\_code\_t**](structrs__code__t.md) | [**code**](#variable-code)  <br> |
|  unsigned | [**e**](#variable-e)  <br> |
|  uint8\_t | [**exp**](#variable-exp)  <br> |
|  uint8\_t | [**gen**](#variable-gen)  <br> |
|  unsigned | [**k**](#variable-k)  <br> |
|  uint8\_t | [**log**](#variable-log)  <br> |
|  unsigned | [**n**](#variable-n)  <br> |












































## Detailed Description


Transparent and allocation-free so a caller can put one on the stack or in its own state, and so the derived sizes are readable without an accessor. Build it with [**rs\_init**](rs__core_8h.md#function-rs_init) and then treat it as read-only: it carries no running state, and every function below takes it as `const`. 


    
## Public Attributes Documentation




### variable code 

```C++
rs_code_t rs_t::code;
```



as given to [**rs\_init**](rs__core_8h.md#function-rs_init) 
 


        

<hr>



### variable e 

```C++
unsigned rs_t::e;
```



correctable symbols, `nroots / 2` 


        

<hr>



### variable exp 

```C++
uint8_t rs_t::exp[2 *RS_N_MAX];
```



`a^i`, doubled to avoid a modulo 
 


        

<hr>



### variable gen 

```C++
uint8_t rs_t::gen[RS_NROOTS_MAX+1];
```



`g(x)`, `gen[i]` for `x^i` 
 


        

<hr>



### variable k 

```C++
unsigned rs_t::k;
```



information symbols, `n - nroots` 


        

<hr>



### variable log 

```C++
uint8_t rs_t::log[RS_N_MAX+1];
```



`log_a`, index 0 unused 
 


        

<hr>



### variable n 

```C++
unsigned rs_t::n;
```



symbols per codeword, `2^J - 1` 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/rs/rs_core.h`

