

# Struct dp\_tonemeas\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_tonemeas\_state\_t**](structdp__tonemeas__state__t.md)



_ToneMeasure state: owned window, FFT plan and analysis scratch._ [More...](#detailed-description)

* `#include <tonemeas_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**beta**](#variable-beta)  <br> |
|  size\_t | [**dc\_guard**](#variable-dc_guard)  <br> |
|  double | [**enbw**](#variable-enbw)  <br> |
|  unsigned char \* | [**excl**](#variable-excl)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  size\_t | [**lobe\_bins**](#variable-lobe_bins)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**n\_harm**](#variable-n_harm)  <br> |
|  size\_t | [**nfft**](#variable-nfft)  <br> |
|  [**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* | [**psd**](#variable-psd)  <br> |
|  float \* | [**pwr**](#variable-pwr)  <br> |
|  size\_t | [**spur\_guard\_bins**](#variable-spur_guard_bins)  <br> |












































## Detailed Description


Allocate with [**dp\_tonemeas\_create()**](tonemeas__core_8h.md#function-dp_tonemeas_create). `nfft = next_pow_two(n * MEASURE_PAD)` is the zero-padded transform length; `enbw` is the window's equivalent-noise bandwidth (bins); `lobe_bins` is the main-lobe half-width L over which a component's power is integrated; `spur_guard_bins` (&gt;= L) is the wider fundamental keep-out used by the worst-spur search so the fundamental's own sidelobes are never reported as a spur. 


    
## Public Attributes Documentation




### variable beta 

```C++
double dp_tonemeas_state_t::beta;
```



Kaiser shape auto-selected at create() from the dynamic-range target. 
 


        

<hr>



### variable dc\_guard 

```C++
size_t dp_tonemeas_state_t::dc_guard;
```



Extra bins excluded beyond L around DC, from create(). 
 


        

<hr>



### variable enbw 

```C++
double dp_tonemeas_state_t::enbw;
```



Window equivalent noise bandwidth, bins. 
 


        

<hr>



### variable excl 

```C++
unsigned char* dp_tonemeas_state_t::excl;
```



Owned DC/fundamental/harmonic exclusion mask scratch, length nfft. 
 


        

<hr>



### variable fs 

```C++
double dp_tonemeas_state_t::fs;
```



Sample rate, Hz. 
 


        

<hr>



### variable lobe\_bins 

```C++
size_t dp_tonemeas_state_t::lobe_bins;
```



Window main-lobe half-width L, bins. 
 


        

<hr>



### variable n 

```C++
size_t dp_tonemeas_state_t::n;
```



Capture / frame length, samples. 
 


        

<hr>



### variable n\_harm 

```C++
size_t dp_tonemeas_state_t::n_harm;
```



Harmonics tracked (k = 2..n\_harm). 
 


        

<hr>



### variable nfft 

```C++
size_t dp_tonemeas_state_t::nfft;
```



Zero-padded transform length, bins. 
 


        

<hr>



### variable psd 

```C++
dp_psd_state_t* dp_tonemeas_state_t::psd;
```



Owned averaging PSD core (window, FFT, frame mean, 0-dBFS reference). 
 


        

<hr>



### variable pwr 

```C++
float* dp_tonemeas_state_t::pwr;
```



Owned power-spectrum scratch, length nfft; overwritten by every call. 
 


        

<hr>



### variable spur\_guard\_bins 

```C++
size_t dp_tonemeas_state_t::spur_guard_bins;
```



Fundamental keep-out half-width for the worst-spur search, bins (&gt;= L). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/tonemeas/tonemeas_core.h`

