

# File dp\_interrupt\_guard\_core.h



[**FileList**](files.md) **>** [**dp\_interrupt\_guard**](dir_001936014fd0d8bf32545bf8d71a57c6.md) **>** [**dp\_interrupt\_guard\_core.h**](dp__interrupt__guard__core_8h.md)

[Go to the source code of this file](dp__interrupt__guard__core_8h_source.md)



* `#include "dp_interrupt.h"`
* `#include <stddef.h>`
* `#include <stdint.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef [**dp\_interrupt\_guard\_t**](dp__interrupt__guard__core_8h.md#typedef-dp_interrupt_guard_t) | [**dp\_interrupt\_guard\_state\_t**](#typedef-dp_interrupt_guard_state_t)  <br> |
| typedef struct dp\_interrupt\_guard | [**dp\_interrupt\_guard\_t**](#typedef-dp_interrupt_guard_t)  <br>_A scoped handle to the process-wide interrupt facility._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_interrupt\_guard\_t**](dp__interrupt__guard__core_8h.md#typedef-dp_interrupt_guard_t) \* | [**dp\_interrupt\_guard\_create**](#function-dp_interrupt_guard_create) (const int32\_t \* signals, size\_t n\_signals, uint32\_t latency\_ms) <br>_Clear the flag, optionally install handlers, and remember what to undo._  |
|  void | [**dp\_interrupt\_guard\_destroy**](#function-dp_interrupt_guard_destroy) ([**dp\_interrupt\_guard\_t**](dp__interrupt__guard__core_8h.md#typedef-dp_interrupt_guard_t) \* guard) <br>_Restore every handler and latency this guard changed._  |
|  void | [**dp\_interrupt\_guard\_interrupt**](#function-dp_interrupt_guard_interrupt) ([**dp\_interrupt\_guard\_t**](dp__interrupt__guard__core_8h.md#typedef-dp_interrupt_guard_t) \* guard) <br>_Ask every blocking wait in this process to stop._  |
|  int | [**dp\_interrupt\_guard\_interrupted**](#function-dp_interrupt_guard_interrupted) (const [**dp\_interrupt\_guard\_t**](dp__interrupt__guard__core_8h.md#typedef-dp_interrupt_guard_t) \* guard) <br>_Non-zero once a stop has been requested._  |
|  uint32\_t | [**dp\_interrupt\_guard\_latency\_ms**](#function-dp_interrupt_guard_latency_ms) (const [**dp\_interrupt\_guard\_t**](dp__interrupt__guard__core_8h.md#typedef-dp_interrupt_guard_t) \* guard) <br>_The wait slice every blocking wait in this process uses._  |
|  void | [**dp\_interrupt\_guard\_resume**](#function-dp_interrupt_guard_resume) ([**dp\_interrupt\_guard\_t**](dp__interrupt__guard__core_8h.md#typedef-dp_interrupt_guard_t) \* guard) <br>_Clear the flag so waits proceed again._  |




























## Public Types Documentation




### typedef dp\_interrupt\_guard\_state\_t 

```C++
typedef dp_interrupt_guard_t dp_interrupt_guard_state_t;
```




<hr>



### typedef dp\_interrupt\_guard\_t 

_A scoped handle to the process-wide interrupt facility._ 
```C++
typedef struct dp_interrupt_guard dp_interrupt_guard_t;
```



The flag above is process-wide and stays so, so this is a handle to a facility rather than an instance of one: two guards observe the same flag. What a guard scopes is the _arming_  which signals it installed, and the latency it overrode  so that both can be undone exactly, by the code that did them, without a caller tracking it.


It exists because that bookkeeping had been living in the Python binding, which is the one place doppler does not put logic. See docs/design/io-termination.md. 


        

<hr>
## Public Functions Documentation




### function dp\_interrupt\_guard\_create 

_Clear the flag, optionally install handlers, and remember what to undo._ 
```C++
dp_interrupt_guard_t * dp_interrupt_guard_create (
    const int32_t * signals,
    size_t n_signals,
    uint32_t latency_ms
) 
```



Construction is what ARMS: on return the handlers are installed and the flag is clear. A stale flag would otherwise refuse the first wait inside the very block that just armed it.




**Parameters:**


* `signals` Signals to install on; may be NULL for none, in which case the guard is only a handle to the flag. 
* `n_signals` How many `signals` holds. 
* `latency_ms` Wait-slice override; 0 leaves the process setting alone, and only a non-zero value is restored. Fixed width rather than `unsigned`, because a public ABI should not carry a platform-dependent one. 



**Returns:**

A guard, or NULL if a handler could not be installed  in which case any already installed by this call are restored first, so a failed create arms nothing.



```C++
>>> from doppler.interrupt import Interrupt
>>> it = Interrupt([])
>>> it.interrupted()
0
```
 


        

<hr>



### function dp\_interrupt\_guard\_destroy 

_Restore every handler and latency this guard changed._ 
```C++
void dp_interrupt_guard_destroy (
    dp_interrupt_guard_t * guard
) 
```



Does NOT clear the flag: a caller that was interrupted still needs to see that it was, after the block that noticed has exited.




**Parameters:**


* `guard` Guard; NULL is a no-op. 




        

<hr>



### function dp\_interrupt\_guard\_interrupt 

_Ask every blocking wait in this process to stop._ 
```C++
void dp_interrupt_guard_interrupt (
    dp_interrupt_guard_t * guard
) 
```



The object's face onto [**dp\_interrupt()**](dp__interrupt_8h.md#function-dp_interrupt). It takes a guard because that is how a method is called, not because the request is scoped to one  the flag is process-wide, and a request through any guard is seen by every waiter.




**Parameters:**


* `guard` Guard; NULL is a no-op.


```C++
>>> from doppler.interrupt import Interrupt
>>> it = Interrupt([])
>>> it.interrupt()
>>> it.interrupted()
1
```
 


        

<hr>



### function dp\_interrupt\_guard\_interrupted 

_Non-zero once a stop has been requested._ 
```C++
int dp_interrupt_guard_interrupted (
    const dp_interrupt_guard_t * guard
) 
```





**Parameters:**


* `guard` Guard; NULL reads the flag anyway, since it is process-wide and a guard is not what holds it. 



**Returns:**

Non-zero if interrupted.



```C++
>>> from doppler.interrupt import Interrupt
>>> import numpy as np
>>> it = Interrupt(np.array([], dtype=np.int32))
>>> it.interrupted()
0
>>> it.interrupt()
>>> it.interrupted()
1
```
 


        

<hr>



### function dp\_interrupt\_guard\_latency\_ms 

_The wait slice every blocking wait in this process uses._ 
```C++
uint32_t dp_interrupt_guard_latency_ms (
    const dp_interrupt_guard_t * guard
) 
```



The readback for the constructor's `latency_ms`, and it reads the PROCESS setting rather than what this guard asked for  those differ when the guard passed 0, which means "leave it alone". A value a caller can set and not read back is a value they cannot reason about.




**Parameters:**


* `guard` Guard; NULL reads the process setting anyway. 



**Returns:**

Milliseconds.



```C++
>>> import numpy as np
>>> from doppler.interrupt import Interrupt
>>> it = Interrupt(np.array([], dtype=np.int32), latency_ms=25)
>>> it.latency_ms()
25
```
 


        

<hr>



### function dp\_interrupt\_guard\_resume 

_Clear the flag so waits proceed again._ 
```C++
void dp_interrupt_guard_resume (
    dp_interrupt_guard_t * guard
) 
```





**Parameters:**


* `guard` Guard; NULL is still honoured, for the reason above.


```C++
>>> from doppler.interrupt import Interrupt
>>> it = Interrupt([])
>>> it.interrupt()
>>> it.resume()
>>> it.interrupted()
0
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_interrupt_guard/dp_interrupt_guard_core.h`

