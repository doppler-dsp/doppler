

# Struct despreader\_state\_t



[**ClassList**](annotated.md) **>** [**despreader\_state\_t**](structdespreader__state__t.md)



_Despreader state._ [More...](#detailed-description)

* `#include <despreader_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**bit\_acc**](#variable-bit_acc)  <br> |
|  size\_t | [**bit\_phase**](#variable-bit_phase)  <br> |
|  [**costas\_state\_t**](structcostas__state__t.md) | [**car**](#variable-car)  <br> |
|  [**dll\_state\_t**](structdll__state__t.md) | [**code**](#variable-code)  <br> |
|  uint8\_t \* | [**code\_copy**](#variable-code_copy)  <br> |
|  size\_t | [**epoch\_count**](#variable-epoch_count)  <br> |
|  size\_t | [**epochs\_in\_bit**](#variable-epochs_in_bit)  <br> |
|  size\_t \* | [**flip\_hist**](#variable-flip_hist)  <br> |
|  int | [**have\_prev**](#variable-have_prev)  <br> |
|  size\_t | [**periods\_per\_bit**](#variable-periods_per_bit)  <br> |
|  int | [**prev\_sign**](#variable-prev_sign)  <br> |
|  [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* | [**tlm\_ctx**](#variable-tlm_ctx)  <br> |












































## Detailed Description


Allocate with [**despreader\_create()**](despreader__core_8h.md#function-despreader_create). Embeds the carrier (`car`) and code (`code`) loops by value; the despreader owns the copied spreading code and the bit-sync histogram. 


    
## Public Attributes Documentation




### variable bit\_acc 

```C++
double despreader_state_t::bit_acc;
```



running sum of Re(prompt) over the bit. 


        

<hr>



### variable bit\_phase 

```C++
size_t despreader_state_t::bit_phase;
```



detected bit boundary (argmax flip\_hist). 


        

<hr>



### variable car 

```C++
costas_state_t despreader_state_t::car;
```



carrier (Costas/FLL-assisted-PLL) loop. 


        

<hr>



### variable code 

```C++
dll_state_t despreader_state_t::code;
```



code (early/prompt/late DLL) loop. 


        

<hr>



### variable code\_copy 

```C++
uint8_t* despreader_state_t::code_copy;
```



owned copy of the spreading code. 


        

<hr>



### variable epoch\_count 

```C++
size_t despreader_state_t::epoch_count;
```



code periods processed so far. 


        

<hr>



### variable epochs\_in\_bit 

```C++
size_t despreader_state_t::epochs_in_bit;
```



periods accumulated in the current bit. 


        

<hr>



### variable flip\_hist 

```C++
size_t* despreader_state_t::flip_hist;
```



prompt sign-flip histogram, length np. 


        

<hr>



### variable have\_prev 

```C++
int despreader_state_t::have_prev;
```



prev\_sign valid. 


        

<hr>



### variable periods\_per\_bit 

```C++
size_t despreader_state_t::periods_per_bit;
```



code periods per data bit (&gt;=1). 


        

<hr>



### variable prev\_sign 

```C++
int despreader_state_t::prev_sign;
```



previous prompt sign (+1/-1). 


        

<hr>



### variable tlm\_ctx 

```C++
dp_tlm_t* despreader_state_t::tlm_ctx;
```



telemetry gate: non-NULL when the embedded loops are attached (despreader\_set\_telemetry); the probes live on car.tlm / code.tlm. Not serialized (field-wise triplet skips it). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/despreader/despreader_core.h`

