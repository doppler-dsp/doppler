

# File dp\_parallel.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_parallel.h**](dp__parallel_8h.md)

[Go to the source code of this file](dp__parallel_8h_source.md)



* `#include "clib_common.h"`
* `#include "dp_thread.h"`
* `#include <stdatomic.h>`
* `#include <stddef.h>`
* `#include <stdlib.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_pf\_shared\_t**](structdp__pf__shared__t.md) <br> |
| struct | [**dp\_pool\_t**](structdp__pool__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|   | [**DP\_THREAD\_FN**](#function-dp_thread_fn) (dp\_pf\_worker, arg) <br> |
|   | [**DP\_THREAD\_FN**](#function-dp_thread_fn) (dp\_pool\_worker, arg) <br> |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_parallel\_for**](#function-dp_parallel_for) (size\_t n, void(\*)(size\_t, void \*) body, void \* ctx, int max\_threads) <br> |
|  [**dp\_pool\_t**](structdp__pool__t.md) \* | [**dp\_pool\_create**](#function-dp_pool_create) (int max\_threads) <br> |
|  void | [**dp\_pool\_destroy**](#function-dp_pool_destroy) ([**dp\_pool\_t**](structdp__pool__t.md) \* p) <br> |
|  void | [**dp\_pool\_run**](#function-dp_pool_run) ([**dp\_pool\_t**](structdp__pool__t.md) \* p, size\_t n, void(\*)(size\_t, void \*) body, void \* ctx) <br> |
|  int | [**dp\_pool\_threads**](#function-dp_pool_threads) (const [**dp\_pool\_t**](structdp__pool__t.md) \* p) <br> |


























## Public Functions Documentation




### function DP\_THREAD\_FN 

```C++
DP_THREAD_FN (
    dp_pf_worker,
    arg
) 
```




<hr>



### function DP\_THREAD\_FN 

```C++
DP_THREAD_FN (
    dp_pool_worker,
    arg
) 
```




<hr>
## Public Static Functions Documentation




### function dp\_parallel\_for 

```C++
static inline void dp_parallel_for (
    size_t n,
    void(*)(size_t, void *) body,
    void * ctx,
    int max_threads
) 
```




<hr>



### function dp\_pool\_create 

```C++
static inline dp_pool_t * dp_pool_create (
    int max_threads
) 
```




<hr>



### function dp\_pool\_destroy 

```C++
static inline void dp_pool_destroy (
    dp_pool_t * p
) 
```




<hr>



### function dp\_pool\_run 

```C++
static inline void dp_pool_run (
    dp_pool_t * p,
    size_t n,
    void(*)(size_t, void *) body,
    void * ctx
) 
```




<hr>



### function dp\_pool\_threads 

```C++
static inline int dp_pool_threads (
    const dp_pool_t * p
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_parallel.h`

