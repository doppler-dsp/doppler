

# File dp\_interrupt.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_interrupt.h**](dp__interrupt_8h.md)

[Go to the source code of this file](dp__interrupt_8h_source.md)

_Asking a blocking wait to stop, whatever it is waiting on._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include <stddef.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_interrupt\_guard | [**dp\_interrupt\_guard\_t**](#typedef-dp_interrupt_guard_t)  <br>_A scoped handle to the process-wide interrupt facility._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_interrupt**](#function-dp_interrupt) (void) <br>_Ask every blocking wait in this process to stop._  |
|  [**dp\_interrupt\_guard\_t**](dp__interrupt_8h.md#typedef-dp_interrupt_guard_t) \* | [**dp\_interrupt\_guard\_create**](#function-dp_interrupt_guard_create) (const int \* signals, size\_t n\_signals, unsigned latency\_ms) <br>_Clear the flag, optionally install handlers, and remember what to undo._  |
|  void | [**dp\_interrupt\_guard\_destroy**](#function-dp_interrupt_guard_destroy) ([**dp\_interrupt\_guard\_t**](dp__interrupt_8h.md#typedef-dp_interrupt_guard_t) \* guard) <br>_Restore every handler and latency this guard changed._  |
|  void | [**dp\_interrupt\_guard\_interrupt**](#function-dp_interrupt_guard_interrupt) ([**dp\_interrupt\_guard\_t**](dp__interrupt_8h.md#typedef-dp_interrupt_guard_t) \* guard) <br>_Ask every blocking wait in this process to stop._  |
|  int | [**dp\_interrupt\_guard\_interrupted**](#function-dp_interrupt_guard_interrupted) (const [**dp\_interrupt\_guard\_t**](dp__interrupt_8h.md#typedef-dp_interrupt_guard_t) \* guard) <br>_Non-zero once a stop has been requested._  |
|  void | [**dp\_interrupt\_guard\_resume**](#function-dp_interrupt_guard_resume) ([**dp\_interrupt\_guard\_t**](dp__interrupt_8h.md#typedef-dp_interrupt_guard_t) \* guard) <br>_Clear the flag so waits proceed again._  |
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


    
## Public Types Documentation




### typedef dp\_interrupt\_guard\_t 

_A scoped handle to the process-wide interrupt facility._ 
```C++
typedef struct dp_interrupt_guard dp_interrupt_guard_t;
```



The flag above is process-wide and stays so, so this is a handle to a facility rather than an instance of one: two guards observe the same flag. What a guard scopes is the _arming_  which signals it installed, and the latency it overrode  so that both can be undone exactly, by the code that did them, without a caller tracking it.


It exists because that bookkeeping had been living in the Python binding, which is the one place doppler does not put logic. See docs/design/io-termination.md. 


        

<hr>
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



### function dp\_interrupt\_guard\_create 

_Clear the flag, optionally install handlers, and remember what to undo._ 
```C++
dp_interrupt_guard_t * dp_interrupt_guard_create (
    const int * signals,
    size_t n_signals,
    unsigned latency_ms
) 
```



Construction is what ARMS: on return the handlers are installed and the flag is clear. A stale flag would otherwise refuse the first wait inside the very block that just armed it.




**Parameters:**


* `signals` Signals to install on; may be NULL for none, in which case the guard is only a handle to the flag. 
* `n_signals` How many `signals` holds. 
* `latency_ms` Wait-slice override; 0 leaves the process setting alone, and only a non-zero value is restored. 



**Returns:**

A guard, or NULL if a handler could not be installed  in which case any already installed by this call are restored first, so a failed create arms nothing.



```C++
>>> from doppler.interrupt import Interrupt
>>> it = Interrupt()
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
>>> it = Interrupt()
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
>>> it = Interrupt()
>>> it.interrupt()
>>> it.resume()
>>> it.interrupted()
0
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

