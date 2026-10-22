

# Struct dp\_async\_dsss\_pool\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_async\_dsss\_pool\_state\_t**](structdp__async__dsss__pool__state__t.md)



_AsyncDsssPool state._ [More...](#detailed-description)

* `#include <async_dsss_pool_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**feed\_n**](#variable-feed_n)  <br> |
|  const float \_Complex \* | [**feed\_x**](#variable-feed_x)  <br> |
|  size\_t | [**n\_receivers**](#variable-n_receivers)  <br> |
|  size\_t \* | [**n\_sym**](#variable-n_sym)  <br> |
|  size\_t | [**period**](#variable-period)  <br> |
|  uint64\_t | [**released\_duplicate**](#variable-released_duplicate)  <br> |
|  uint64\_t | [**released\_lost**](#variable-released_lost)  <br> |
|  [**dp\_async\_dsss\_receiver\_state\_t**](structdp__async__dsss__receiver__state__t.md) \*\* | [**rx**](#variable-rx)  <br> |
|  uint64\_t | [**samples**](#variable-samples)  <br> |
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




### variable feed\_n 

```C++
size_t dp_async_dsss_pool_state_t::feed_n;
```




<hr>



### variable feed\_x 

```C++
const float _Complex* dp_async_dsss_pool_state_t::feed_x;
```



The block push() is stepping. 
 


        

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



Samples a code period: the controller runs every period of `samples`. 
 


        

<hr>



### variable released\_duplicate 

```C++
uint64_t dp_async_dsss_pool_state_t::released_duplicate;
```



Duplicate holders it reset. 
 


        

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



The receivers, n\_receivers. 


        

<hr>



### variable samples 

```C++
uint64_t dp_async_dsss_pool_state_t::samples;
```



Input samples pushed since create or reset: the pool's shared clock. 
 


        

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

