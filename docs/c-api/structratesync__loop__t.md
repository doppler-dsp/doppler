

# Struct ratesync\_loop\_t



[**ClassList**](annotated.md) **>** [**ratesync\_loop\_t**](structratesync__loop__t.md)



_The symbol-timing loop, independent of what feeds it._ [More...](#detailed-description)

* `#include <ratesync_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**avgs**](#variable-avgs)  <br> |
|  double | [**bn**](#variable-bn)  <br> |
|  double | [**ctrl**](#variable-ctrl)  <br> |
|  int | [**have\_prev**](#variable-have_prev)  <br> |
|  double | [**last\_error**](#variable-last_error)  <br> |
|  [**loop\_filter\_state\_t**](structloop__filter__state__t.md) | [**lf**](#variable-lf)  <br> |
|  [**lockdet\_state\_t**](structlockdet__state__t.md) | [**lock**](#variable-lock)  <br> |
|  size\_t | [**lock\_count**](#variable-lock_count)  <br> |
|  double | [**lock\_stat**](#variable-lock_stat)  <br> |
|  double | [**lock\_sum**](#variable-lock_sum)  <br> |
|  size\_t | [**m**](#variable-m)  <br> |
|  size\_t | [**out\_count**](#variable-out_count)  <br> |
|  float complex | [**prev\_on**](#variable-prev_on)  <br> |
|  size\_t | [**prime\_left**](#variable-prime_left)  <br> |
|  size\_t | [**prime\_taps**](#variable-prime_taps)  <br> |
|  double | [**pwr\_avg**](#variable-pwr_avg)  <br> |
|  int | [**pwr\_seeded**](#variable-pwr_seeded)  <br> |
|  double | [**rate\_est**](#variable-rate_est)  <br> |
|  float complex | [**ring**](#variable-ring)  <br> |
|  size\_t | [**ring\_n**](#variable-ring_n)  <br> |
|  double | [**sps**](#variable-sps)  <br> |
|  int | [**ted**](#variable-ted)  <br> |
|  const [**resamp\_state\_t**](structresamp__state__t.md) \* | [**term**](#variable-term)  <br> |
|  double | [**term\_rate**](#variable-term_rate)  <br> |
|  [**ratesync\_tlm\_t**](structratesync__tlm__t.md) | [**tlm**](#variable-tlm)  <br> |
|  double | [**zeta**](#variable-zeta)  <br> |












































## Detailed Description


Everything RateSync does _after_ the cascade emits an output: the strobe ring, the TED, the PI loop, the lock detector and the telemetry. It holds no filter and no cascade — it consumes a stream of terminal-stage outputs and produces a per-input rate deviation (`ctrl`) for whoever owns the accumulator those outputs came from.


That split is what lets a receiver reuse this loop verbatim. RateSync owns a `RateConverter` and steers it directly; MpskReceiver owns a `Ddc`/`Ddcr` (mix + the same cascade) and steers the _same_ accumulator through the DDC's `rate_ctrl` port. Both drive one implementation of the timing loop, so a fix to the TED or the normaliser reaches both — the two are not peers that can drift apart.


The loop must be told the geometry of the accumulator it is steering ([**ratesync\_loop\_set\_cascade()**](ratesync__core_8h.md#function-ratesync_loop_set_cascade)): the terminal stage's own rate, because that is the scale `ctrl` is referenced to, and the terminal bank's tap count, because that is how many outputs are delay-line fill rather than signal. 


    
## Public Attributes Documentation




### variable avgs 

```C++
size_t ratesync_loop_t::avgs;
```



non-coherent block size (looks/decision). 
 


        

<hr>



### variable bn 

```C++
double ratesync_loop_t::bn;
```



loop noise bandwidth (retained). 
 


        

<hr>



### variable ctrl 

```C++
double ratesync_loop_t::ctrl;
```



per-input rate deviation now applied. 
 


        

<hr>



### variable have\_prev 

```C++
int ratesync_loop_t::have_prev;
```



a previous on-time strobe exists. 
 


        

<hr>



### variable last\_error 

```C++
double ratesync_loop_t::last_error;
```



last normalised TED error. 
 


        

<hr>



### variable lf 

```C++
loop_filter_state_t ratesync_loop_t::lf;
```



2nd-order timing PI loop. 
 


        

<hr>



### variable lock 

```C++
lockdet_state_t ratesync_loop_t::lock;
```



declare/drop rule stepped on lock\_stat. 
 


        

<hr>



### variable lock\_count 

```C++
size_t ratesync_loop_t::lock_count;
```



looks accumulated in the current block. 
 


        

<hr>



### variable lock\_stat 

```C++
double ratesync_loop_t::lock_stat;
```



last block-averaged lock\_signal. 
 


        

<hr>



### variable lock\_sum 

```C++
double ratesync_loop_t::lock_sum;
```



running sum over the current avgs block. 
 


        

<hr>



### variable m 

```C++
size_t ratesync_loop_t::m;
```



terminal outputs per symbol (&gt;= 2, even). 
 


        

<hr>



### variable out\_count 

```C++
size_t ratesync_loop_t::out_count;
```



terminal outputs seen (mod m: strobe phase). 
 


        

<hr>



### variable prev\_on 

```C++
float complex ratesync_loop_t::prev_on;
```



previous on-time strobe. 
 


        

<hr>



### variable prime\_left 

```C++
size_t ratesync_loop_t::prime_left;
```



strobes still to discard (cascade filling). 
 


        

<hr>



### variable prime\_taps 

```C++
size_t ratesync_loop_t::prime_taps;
```



terminal bank taps; sets the prime length. 
 


        

<hr>



### variable pwr\_avg 

```C++
double ratesync_loop_t::pwr_avg;
```



running \|on\|^2+\|mid\|^2 (the TED normaliser). 
 


        

<hr>



### variable pwr\_seeded 

```C++
int ratesync_loop_t::pwr_seeded;
```



pwr\_avg has taken its first value. 
 


        

<hr>



### variable rate\_est 

```C++
double ratesync_loop_t::rate_est;
```



smoothed tracked samples/symbol. 
 


        

<hr>



### variable ring 

```C++
float complex ratesync_loop_t::ring[RATESYNC_MAX_M/2+1];
```



Newest-first ring of the last m/2+1 outputs: the transition gate is m/2 outputs behind the on-time strobe, so it is simply the element at index m/2. (Written without brackets on purpose: mkdoxy renders this comment into markdown, where a bare `name[i]` parses as a link reference and fails the strict docs build.) 


        

<hr>



### variable ring\_n 

```C++
size_t ratesync_loop_t::ring_n;
```




<hr>



### variable sps 

```C++
double ratesync_loop_t::sps;
```



nominal samples per symbol (any double). 
 


        

<hr>



### variable ted 

```C++
int ratesync_loop_t::ted;
```



RATESYNC\_TED\_GARDNER / \_DTTL. 
 


        

<hr>



### variable term 

```C++
const resamp_state_t* ratesync_loop_t::term;
```



The terminal stage itself, borrowed for TELEMETRY ONLY: the loop steers this accumulator but does not own it, and `mu` — the sampling phase the steering produces — is otherwise unobservable from outside the cascade. NULL when the owner bound the geometry by hand ([**ratesync\_loop\_set\_cascade()**](ratesync__core_8h.md#function-ratesync_loop_set_cascade)) rather than from a cascade; the probe then reports 0. Never dereferenced on the hot path. 


        

<hr>



### variable term\_rate 

```C++
double ratesync_loop_t::term_rate;
```



terminal stage's own rate; the ctrl scale. 
 


        

<hr>



### variable tlm 

```C++
ratesync_tlm_t ratesync_loop_t::tlm;
```



live telemetry attachment; zeroed in blobs. 
 


        

<hr>



### variable zeta 

```C++
double ratesync_loop_t::zeta;
```



damping factor (retained). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ratesync/ratesync_core.h`

