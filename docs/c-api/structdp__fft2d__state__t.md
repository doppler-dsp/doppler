

# Struct dp\_fft2d\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_fft2d\_state\_t**](structdp__fft2d__state__t.md)



_2-D FFT engine state: two fixed ny x nx plans plus scratch._ [More...](#detailed-description)

* `#include <fft2d_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**nx**](#variable-nx)  <br> |
|  size\_t | [**ny**](#variable-ny)  <br> |
|  pocketfft\_plan \* | [**plan\_f32**](#variable-plan_f32)  <br> |
|  pocketfft\_plan \* | [**plan\_f64**](#variable-plan_f64)  <br> |
|  int | [**sign**](#variable-sign)  <br> |
|  double \_Complex \* | [**work\_trunc**](#variable-work_trunc)  <br> |












































## Detailed Description


Every field is set by [**dp\_fft2d\_create()**](fft2d__core_8h.md#function-dp_fft2d_create) and immutable afterward, except the lazily allocated `work_trunc`. Allocate with [**dp\_fft2d\_create()**](fft2d__core_8h.md#function-dp_fft2d_create); free with [**dp\_fft2d\_destroy()**](fft2d__core_8h.md#function-dp_fft2d_destroy). 


    
## Public Attributes Documentation




### variable nx 

```C++
size_t dp_fft2d_state_t::nx;
```



Column count. 
 


        

<hr>



### variable ny 

```C++
size_t dp_fft2d_state_t::ny;
```



Row count. 
 


        

<hr>



### variable plan\_f32 

```C++
pocketfft_plan* dp_fft2d_state_t::plan_f32;
```



CF32 2-D plan. 


        

<hr>



### variable plan\_f64 

```C++
pocketfft_plan* dp_fft2d_state_t::plan_f64;
```



CF64 2-D plan. 


        

<hr>



### variable sign 

```C++
int dp_fft2d_state_t::sign;
```



-1 forward, +1 inverse. 


        

<hr>



### variable work\_trunc 

```C++
double _Complex* dp_fft2d_state_t::work_trunc;
```



Scratch for a short `out`. A pocketfft 2-D plan is fixed at ny\*nx and writes the whole surface, so it cannot be pointed at a smaller buffer; a truncating call transforms here and copies the prefix. Sized for the widest element (CF64) so one buffer serves both plans, and allocated lazily  max\_out is ny\*nx on every sized call, which is everything the Python binding does. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/fft2d/fft2d_core.h`

