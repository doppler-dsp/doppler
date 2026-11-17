

# Struct dp\_costas\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_costas\_state\_t**](structdp__costas__state__t.md)



_Costas loop state._ [More...](#detailed-description)

* `#include <costas_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int32\_t | [**\_pad**](#variable-_pad)  <br> |
|  int32\_t | [**\_pad\_order**](#variable-_pad_order)  <br> |
|  float \_Complex | [**acc**](#variable-acc)  <br> |
|  size\_t | [**acc\_n**](#variable-acc_n)  <br> |
|  double | [**bn**](#variable-bn)  <br> |
|  double | [**bn\_fll**](#variable-bn_fll)  <br> |
|  int | [**have\_prev**](#variable-have_prev)  <br> |
|  double | [**k\_fll**](#variable-k_fll)  <br> |
|  double | [**last\_error**](#variable-last_error)  <br> |
|  [**dp\_loop\_filter\_state\_t**](structdp__loop__filter__state__t.md) | [**lf**](#variable-lf)  <br> |
|  [**dp\_lockdet\_state\_t**](structdp__lockdet__state__t.md) | [**lock**](#variable-lock)  <br> |
|  double | [**lock\_metric**](#variable-lock_metric)  <br> |
|  [**dp\_lo\_state\_t**](structdp__lo__state__t.md) | [**nco**](#variable-nco)  <br> |
|  int32\_t | [**order**](#variable-order)  <br> |
|  float \_Complex | [**prev**](#variable-prev)  <br> |
|  double | [**seed\_norm\_freq**](#variable-seed_norm_freq)  <br> |
|  [**dp\_state\_filter\_state\_t**](structdp__state__filter__state__t.md) | [**sf**](#variable-sf)  <br> |
|  [**costas\_tlm\_t**](structcostas__tlm__t.md) | [**tlm**](#variable-tlm)  <br> |
|  size\_t | [**tsamps**](#variable-tsamps)  <br> |
|  double | [**zeta**](#variable-zeta)  <br> |












































## Detailed Description


Allocate with [**dp\_costas\_create()**](costas__core_8h.md#function-dp_costas_create), or embed by value and [**dp\_costas\_init()**](costas__core_8h.md#function-dp_costas_init). The carrier NCO (`nco`) and PI loop (`lf`) are public sub-components so the inline composition helpers can drive them; treat the integrate-and-dump and diagnostic fields as internal.


Two loop filters are held and one is active (`order`, set by [**dp\_costas\_set\_order()**](costas__core_8h.md#function-dp_costas_set_order)): `lf`, the second-order loop every Costas starts on, and `sf`, a third-order StateFilter that also holds the carrier's rate, so it tracks a frequency ramp with no steady phase error (docs/design/costas.md section 3). `lf` always carries the retained `bn` and `zeta`; at order 3 `sf` runs at the same `bn`. 


    
## Public Attributes Documentation




### variable \_pad 

```C++
int32_t dp_costas_state_t::_pad;
```



The 4 bytes the compiler would put before the double, named so dp\_costas\_init zeroes them: the blob is the struct whole, and a stack-built loop copied into a receiver otherwise carried 4 stack bytes into its blob. The layout and size are unchanged. 


        

<hr>



### variable \_pad\_order 

```C++
int32_t dp_costas_state_t::_pad_order;
```



written 0: the struct is the blob. 
 


        

<hr>



### variable acc 

```C++
float _Complex dp_costas_state_t::acc;
```



running coherent I&D accumulator. 
 


        

<hr>



### variable acc\_n 

```C++
size_t dp_costas_state_t::acc_n;
```



samples accumulated into `acc`. 
 


        

<hr>



### variable bn 

```C++
double dp_costas_state_t::bn;
```



PLL loop noise bandwidth (retained). 
 


        

<hr>



### variable bn\_fll 

```C++
double dp_costas_state_t::bn_fll;
```



FLL-assist bandwidth (0 = pure PLL). 
 


        

<hr>



### variable have\_prev 

```C++
int dp_costas_state_t::have_prev;
```



prev valid (skip FLL on the 1st symbol). 


        

<hr>



### variable k\_fll 

```C++
double dp_costas_state_t::k_fll;
```



derived FLL gain (per-symbol freq pull). 
 


        

<hr>



### variable last\_error 

```C++
double dp_costas_state_t::last_error;
```



last PLL discriminator (loop stress). 
 


        

<hr>



### variable lf 

```C++
dp_loop_filter_state_t dp_costas_state_t::lf;
```



2nd-order carrier PI loop (PLL). 
 


        

<hr>



### variable lock 

```C++
dp_lockdet_state_t dp_costas_state_t::lock;
```



decision rule on lock\_metric: thresholds
* verify counters, stepped per symbol. 
 




        

<hr>



### variable lock\_metric 

```C++
double dp_costas_state_t::lock_metric;
```



EMA of \|Re P\|/\|P\| (1 = locked). 
 


        

<hr>



### variable nco 

```C++
dp_lo_state_t dp_costas_state_t::nco;
```



integer carrier NCO (uint32 phase). 
 


        

<hr>



### variable order 

```C++
int32_t dp_costas_state_t::order;
```



the active filter: 2 (`lf`) or 3 (`sf`); 0 only in the all-zero Costas: one never initialised, as a receiver holds its spare loops (tsamps 0) until it seeds them, which round-trips as is. 
 


        

<hr>



### variable prev 

```C++
float _Complex dp_costas_state_t::prev;
```



previous symbol's prompt (FLL cross). 
 


        

<hr>



### variable seed\_norm\_freq 

```C++
double dp_costas_state_t::seed_norm_freq;
```



create-time carrier freq, for reset. 
 


        

<hr>



### variable sf 

```C++
dp_state_filter_state_t dp_costas_state_t::sf;
```



3rd-order loop (frequency and rate); all zero until order 3 is first selected. 
 


        

<hr>



### variable tlm 

```C++
costas_tlm_t dp_costas_state_t::tlm;
```



live telemetry attachment; zeroed in blobs 


        

<hr>



### variable tsamps 

```C++
size_t dp_costas_state_t::tsamps;
```



samples per symbol (integrate-and-dump). 


        

<hr>



### variable zeta 

```C++
double dp_costas_state_t::zeta;
```



damping factor (retained). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/costas/costas_core.h`

