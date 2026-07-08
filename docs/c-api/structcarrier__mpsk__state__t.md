

# Struct carrier\_mpsk\_state\_t



[**ClassList**](annotated.md) **>** [**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md)



_M-PSK carrier loop state._ [More...](#detailed-description)

* `#include <carrier_mpsk_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float complex | [**acc**](#variable-acc)  <br> |
|  size\_t | [**acc\_n**](#variable-acc_n)  <br> |
|  double | [**bn**](#variable-bn)  <br> |
|  double | [**bn\_fll**](#variable-bn_fll)  <br> |
|  int | [**have\_prev**](#variable-have_prev)  <br> |
|  double | [**k\_fll**](#variable-k_fll)  <br> |
|  double | [**last\_error**](#variable-last_error)  <br> |
|  [**loop\_filter\_state\_t**](structloop__filter__state__t.md) | [**lf**](#variable-lf)  <br> |
|  double | [**lock\_metric**](#variable-lock_metric)  <br> |
|  int | [**m**](#variable-m)  <br> |
|  [**lo\_state\_t**](structlo__state__t.md) | [**nco**](#variable-nco)  <br> |
|  float complex | [**prev**](#variable-prev)  <br> |
|  double | [**prev\_abs**](#variable-prev_abs)  <br> |
|  double | [**seed\_norm\_freq**](#variable-seed_norm_freq)  <br> |
|  size\_t | [**tsamps**](#variable-tsamps)  <br> |
|  double | [**zeta**](#variable-zeta)  <br> |












































## Detailed Description


Allocate with [**carrier\_mpsk\_create()**](carrier__mpsk__core_8h.md#function-carrier_mpsk_create), or embed by value and [**carrier\_mpsk\_init()**](carrier__mpsk__core_8h.md#function-carrier_mpsk_init). The carrier NCO (`nco`) and PI loop (`lf`) are public sub-components so the inline composition helpers can drive them; treat the integrate-and-dump and diagnostic fields as internal. 


    
## Public Attributes Documentation




### variable acc 

```C++
float complex carrier_mpsk_state_t::acc;
```



running coherent I&D accumulator. 


        

<hr>



### variable acc\_n 

```C++
size_t carrier_mpsk_state_t::acc_n;
```



samples accumulated into `acc`. 


        

<hr>



### variable bn 

```C++
double carrier_mpsk_state_t::bn;
```



PLL loop noise bandwidth (retained). 


        

<hr>



### variable bn\_fll 

```C++
double carrier_mpsk_state_t::bn_fll;
```



FLL-assist bandwidth (0 = pure PLL). 


        

<hr>



### variable have\_prev 

```C++
int carrier_mpsk_state_t::have_prev;
```



prev valid (skip FLL on the 1st symbol). 


        

<hr>



### variable k\_fll 

```C++
double carrier_mpsk_state_t::k_fll;
```



derived FLL gain (per-symbol freq pull). 


        

<hr>



### variable last\_error 

```C++
double carrier_mpsk_state_t::last_error;
```



last PLL discriminator (loop stress). 


        

<hr>



### variable lf 

```C++
loop_filter_state_t carrier_mpsk_state_t::lf;
```



2nd-order carrier PI loop (PLL). 


        

<hr>



### variable lock\_metric 

```C++
double carrier_mpsk_state_t::lock_metric;
```



EMA of Re(P conj a)/\|P\| (1 = locked). 


        

<hr>



### variable m 

```C++
int carrier_mpsk_state_t::m;
```



constellation order M (2, 4, 8). 


        

<hr>



### variable nco 

```C++
lo_state_t carrier_mpsk_state_t::nco;
```



integer carrier NCO (uint32 phase). 


        

<hr>



### variable prev 

```C++
float complex carrier_mpsk_state_t::prev;
```



previous _data-wiped_ prompt (FLL cross). 


        

<hr>



### variable prev\_abs 

```C++
double carrier_mpsk_state_t::prev_abs;
```



\|previous prompt\| (FLL normalization). 


        

<hr>



### variable seed\_norm\_freq 

```C++
double carrier_mpsk_state_t::seed_norm_freq;
```



create-time carrier freq, for reset. 


        

<hr>



### variable tsamps 

```C++
size_t carrier_mpsk_state_t::tsamps;
```



samples per symbol (integrate-and-dump). 


        

<hr>



### variable zeta 

```C++
double carrier_mpsk_state_t::zeta;
```



damping factor (retained). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/carrier_mpsk/carrier_mpsk_core.h`

