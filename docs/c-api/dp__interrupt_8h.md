

# File dp\_interrupt.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_interrupt.h**](dp__interrupt_8h.md)

[Go to the source code of this file](dp__interrupt_8h_source.md)

_Asking a blocking wait to stop, whatever it is waiting on._ [More...](#detailed-description)

* `#include "clib_common.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_interrupt**](#function-dp_interrupt) (void) <br>_Ask every blocking wait in this process to stop._  |
|  unsigned | [**dp\_interrupt\_latency\_ms**](#function-dp_interrupt_latency_ms) (void) <br>_The interrupt latency in force._  |
|  int | [**dp\_interrupt\_on\_signal**](#function-dp_interrupt_on_signal) (int sig) <br>_Install a handler for_ `sig` _that calls_[_**dp\_interrupt()**_](dp__interrupt_8h.md#function-dp_interrupt) _._ |
|  int | [**dp\_interrupted**](#function-dp_interrupted) (void) <br>_Non-zero when an interrupt is pending._  |
|  int | [**dp\_restore\_signal**](#function-dp_restore_signal) (int sig) <br>_Put back whatever handler_ [_**dp\_interrupt\_on\_signal()**_](dp__interrupt_8h.md#function-dp_interrupt_on_signal) _displaced._ |
|  void | [**dp\_resume**](#function-dp_resume) (void) <br>_Clear the interrupt, so blocking waits block again._  |
|  void | [**dp\_set\_interrupt\_latency\_ms**](#function-dp_set_interrupt_latency_ms) (unsigned ms) <br>_How soon a blocking wait must notice an interrupt._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_INTERRUPT\_LATENCY\_DEFAULT\_MS**](dp__interrupt_8h.md#define-dp_interrupt_latency_default_ms)  `100u`<br>_Default interrupt latency, in milliseconds._  |

## Detailed Description


doppler moves samples over three transports — a NATS subject, a double-mapped ring, and a capture file — and every one of them has a consumer-side wait that a caller may need to abandon. This is the one flag all three consult, so that Ctrl+C means the same thing wherever the samples are coming from.


It lives in the core library rather than in the optional stream component because two of its three callers are core: a file writer and a ring buffer are available in a build with no NATS at all. It was in `native/src/stream/stream_core.c` until it acquired that second caller, which is also when it turned out to have no NATS dependency to begin with — a `volatile sig_atomic_t` and four accessors over libc.


See `docs/design/io-termination.md` for the contract this is one third of; the other two are end-of-stream and durable completion. 


    
## Public Functions Documentation




### function dp\_interrupt 

_Ask every blocking wait in this process to stop._ 
```C++
void dp_interrupt (
    void
) 
```



Assigns to a `volatile sig_atomic_t` and does nothing else, which is the only thing the C standard promises can be done from a signal handler without tearing — and being callable from a handler is the entire point of this API.


The flag is **sticky**: one handler firing may have to release several parked loops, so it stays set until [**dp\_resume()**](dp__interrupt_8h.md#function-dp_resume) clears it.



```C++
static void on_sigint (int sig) { (void)sig; dp_interrupt (); }
signal (SIGINT, on_sigint);
```
 


        

<hr>



### function dp\_interrupt\_latency\_ms 

_The interrupt latency in force._ 
```C++
unsigned dp_interrupt_latency_ms (
    void
) 
```





**Returns:**

Milliseconds. 





        

<hr>



### function dp\_interrupt\_on\_signal 

_Install a handler for_ `sig` _that calls_[_**dp\_interrupt()**_](dp__interrupt_8h.md#function-dp_interrupt) _._
```C++
int dp_interrupt_on_signal (
    int sig
) 
```



Uses `sigaction` and **chains** to whatever handler was already installed, so adding this to a program does not silently disable the one it had.


Install it EARLY — before opening transports, not after. A signal arriving before this call is not ignored, it terminates the process, and that window is real: measured at ~5 ms for a dynamically linked binary, which is long enough for a supervisor's stop signal to land inside it.




**Parameters:**


* `sig` Signal number, e.g. `SIGINT`. 



**Returns:**

DP\_OK, or [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) if the handler could not be installed or all handler slots are in use. 





        

<hr>



### function dp\_interrupted 

_Non-zero when an interrupt is pending._ 
```C++
int dp_interrupted (
    void
) 
```



The check a hand-written loop makes between blocks. A wait that cannot be sliced — a busy-spin over a ring, a read of a file still being appended to — polls this and gives up when it is set.




**Returns:**

Non-zero when interrupted, 0 otherwise. 





        

<hr>



### function dp\_restore\_signal 

_Put back whatever handler_ [_**dp\_interrupt\_on\_signal()**_](dp__interrupt_8h.md#function-dp_interrupt_on_signal) _displaced._
```C++
int dp_restore_signal (
    int sig
) 
```





**Parameters:**


* `sig` Signal number previously passed to [**dp\_interrupt\_on\_signal()**](dp__interrupt_8h.md#function-dp_interrupt_on_signal). 



**Returns:**

DP\_OK, or [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid) if `sig` was never installed. 





        

<hr>



### function dp\_resume 

_Clear the interrupt, so blocking waits block again._ 
```C++
void dp_resume (
    void
) 
```




<hr>



### function dp\_set\_interrupt\_latency\_ms 

_How soon a blocking wait must notice an interrupt._ 
```C++
void dp_set_interrupt_latency_ms (
    unsigned ms
) 
```



A wait that cannot be woken is taken in slices, with the flag checked between them. This is the size of that slice, expressed as the thing a caller actually cares about — the worst-case delay between [**dp\_interrupt()**](dp__interrupt_8h.md#function-dp_interrupt) and the wait returning — rather than as an implementation detail.


It is a knob because the right answer is not the library's to know. A human pressing Ctrl+C cannot perceive 100 ms; a control loop that must hand back within one symbol period can, and a battery-powered sensor would rather wake once a second than ten times. The cost is one wakeup per slice on an otherwise idle waiter.


Process-wide, like the flag it serves. Takes effect on the next slice, so a wait already blocked adopts it within one old slice.




**Parameters:**


* `ms` Milliseconds; 0 selects [**DP\_INTERRUPT\_LATENCY\_DEFAULT\_MS**](dp__interrupt_8h.md#define-dp_interrupt_latency_default_ms). 




        

<hr>
## Macro Definition Documentation





### define DP\_INTERRUPT\_LATENCY\_DEFAULT\_MS 

_Default interrupt latency, in milliseconds._ 
```C++
#define DP_INTERRUPT_LATENCY_DEFAULT_MS `100u`
```



Ten wakeups a second on an idle waiter, and a delay no human perceives when they press Ctrl+C. It is a default rather than a constant of the design: see [**dp\_set\_interrupt\_latency\_ms()**](dp__interrupt_8h.md#function-dp_set_interrupt_latency_ms). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_interrupt.h`

