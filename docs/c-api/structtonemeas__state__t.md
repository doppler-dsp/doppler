

# Struct tonemeas\_state\_t



[**ClassList**](annotated.md) **>** [**tonemeas\_state\_t**](structtonemeas__state__t.md)



_ToneMeasure state: owned window, FFT plan and analysis scratch._ [More...](#detailed-description)

* `#include <tonemeas_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
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
|  int | [**window**](#variable-window)  <br> |












































## Detailed Description


Allocate with [**tonemeas\_create()**](tonemeas__core_8h.md#function-tonemeas_create). `nfft = next_pow2(n * pad)` is the zero-padded transform length; `cg`/`s2`/`enbw` are the window's coherent gain, power and equivalent-noise bandwidth (bins); `lobe_bins` is the main-lobe half-width L over which component power is integrated. 


    
## Public Attributes Documentation




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



### variable window 

```C++
int tonemeas_state_t::window;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/tonemeas/tonemeas_core.h`

