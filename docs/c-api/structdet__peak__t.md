

# Struct det\_peak\_t



[**ClassList**](annotated.md) **>** [**det\_peak\_t**](structdet__peak__t.md)



_One listed peak of a surface: its cell and value._ [More...](#detailed-description)

* `#include <det_private.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**col**](#variable-col)  <br> |
|  size\_t | [**row**](#variable-row)  <br> |
|  float | [**value**](#variable-value)  <br> |












































## Detailed Description


One listed peak of a surface: a cell and its value, in the surface's own units (a magnitude on a coherent surface, a power on a non-coherent one). What det\_peak\_list() ([**det\_private.h**](det__private_8h.md)) returns, and what the acquisition engine keeps per dwell. 


    
## Public Attributes Documentation




### variable col 

```C++
size_t det_peak_t::col;
```



surface column 
 


        

<hr>



### variable row 

```C++
size_t det_peak_t::row;
```



surface row 
 


        

<hr>



### variable value 

```C++
float det_peak_t::value;
```



the cell's value 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/detector/det_private.h`

