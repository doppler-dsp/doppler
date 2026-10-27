

# Struct dp\_reader\_t



[**ClassList**](annotated.md) **>** [**dp\_reader\_t**](structdp__reader__t.md)



_Bounds-checked read cursor over a state blob; the mirror of_ [_**dp\_writer\_t**_](structdp__writer__t.md) _with the same sticky-error model (an overrun sets_`err` _, and later reads leave their destination untouched)._

* `#include <dp_state.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  const uint8\_t \* | [**buf**](#variable-buf)  <br> |
|  size\_t | [**cap**](#variable-cap)  <br> |
|  int | [**err**](#variable-err)  <br> |
|  size\_t | [**off**](#variable-off)  <br> |












































## Public Attributes Documentation




### variable buf 

```C++
const uint8_t* dp_reader_t::buf;
```



Start of the blob being read (not owned). 
 


        

<hr>



### variable cap 

```C++
size_t dp_reader_t::cap;
```



Blob size in bytes; reads past it refused. 
 


        

<hr>



### variable err 

```C++
int dp_reader_t::err;
```



Sticky: 1 after any overrun, then reads no-op. 


        

<hr>



### variable off 

```C++
size_t dp_reader_t::off;
```



Bytes consumed so far; next read starts here. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dp_state.h`

