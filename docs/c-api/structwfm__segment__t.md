

# Struct wfm\_segment\_t



[**ClassList**](annotated.md) **>** [**wfm\_segment\_t**](structwfm__segment__t.md)



_One composer segment: one or more sources summed over the same span, then a trailing off-time gap._ [More...](#detailed-description)

* `#include <wfm_compose.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**fs**](#variable-fs)  <br> |
|  size\_t | [**n\_sources**](#variable-n_sources)  <br> |
|  size\_t | [**num\_samples**](#variable-num_samples)  <br> |
|  size\_t | [**num\_samples\_hi**](#variable-num_samples_hi)  <br> |
|  size\_t | [**off\_samples**](#variable-off_samples)  <br> |
|  size\_t | [**off\_samples\_hi**](#variable-off_samples_hi)  <br> |
|  unsigned | [**ranged**](#variable-ranged)  <br> |
|  [**wfm\_source\_t**](structwfm__source__t.md) \* | [**sources**](#variable-sources)  <br> |












































## Detailed Description


A 1-source segment is byte-identical to driving that source's `synth` directly. `num_samples` is the on-time; `off_samples` is a trailing gap of zeros. Durations in seconds are `round(duration * fs)` — the caller resolves. 


    
## Public Attributes Documentation




### variable fs 

```C++
double wfm_segment_t::fs;
```




<hr>



### variable n\_sources 

```C++
size_t wfm_segment_t::n_sources;
```




<hr>



### variable num\_samples 

```C++
size_t wfm_segment_t::num_samples;
```




<hr>



### variable num\_samples\_hi 

```C++
size_t wfm_segment_t::num_samples_hi;
```




<hr>



### variable off\_samples 

```C++
size_t wfm_segment_t::off_samples;
```




<hr>



### variable off\_samples\_hi 

```C++
size_t wfm_segment_t::off_samples_hi;
```




<hr>



### variable ranged 

```C++
unsigned wfm_segment_t::ranged;
```




<hr>



### variable sources 

```C++
wfm_source_t* wfm_segment_t::sources;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_compose.h`

