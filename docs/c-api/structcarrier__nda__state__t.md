

# Struct carrier\_nda\_state\_t



[**ClassList**](annotated.md) **>** [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md)



_NDA M-th-power carrier loop state._ [More...](#detailed-description)

* `#include <carrier_nda_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**agc\_state\_t**](structagc__state__t.md) | [**agc**](#variable-agc)  <br> |
|  [**boxcar\_state\_t**](structboxcar__state__t.md) | [**arm**](#variable-arm)  <br> |
|  size\_t | [**arm\_len**](#variable-arm_len)  <br> |
|  double | [**bn**](#variable-bn)  <br> |
|  double | [**ctl\_cyc**](#variable-ctl_cyc)  <br> |
|  double | [**last\_error**](#variable-last_error)  <br> |
|  [**loop\_filter\_state\_t**](structloop__filter__state__t.md) | [**lf**](#variable-lf)  <br> |
|  double | [**lock**](#variable-lock)  <br> |
|  double | [**lock\_scale**](#variable-lock_scale)  <br> |
|  int | [**m**](#variable-m)  <br> |
|  int | [**n**](#variable-n)  <br> |
|  [**lo\_state\_t**](structlo__state__t.md) | [**nco**](#variable-nco)  <br> |
|  double | [**seed\_norm\_freq**](#variable-seed_norm_freq)  <br> |
|  size\_t | [**sps**](#variable-sps)  <br> |
|  double | [**zeta**](#variable-zeta)  <br> |












































## Detailed Description


Allocate with [**carrier\_nda\_create()**](carrier__nda__core_8h.md#function-carrier_nda_create), or embed by value and [**carrier\_nda\_init()**](carrier__nda__core_8h.md#function-carrier_nda_init). The carrier NCO (`nco`) and PI loop (`lf`) are public sub-components so a composing receiver can drive the same NCO; treat the arm accumulator and the diagnostics as internal. 


    
## Public Attributes Documentation




### variable agc 

```C++
agc_state_t carrier_nda_state_t::agc;
```



per-sample log-domain AGC on the arm sample (normalizes to unit average power). 


        

<hr>



### variable arm 

```C++
boxcar_state_t carrier_nda_state_t::arm;
```



I/Q boxcar moving-average arm (sps/n). 


        

<hr>



### variable arm\_len 

```C++
size_t carrier_nda_state_t::arm_len;
```



moving-average window length (= sps / n). 


        

<hr>



### variable bn 

```C++
double carrier_nda_state_t::bn;
```



PLL loop noise bandwidth (retained). 


        

<hr>



### variable ctl\_cyc 

```C++
double carrier_nda_state_t::ctl_cyc;
```



NCO control (cyc/sample) for next wipeoff. 


        

<hr>



### variable last\_error 

```C++
double carrier_nda_state_t::last_error;
```



last phase discriminator (loop stress). 


        

<hr>



### variable lf 

```C++
loop_filter_state_t carrier_nda_state_t::lf;
```



2nd-order carrier PI loop. 


        

<hr>



### variable lock 

```C++
double carrier_nda_state_t::lock;
```



EMA of the lock signal (1 = locked). 


        

<hr>



### variable lock\_scale 

```C++
double carrier_nda_state_t::lock_scale;
```



per-M lock-signal scale (1/0.619/0.412). 


        

<hr>



### variable m 

```C++
int carrier_nda_state_t::m;
```



constellation order M (2, 4, 8). 


        

<hr>



### variable n 

```C++
int carrier_nda_state_t::n;
```



sets the MA window (= a 1/n-symbol box). 


        

<hr>



### variable nco 

```C++
lo_state_t carrier_nda_state_t::nco;
```



integer carrier NCO (uint32 phase). 


        

<hr>



### variable seed\_norm\_freq 

```C++
double carrier_nda_state_t::seed_norm_freq;
```



create-time carrier freq, for reset. 


        

<hr>



### variable sps 

```C++
size_t carrier_nda_state_t::sps;
```



samples per symbol. 


        

<hr>



### variable zeta 

```C++
double carrier_nda_state_t::zeta;
```



damping factor (retained). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/carrier_nda/carrier_nda_core.h`

