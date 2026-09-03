

# Struct fft\_state\_t



[**ClassList**](annotated.md) **>** [**fft\_state\_t**](structfft__state__t.md)





* `#include <fft_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**n**](#variable-n)  <br> |
|  pocketfft\_plan \* | [**plan\_f32**](#variable-plan_f32)  <br> |
|  pocketfft\_plan \* | [**plan\_f64**](#variable-plan_f64)  <br> |
|  int | [**sign**](#variable-sign)  <br> |
|  double \_Complex \* | [**work\_trunc**](#variable-work_trunc)  <br> |












































## Public Attributes Documentation




### variable n 

```C++
size_t fft_state_t::n;
```



Transform length (samples). 


        

<hr>



### variable plan\_f32 

```C++
pocketfft_plan* fft_state_t::plan_f32;
```



CF32 1-D plan. 


        

<hr>



### variable plan\_f64 

```C++
pocketfft_plan* fft_state_t::plan_f64;
```



CF64 1-D plan. 


        

<hr>



### variable sign 

```C++
int fft_state_t::sign;
```



-1 forward, +1 inverse. 
 


        

<hr>



### variable work\_trunc 

```C++
double _Complex* fft_state_t::work_trunc;
```



Scratch for a short `out`. A pocketfft plan is fixed at n and writes all n bins, so it cannot be pointed at a smaller buffer; a truncating call transforms here and copies the prefix. Sized for the widest element (CF64) so one buffer serves both plans. Allocated lazily  max\_out is n on every sized call, which is everything the Python binding and every in-tree caller does. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/fft/fft_core.h`

