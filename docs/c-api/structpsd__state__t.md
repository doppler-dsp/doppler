

# Struct psd\_state\_t



[**ClassList**](annotated.md) **>** [**psd\_state\_t**](structpsd__state__t.md)



_PSD state. Allocate with_ [_**psd\_create()**_](psd__core_8h.md#function-psd_create) _._

* `#include <psd_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**acc\_trace\_state\_t**](structacc__trace__state__t.md) \* | [**avg**](#variable-avg)  <br> |
|  size\_t | [**bits**](#variable-bits)  <br> |
|  double | [**cg**](#variable-cg)  <br> |
|  float \* | [**dbbuf**](#variable-dbbuf)  <br> |
|  double | [**enbw**](#variable-enbw)  <br> |
|  [**fft\_state\_t**](structfft__state__t.md) \* | [**fft**](#variable-fft)  <br> |
|  float complex \* | [**frame**](#variable-frame)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  double | [**full\_scale**](#variable-full_scale)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**nfft**](#variable-nfft)  <br> |
|  float \* | [**pwr**](#variable-pwr)  <br> |
|  double | [**s2**](#variable-s2)  <br> |
|  float complex \* | [**spec**](#variable-spec)  <br> |
|  float \* | [**w**](#variable-w)  <br> |












































## Public Attributes Documentation




### variable avg 

```C++
acc_trace_state_t* psd_state_t::avg;
```



Per-bin power averager, length nfft. 


        

<hr>



### variable bits 

```C++
size_t psd_state_t::bits;
```



ADC depth that set full\_scale, else 0. 


        

<hr>



### variable cg 

```C++
double psd_state_t::cg;
```



Window coherent gain, sum(w). 


        

<hr>



### variable dbbuf 

```C++
float* psd_state_t::dbbuf;
```



dB-trace scratch, length nfft. 


        

<hr>



### variable enbw 

```C++
double psd_state_t::enbw;
```



Equivalent noise bandwidth, bins. 


        

<hr>



### variable fft 

```C++
fft_state_t* psd_state_t::fft;
```



Forward cf32 plan, size nfft. 


        

<hr>



### variable frame 

```C++
float complex* psd_state_t::frame;
```



Windowed + zero-padded, length nfft. 


        

<hr>



### variable fs 

```C++
double psd_state_t::fs;
```



Sample rate, Hz. 


        

<hr>



### variable full\_scale 

```C++
double psd_state_t::full_scale;
```



Amplitude that reads 0 dBFS. 


        

<hr>



### variable n 

```C++
size_t psd_state_t::n;
```



Window / frame length (samples). 


        

<hr>



### variable nfft 

```C++
size_t psd_state_t::nfft;
```



Zero-padded transform length. 


        

<hr>



### variable pwr 

```C++
float* psd_state_t::pwr;
```



DC-centred power scratch, length nfft. 


        

<hr>



### variable s2 

```C++
double psd_state_t::s2;
```



Window power, sum(w^2). 


        

<hr>



### variable spec 

```C++
float complex* psd_state_t::spec;
```



FFT output scratch, length nfft. 


        

<hr>



### variable w 

```C++
float* psd_state_t::w;
```



Window, length n. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/psd/psd_core.h`

