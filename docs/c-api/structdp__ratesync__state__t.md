

# Struct dp\_ratesync\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_ratesync\_state\_t**](structdp__ratesync__state__t.md)



_RateSync state: a matched-filter cascade and the timing loop._ [More...](#detailed-description)

* `#include <ratesync_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**acq\_origin**](#variable-acq_origin)  <br> |
|  double | [**acq\_quality**](#variable-acq_quality)  <br> |
|  int | [**acq\_run**](#variable-acq_run)  <br> |
|  size\_t | [**acq\_symbols**](#variable-acq_symbols)  <br> |
|  double | [**beta**](#variable-beta)  <br> |
|  double | [**decim**](#variable-decim)  <br> |
|  double | [**delay**](#variable-delay)  <br> |
|  [**dp\_timing\_est\_state\_t**](structdp__timing__est__state__t.md) \* | [**est**](#variable-est)  <br> |
|  uint64\_t | [**in\_count**](#variable-in_count)  <br> |
|  [**ratesync\_loop\_t**](structratesync__loop__t.md) | [**loop**](#variable-loop)  <br> |
|  [**dp\_RateConverter\_state\_t**](structdp__RateConverter__state__t.md) \* | [**mf**](#variable-mf)  <br> |
|  size\_t | [**num\_phases**](#variable-num_phases)  <br> |
|  int | [**pulse**](#variable-pulse)  <br> |
|  size\_t | [**span**](#variable-span)  <br> |
|  uint64\_t | [**term\_in**](#variable-term_in)  <br> |












































## Detailed Description


The matched filter is a heap `RateConverter` child (it owns the cascade, the banks and every delay line); the loop is embedded by value. 


    
## Public Attributes Documentation




### variable acq\_origin 

```C++
uint64_t dp_ratesync_state_t::acq_origin;
```



`in_count` at the estimator's first sample of this acquisition. 
 


        

<hr>



### variable acq\_quality 

```C++
double dp_ratesync_state_t::acq_quality;
```



an estimate at or above this seeds the loop. 


        

<hr>



### variable acq\_run 

```C++
int dp_ratesync_state_t::acq_run;
```



the estimator is on an acquisition. 
 


        

<hr>



### variable acq\_symbols 

```C++
size_t dp_ratesync_state_t::acq_symbols;
```



symbols an attempt estimates over; 0: off. 
 


        

<hr>



### variable beta 

```C++
double dp_ratesync_state_t::beta;
```



RRC roll-off. 
 


        

<hr>



### variable decim 

```C++
double dp_ratesync_state_t::decim;
```



inputs to one terminal-stage input: the whole decimation ahead of the terminal stage. 
 


        

<hr>



### variable delay 

```C++
double dp_ratesync_state_t::delay;
```



the cascade's group delay, input samples ([**dp\_RateConverter\_get\_group\_delay()**](RateConverter__core_8h.md#function-dp_rateconverter_get_group_delay)). 
 


        

<hr>



### variable est 

```C++
dp_timing_est_state_t* dp_ratesync_state_t::est;
```



the feedforward estimator, matched, owned; NULL when `sps` is under TIMING\_EST\_MATCHED\_SPS, where there is no built-in acquisition. 
 


        

<hr>



### variable in\_count 

```C++
uint64_t dp_ratesync_state_t::in_count;
```



inputs taken since create or reset. 
 


        

<hr>



### variable loop 

```C++
ratesync_loop_t dp_ratesync_state_t::loop;
```



timing loop closed around it. 
 


        

<hr>



### variable mf 

```C++
dp_RateConverter_state_t* dp_ratesync_state_t::mf;
```



cascade; terminal stage is the MF. 
 


        

<hr>



### variable num\_phases 

```C++
size_t dp_ratesync_state_t::num_phases;
```



bank arms (power of two). 
 


        

<hr>



### variable pulse 

```C++
int dp_ratesync_state_t::pulse;
```



RATESYNC\_PULSE\_IANDD / \_RRC. 
 


        

<hr>



### variable span 

```C++
size_t dp_ratesync_state_t::span;
```



one-sided RRC span, symbols. 
 


        

<hr>



### variable term\_in 

```C++
uint64_t dp_ratesync_state_t::term_in;
```



inputs the terminal stage has been handed. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ratesync/ratesync_core.h`

