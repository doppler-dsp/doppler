

# Struct dp\_imdmeas\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_imdmeas\_state\_t**](structdp__imdmeas__state__t.md)



_IMDMeasure state: owned window, FFT plan and one-sided power scratch._ 

* `#include <imdmeas_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**beta**](#variable-beta)  <br> |
|  double | [**enbw**](#variable-enbw)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  size\_t | [**lobe\_bins**](#variable-lobe_bins)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**nfft**](#variable-nfft)  <br> |
|  [**dp\_psd\_state\_t**](structdp__psd__state__t.md) \* | [**psd**](#variable-psd)  <br> |
|  float \* | [**pwr**](#variable-pwr)  <br> |
|  size\_t | [**spur\_guard\_bins**](#variable-spur_guard_bins)  <br> |












































## Public Attributes Documentation




### variable beta 

```C++
double dp_imdmeas_state_t::beta;
```



Kaiser shape chosen at create from the dynamic-range target (measure\_resolve\_dr). 


        

<hr>



### variable enbw 

```C++
double dp_imdmeas_state_t::enbw;
```



Window equivalent noise bandwidth (bins of the unpadded n-point grid); set at create. 


        

<hr>



### variable fs 

```C++
double dp_imdmeas_state_t::fs;
```



Sample rate, Hz. 
 


        

<hr>



### variable lobe\_bins 

```C++
size_t dp_imdmeas_state_t::lobe_bins;
```



Main-lobe half-width L (bins of the padded nfft grid): each tone/product's power is summed over bins c-L..c+L. Set at create. 


        

<hr>



### variable n 

```C++
size_t dp_imdmeas_state_t::n;
```



Capture / frame length, samples. 
 


        

<hr>



### variable nfft 

```C++
size_t dp_imdmeas_state_t::nfft;
```



Zero-padded transform length, bins. 
 


        

<hr>



### variable psd 

```C++
dp_psd_state_t* dp_imdmeas_state_t::psd;
```



Owned averaging PSD core (Kaiser window, zero-padded FFT, mean trace); built at create, reset by every analyze call. 


        

<hr>



### variable pwr 

```C++
float* dp_imdmeas_state_t::pwr;
```



Owned scratch of nfft floats: the one-sided power analyze() reads, or the two-sided power spectrum\_dbfs() converts. Overwritten per call. 


        

<hr>



### variable spur\_guard\_bins 

```C++
size_t dp_imdmeas_state_t::spur_guard_bins;
```



Tone keep-out (bins, &gt;= L) for the two-tone search: the tones must sit more than this far from DC and from each other. Set at create. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/imdmeas/imdmeas_core.h`

