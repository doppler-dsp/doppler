

# Struct conv\_enc\_state\_t



[**ClassList**](annotated.md) **>** [**conv\_enc\_state\_t**](structconv__enc__state__t.md)



_A code and the register encoding it, together._ [More...](#detailed-description)

* `#include <conv_enc_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**conv\_code\_t**](structconv__code__t.md) | [**code**](#variable-code)  <br> |
|  [**conv\_enc\_t**](structconv__enc__t.md) | [**enc**](#variable-enc)  <br> |












































## Detailed Description


Pointer-free and small — the code is copied, so the caller's may be temporary and the encoder cannot be invalidated by something it does not own. 


    
## Public Attributes Documentation




### variable code 

```C++
conv_code_t conv_enc_state_t::code;
```



the code; copied at create 
 


        

<hr>



### variable enc 

```C++
conv_enc_t conv_enc_state_t::enc;
```



the k-1 previous inputs, newest high 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/conv_enc/conv_enc_core.h`

