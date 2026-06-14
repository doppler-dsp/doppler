

# Struct nprmeas\_state\_t



[**ClassList**](annotated.md) **>** [**nprmeas\_state\_t**](structnprmeas__state__t.md)



_NPRMeasure state: owned window, FFT plan and one-sided power scratch._ 

* `#include <nprmeas_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**cg**](#variable-cg)  <br> |
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




### variable cg 

```C++
double nprmeas_state_t::cg;
```




<hr>



### variable enbw 

```C++
double nprmeas_state_t::enbw;
```




<hr>



### variable fft 

```C++
fft_state_t* nprmeas_state_t::fft;
```




<hr>



### variable frame 

```C++
float complex* nprmeas_state_t::frame;
```




<hr>



### variable fs 

```C++
double nprmeas_state_t::fs;
```




<hr>



### variable full\_scale 

```C++
double nprmeas_state_t::full_scale;
```




<hr>



### variable n 

```C++
size_t nprmeas_state_t::n;
```




<hr>



### variable nfft 

```C++
size_t nprmeas_state_t::nfft;
```




<hr>



### variable pwr 

```C++
float* nprmeas_state_t::pwr;
```




<hr>



### variable s2 

```C++
double nprmeas_state_t::s2;
```




<hr>



### variable spec 

```C++
float complex* nprmeas_state_t::spec;
```




<hr>



### variable w 

```C++
float* nprmeas_state_t::w;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/nprmeas/nprmeas_core.h`

