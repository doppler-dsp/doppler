

# Struct dp\_carrier\_mpsk\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_carrier\_mpsk\_state\_t**](structdp__carrier__mpsk__state__t.md)



_M-PSK carrier loop state._ [More...](#detailed-description)

* `#include <carrier_mpsk_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float \_Complex | [**acc**](#variable-acc)  <br> |
|  size\_t | [**acc\_n**](#variable-acc_n)  <br> |
|  double | [**bn**](#variable-bn)  <br> |
|  double | [**bn\_fll**](#variable-bn_fll)  <br> |
|  int | [**have\_prev**](#variable-have_prev)  <br> |
|  double | [**k\_fll**](#variable-k_fll)  <br> |
|  double | [**last\_error**](#variable-last_error)  <br> |
|  [**dp\_loop\_filter\_state\_t**](structdp__loop__filter__state__t.md) | [**lf**](#variable-lf)  <br> |
|  double | [**lock\_metric**](#variable-lock_metric)  <br> |
|  int | [**m**](#variable-m)  <br> |
|  [**dp\_lo\_state\_t**](structdp__lo__state__t.md) | [**nco**](#variable-nco)  <br> |
|  float \_Complex | [**prev**](#variable-prev)  <br> |
|  double | [**prev\_abs**](#variable-prev_abs)  <br> |
|  double | [**seed\_norm\_freq**](#variable-seed_norm_freq)  <br> |
|  size\_t | [**tsamps**](#variable-tsamps)  <br> |
|  double | [**zeta**](#variable-zeta)  <br> |












































## Detailed Description


Allocate with [**dp\_carrier\_mpsk\_create()**](carrier__mpsk__core_8h.md#function-dp_carrier_mpsk_create), or embed by value and [**carrier\_mpsk\_init()**](carrier__mpsk__core_8h.md#function-carrier_mpsk_init). The carrier NCO (`nco`) and PI loop (`lf`) are public sub-components so the inline composition helpers can drive them; treat the integrate-and-dump and diagnostic fields as internal. 


    
## Public Attributes Documentation




### variable acc 

```C++
float _Complex dp_carrier_mpsk_state_t::acc;
```



running coherent I&D accumulator. 
 


        

<hr>



### variable acc\_n 

```C++
size_t dp_carrier_mpsk_state_t::acc_n;
```



samples accumulated into `acc`. 
 


        

<hr>



### variable bn 

```C++
double dp_carrier_mpsk_state_t::bn;
```



PLL loop noise bandwidth (retained). 
 


        

<hr>



### variable bn\_fll 

```C++
double dp_carrier_mpsk_state_t::bn_fll;
```



FLL-assist bandwidth (0 = pure PLL). 
 


        

<hr>



### variable have\_prev 

```C++
int dp_carrier_mpsk_state_t::have_prev;
```



prev valid (skip FLL on the 1st symbol). 


        

<hr>



### variable k\_fll 

```C++
double dp_carrier_mpsk_state_t::k_fll;
```



derived FLL gain (per-symbol freq pull). 
 


        

<hr>



### variable last\_error 

```C++
double dp_carrier_mpsk_state_t::last_error;
```



last PLL discriminator (loop stress). 
 


        

<hr>



### variable lf 

```C++
dp_loop_filter_state_t dp_carrier_mpsk_state_t::lf;
```



2nd-order carrier PI loop (PLL). 
 


        

<hr>



### variable lock\_metric 

```C++
double dp_carrier_mpsk_state_t::lock_metric;
```



EMA of Re(P conj a)/\|P\| (1 = locked). 
 


        

<hr>



### variable m 

```C++
int dp_carrier_mpsk_state_t::m;
```



constellation order M (2, 4, 8). 
 


        

<hr>



### variable nco 

```C++
dp_lo_state_t dp_carrier_mpsk_state_t::nco;
```



integer carrier NCO (uint32 phase). 
 


        

<hr>



### variable prev 

```C++
float _Complex dp_carrier_mpsk_state_t::prev;
```



previous _data-wiped_ prompt (FLL cross). 


        

<hr>



### variable prev\_abs 

```C++
double dp_carrier_mpsk_state_t::prev_abs;
```



\|previous prompt\| (FLL normalization). 
 


        

<hr>



### variable seed\_norm\_freq 

```C++
double dp_carrier_mpsk_state_t::seed_norm_freq;
```



create-time carrier freq, for reset. 
 


        

<hr>



### variable tsamps 

```C++
size_t dp_carrier_mpsk_state_t::tsamps;
```



samples per symbol (integrate-and-dump). 


        

<hr>



### variable zeta 

```C++
double dp_carrier_mpsk_state_t::zeta;
```



damping factor (retained). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/carrier_mpsk/carrier_mpsk_core.h`

