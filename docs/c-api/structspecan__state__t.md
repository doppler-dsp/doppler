

# Struct specan\_state\_t



[**ClassList**](annotated.md) **>** [**specan\_state\_t**](structspecan__state__t.md)



_Specan state. Allocate with_ [_**specan\_create()**_](specan__core_8h.md#function-specan_create) _._

* `#include <specan_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**beta**](#variable-beta)  <br> |
|  double | [**center**](#variable-center)  <br> |
|  [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* | [**ddc**](#variable-ddc)  <br> |
|  size\_t | [**disp\_lo**](#variable-disp_lo)  <br> |
|  size\_t | [**disp\_n**](#variable-disp_n)  <br> |
|  double | [**fs\_in**](#variable-fs_in)  <br> |
|  double | [**fs\_out**](#variable-fs_out)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**navg**](#variable-navg)  <br> |
|  size\_t | [**nfft**](#variable-nfft)  <br> |
|  float complex \* | [**pend**](#variable-pend)  <br> |
|  size\_t | [**pend\_cap**](#variable-pend_cap)  <br> |
|  size\_t | [**pend\_len**](#variable-pend_len)  <br> |
|  [**welch\_state\_t**](structwelch__state__t.md) \* | [**psd**](#variable-psd)  <br> |
|  float \* | [**pwr**](#variable-pwr)  <br> |
|  double | [**rbw**](#variable-rbw)  <br> |
|  double | [**ref\_db**](#variable-ref_db)  <br> |
|  float complex \* | [**scratch**](#variable-scratch)  <br> |
|  size\_t | [**scratch\_cap**](#variable-scratch_cap)  <br> |
|  double | [**span**](#variable-span)  <br> |
|  double | [**src\_center**](#variable-src_center)  <br> |












































## Public Attributes Documentation




### variable beta 

```C++
double specan_state_t::beta;
```



Kaiser beta realising [**rbw**](structspecan__state__t.md#variable-rbw). 


        

<hr>



### variable center 

```C++
double specan_state_t::center;
```



Display center frequency, Hz. 


        

<hr>



### variable ddc 

```C++
ddc_state_t* specan_state_t::ddc;
```



Tuner + decimator (mix to DC, resample). 


        

<hr>



### variable disp\_lo 

```C++
size_t specan_state_t::disp_lo;
```



First display bin in the DC-centred array. 


        

<hr>



### variable disp\_n 

```C++
size_t specan_state_t::disp_n;
```



Display band length (cropped bins). 


        

<hr>



### variable fs\_in 

```C++
double specan_state_t::fs_in;
```



Input sample rate, Hz. 


        

<hr>



### variable fs\_out 

```C++
double specan_state_t::fs_out;
```



Decimated rate, Hz (= span·1.28, ≤ fs\_in). 


        

<hr>



### variable n 

```C++
size_t specan_state_t::n;
```



Segment / window length (samples). 


        

<hr>



### variable navg 

```C++
size_t specan_state_t::navg;
```



Segments averaged per emitted frame. 


        

<hr>



### variable nfft 

```C++
size_t specan_state_t::nfft;
```



Zero-padded transform length. 


        

<hr>



### variable pend 

```C++
float complex* specan_state_t::pend;
```



Decimated samples awaiting a frame. 


        

<hr>



### variable pend\_cap 

```C++
size_t specan_state_t::pend_cap;
```



Elements allocated in [**pend**](structspecan__state__t.md#variable-pend). 


        

<hr>



### variable pend\_len 

```C++
size_t specan_state_t::pend_len;
```



Valid samples in [**pend**](structspecan__state__t.md#variable-pend). 


        

<hr>



### variable psd 

```C++
welch_state_t* specan_state_t::psd;
```



Averaging PSD at the decimated rate. 


        

<hr>



### variable pwr 

```C++
float* specan_state_t::pwr;
```



Two-sided linear power scratch, length nfft. 


        

<hr>



### variable rbw 

```C++
double specan_state_t::rbw;
```



Requested resolution bandwidth, Hz. 


        

<hr>



### variable ref\_db 

```C++
double specan_state_t::ref_db;
```



dB offset added to the display spectrum. 


        

<hr>



### variable scratch 

```C++
float complex* specan_state_t::scratch;
```



Ddc output scratch, capacity scratch\_cap. 


        

<hr>



### variable scratch\_cap 

```C++
size_t specan_state_t::scratch_cap;
```



Elements allocated in [**scratch**](structspecan__state__t.md#variable-scratch). 


        

<hr>



### variable span 

```C++
double specan_state_t::span;
```



Display span, Hz. 


        

<hr>



### variable src\_center 

```C++
double specan_state_t::src_center;
```



Source center frequency, Hz. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/specan/specan_core.h`

