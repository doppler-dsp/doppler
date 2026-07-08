

# Struct tonemeas\_state\_t



[**ClassList**](annotated.md) **>** [**tonemeas\_state\_t**](structtonemeas__state__t.md)



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
|  [**psd\_state\_t**](structpsd__state__t.md) \* | [**psd**](#variable-psd)  <br> |
|  float \* | [**pwr**](#variable-pwr)  <br> |
|  size\_t | [**spur\_guard\_bins**](#variable-spur_guard_bins)  <br> |












































## Detailed Description


Allocate with [**tonemeas\_create()**](tonemeas__core_8h.md#function-tonemeas_create). `nfft = next_pow2(n * MEASURE_PAD)` is the zero-padded transform length; `enbw` is the window's equivalent-noise bandwidth (bins); `lobe_bins` is the main-lobe half-width L over which a component's power is integrated; `spur_guard_bins` (&gt;= L) is the wider fundamental keep-out used by the worst-spur search so the fundamental's own sidelobes are never reported as a spur. 


    
## Public Attributes Documentation




### variable beta 

```C++
double tonemeas_state_t::beta;
```




<hr>



### variable dc\_guard 

```C++
size_t tonemeas_state_t::dc_guard;
```




<hr>



### variable enbw 

```C++
double tonemeas_state_t::enbw;
```




<hr>



### variable excl 

```C++
unsigned char* tonemeas_state_t::excl;
```




<hr>



### variable fs 

```C++
double tonemeas_state_t::fs;
```




<hr>



### variable lobe\_bins 

```C++
size_t tonemeas_state_t::lobe_bins;
```




<hr>



### variable n 

```C++
size_t tonemeas_state_t::n;
```




<hr>



### variable n\_harm 

```C++
size_t tonemeas_state_t::n_harm;
```




<hr>



### variable nfft 

```C++
size_t tonemeas_state_t::nfft;
```




<hr>



### variable psd 

```C++
psd_state_t* tonemeas_state_t::psd;
```




<hr>



### variable pwr 

```C++
float* tonemeas_state_t::pwr;
```




<hr>



### variable spur\_guard\_bins 

```C++
size_t tonemeas_state_t::spur_guard_bins;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/tonemeas/tonemeas_core.h`

