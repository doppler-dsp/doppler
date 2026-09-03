

# Struct fft2d\_state\_t



[**ClassList**](annotated.md) **>** [**fft2d\_state\_t**](structfft2d__state__t.md)





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












































## Public Attributes Documentation




### variable nx 

```C++
size_t fft2d_state_t::nx;
```



Column count. 
 


        

<hr>



### variable ny 

```C++
size_t fft2d_state_t::ny;
```



Row count. 
 


        

<hr>



### variable plan\_f32 

```C++
pocketfft_plan* fft2d_state_t::plan_f32;
```



CF32 2-D plan. 


        

<hr>



### variable plan\_f64 

```C++
pocketfft_plan* fft2d_state_t::plan_f64;
```



CF64 2-D plan. 


        

<hr>



### variable sign 

```C++
int fft2d_state_t::sign;
```



-1 forward, +1 inverse. 


        

<hr>



### variable work\_trunc 

```C++
double _Complex* fft2d_state_t::work_trunc;
```



Scratch for a short `out`. A pocketfft 2-D plan is fixed at ny\*nx and writes the whole surface, so it cannot be pointed at a smaller buffer; a truncating call transforms here and copies the prefix. Sized for the widest element (CF64) so one buffer serves both plans, and allocated lazily  max\_out is ny\*nx on every sized call, which is everything the Python binding does. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/fft2d/fft2d_core.h`

