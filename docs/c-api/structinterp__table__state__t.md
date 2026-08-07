

# Struct interp\_table\_state\_t



[**ClassList**](annotated.md) **>** [**interp\_table\_state\_t**](structinterp__table__state__t.md)



_InterpolatedTable state._ [More...](#detailed-description)

* `#include <interp_table_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**method**](#variable-method)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  double complex \* | [**table**](#variable-table)  <br> |












































## Detailed Description


Allocate with [**interp\_table\_create()**](interp__table__core_8h.md#function-interp_table_create). `table` is a private copy (the caller's own array is not aliased or retained). 


    
## Public Attributes Documentation




### variable method 

```C++
int interp_table_state_t::method;
```



0=floor, 1=nearest, 2=linear 
 


        

<hr>



### variable n 

```C++
size_t interp_table_state_t::n;
```



table length (one period) 
 


        

<hr>



### variable table 

```C++
double complex* interp_table_state_t::table;
```



owned copy, length n 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/interp_table/interp_table_core.h`

