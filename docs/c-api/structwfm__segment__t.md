

# Struct wfm\_segment\_t



[**ClassList**](annotated.md) **>** [**wfm\_segment\_t**](structwfm__segment__t.md)



_One composer segment: a_ `synth` _config + on/off sample counts._[More...](#detailed-description)

* `#include <wfm_compose.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**freq**](#variable-freq)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  int | [**lfsr**](#variable-lfsr)  <br> |
|  size\_t | [**num\_samples**](#variable-num_samples)  <br> |
|  size\_t | [**off\_samples**](#variable-off_samples)  <br> |
|  int | [**pn\_length**](#variable-pn_length)  <br> |
|  uint64\_t | [**pn\_poly**](#variable-pn_poly)  <br> |
|  uint32\_t | [**seed**](#variable-seed)  <br> |
|  double | [**snr**](#variable-snr)  <br> |
|  int | [**snr\_mode**](#variable-snr_mode)  <br> |
|  int | [**sps**](#variable-sps)  <br> |
|  int | [**type**](#variable-type)  <br> |












































## Detailed Description


The nine synth fields mirror `synth_create()` exactly. `num_samples` is the on-time (samples emitted from the synth); `off_samples` is a trailing gap of zeros inserted after the segment (off-time). Durations in seconds are `round(duration * fs)` — the caller resolves them. 


    
## Public Attributes Documentation




### variable freq 

```C++
double wfm_segment_t::freq;
```




<hr>



### variable fs 

```C++
double wfm_segment_t::fs;
```




<hr>



### variable lfsr 

```C++
int wfm_segment_t::lfsr;
```




<hr>



### variable num\_samples 

```C++
size_t wfm_segment_t::num_samples;
```




<hr>



### variable off\_samples 

```C++
size_t wfm_segment_t::off_samples;
```




<hr>



### variable pn\_length 

```C++
int wfm_segment_t::pn_length;
```




<hr>



### variable pn\_poly 

```C++
uint64_t wfm_segment_t::pn_poly;
```




<hr>



### variable seed 

```C++
uint32_t wfm_segment_t::seed;
```




<hr>



### variable snr 

```C++
double wfm_segment_t::snr;
```




<hr>



### variable snr\_mode 

```C++
int wfm_segment_t::snr_mode;
```




<hr>



### variable sps 

```C++
int wfm_segment_t::sps;
```




<hr>



### variable type 

```C++
int wfm_segment_t::type;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfmgen/wfm_compose.h`

