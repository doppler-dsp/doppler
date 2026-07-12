

# Struct ppe\_state\_t



[**ClassList**](annotated.md) **>** [**ppe\_state\_t**](structppe__state__t.md)



_PolynomialPhaseEstimator state (FFT plan + rate grid + scratch)._ [More...](#detailed-description)

* `#include <ppe_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float complex \* | [**buf**](#variable-buf)  <br> |
|  double | [**drate**](#variable-drate)  <br> |
|  [**fft\_state\_t**](structfft__state__t.md) \* | [**fft**](#variable-fft)  <br> |
|  float \* | [**mag**](#variable-mag)  <br> |
|  size\_t | [**max\_len**](#variable-max_len)  <br> |
|  double | [**max\_rate**](#variable-max_rate)  <br> |
|  size\_t | [**n\_rate**](#variable-n_rate)  <br> |
|  size\_t | [**nfft**](#variable-nfft)  <br> |
|  double \* | [**rowfrq**](#variable-rowfrq)  <br> |
|  double \* | [**rowpk**](#variable-rowpk)  <br> |
|  float complex \* | [**spec**](#variable-spec)  <br> |
|  float \* | [**win**](#variable-win)  <br> |












































## Detailed Description


Allocate with [**ppe\_create()**](ppe__core_8h.md#function-ppe_create). 


    
## Public Attributes Documentation




### variable buf 

```C++
float complex* ppe_state_t::buf;
```



windowed, dechirped, zero-padded input, nfft. 


        

<hr>



### variable drate 

```C++
double ppe_state_t::drate;
```



chirp-rate grid step. 


        

<hr>



### variable fft 

```C++
fft_state_t* ppe_state_t::fft;
```



forward plan, size nfft. 


        

<hr>



### variable mag 

```C++
float* ppe_state_t::mag;
```



dB magnitude scratch, nfft. 


        

<hr>



### variable max\_len 

```C++
size_t ppe_state_t::max_len;
```



max input length (sizes the plan/scratch). 


        

<hr>



### variable max\_rate 

```C++
double ppe_state_t::max_rate;
```



chirp-rate search half-span (cycles/sample^2). 


        

<hr>



### variable n\_rate 

```C++
size_t ppe_state_t::n_rate;
```



number of chirp-rate hypotheses (1 if max\_rate=0). 


        

<hr>



### variable nfft 

```C++
size_t ppe_state_t::nfft;
```



zero-padded transform length (next pow2 of max\_len). 


        

<hr>



### variable rowfrq 

```C++
double* ppe_state_t::rowfrq;
```



per-rate winning frequency, n\_rate. 


        

<hr>



### variable rowpk 

```C++
double* ppe_state_t::rowpk;
```



per-rate winning peak dB, n\_rate. 


        

<hr>



### variable spec 

```C++
float complex* ppe_state_t::spec;
```



FFT output, nfft. 


        

<hr>



### variable win 

```C++
float* ppe_state_t::win;
```



window scratch, max\_len. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ppe/ppe_core.h`

