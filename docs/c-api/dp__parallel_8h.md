

# File dp\_parallel.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_parallel.h**](dp__parallel_8h.md)

[Go to the source code of this file](dp__parallel_8h_source.md)



* `#include <pthread.h>`
* `#include <stdatomic.h>`
* `#include <stddef.h>`
* `#include <stdlib.h>`
* `#include <unistd.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_pf\_shared\_t**](structdp__pf__shared__t.md) <br> |
























## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_parallel\_for**](#function-dp_parallel_for) (size\_t n, void(\*)(size\_t, void \*) body, void \* ctx, int max\_threads) <br> |
|  void \* | [**dp\_pf\_worker**](#function-dp_pf_worker) (void \* arg) <br> |


























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



### function dp\_pf\_worker 

```C++
static void * dp_pf_worker (
    void * arg
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_parallel.h`

