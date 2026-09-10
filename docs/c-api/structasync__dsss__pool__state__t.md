

# Struct async\_dsss\_pool\_state\_t



[**ClassList**](annotated.md) **>** [**async\_dsss\_pool\_state\_t**](structasync__dsss__pool__state__t.md)



_AsyncDsssPool state._ [More...](#detailed-description)

* `#include <async_dsss_pool_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**acq\_state\_t**](structacq__state__t.md) \* | [**acq**](#variable-acq)  <br> |
|  double | [**carrier\_freq\_hz**](#variable-carrier_freq_hz)  <br> |
|  int | [**cell**](#variable-cell)  <br> |
|  double | [**chip\_rate**](#variable-chip_rate)  <br> |
|  uint8\_t \* | [**code**](#variable-code)  <br> |
|  size\_t | [**code\_len**](#variable-code_len)  <br> |
|  uint64\_t | [**dropped**](#variable-dropped)  <br> |
|  uint64\_t | [**events**](#variable-events)  <br> |
|  size\_t | [**feed\_n**](#variable-feed_n)  <br> |
|  const float \_Complex \* | [**feed\_x**](#variable-feed_x)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  [**acq\_result\_t**](structacq__result__t.md) \* | [**hits**](#variable-hits)  <br> |
|  [**dp\_event\_log\_t**](dp__event__log__core_8h.md#typedef-dp_event_log_t) \* | [**log**](#variable-log)  <br> |
|  uint64\_t | [**max\_on\_samples**](#variable-max_on_samples)  <br> |
|  size\_t | [**max\_peaks**](#variable-max_peaks)  <br> |
|  size\_t | [**n\_assigned**](#variable-n_assigned)  <br> |
|  size\_t | [**n\_slots**](#variable-n_slots)  <br> |
|  size\_t \* | [**n\_sym**](#variable-n_sym)  <br> |
|  [**dp\_pool\_t**](structdp__pool__t.md) \* | [**pool**](#variable-pool)  <br> |
|  [**async\_dsss\_pool\_row\_t**](structasync__dsss__pool__row__t.md) \* | [**rows**](#variable-rows)  <br> |
|  [**async\_dsss\_receiver\_state\_t**](structasync__dsss__receiver__state__t.md) \*\* | [**rx**](#variable-rx)  <br> |
|  uint64\_t | [**samples\_consumed**](#variable-samples_consumed)  <br> |
|  size\_t | [**spc**](#variable-spc)  <br> |
|  float \_Complex \* | [**sym\_buf**](#variable-sym_buf)  <br> |
|  size\_t | [**sym\_cap**](#variable-sym_cap)  <br> |
|  double | [**symbol\_rate**](#variable-symbol_rate)  <br> |
|  int | [**threads**](#variable-threads)  <br> |












































## Detailed Description


Allocate with [**async\_dsss\_pool\_create()**](async__dsss__pool__core_8h.md#function-async_dsss_pool_create). 


    
## Public Attributes Documentation




### variable acq 

```C++
acq_state_t* async_dsss_pool_state_t::acq;
```




<hr>



### variable carrier\_freq\_hz 

```C++
double async_dsss_pool_state_t::carrier_freq_hz;
```




<hr>



### variable cell 

```C++
int async_dsss_pool_state_t::cell;
```



1: the receivers are the cell mode's ([**async\_dsss\_pool\_create\_cell()**](async__dsss__pool__core_8h.md#function-async_dsss_pool_create_cell)); 0: the hand-off flavour's. Keys the blob. 
 


        

<hr>



### variable chip\_rate 

```C++
double async_dsss_pool_state_t::chip_rate;
```




<hr>



### variable code 

```C++
uint8_t* async_dsss_pool_state_t::code;
```




<hr>



### variable code\_len 

```C++
size_t async_dsss_pool_state_t::code_len;
```




<hr>



### variable dropped 

```C++
uint64_t async_dsss_pool_state_t::dropped;
```



Detections dropped for want of a free slot. 
 


        

<hr>



### variable events 

```C++
uint64_t async_dsss_pool_state_t::events;
```



Transitions since create/reset, logged or not. 


        

<hr>



### variable feed\_n 

```C++
size_t async_dsss_pool_state_t::feed_n;
```




<hr>



### variable feed\_x 

```C++
const float _Complex* async_dsss_pool_state_t::feed_x;
```




<hr>



### variable fs 

```C++
double async_dsss_pool_state_t::fs;
```




<hr>



### variable hits 

```C++
acq_result_t* async_dsss_pool_state_t::hits;
```



max\_peaks, the searcher's list 
 


        

<hr>



### variable log 

```C++
dp_event_log_t* async_dsss_pool_state_t::log;
```



borrowed; NULL = none attached 


        

<hr>



### variable max\_on\_samples 

```C++
uint64_t async_dsss_pool_state_t::max_on_samples;
```



max\_emitter\_on\_time\_secs in samples; 0 = never released for time. 
 


        

<hr>



### variable max\_peaks 

```C++
size_t async_dsss_pool_state_t::max_peaks;
```




<hr>



### variable n\_assigned 

```C++
size_t async_dsss_pool_state_t::n_assigned;
```



Slots assigned right now. 
 


        

<hr>



### variable n\_slots 

```C++
size_t async_dsss_pool_state_t::n_slots;
```



Receivers held; never exceeded. 
 


        

<hr>



### variable n\_sym 

```C++
size_t* async_dsss_pool_state_t::n_sym;
```



n\_slots: the last push's count 
 


        

<hr>



### variable pool 

```C++
dp_pool_t* async_dsss_pool_state_t::pool;
```



the receivers' threads 
 


        

<hr>



### variable rows 

```C++
async_dsss_pool_row_t* async_dsss_pool_state_t::rows;
```



n\_slots 
 


        

<hr>



### variable rx 

```C++
async_dsss_receiver_state_t** async_dsss_pool_state_t::rx;
```



n\_slots, created idle. 
 


        

<hr>



### variable samples\_consumed 

```C++
uint64_t async_dsss_pool_state_t::samples_consumed;
```



Input samples pushed: the stream position every event is stamped at. 
 


        

<hr>



### variable spc 

```C++
size_t async_dsss_pool_state_t::spc;
```




<hr>



### variable sym\_buf 

```C++
float _Complex* async_dsss_pool_state_t::sym_buf;
```



n\_slots \* sym\_cap, grown on demand 


        

<hr>



### variable sym\_cap 

```C++
size_t async_dsss_pool_state_t::sym_cap;
```



per slot, symbols 
 


        

<hr>



### variable symbol\_rate 

```C++
double async_dsss_pool_state_t::symbol_rate;
```




<hr>



### variable threads 

```C++
int async_dsss_pool_state_t::threads;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/async_dsss_pool/async_dsss_pool_core.h`

