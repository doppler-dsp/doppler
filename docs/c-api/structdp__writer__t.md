

# Struct dp\_writer\_t



[**ClassList**](annotated.md) **>** [**dp\_writer\_t**](structdp__writer__t.md)



_Bounds-checked write cursor over a caller-owned state blob._ [More...](#detailed-description)

* `#include <dp_state.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t \* | [**buf**](#variable-buf)  <br> |
|  size\_t | [**cap**](#variable-cap)  <br> |
|  int | [**err**](#variable-err)  <br> |
|  size\_t | [**off**](#variable-off)  <br> |












































## Detailed Description


Sticky-error model: a bounds overrun sets `err` and subsequent writes no-op, so call sites stay flat (check `w.err` once at the end if desired). Overrun cannot happen when the caller allocated obj\_state\_bytes() — it is a guard. 


    
## Public Attributes Documentation




### variable buf 

```C++
uint8_t* dp_writer_t::buf;
```



Start of the blob being written (not owned). 
 


        

<hr>



### variable cap 

```C++
size_t dp_writer_t::cap;
```



Blob capacity in bytes; writes past it are refused. 


        

<hr>



### variable err 

```C++
int dp_writer_t::err;
```



Sticky: 1 after any overrun, then writes no-op. 
 


        

<hr>



### variable off 

```C++
size_t dp_writer_t::off;
```



Bytes written so far; next write lands here. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dp_state.h`

