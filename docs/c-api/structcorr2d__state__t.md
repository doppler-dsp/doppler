

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
|  size\_t | [**n\_out**](#variable-n_out)  <br> |
|  size\_t | [**nx**](#variable-nx)  <br> |
|  size\_t | [**nx\_out**](#variable-nx_out)  <br> |
|  size\_t | [**ny**](#variable-ny)  <br> |
|  size\_t | [**ny\_out**](#variable-ny_out)  <br> |
|  float complex \* | [**ref\_spec**](#variable-ref_spec)  <br> |
|  float complex \* | [**work\_fft**](#variable-work_fft)  <br> |
|  float complex \* | [**work\_pad**](#variable-work_pad)  <br> |
|  float complex \* | [**zcol**](#variable-zcol)  <br> |
|  float complex \* | [**zcolout**](#variable-zcolout)  <br> |
|  float complex \* | [**ztmp**](#variable-ztmp)  <br> |












































## Detailed Description


Allocate with [**corr2d\_create()**](corr2d__core_8h.md#function-corr2d_create); never stack-allocate. All heap buffers are `ny * nx` complex floats stored in row-major order. 


    
## Public Attributes Documentation




### variable accum 

```C++
float complex* corr2d_state_t::accum;
```



Coherent product-spectrum accumulator. 


        

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



Forward 2-D plan (sign = -1) at (ny, nx). 


        

<hr>



### variable inv 

```C++
fft2d_state_t* corr2d_state_t::inv;
```



Inverse 2-D plan (sign = +1) at (ny\_out,…). 


        

<hr>



### variable n 

```C++
size_t corr2d_state_t::n;
```



ny \* nx — total element count. 


        

<hr>



### variable n\_out 

```C++
size_t corr2d_state_t::n_out;
```



ny\_out \* nx\_out — output element count. 


        

<hr>



### variable nx 

```C++
size_t corr2d_state_t::nx;
```



Column count. 


        

<hr>



### variable nx\_out 

```C++
size_t corr2d_state_t::nx_out;
```



Output columns (== nx unless decoupled). 


        

<hr>



### variable ny 

```C++
size_t corr2d_state_t::ny;
```



Row count. 


        

<hr>



### variable ny\_out 

```C++
size_t corr2d_state_t::ny_out;
```



Output rows (== ny unless decoupled). 


        

<hr>



### variable ref\_spec 

```C++
float complex* corr2d_state_t::ref_spec;
```



conj(FFT2(ref)), pre-computed. (ny, nx) 


        

<hr>



### variable work\_fft 

```C++
float complex* corr2d_state_t::work_fft;
```



Scratch: FFT2(in) · ref\_spec (product). 


        

<hr>



### variable work\_pad 

```C++
float complex* corr2d_state_t::work_pad;
```



Zero-padded product, (ny\_out, nx\_out). 


        

<hr>



### variable zcol 

```C++
float complex* corr2d_state_t::zcol;
```



Column gather scratch, (ny). 


        

<hr>



### variable zcolout 

```C++
float complex* corr2d_state_t::zcolout;
```



Column-padded scratch, (ny\_out). 


        

<hr>



### variable ztmp 

```C++
float complex* corr2d_state_t::ztmp;
```



Row-padded intermediate, (ny, nx\_out). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/corr2d/corr2d_core.h`

