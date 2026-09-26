

# Struct dp\_interp\_table\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_interp\_table\_state\_t**](structdp__interp__table__state__t.md)



_InterpolatedTable state._ [More...](#detailed-description)

* `#include <interp_table_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**method**](#variable-method)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  double \_Complex \* | [**table**](#variable-table)  <br> |












































## Detailed Description


Allocate with [**dp\_interp\_table\_create()**](interp__table__core_8h.md#function-dp_interp_table_create). `table` is a private copy (the caller's own array is not aliased or retained). 


    
## Public Attributes Documentation




### variable method 

```C++
int dp_interp_table_state_t::method;
```



0=floor, 1=nearest, 2=linear 
 


        

<hr>



### variable n 

```C++
size_t dp_interp_table_state_t::n;
```



table length (one period) 
 


        

<hr>



### variable table 

```C++
double _Complex* dp_interp_table_state_t::table;
```



owned copy, length n 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/interp_table/interp_table_core.h`

