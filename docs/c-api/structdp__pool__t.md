

# Struct dp\_pool\_t



[**ClassList**](annotated.md) **>** [**dp\_pool\_t**](structdp__pool__t.md)





* `#include <dp_parallel.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**busy**](#variable-busy)  <br> |
|  pthread\_cond\_t | [**done**](#variable-done)  <br> |
|  unsigned long | [**gen**](#variable-gen)  <br> |
|  int | [**helpers**](#variable-helpers)  <br> |
|  pthread\_mutex\_t | [**mu**](#variable-mu)  <br> |
|  int | [**stop**](#variable-stop)  <br> |
|  pthread\_t \* | [**th**](#variable-th)  <br> |
|  pthread\_cond\_t | [**wake**](#variable-wake)  <br> |
|  [**dp\_pf\_shared\_t**](structdp__pf__shared__t.md) | [**work**](#variable-work)  <br> |












































## Public Attributes Documentation




### variable busy 

```C++
int dp_pool_t::busy;
```




<hr>



### variable done 

```C++
pthread_cond_t dp_pool_t::done;
```




<hr>



### variable gen 

```C++
unsigned long dp_pool_t::gen;
```




<hr>



### variable helpers 

```C++
int dp_pool_t::helpers;
```




<hr>



### variable mu 

```C++
pthread_mutex_t dp_pool_t::mu;
```




<hr>



### variable stop 

```C++
int dp_pool_t::stop;
```




<hr>



### variable th 

```C++
pthread_t* dp_pool_t::th;
```




<hr>



### variable wake 

```C++
pthread_cond_t dp_pool_t::wake;
```




<hr>



### variable work 

```C++
dp_pf_shared_t dp_pool_t::work;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_parallel.h`

