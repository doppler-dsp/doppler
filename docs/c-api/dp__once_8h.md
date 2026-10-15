

# File dp\_once.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_once.h**](dp__once_8h.md)

[Go to the source code of this file](dp__once_8h_source.md)

_Run a function exactly once, however many threads ask at once._ [More...](#detailed-description)

* `#include <pthread.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_once\_t**](structdp__once__t.md) <br>_The state of one one-time build; start it at_ [_**DP\_ONCE\_INIT**_](dp__once_8h.md#define-dp_once_init) _._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_once**](#function-dp_once) ([**dp\_once\_t**](structdp__once__t.md) \* once, void(\*)(void) fn) <br>_Run_ `fn` _once, and return only after it has finished._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_ONCE\_INIT**](dp__once_8h.md#define-dp_once_init)  `{ PTHREAD\_ONCE\_INIT }`<br>_The initializer for a_ [_**dp\_once\_t**_](structdp__once__t.md) _whose build has not run._ |

## Detailed Description


The library builds some tables lazily, on first use: the AWGN and LO sine LUTs, the CCSDS Reed-Solomon field tables and its marker's bits. A plain `static int ready` flag guarding such a table is a DATA RACE, not a harmless double build. Nothing orders the flag's store after the table's stores for another thread, so that thread can see `ready == 1` and read a table still being written (arm64 reorders those stores, and so may the compiler). Two threads can also both see 0 and both write. First use is concurrent in shipped code: `Plan.prepare` fans the per-source builds across [**dp\_parallel.h**](dp__parallel_8h.md), and every block method releases the GIL (doppler#2328, gh-817).


[**dp\_once()**](dp__once_8h.md#function-dp_once) is the one guard for all of them, and it is the platform's own once primitive behind one signature: `pthread_once` on POSIX, `InitOnceExecuteOnce` on Windows, the pair rs.c used privately since gh-817. The first caller runs the build; every other caller BLOCKS in the primitive until it has finished, then returns and sees everything it wrote. A blocked waiter sleeps rather than spins, so a waiter that outranks the builder on its core cannot starve it, and an oversubscribed machine spends nothing on the wait. glibc also restarts, in a forked child, a once whose build `fork()` interrupted, where a hand-rolled flag would wait forever.


The implementation lives in dp\_once.c, so `<windows.h>` stays in that one file: a component that builds a table includes this header and pulls in `<pthread.h>` at most. On glibc before 2.34 `pthread_once` needs `-lpthread`, which the `dp_once_core` target carries for every component that links it. 


    
## Public Functions Documentation




### function dp\_once 

_Run_ `fn` _once, and return only after it has finished._
```C++
void dp_once (
    dp_once_t * once,
    void(*)(void) fn
) 
```



Every caller, on any thread, returns after the single call of `fn` has completed, and sees everything `fn` wrote. The callers that lose the race to run it block until it is done.




**Parameters:**


* `once` Its state, statically initialized with [**DP\_ONCE\_INIT**](dp__once_8h.md#define-dp_once_init). 
* `fn` The build; it must not call [**dp\_once()**](dp__once_8h.md#function-dp_once) on the same `once`.


```C++
static float     table[256];
static dp_once_t table_once = DP_ONCE_INIT;

static void
table_build (void)
{
  for (int i = 0; i < 256; i++)
    table[i] = (float)i;
}

// In every function that reads the table:
dp_once (&table_once, table_build);
```
 


        

<hr>
## Macro Definition Documentation





### define DP\_ONCE\_INIT 

_The initializer for a_ [_**dp\_once\_t**_](structdp__once__t.md) _whose build has not run._
```C++
#define DP_ONCE_INIT `{ PTHREAD_ONCE_INIT }`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dp_once.h`

