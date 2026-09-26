

# Struct dp\_symsync\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_symsync\_state\_t**](structdp__symsync__state__t.md)



_SymbolSync state._ [More...](#detailed-description)

* `#include <symsync_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**avgs**](#variable-avgs)  <br> |
|  uint32\_t | [**base\_inc**](#variable-base_inc)  <br> |
|  double | [**bn**](#variable-bn)  <br> |
|  [**dp\_farrow\_state\_t**](structdp__farrow__state__t.md) | [**farrow**](#variable-farrow)  <br> |
|  int | [**have\_ontime**](#variable-have_ontime)  <br> |
|  double | [**last\_error**](#variable-last_error)  <br> |
|  [**dp\_loop\_filter\_state\_t**](structdp__loop__filter__state__t.md) | [**lf**](#variable-lf)  <br> |
|  [**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) | [**lock**](#variable-lock)  <br> |
|  size\_t | [**lock\_count**](#variable-lock_count)  <br> |
|  double | [**lock\_stat**](#variable-lock_stat)  <br> |
|  double | [**lock\_sum**](#variable-lock_sum)  <br> |
|  float \_Complex | [**mid**](#variable-mid)  <br> |
|  float \_Complex | [**prev\_ontime**](#variable-prev_ontime)  <br> |
|  double | [**pwr\_avg**](#variable-pwr_avg)  <br> |
|  double | [**rate\_est**](#variable-rate_est)  <br> |
|  size\_t | [**sps**](#variable-sps)  <br> |
|  int | [**ted**](#variable-ted)  <br> |
|  [**dp\_nco\_state\_t**](structdp__nco__state__t.md) | [**timing**](#variable-timing)  <br> |
|  [**symsync\_tlm\_t**](structsymsync__tlm__t.md) | [**tlm**](#variable-tlm)  <br> |
|  double | [**zeta**](#variable-zeta)  <br> |












































## Detailed Description


Allocate with [**dp\_symsync\_create()**](symsync__core_8h.md#function-dp_symsync_create). Embeds the integer timing NCO, the Farrow interpolator and the PI loop filter by value; treat the TED history as internal. 


    
## Public Attributes Documentation




### variable avgs 

```C++
size_t dp_symsync_state_t::avgs;
```



non-coherent block size (looks/decision). 
 


        

<hr>



### variable base\_inc 

```C++
uint32_t dp_symsync_state_t::base_inc;
```



nominal NCO inc (one wrap / symbol). 
 


        

<hr>



### variable bn 

```C++
double dp_symsync_state_t::bn;
```



loop noise bandwidth (retained). 
 


        

<hr>



### variable farrow 

```C++
dp_farrow_state_t dp_symsync_state_t::farrow;
```



fractional interpolator. 
 


        

<hr>



### variable have\_ontime 

```C++
int dp_symsync_state_t::have_ontime;
```



a previous on-time sample exists. 
 


        

<hr>



### variable last\_error 

```C++
double dp_symsync_state_t::last_error;
```



last TED timing error. 
 


        

<hr>



### variable lf 

```C++
dp_loop_filter_state_t dp_symsync_state_t::lf;
```



2nd-order timing PI loop. 
 


        

<hr>



### variable lock 

```C++
dp_lockdet_state_t dp_symsync_state_t::lock;
```



decision rule: thresholds + verify counters stepped on lock\_stat each avgs-look block. 


        

<hr>



### variable lock\_count 

```C++
size_t dp_symsync_state_t::lock_count;
```



looks accumulated in the current block. 
 


        

<hr>



### variable lock\_stat 

```C++
double dp_symsync_state_t::lock_stat;
```



last block-averaged lock\_signal = mean(2\*(\|on-time\|^2-\|mid\|^2) /(\|on-time\|^2+\|mid\|^2)) over avgs looks; compare against the configured threshold (see dp\_symsync\_configure\_lock). 
 


        

<hr>



### variable lock\_sum 

```C++
double dp_symsync_state_t::lock_sum;
```



running sum of lock\_signal over the current avgs-symbol block (mirrors [**dp\_dll\_state\_t**](structdp__dll__state__t.md)'s lock\_sum/lock\_count/n\_looks pattern). 
 


        

<hr>



### variable mid 

```C++
float _Complex dp_symsync_state_t::mid;
```



mid-symbol (transition-gate) sample. 
 


        

<hr>



### variable prev\_ontime 

```C++
float _Complex dp_symsync_state_t::prev_ontime;
```



previous on-time interpolant. 
 


        

<hr>



### variable pwr\_avg 

```C++
double dp_symsync_state_t::pwr_avg;
```



running symbol power (TED normaliser). 
 


        

<hr>



### variable rate\_est 

```C++
double dp_symsync_state_t::rate_est;
```



smoothed tracked samples/symbol. 
 


        

<hr>



### variable sps 

```C++
size_t dp_symsync_state_t::sps;
```



nominal samples per symbol. 
 


        

<hr>



### variable ted 

```C++
int dp_symsync_state_t::ted;
```



SYMSYNC\_TED\_GARDNER / \_DTTL. 
 


        

<hr>



### variable timing 

```C++
dp_nco_state_t dp_symsync_state_t::timing;
```



integer timing NCO (phase/phase\_inc). 
 


        

<hr>



### variable tlm 

```C++
symsync_tlm_t dp_symsync_state_t::tlm;
```



live telemetry attachment; zeroed in blobs 
 


        

<hr>



### variable zeta 

```C++
double dp_symsync_state_t::zeta;
```



damping factor (retained). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/symsync/symsync_core.h`

