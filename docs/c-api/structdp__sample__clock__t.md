

# Struct dp\_sample\_clock\_t



[**ClassList**](annotated.md) **>** [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md)



[More...](#detailed-description)

* `#include <timing_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**epoch\_mono\_ns**](#variable-epoch_mono_ns)  <br> |
|  uint64\_t | [**epoch\_real\_ns**](#variable-epoch_real_ns)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  int | [**has\_anchor**](#variable-has_anchor)  <br> |
|  uint64\_t | [**max\_late\_ns**](#variable-max_late_ns)  <br> |
|  uint64\_t | [**n**](#variable-n)  <br> |
|  int | [**resync**](#variable-resync)  <br> |
|  uint64\_t | [**underruns**](#variable-underruns)  <br> |












































## Detailed Description


Sample-clock state. Treat the fields as read-only from the outside; mutate only through the functions below. 


    
## Public Attributes Documentation




### variable epoch\_mono\_ns 

```C++
uint64_t dp_sample_clock_t::epoch_mono_ns;
```



CLOCK\_MONOTONIC baseline for pacing. 


        

<hr>



### variable epoch\_real\_ns 

```C++
uint64_t dp_sample_clock_t::epoch_real_ns;
```



CLOCK\_REALTIME baseline for stamping. 


        

<hr>



### variable fs 

```C++
double dp_sample_clock_t::fs;
```



sample rate (Hz). 


        

<hr>



### variable has\_anchor 

```C++
int dp_sample_clock_t::has_anchor;
```



nonzero once track() has adopted a real observed (timestamp, n) pair; init()/reset() start at 0  the receive-side counterpart of the TX-side epoch capture, so the first track() call always adopts rather than tolerance-gating against an arbitrary construction-time guess. 


        

<hr>



### variable max\_late\_ns 

```C++
uint64_t dp_sample_clock_t::max_late_ns;
```



worst lateness observed (ns). 


        

<hr>



### variable n 

```C++
uint64_t dp_sample_clock_t::n;
```



cumulative samples advanced. 


        

<hr>



### variable resync 

```C++
int dp_sample_clock_t::resync;
```



nonzero: pace() re-anchors on underrun. 


        

<hr>



### variable underruns 

```C++
uint64_t dp_sample_clock_t::underruns;
```



pace() calls that arrived past deadline. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/timing/timing_core.h`

