

# Struct corr2d\_state\_t



[**ClassList**](annotated.md) **>** [**corr2d\_state\_t**](structcorr2d__state__t.md)



_2-D FFT correlator state._ [More...](#detailed-description)

* `#include <corr2d_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float complex \* | [**accum**](#variable-accum)  <br> |
|  size\_t | [**count**](#variable-count)  <br> |
|  size\_t | [**dwell**](#variable-dwell)  <br> |
|  [**fft2d\_state\_t**](structfft2d__state__t.md) \* | [**fwd**](#variable-fwd)  <br> |
|  [**fft2d\_state\_t**](structfft2d__state__t.md) \* | [**inv**](#variable-inv)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**nx**](#variable-nx)  <br> |
|  size\_t | [**ny**](#variable-ny)  <br> |
|  float complex \* | [**ref\_spec**](#variable-ref_spec)  <br> |
|  float complex \* | [**work\_fft**](#variable-work_fft)  <br> |
|  float complex \* | [**work\_ifft**](#variable-work_ifft)  <br> |












































## Detailed Description


Allocate with [**corr2d\_create()**](corr2d__core_8h.md#function-corr2d_create); never stack-allocate. All heap buffers are `ny * nx` complex floats stored in row-major order. 


    
## Public Attributes Documentation




### variable accum 

```C++
float complex* corr2d_state_t::accum;
```



Coherent integration accumulator. 


        

<hr>



### variable count 

```C++
size_t corr2d_state_t::count;
```



Frames accumulated (0 … dwell-1). 


        

<hr>



### variable dwell 

```C++
size_t corr2d_state_t::dwell;
```



Integration depth. 


        

<hr>



### variable fwd 

```C++
fft2d_state_t* corr2d_state_t::fwd;
```



Forward 2-D plan (sign = -1). 


        

<hr>



### variable inv 

```C++
fft2d_state_t* corr2d_state_t::inv;
```



Inverse 2-D plan (sign = +1). 


        

<hr>



### variable n 

```C++
size_t corr2d_state_t::n;
```



ny \* nx — total element count. 


        

<hr>



### variable nx 

```C++
size_t corr2d_state_t::nx;
```



Column count. 


        

<hr>



### variable ny 

```C++
size_t corr2d_state_t::ny;
```



Row count. 


        

<hr>



### variable ref\_spec 

```C++
float complex* corr2d_state_t::ref_spec;
```



conj(FFT2(ref)), pre-computed. 


        

<hr>



### variable work\_fft 

```C++
float complex* corr2d_state_t::work_fft;
```



Scratch: FFT2(in) output. 


        

<hr>



### variable work\_ifft 

```C++
float complex* corr2d_state_t::work_ifft;
```



Scratch: IFFT2 output before accumulate. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/corr2d/corr2d_core.h`

