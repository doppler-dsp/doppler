

# Struct nprmeas\_state\_t



[**ClassList**](annotated.md) **>** [**nprmeas\_state\_t**](structnprmeas__state__t.md)



_NPRMeasure state: owned window, FFT plan and one-sided power scratch._ 

* `#include <nprmeas_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**beta**](#variable-beta)  <br> |
|  double | [**enbw**](#variable-enbw)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**nfft**](#variable-nfft)  <br> |
|  [**psd\_state\_t**](structpsd__state__t.md) \* | [**psd**](#variable-psd)  <br> |
|  float \* | [**pwr**](#variable-pwr)  <br> |
|  size\_t | [**spur\_guard\_bins**](#variable-spur_guard_bins)  <br> |












































## Public Attributes Documentation




### variable beta 

```C++
double nprmeas_state_t::beta;
```




<hr>



### variable enbw 

```C++
double nprmeas_state_t::enbw;
```




<hr>



### variable fs 

```C++
double nprmeas_state_t::fs;
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



### variable psd 

```C++
psd_state_t* nprmeas_state_t::psd;
```




<hr>



### variable pwr 

```C++
float* nprmeas_state_t::pwr;
```




<hr>



### variable spur\_guard\_bins 

```C++
size_t nprmeas_state_t::spur_guard_bins;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/nprmeas/nprmeas_core.h`

