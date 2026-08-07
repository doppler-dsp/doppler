

# Struct dp\_pf\_shared\_t



[**ClassList**](annotated.md) **>** [**dp\_pf\_shared\_t**](structdp__pf__shared__t.md)





* `#include <dp_parallel.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  void(\* | [**body**](#variable-body)  <br> |
|  void \* | [**ctx**](#variable-ctx)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  atomic\_size\_t | [**next**](#variable-next)  <br> |












































## Public Attributes Documentation




### variable body 

```C++
void(* dp_pf_shared_t::body) (size_t, void *);
```




<hr>



### variable ctx 

```C++
void* dp_pf_shared_t::ctx;
```




<hr>



### variable n 

```C++
size_t dp_pf_shared_t::n;
```




<hr>



### variable next 

```C++
atomic_size_t dp_pf_shared_t::next;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_parallel.h`

