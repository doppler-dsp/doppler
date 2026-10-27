

# Struct dp\_pf\_shared\_t



[**ClassList**](annotated.md) **>** [**dp\_pf\_shared\_t**](structdp__pf__shared__t.md)



_One parallel-for run: the range, the work, and the shared cursor._ [More...](#detailed-description)

* `#include <dp_parallel.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  void(\* | [**body**](#variable-body)  <br> |
|  void \* | [**ctx**](#variable-ctx)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  atomic\_size\_t | [**next**](#variable-next)  <br> |












































## Detailed Description


Shared by every worker of a dp\_parallel\_for() or dp\_pool\_run() call; workers claim indices by fetch-add on `next`. Internal: callers never build one. 


    
## Public Attributes Documentation




### variable body 

```C++
void(* dp_pf_shared_t::body) (size_t, void *);
```



per-index work, [**body(i, ctx)**](structdp__pf__shared__t.md#variable-body) 
 


        

<hr>



### variable ctx 

```C++
void* dp_pf_shared_t::ctx;
```



shared read-only context, passed through to every [**body()**](structdp__pf__shared__t.md#variable-body) call 
 


        

<hr>



### variable n 

```C++
size_t dp_pf_shared_t::n;
```



index count: work covers [0, n) 
 


        

<hr>



### variable next 

```C++
atomic_size_t dp_pf_shared_t::next;
```



lock-free work cursor (fetch-add): the next unclaimed index 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dp_parallel.h`

