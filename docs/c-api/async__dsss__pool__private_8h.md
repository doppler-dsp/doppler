

# File async\_dsss\_pool\_private.h



[**FileList**](files.md) **>** [**async\_dsss\_pool**](dir_8a6668f3097fb7847a23b1f65b3df26f.md) **>** [**async\_dsss\_pool\_private.h**](async__dsss__pool__private_8h.md)

[Go to the source code of this file](async__dsss__pool__private_8h_source.md)

_The ghost rule's counterfactual, for the measurement that keeps the rule as it is._ [More...](#detailed-description)

* `#include "doppler/async_dsss_pool/async_dsss_pool_core.h"`
* `#include <stddef.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_\_async\_dsss\_pool\_ghost\_of\_any\_k**](#function-dp__async_dsss_pool_ghost_of_any_k) (const [**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md) \* state, size\_t i, size\_t j) <br>[_**dp\_async\_dsss\_pool\_ghost\_of()**_](async__dsss__pool__core_8h.md#function-dp_async_dsss_pool_ghost_of) _with k = 0 admitted: whether receiver_`j` _would be released as receiver_`i's` _ghost if the rule took a carrier on its parent's own lattice point as well._ |




























## Detailed Description


Not part of the public API. The pool's ghost rule ([**dp\_async\_dsss\_pool\_ghost\_of()**](async__dsss__pool__core_8h.md#function-dp_async_dsss_pool_ghost_of), docs/design/async-dsss-receiver.md §8) takes only a whole NONZERO number of lattice steps: a ghost on its parent's own carrier (k = 0) is left alone, because a true emitter that shares a stronger one's carrier looks the same. Showing that takes the rule with k = 0 admitted, evaluated on the same rings by the same tests, which is this. 


    
## Public Functions Documentation




### function dp\_\_async\_dsss\_pool\_ghost\_of\_any\_k 

[_**dp\_async\_dsss\_pool\_ghost\_of()**_](async__dsss__pool__core_8h.md#function-dp_async_dsss_pool_ghost_of) _with k = 0 admitted: whether receiver_`j` _would be released as receiver_`i's` _ghost if the rule took a carrier on its parent's own lattice point as well._
```C++
int dp__async_dsss_pool_ghost_of_any_k (
    const dp_async_dsss_pool_state_t * state,
    size_t i,
    size_t j
) 
```



The same window, the same code-lock test, the same stillness and lattice tolerance; only k = 0 is no longer refused. The pool never calls it.




**Parameters:**


* `state` Must be non-NULL. 
* `i` The stronger holder. 
* `j` The candidate. 



**Returns:**

1 if the counterfactual rule takes `j`; 0 otherwise, and for an index past the pool, `i` equal to `j`, or a ring not yet full. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/async_dsss_pool/async_dsss_pool_private.h`

