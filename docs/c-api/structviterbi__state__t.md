

# Struct viterbi\_state\_t



[**ClassList**](annotated.md) **>** [**viterbi\_state\_t**](structviterbi__state__t.md)



_Viterbi state._ [More...](#detailed-description)

* `#include <viterbi_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**conv\_code\_t**](structconv__code__t.md) | [**code**](#variable-code)  <br> |
|  uint8\_t \* | [**dec**](#variable-dec)  <br> |
|  size\_t | [**depth**](#variable-depth)  <br> |
|  size\_t | [**fill**](#variable-fill)  <br> |
|  size\_t | [**head**](#variable-head)  <br> |
|  unsigned \* | [**inbit**](#variable-inbit)  <br> |
|  uint32\_t | [**nstate**](#variable-nstate)  <br> |
|  unsigned \* | [**out0**](#variable-out0)  <br> |
|  unsigned \* | [**out1**](#variable-out1)  <br> |
|  float \* | [**pm**](#variable-pm)  <br> |
|  float \* | [**pm2**](#variable-pm2)  <br> |
|  uint32\_t \* | [**pred0**](#variable-pred0)  <br> |
|  uint32\_t \* | [**pred1**](#variable-pred1)  <br> |












































## Detailed Description


Allocate with [**viterbi\_create()**](viterbi__core_8h.md#function-viterbi_create). 


    
## Public Attributes Documentation




### variable code 

```C++
conv_code_t viterbi_state_t::code;
```




<hr>



### variable dec 

```C++
uint8_t* viterbi_state_t::dec;
```



depth x nstate decisions: which predecessor won 
 


        

<hr>



### variable depth 

```C++
size_t viterbi_state_t::depth;
```




<hr>



### variable fill 

```C++
size_t viterbi_state_t::fill;
```



steps recorded, saturating at depth 
 


        

<hr>



### variable head 

```C++
size_t viterbi_state_t::head;
```



ring cursor, in steps 
 


        

<hr>



### variable inbit 

```C++
unsigned* viterbi_state_t::inbit;
```




<hr>



### variable nstate 

```C++
uint32_t viterbi_state_t::nstate;
```




<hr>



### variable out0 

```C++
unsigned* viterbi_state_t::out0;
```




<hr>



### variable out1 

```C++
unsigned* viterbi_state_t::out1;
```




<hr>



### variable pm 

```C++
float* viterbi_state_t::pm;
```



path metric per state 
 


        

<hr>



### variable pm2 

```C++
float* viterbi_state_t::pm2;
```



the next step's, swapped rather than copied 
 


        

<hr>



### variable pred0 

```C++
uint32_t* viterbi_state_t::pred0;
```




<hr>



### variable pred1 

```C++
uint32_t* viterbi_state_t::pred1;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/viterbi/viterbi_core.h`

