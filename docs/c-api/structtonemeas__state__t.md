

# Struct tonemeas\_state\_t



[**ClassList**](annotated.md) **>** [**tonemeas\_state\_t**](structtonemeas__state__t.md)



_ToneMeasure state: owned window, FFT plan and analysis scratch._ [More...](#detailed-description)

* `#include <tonemeas_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float | [**beta**](#variable-beta)  <br> |
|  double | [**cg**](#variable-cg)  <br> |
|  size\_t | [**dc\_guard**](#variable-dc_guard)  <br> |
|  double | [**enbw**](#variable-enbw)  <br> |
|  unsigned char \* | [**excl**](#variable-excl)  <br> |
|  [**fft\_state\_t**](structfft__state__t.md) \* | [**fft**](#variable-fft)  <br> |
|  float complex \* | [**frame**](#variable-frame)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  double | [**full\_scale**](#variable-full_scale)  <br> |
|  size\_t | [**lobe\_bins**](#variable-lobe_bins)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**n\_harm**](#variable-n_harm)  <br> |
|  size\_t | [**nfft**](#variable-nfft)  <br> |
|  float \* | [**pwr**](#variable-pwr)  <br> |
|  double | [**s2**](#variable-s2)  <br> |
|  float complex \* | [**spec**](#variable-spec)  <br> |
|  float \* | [**w**](#variable-w)  <br> |
|  int | [**window**](#variable-window)  <br> |












































## Detailed Description


Allocate with [**tonemeas\_create()**](tonemeas__core_8h.md#function-tonemeas_create). `nfft = next_pow2(n * pad)` is the zero-padded transform length; `cg`/`s2`/`enbw` are the window's coherent gain, power and equivalent-noise bandwidth (bins); `lobe_bins` is the main-lobe half-width L over which component power is integrated. 


    
## Public Attributes Documentation




### variable beta 

```C++
float tonemeas_state_t::beta;
```




<hr>



### variable cg 

```C++
double tonemeas_state_t::cg;
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



### variable fft 

```C++
fft_state_t* tonemeas_state_t::fft;
```




<hr>



### variable frame 

```C++
float complex* tonemeas_state_t::frame;
```




<hr>



### variable fs 

```C++
double tonemeas_state_t::fs;
```




<hr>



### variable full\_scale 

```C++
double tonemeas_state_t::full_scale;
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



### variable pwr 

```C++
float* tonemeas_state_t::pwr;
```




<hr>



### variable s2 

```C++
double tonemeas_state_t::s2;
```




<hr>



### variable spec 

```C++
float complex* tonemeas_state_t::spec;
```




<hr>



### variable w 

```C++
float* tonemeas_state_t::w;
```




<hr>



### variable window 

```C++
int tonemeas_state_t::window;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/tonemeas/tonemeas_core.h`

