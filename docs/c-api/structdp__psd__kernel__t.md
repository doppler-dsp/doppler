

# Struct dp\_psd\_kernel\_t



[**ClassList**](annotated.md) **>** [**dp\_psd\_kernel\_t**](structdp__psd__kernel__t.md)



_PSD's per-frame kernel: one frame of n samples in, nfft DC-centred power bins out, and their reading against the full-scale reference._ [More...](#detailed-description)

* `#include <psd_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**cg**](#variable-cg)  <br> |
|  double | [**enbw**](#variable-enbw)  <br> |
|  [**dp\_fft\_state\_t**](structdp__fft__state__t.md) \* | [**fft**](#variable-fft)  <br> |
|  float \_Complex \* | [**frame**](#variable-frame)  <br> |
|  double | [**full\_scale**](#variable-full_scale)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**nfft**](#variable-nfft)  <br> |
|  float \* | [**pwr**](#variable-pwr)  <br> |
|  double | [**s2**](#variable-s2)  <br> |
|  float \_Complex \* | [**spec**](#variable-spec)  <br> |
|  float \* | [**w**](#variable-w)  <br> |












































## Detailed Description


The one place a frame becomes a spectrum. It is held BY VALUE by every object that turns frames into spectra  PSD, which folds each into its average, and the Spectrogram, which emits each one  so neither owns a copy of the window, the transform or the dBFS reference, and neither carries the other's machinery. Fill it with [**dp\_psd\_kernel\_init()**](psd__core_8h.md#function-dp_psd_kernel_init) and release it with [**dp\_psd\_kernel\_free()**](psd__core_8h.md#function-dp_psd_kernel_free); the frame calls below use its scratch, so one kernel serves one thread at a time. 


    
## Public Attributes Documentation




### variable cg 

```C++
double dp_psd_kernel_t::cg;
```



Window coherent gain, sum(w). 
 


        

<hr>



### variable enbw 

```C++
double dp_psd_kernel_t::enbw;
```



Equivalent noise bandwidth, bins: n \* s2 / cg^2 (1.5 for Hann). 
 


        

<hr>



### variable fft 

```C++
dp_fft_state_t* dp_psd_kernel_t::fft;
```



Forward cf32 plan, size nfft. 
 


        

<hr>



### variable frame 

```C++
float _Complex* dp_psd_kernel_t::frame;
```



Windowed + zero-padded, length nfft. 
 


        

<hr>



### variable full\_scale 

```C++
double dp_psd_kernel_t::full_scale;
```



Amplitude that reads 0 dBFS. 
 


        

<hr>



### variable n 

```C++
size_t dp_psd_kernel_t::n;
```



Window / frame length (samples). 
 


        

<hr>



### variable nfft 

```C++
size_t dp_psd_kernel_t::nfft;
```



Zero-padded transform length. 
 


        

<hr>



### variable pwr 

```C++
float* dp_psd_kernel_t::pwr;
```



DC-centred power scratch, length nfft. 


        

<hr>



### variable s2 

```C++
double dp_psd_kernel_t::s2;
```



Window power, sum(w^2). 
 


        

<hr>



### variable spec 

```C++
float _Complex* dp_psd_kernel_t::spec;
```



FFT output scratch, length nfft. 
 


        

<hr>



### variable w 

```C++
float* dp_psd_kernel_t::w;
```



Window, length n. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/psd/psd_core.h`

