

# Struct dp\_async\_dsss\_pool\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md)



_AsyncDsssPool state._ [More...](#detailed-description)

* `#include <async_dsss_pool_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double \* | [**car\_hist**](#variable-car_hist)  <br> |
|  double | [**carrier\_hz**](#variable-carrier_hz)  <br> |
|  float \_Complex \* | [**carry**](#variable-carry)  <br> |
|  size\_t | [**carry\_len**](#variable-carry_len)  <br> |
|  uint8\_t \* | [**cl\_hist**](#variable-cl_hist)  <br> |
|  [**resamp\_state\_t**](structresamp__state__t.md) \* | [**fe**](#variable-fe)  <br> |
|  float \_Complex \* | [**fe\_buf**](#variable-fe_buf)  <br> |
|  size\_t | [**fe\_cap**](#variable-fe_cap)  <br> |
|  uint64\_t | [**fe\_out**](#variable-fe_out)  <br> |
|  double | [**fe\_ratio**](#variable-fe_ratio)  <br> |
|  uint64\_t | [**fed\_in**](#variable-fed_in)  <br> |
|  size\_t | [**feed\_n**](#variable-feed_n)  <br> |
|  const float \_Complex \* | [**feed\_x**](#variable-feed_x)  <br> |
|  size\_t | [**ghost\_window**](#variable-ghost_window)  <br> |
|  size\_t | [**group**](#variable-group)  <br> |
|  size\_t \* | [**hist\_len**](#variable-hist_len)  <br> |
|  size\_t | [**hist\_pos**](#variable-hist_pos)  <br> |
|  double | [**in\_spc**](#variable-in_spc)  <br> |
|  double | [**lattice\_hz**](#variable-lattice_hz)  <br> |
|  size\_t | [**n\_receivers**](#variable-n_receivers)  <br> |
|  size\_t \* | [**n\_sym**](#variable-n_sym)  <br> |
|  size\_t | [**period**](#variable-period)  <br> |
|  uint64\_t | [**released\_duplicate**](#variable-released_duplicate)  <br> |
|  uint64\_t | [**released\_ghost**](#variable-released_ghost)  <br> |
|  uint64\_t | [**released\_lost**](#variable-released_lost)  <br> |
|  [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \*\* | [**rx**](#variable-rx)  <br> |
|  uint64\_t | [**samples**](#variable-samples)  <br> |
|  [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \* | [**search**](#variable-search)  <br> |
|  uint64\_t | [**search\_periods**](#variable-search_periods)  <br> |
|  bool | [**searching**](#variable-searching)  <br> |
|  float \_Complex \* | [**sym\_buf**](#variable-sym_buf)  <br> |
|  size\_t | [**sym\_cap**](#variable-sym_cap)  <br> |
|  size\_t | [**sym\_extra**](#variable-sym_extra)  <br> |
|  int | [**threads**](#variable-threads)  <br> |
|  [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* | [**tlm**](#variable-tlm)  <br> |
|  uint32\_t | [**tlm\_decim**](#variable-tlm_decim)  <br> |
|  char | [**tlm\_prefix**](#variable-tlm_prefix)  <br> |
|  [**dp\_pool\_t**](structdp__pool__t.md) \* | [**workers**](#variable-workers)  <br> |












































## Detailed Description


Allocate with [**dp\_async\_dsss\_pool\_create()**](async__dsss__pool__core_8h.md#function-dp_async_dsss_pool_create). 


    
## Public Attributes Documentation




### variable car\_hist 

```C++
double* dp_async_dsss_pool_state_t::car_hist;
```



n\_receivers rings of ghost\_window carrier estimates (status().doppler\_hz), Hz. 
 


        

<hr>



### variable carrier\_hz 

```C++
double dp_async_dsss_pool_state_t::carrier_hz;
```



The receivers' carrier\_freq\_hz, for the dilation status() carries a phase by. 
 


        

<hr>



### variable carry 

```C++
float _Complex* dp_async_dsss_pool_state_t::carry;
```



The pushed samples past the last whole group, not yet fed: group long. 
 


        

<hr>



### variable carry\_len 

```C++
size_t dp_async_dsss_pool_state_t::carry_len;
```



How many; &lt; group. 
 


        

<hr>



### variable cl\_hist 

```C++
uint8_t* dp_async_dsss_pool_state_t::cl_hist;
```



The same rings of status().code\_locked. 


        

<hr>



### variable fe 

```C++
resamp_state_t* dp_async_dsss_pool_state_t::fe;
```



The pool's one front end: the input resampled to the search's rate, a power of two samples a code period, which the search and every receiver are built at; NULL when the input's period is already whole (§5). 
 


        

<hr>



### variable fe\_buf 

```C++
float _Complex* dp_async_dsss_pool_state_t::fe_buf;
```



Its output, one push's worth. 
 


        

<hr>



### variable fe\_cap 

```C++
size_t dp_async_dsss_pool_state_t::fe_cap;
```



fe\_buf's capacity, samples. 
 


        

<hr>



### variable fe\_out 

```C++
uint64_t dp_async_dsss_pool_state_t::fe_out;
```



Samples it has produced since create or reset. 


        

<hr>



### variable fe\_ratio 

```C++
double dp_async_dsss_pool_state_t::fe_ratio;
```



Its rate: receiver samples an input sample; 1 without it. 
 


        

<hr>



### variable fed\_in 

```C++
uint64_t dp_async_dsss_pool_state_t::fed_in;
```



The input sample status() reports at: the first whole input sample at or after the receivers' next one (samples - carry\_len without fe). 
 


        

<hr>



### variable feed\_n 

```C++
size_t dp_async_dsss_pool_state_t::feed_n;
```



Samples in feed\_x: one group while push() steps. 
 


        

<hr>



### variable feed\_x 

```C++
const float _Complex* dp_async_dsss_pool_state_t::feed_x;
```



The block push() is stepping. 
 


        

<hr>



### variable ghost\_window 

```C++
size_t dp_async_dsss_pool_state_t::ghost_window;
```



Controller steps (code periods) in the ghost rule's window. 
 


        

<hr>



### variable group 

```C++
size_t dp_async_dsss_pool_state_t::group;
```



Samples a join: period times ASYNC\_DSSS\_POOL\_PERIODS\_PER\_JOIN. 


        

<hr>



### variable hist\_len 

```C++
size_t* dp_async_dsss_pool_state_t::hist_len;
```



Entries each ring holds: the controller steps the receiver has been tracking in a row, at most ghost\_window. Every other entry is 0. 
 


        

<hr>



### variable hist\_pos 

```C++
size_t dp_async_dsss_pool_state_t::hist_pos;
```



The ring slot the next step writes; one clock for every ring. 
 


        

<hr>



### variable in\_spc 

```C++
double dp_async_dsss_pool_state_t::in_spc;
```



Input samples a chip. 
 


        

<hr>



### variable lattice\_hz 

```C++
double dp_async_dsss_pool_state_t::lattice_hz;
```



The ghost lattice spacing: chip\_rate / (2 code\_len), half the code-epoch rate. 


        

<hr>



### variable n\_receivers 

```C++
size_t dp_async_dsss_pool_state_t::n_receivers;
```



Receivers, fixed at create. 
 


        

<hr>



### variable n\_sym 

```C++
size_t* dp_async_dsss_pool_state_t::n_sym;
```



Symbols each receiver emitted, last push. 


        

<hr>



### variable period 

```C++
size_t dp_async_dsss_pool_state_t::period;
```



Samples a code period: one receiver call. 
 


        

<hr>



### variable released\_duplicate 

```C++
uint64_t dp_async_dsss_pool_state_t::released_duplicate;
```



Duplicate holders it reset. 
 


        

<hr>



### variable released\_ghost 

```C++
uint64_t dp_async_dsss_pool_state_t::released_ghost;
```



Sidelobe ghosts it reset. 
 


        

<hr>



### variable released\_lost 

```C++
uint64_t dp_async_dsss_pool_state_t::released_lost;
```



Lost receivers the controller reset. 


        

<hr>



### variable rx 

```C++
dp_async_dsss_receiver_state_t** dp_async_dsss_pool_state_t::rx;
```



The receivers, n\_receivers: pooled, so idle until seeded (set\_pooled()). 
 


        

<hr>



### variable samples 

```C++
uint64_t dp_async_dsss_pool_state_t::samples;
```



Input samples pushed since create or reset: the pool's shared clock. 
 


        

<hr>



### variable search 

```C++
dp_async_dsss_receiver_state_t* dp_async_dsss_pool_state_t::search;
```



The controller's one search: a search-only receiver (set\_search\_only()) fed the same periods while any receiver is idle. 
 


        

<hr>



### variable search\_periods 

```C++
uint64_t dp_async_dsss_pool_state_t::search_periods;
```



Periods the search was fed since create or reset: what one search cost. 
 


        

<hr>



### variable searching 

```C++
bool dp_async_dsss_pool_state_t::searching;
```



Whether the search was fed the last period: it restarts when it resumes. 
 


        

<hr>



### variable sym\_buf 

```C++
float _Complex* dp_async_dsss_pool_state_t::sym_buf;
```



n\_receivers rows of sym\_cap symbols. 


        

<hr>



### variable sym\_cap 

```C++
size_t dp_async_dsss_pool_state_t::sym_cap;
```



Symbols one row holds. 
 


        

<hr>



### variable sym\_extra 

```C++
size_t dp_async_dsss_pool_state_t::sym_extra;
```



Symbols one code period carries, rounded up, plus one: what a call can emit past its input's own ([**dp\_async\_dsss\_receiver\_steps()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_steps)). 


        

<hr>



### variable threads 

```C++
int dp_async_dsss_pool_state_t::threads;
```



Workers asked for; 0 = the online cores. 


        

<hr>



### variable tlm 

```C++
dp_tlm_t* dp_async_dsss_pool_state_t::tlm;
```



Attached context; NULL = detached. 
 


        

<hr>



### variable tlm\_decim 

```C++
uint32_t dp_async_dsss_pool_state_t::tlm_decim;
```



Its decimation. 
 


        

<hr>



### variable tlm\_prefix 

```C++
char dp_async_dsss_pool_state_t::tlm_prefix[DP_TLM_NAME_MAX];
```



Its prefix. 
 


        

<hr>



### variable workers 

```C++
dp_pool_t* dp_async_dsss_pool_state_t::workers;
```



NULL when one thread: push() runs serially. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/async_dsss_pool/async_dsss_pool_core.h`

