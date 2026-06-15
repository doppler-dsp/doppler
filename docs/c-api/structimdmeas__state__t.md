

# Struct imdmeas\_state\_t



[**ClassList**](annotated.md) **>** [**imdmeas\_state\_t**](structimdmeas__state__t.md)



_IMDMeasure state: owned window, FFT plan and one-sided power scratch._ 

* `#include <imdmeas_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**enbw**](#variable-enbw)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  size\_t | [**lobe\_bins**](#variable-lobe_bins)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**nfft**](#variable-nfft)  <br> |
|  [**psd\_state\_t**](structpsd__state__t.md) \* | [**psd**](#variable-psd)  <br> |
|  float \* | [**pwr**](#variable-pwr)  <br> |












































## Public Attributes Documentation




### variable enbw 

```C++
double imdmeas_state_t::enbw;
```




<hr>



### variable fs 

```C++
double imdmeas_state_t::fs;
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



### variable psd 

```C++
psd_state_t* imdmeas_state_t::psd;
```




<hr>



### variable pwr 

```C++
float* imdmeas_state_t::pwr;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/imdmeas/imdmeas_core.h`

