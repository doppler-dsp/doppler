

# Group interrupt



[**Modules**](modules.md) **>** [**interrupt**](group__interrupt.md)



[More...](#detailed-description)






































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_stream\_interrupt**](#function-dp_stream_interrupt) (void) <br>_Ask every blocking receive in this process to return now._  |
|  unsigned | [**dp\_stream\_interrupt\_latency\_ms**](#function-dp_stream_interrupt_latency_ms) (void) <br>_The interrupt latency in force._  |
|  int | [**dp\_stream\_interrupt\_on\_signal**](#function-dp_stream_interrupt_on_signal) (int sig) <br>_Install a handler for_ `sig` _that calls_[_**dp\_stream\_interrupt()**_](group__interrupt.md#function-dp_stream_interrupt) _._ |
|  int | [**dp\_stream\_interrupted**](#function-dp_stream_interrupted) (void) <br>_Non-zero when an interrupt is pending._  |
|  int | [**dp\_stream\_restore\_signal**](#function-dp_stream_restore_signal) (int sig) <br>_Restore the handler that was in place before._  |
|  void | [**dp\_stream\_resume**](#function-dp_stream_resume) (void) <br>_Clear the interrupt, so blocking receives block again._  |
|  void | [**dp\_stream\_set\_interrupt\_latency\_ms**](#function-dp_stream_set_interrupt_latency_ms) (unsigned ms) <br>_Default interrupt latency, in milliseconds._  |




























## Detailed Description




**Deprecated**

These are the `dp_stream_*` spellings of a primitive that is not specific to streaming. It moved to `dp_interrupt.h` in the core library so a build with no NATS can use it; use `dp_interrupt()`, `dp_interrupted()`, `dp_resume()`, `dp_interrupt_on_signal()` and `dp_restore_signal()` instead. These forward verbatim and are removed once their callers migrate. See `docs/design/io-termination.md`.




A blocking `*_recv` waits inside the NATS client, and a flag your signal handler sets is read by your loop — which the blocking call is keeping you out of. With traffic arriving that is invisible, because every frame returns control to you; the moment a sender stops, Ctrl+C stops working. That is not hypothetical: it shipped, in doppler's own C receiver example.


A bounded `*_set_timeout` is one answer, and the examples relied on it, but it makes every caller trade latency against responsiveness and get it wrong quietly. This is the other: the library checks a flag of its own inside the wait, so a blocking receive stays blocking and still returns when you ask it to. 


    
## Public Functions Documentation




### function dp\_stream\_interrupt 

_Ask every blocking receive in this process to return now._ 
```
void dp_stream_interrupt (
    void
) 
```



Async-signal-safe by construction — it assigns to a `volatile sig_atomic_t` and does nothing else — so the intended caller is a signal handler:



```
static void on_sigint (int sig)
{
  (void)sig;
  dp_stream_interrupt ();
}
```



Every receive already blocked returns [**DP\_ERR\_INTERRUPTED**](clib__common_8h.md#define-dp_err_interrupted) within one internal wait slice (100 ms), and so does every one STARTED while the flag is set — a receive cannot be missed by racing the signal. The flag is process-wide and sticky; [**dp\_stream\_resume()**](group__interrupt.md#function-dp_stream_resume) clears it. 


        

<hr>



### function dp\_stream\_interrupt\_latency\_ms 

_The interrupt latency in force._ 
```
unsigned dp_stream_interrupt_latency_ms (
    void
) 
```





**Returns:**

Milliseconds. 





        

<hr>



### function dp\_stream\_interrupt\_on\_signal 

_Install a handler for_ `sig` _that calls_[_**dp\_stream\_interrupt()**_](group__interrupt.md#function-dp_stream_interrupt) _._
```
int dp_stream_interrupt_on_signal (
    int sig
) 
```



The handler is installed in C, and that is the whole point rather than a convenience. A handler written in a higher-level language runs when its interpreter next regains control, which is precisely what a blocking receive is preventing  the flag would be set only after the wait it is meant to end. Measured, not reasoned: a Python `signal.signal` handler calling the interrupt left a blocked `recv()` blocked forever.


Whatever handler was installed is **chained, not replaced**: it runs immediately after the flag is set. Without that, a signal arriving while the program is not inside a receive would set a flag nobody reads and otherwise do nothing  fixing the blocking case by breaking the ordinary one. For an embedding interpreter this is what keeps its own Ctrl+C behaviour intact.




**Parameters:**


* `sig` Signal number, e.g. `SIGINT`. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID for a signal that cannot be caught. 





        

<hr>



### function dp\_stream\_interrupted 

_Non-zero when an interrupt is pending._ 
```
int dp_stream_interrupted (
    void
) 
```



For a loop that wants to notice without calling recv again, and for a caller that keeps its own flag and wants one source of truth.




**Returns:**

Non-zero when interrupted, 0 otherwise. 





        

<hr>



### function dp\_stream\_restore\_signal 

_Restore the handler that was in place before._ 
```
int dp_stream_restore_signal (
    int sig
) 
```





**Parameters:**


* `sig` Signal number previously passed to [**dp\_stream\_interrupt\_on\_signal()**](group__interrupt.md#function-dp_stream_interrupt_on_signal). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if that signal was never installed. 





        

<hr>



### function dp\_stream\_resume 

_Clear the interrupt, so blocking receives block again._ 
```
void dp_stream_resume (
    void
) 
```



The flag is sticky on purpose: a handler fires once and the loops it unblocks may be several, so an auto-clearing flag would release one caller and leave the rest parked. 


        

<hr>



### function dp\_stream\_set\_interrupt\_latency\_ms 

_Default interrupt latency, in milliseconds._ 
```
void dp_stream_set_interrupt_latency_ms (
    unsigned ms
) 
```



Ten wakeups a second on an idle receiver, and a delay no human perceives when they press Ctrl+C. It is a default rather than a constant of the design: see [**dp\_stream\_set\_interrupt\_latency\_ms()**](group__interrupt.md#function-dp_stream_set_interrupt_latency_ms).


How soon a blocking receive must notice an interrupt.


The library cannot be woken from the NATS client's wait, so it waits in slices and checks the flag between them. This is the size of that slice, expressed as the thing a caller actually cares about — the worst-case delay between [**dp\_stream\_interrupt()**](group__interrupt.md#function-dp_stream_interrupt) and the receive returning — rather than as an implementation detail.


It is a knob because the right answer is not the library's to know. A human pressing Ctrl+C cannot perceive 100 ms; a control loop that must hand back within one symbol period can, and a battery-powered sensor would rather wake once a second than ten times. The cost is one wakeup per slice on an otherwise idle receiver.


Process-wide, like the flag it serves. Takes effect on the next wait slice, so a receive already blocked adopts it within one old slice.




**Parameters:**


* `ms` Milliseconds; 0 selects [**DP\_INTERRUPT\_LATENCY\_DEFAULT\_MS**](dp__interrupt_8h.md#define-dp_interrupt_latency_default_ms). 




        

<hr>

------------------------------


