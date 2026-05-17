

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

------------------------------
The documentation for this class was generated from the following file `native/inc/fft/fft_core.h`

