

# Struct dp\_fft\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_fft\_state\_t**](structdp__fft__state__t.md)



_FFT engine state: two fixed-length pocketfft plans (CF64, CF32) plus a lazily allocated truncation scratch._ [More...](#detailed-description)

* `#include <fft_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**n**](#variable-n)  <br> |
|  pocketfft\_plan \* | [**plan\_f32**](#variable-plan_f32)  <br> |
|  pocketfft\_plan \* | [**plan\_f64**](#variable-plan_f64)  <br> |
|  int | [**sign**](#variable-sign)  <br> |
|  double \_Complex \* | [**work\_trunc**](#variable-work_trunc)  <br> |












































## Detailed Description


All fields are config set at [**dp\_fft\_create()**](fft__core_8h.md#function-dp_fft_create); nothing runs between calls. Allocate with [**dp\_fft\_create()**](fft__core_8h.md#function-dp_fft_create). 


    
## Public Attributes Documentation




### variable n 

```C++
size_t dp_fft_state_t::n;
```



Transform length (samples). 


        

<hr>



### variable plan\_f32 

```C++
pocketfft_plan* dp_fft_state_t::plan_f32;
```



CF32 1-D plan. 


        

<hr>



### variable plan\_f64 

```C++
pocketfft_plan* dp_fft_state_t::plan_f64;
```



CF64 1-D plan. 


        

<hr>



### variable sign 

```C++
int dp_fft_state_t::sign;
```



-1 forward, +1 inverse. 
 


        

<hr>



### variable work\_trunc 

```C++
double _Complex* dp_fft_state_t::work_trunc;
```



Scratch for a short `out`. A pocketfft plan is fixed at n and writes all n bins, so it cannot be pointed at a smaller buffer; a truncating call transforms here and copies the prefix. Sized for the widest element (CF64) so one buffer serves both plans. Allocated lazily  max\_out is n on every sized call, which is everything the Python binding and every in-tree caller does. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/fft/fft_core.h`

