

# Struct dp\_pool



[**ClassList**](annotated.md) **>** [**dp\_pool**](structdp__pool.md)



_dp\_pool\_t's fields; all internal._ 

* `#include <dp_parallel.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**busy**](#variable-busy)  <br> |
|  [**dp\_cond\_t**](dp__thread_8h.md#typedef-dp_cond_t) | [**done**](#variable-done)  <br> |
|  unsigned long | [**gen**](#variable-gen)  <br> |
|  int | [**helpers**](#variable-helpers)  <br> |
|  [**dp\_mutex\_t**](dp__thread_8h.md#typedef-dp_mutex_t) | [**mu**](#variable-mu)  <br> |
|  int | [**stop**](#variable-stop)  <br> |
|  [**dp\_thread\_t**](dp__thread_8h.md#typedef-dp_thread_t) \* | [**th**](#variable-th)  <br> |
|  [**dp\_cond\_t**](dp__thread_8h.md#typedef-dp_cond_t) | [**wake**](#variable-wake)  <br> |
|  [**dp\_pf\_shared\_t**](structdp__pf__shared__t.md) | [**work**](#variable-work)  <br> |












































## Public Attributes Documentation




### variable busy 

```C++
int dp_pool::busy;
```



helpers still draining the current run 
 


        

<hr>



### variable done 

```C++
dp_cond_t dp_pool::done;
```



the caller waits here for the run's end 
 


        

<hr>



### variable gen 

```C++
unsigned long dp_pool::gen;
```



bumped per run; helpers wake on a change 


        

<hr>



### variable helpers 

```C++
int dp_pool::helpers;
```



helper threads actually running 
 


        

<hr>



### variable mu 

```C++
dp_mutex_t dp_pool::mu;
```



guards every field below except work.next 


        

<hr>



### variable stop 

```C++
int dp_pool::stop;
```



set by dp\_pool\_destroy(): helpers exit 


        

<hr>



### variable th 

```C++
dp_thread_t* dp_pool::th;
```



helper thread handles (max\_threads - 1 slots; the first `helpers` are live) 


        

<hr>



### variable wake 

```C++
dp_cond_t dp_pool::wake;
```



helpers wait here between runs 
 


        

<hr>



### variable work 

```C++
dp_pf_shared_t dp_pool::work;
```



the current run's range, body and cursor 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dp_parallel.h`

