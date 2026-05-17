

# Struct resamp\_dpmfs\_state\_t



[**ClassList**](annotated.md) **>** [**resamp\_dpmfs\_state\_t**](structresamp__dpmfs__state__t.md)



_Full DPMFS resampler state (not opaque — allocated by \_create)._ [More...](#detailed-description)

* `#include <resamp_dpmfs_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**M**](#variable-m)  <br> |
|  size\_t | [**N**](#variable-n)  <br> |
|  size\_t | [**buf\_cap**](#variable-buf_cap)  <br> |
|  float \* | [**c**](#variable-c)  <br> |
|  float \_Complex \* | [**delay\_buf**](#variable-delay_buf)  <br> |
|  size\_t | [**delay\_cap**](#variable-delay_cap)  <br> |
|  size\_t | [**delay\_head**](#variable-delay_head)  <br> |
|  size\_t | [**delay\_mask**](#variable-delay_mask)  <br> |
|  float \_Complex \* | [**iad**](#variable-iad)  <br> |
|  uint32\_t | [**phase**](#variable-phase)  <br> |
|  uint32\_t | [**phase\_inc**](#variable-phase_inc)  <br> |
|  double | [**rate**](#variable-rate)  <br> |
|  float \_Complex \* | [**tfd**](#variable-tfd)  <br> |
|  int | [**upsample**](#variable-upsample)  <br> |












































## Detailed Description


The struct is exposed in the header so the extension can embed it in its PyObject without a heap indirection. Treat all fields as private; only the public API functions are supported callers.



## Public Attributes Documentation




### variable M

```C++
size_t resamp_dpmfs_state_t::M;
```




<hr>



### variable N

```C++
size_t resamp_dpmfs_state_t::N;
```




<hr>



### variable buf\_cap

```C++
size_t resamp_dpmfs_state_t::buf_cap;
```




<hr>



### variable c

```C++
float* resamp_dpmfs_state_t::c[2];
```




<hr>



### variable delay\_buf

```C++
float _Complex* resamp_dpmfs_state_t::delay_buf;
```




<hr>



### variable delay\_cap

```C++
size_t resamp_dpmfs_state_t::delay_cap;
```




<hr>



### variable delay\_head

```C++
size_t resamp_dpmfs_state_t::delay_head;
```




<hr>



### variable delay\_mask

```C++
size_t resamp_dpmfs_state_t::delay_mask;
```




<hr>



### variable iad

```C++
float _Complex* resamp_dpmfs_state_t::iad;
```




<hr>



### variable phase

```C++
uint32_t resamp_dpmfs_state_t::phase;
```




<hr>



### variable phase\_inc

```C++
uint32_t resamp_dpmfs_state_t::phase_inc;
```




<hr>



### variable rate

```C++
double resamp_dpmfs_state_t::rate;
```




<hr>



### variable tfd

```C++
float _Complex* resamp_dpmfs_state_t::tfd;
```




<hr>



### variable upsample

```C++
int resamp_dpmfs_state_t::upsample;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/resamp_dpmfs/resamp_dpmfs_core.h`
