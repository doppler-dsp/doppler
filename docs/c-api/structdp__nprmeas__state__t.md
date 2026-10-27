

# Struct dp\_nprmeas\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_nprmeas\_state\_t**](structdp__nprmeas__state__t.md)



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
|  [**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* | [**psd**](#variable-psd)  <br> |
|  float \* | [**pwr**](#variable-pwr)  <br> |
|  size\_t | [**spur\_guard\_bins**](#variable-spur_guard_bins)  <br> |












































## Public Attributes Documentation




### variable beta 

```C++
double dp_nprmeas_state_t::beta;
```



Kaiser beta auto-selected at create() so sidelobes sit below the dynamic-range target. 
 


        

<hr>



### variable enbw 

```C++
double dp_nprmeas_state_t::enbw;
```



Window equivalent noise bandwidth, bins (copied from the PSD core at create()). 
 


        

<hr>



### variable fs 

```C++
double dp_nprmeas_state_t::fs;
```



Sample rate, Hz. 
 


        

<hr>



### variable n 

```C++
size_t dp_nprmeas_state_t::n;
```



Capture / frame length, samples. 
 


        

<hr>



### variable nfft 

```C++
size_t dp_nprmeas_state_t::nfft;
```



Zero-padded transform length, bins. 
 


        

<hr>



### variable psd 

```C++
dp_psd_state_t* dp_nprmeas_state_t::psd;
```



Owned averaging PSD core (Kaiser window, FFT plan, segment average); also holds the dBFS reference. Set at create(). 


        

<hr>



### variable pwr 

```C++
float* dp_nprmeas_state_t::pwr;
```



Owned scratch, nfft floats: one-sided power spectrum (first nfft/2 + 1 used), rewritten by every analyze(). 
 


        

<hr>



### variable spur\_guard\_bins 

```C++
size_t dp_nprmeas_state_t::spur_guard_bins;
```



Minimum notch keep-out, bins: main lobe plus sidelobe skirt of the window. A smaller guard\_hz is raised to this. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/nprmeas/nprmeas_core.h`

