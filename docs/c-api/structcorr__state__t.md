

# Struct corr\_state\_t



[**ClassList**](annotated.md) **>** [**corr\_state\_t**](structcorr__state__t.md)



_1-D FFT correlator state._ [More...](#detailed-description)

* `#include <corr_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float complex \* | [**accum**](#variable-accum)  <br> |
|  size\_t | [**count**](#variable-count)  <br> |
|  size\_t | [**dwell**](#variable-dwell)  <br> |
|  [**fft\_state\_t**](structfft__state__t.md) \* | [**fwd**](#variable-fwd)  <br> |
|  [**fft\_state\_t**](structfft__state__t.md) \* | [**inv**](#variable-inv)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  size\_t | [**n\_out**](#variable-n_out)  <br> |
|  float complex \* | [**ref\_spec**](#variable-ref_spec)  <br> |
|  float complex \* | [**work\_fft**](#variable-work_fft)  <br> |
|  float complex \* | [**work\_pad**](#variable-work_pad)  <br> |












































## Detailed Description


Allocate with [**corr\_create()**](corr__core_8h.md#function-corr_create); never stack-allocate. ref\_spec/work\_fft/accum are each `n` complex floats; work\_pad (`n_out`) exists only on the decoupled-inverse path. 


    
## Public Attributes Documentation




### variable accum 

```C++
float complex* corr_state_t::accum;
```



Coherent product-spectrum accumulator. 


        

<hr>



### variable count 

```C++
size_t corr_state_t::count;
```



Frames accumulated so far (0 … dwell-1). 


        

<hr>



### variable dwell 

```C++
size_t corr_state_t::dwell;
```



Integration depth; dump every dwell calls. 


        

<hr>



### variable fwd 

```C++
fft_state_t* corr_state_t::fwd;
```



Forward plan (sign = -1) at n. 


        

<hr>



### variable inv 

```C++
fft_state_t* corr_state_t::inv;
```



Inverse plan (sign = +1) at n\_out. 


        

<hr>



### variable n 

```C++
size_t corr_state_t::n;
```



FFT / reference length (samples). 


        

<hr>



### variable n\_out 

```C++
size_t corr_state_t::n_out;
```



Output length (== n unless decoupled). 


        

<hr>



### variable ref\_spec 

```C++
float complex* corr_state_t::ref_spec;
```



conj(FFT(ref)), pre-computed at create. 


        

<hr>



### variable work\_fft 

```C++
float complex* corr_state_t::work_fft;
```



Scratch: FFT(in) · ref\_spec (product). 


        

<hr>



### variable work\_pad 

```C++
float complex* corr_state_t::work_pad;
```



Zero-padded product, n\_out (NULL native). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/corr/corr_core.h`

