

# Struct detector2d\_state\_t



[**ClassList**](annotated.md) **>** [**detector2d\_state\_t**](structdetector2d__state__t.md)



_2-D signal detector state._ [More...](#detailed-description)

* `#include <detector2d_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**\_last\_corr\_valid**](#variable-_last_corr_valid)  <br> |
|  [**corr2d\_state\_t**](structcorr2d__state__t.md) \* | [**corr**](#variable-corr)  <br> |
|  float \* | [**mag\_buf**](#variable-mag_buf)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  float | [**noise\_est**](#variable-noise_est)  <br> |
|  size\_t | [**noise\_hi**](#variable-noise_hi)  <br> |
|  size\_t | [**noise\_lo**](#variable-noise_lo)  <br> |
|  [**det\_noise\_mode\_t**](detector2d__core_8h.md#enum-det_noise_mode_t) | [**noise\_mode**](#variable-noise_mode)  <br> |
|  float \* | [**noise\_scratch**](#variable-noise_scratch)  <br> |
|  size\_t | [**nx**](#variable-nx)  <br> |
|  size\_t | [**ny**](#variable-ny)  <br> |
|  float complex \* | [**out\_buf**](#variable-out_buf)  <br> |
|  size\_t | [**peak\_col**](#variable-peak_col)  <br> |
|  float | [**peak\_mag**](#variable-peak_mag)  <br> |
|  size\_t | [**peak\_row**](#variable-peak_row)  <br> |
|  dp\_f32\_t \* | [**ring**](#variable-ring)  <br> |
|  size\_t | [**ring\_cap**](#variable-ring_cap)  <br> |
|  float | [**test\_stat**](#variable-test_stat)  <br> |
|  float | [**threshold**](#variable-threshold)  <br> |












































## Detailed Description


Allocate with [**detector2d\_create()**](detector2d__core_8h.md#function-detector2d_create); never stack-allocate. 


    
## Public Attributes Documentation




### variable \_last\_corr\_valid 

```C++
int detector2d_state_t::_last_corr_valid;
```



1 after the first dump, else 0. 


        

<hr>



### variable corr 

```C++
corr2d_state_t* detector2d_state_t::corr;
```



2-D FFT correlator + int-dump engine. 


        

<hr>



### variable mag\_buf 

```C++
float* detector2d_state_t::mag_buf;
```



\|out\_buf&#91;k&#93;\|, ny\*nx floats. 


        

<hr>



### variable n 

```C++
size_t detector2d_state_t::n;
```



ny \* nx — total frame length. 


        

<hr>



### variable noise\_est 

```C++
float detector2d_state_t::noise_est;
```




<hr>



### variable noise\_hi 

```C++
size_t detector2d_state_t::noise_hi;
```



Noise bin range upper bound (inclusive). 


        

<hr>



### variable noise\_lo 

```C++
size_t detector2d_state_t::noise_lo;
```



Noise bin range lower bound (inclusive). 


        

<hr>



### variable noise\_mode 

```C++
det_noise_mode_t detector2d_state_t::noise_mode;
```




<hr>



### variable noise\_scratch 

```C++
float* detector2d_state_t::noise_scratch;
```



Scratch for median sort. 


        

<hr>



### variable nx 

```C++
size_t detector2d_state_t::nx;
```



Number of columns. 


        

<hr>



### variable ny 

```C++
size_t detector2d_state_t::ny;
```



Number of rows. 


        

<hr>



### variable out\_buf 

```C++
float complex* detector2d_state_t::out_buf;
```



Corr2D output (ny\*nx complex samples). 


        

<hr>



### variable peak\_col 

```C++
size_t detector2d_state_t::peak_col;
```




<hr>



### variable peak\_mag 

```C++
float detector2d_state_t::peak_mag;
```




<hr>



### variable peak\_row 

```C++
size_t detector2d_state_t::peak_row;
```




<hr>



### variable ring 

```C++
dp_f32_t* detector2d_state_t::ring;
```



Double-mapped ring buffer (auto-sized). 


        

<hr>



### variable ring\_cap 

```C++
size_t detector2d_state_t::ring_cap;
```



Ring buffer capacity in complex samples. 


        

<hr>



### variable test\_stat 

```C++
float detector2d_state_t::test_stat;
```




<hr>



### variable threshold 

```C++
float detector2d_state_t::threshold;
```



0 = always fire; &gt;0 = gate on test\_stat. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/detector2d/detector2d_core.h`

