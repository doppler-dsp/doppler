

# Struct imdmeas\_state\_t



[**ClassList**](annotated.md) **>** [**imdmeas\_state\_t**](structimdmeas__state__t.md)



_IMDMeasure state: owned window, FFT plan and one-sided power scratch._ 

* `#include <imdmeas_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**cg**](#variable-cg)  <br> |
|  double | [**enbw**](#variable-enbw)  <br> |
|  [**fft\_state\_t**](structfft__state__t.md) \* | [**fft**](#variable-fft)  <br> |
|  float complex \* | [**frame**](#variable-frame)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  double | [**full\_scale**](#variable-full_scale)  <br> |
|  size\_t | [**lobe\_bins**](#variable-lobe_bins)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**nfft**](#variable-nfft)  <br> |
|  float \* | [**pwr**](#variable-pwr)  <br> |
|  double | [**s2**](#variable-s2)  <br> |
|  float complex \* | [**spec**](#variable-spec)  <br> |
|  float \* | [**w**](#variable-w)  <br> |












































## Public Attributes Documentation




### variable cg 

```C++
double imdmeas_state_t::cg;
```




<hr>



### variable enbw 

```C++
double imdmeas_state_t::enbw;
```




<hr>



### variable fft 

```C++
fft_state_t* imdmeas_state_t::fft;
```




<hr>



### variable frame 

```C++
float complex* imdmeas_state_t::frame;
```




<hr>



### variable fs 

```C++
double imdmeas_state_t::fs;
```




<hr>



### variable full\_scale 

```C++
double imdmeas_state_t::full_scale;
```




<hr>



### variable lobe\_bins 

```C++
size_t imdmeas_state_t::lobe_bins;
```




<hr>



### variable n 

```C++
size_t imdmeas_state_t::n;
```




<hr>



### variable nfft 

```C++
size_t imdmeas_state_t::nfft;
```




<hr>



### variable pwr 

```C++
float* imdmeas_state_t::pwr;
```




<hr>



### variable s2 

```C++
double imdmeas_state_t::s2;
```




<hr>



### variable spec 

```C++
float complex* imdmeas_state_t::spec;
```




<hr>



### variable w 

```C++
float* imdmeas_state_t::w;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/imdmeas/imdmeas_core.h`

