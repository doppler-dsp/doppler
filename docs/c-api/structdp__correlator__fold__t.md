

# Struct dp\_correlator\_fold\_t



[**ClassList**](annotated.md) **>** [**dp\_correlator\_fold\_t**](structdp__correlator__fold__t.md)



_A C reducer: called once per map, in h order, by_ [_**dp\_correlator\_execute\_fold()**_](correlator__core_8h.md#function-dp_correlator_execute_fold) _._[More...](#detailed-description)

* `#include <correlator_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  void \* | [**ctx**](#variable-ctx)  <br> |
|  void(\* | [**fold**](#variable-fold)  <br> |












































## Detailed Description


`map` holds `n_out` values laid out as execute() writes them, and is valid only for the duration of the call. 


    
## Public Attributes Documentation




### variable ctx 

```C++
void* dp_correlator_fold_t::ctx;
```



Passed to every call of [**fold**](structdp__correlator__fold__t.md#variable-fold). 


        

<hr>



### variable fold 

```C++
void(* dp_correlator_fold_t::fold) (void *ctx, size_t h, const float _Complex *map, size_t n_out);
```



Fold map `h` into the result `ctx` names. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/correlator/correlator_core.h`

