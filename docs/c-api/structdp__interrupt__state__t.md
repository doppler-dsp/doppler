

# Struct dp\_interrupt\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_interrupt\_state\_t**](structdp__interrupt__state__t.md)



_Ask every blocking wait in this process to stop._ [More...](#detailed-description)

* `#include <dp_interrupt.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  volatile sig\_atomic\_t | [**flag**](#variable-flag)  <br> |
|  unsigned | [**latency\_ms**](#variable-latency_ms)  <br> |












































## Detailed Description


Assigns to a `volatile sig_atomic_t` and does nothing else, which is the only thing the C standard promises can be done from a signal handler without tearing — and being callable from a handler is the entire point of this API.


The flag is **sticky**: one handler firing may have to release several parked loops, so it stays set until [**dp\_resume()**](dp__interrupt_8h.md#function-dp_resume) clears it.



```C++
static void on_sigint (int sig) { (void)sig; dp_interrupt (); }
signal (SIGINT, on_sigint);
```



The state one process shares: the flag, and the wait slice.


Public only so a Python extension can hand its address to another via a capsule. A C caller never touches it  one archive means one copy and nothing to bind. 


    
## Public Attributes Documentation




### variable flag 

```C++
volatile sig_atomic_t dp_interrupt_state_t::flag;
```



set by a handler; read by waits 


        

<hr>



### variable latency\_ms 

```C++
unsigned dp_interrupt_state_t::latency_ms;
```



wait slice, milliseconds 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_interrupt.h`

