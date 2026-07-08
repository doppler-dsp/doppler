

# Struct wfm\_source\_t



[**ClassList**](annotated.md) **>** [**wfm\_source\_t**](structwfm__source__t.md)



_One additive source within a segment: a_ `synth` _config + its level._[More...](#detailed-description)

* `#include <wfm_compose.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t \* | [**bits**](#variable-bits)  <br> |
|  double | [**f\_end**](#variable-f_end)  <br> |
|  double | [**f\_end\_hi**](#variable-f_end_hi)  <br> |
|  double | [**freq**](#variable-freq)  <br> |
|  double | [**freq\_hi**](#variable-freq_hi)  <br> |
|  double | [**level**](#variable-level)  <br> |
|  double | [**level\_hi**](#variable-level_hi)  <br> |
|  int | [**lfsr**](#variable-lfsr)  <br> |
|  int | [**modulation**](#variable-modulation)  <br> |
|  size\_t | [**n\_bits**](#variable-n_bits)  <br> |
|  size\_t | [**n\_symbols**](#variable-n_symbols)  <br> |
|  int | [**pn\_length**](#variable-pn_length)  <br> |
|  uint64\_t | [**pn\_poly**](#variable-pn_poly)  <br> |
|  int | [**pulse**](#variable-pulse)  <br> |
|  unsigned | [**ranged**](#variable-ranged)  <br> |
|  double | [**rrc\_beta**](#variable-rrc_beta)  <br> |
|  int | [**rrc\_span**](#variable-rrc_span)  <br> |
|  uint32\_t | [**seed**](#variable-seed)  <br> |
|  double | [**snr**](#variable-snr)  <br> |
|  double | [**snr\_hi**](#variable-snr_hi)  <br> |
|  int | [**snr\_mode**](#variable-snr_mode)  <br> |
|  int | [**sps**](#variable-sps)  <br> |
|  float \_Complex \* | [**symbols**](#variable-symbols)  <br> |
|  int | [**type**](#variable-type)  <br> |












































## Detailed Description


The nine synth fields mirror `wfm_synth_create()` (minus `fs`, which is the segment's — one receiver, one sample rate). `level` is the source's average power in dBFS (≤0); the segment sums its sources, each scaled by `10^(level/20)`.


Any of `freq`/`snr`/`level`/`f_end` may be a per-repeat uniform draw: set the matching `WFM_RANGE_*` bit in `ranged`, leave the scalar as `lo`, and put `hi` in the `*_hi` companion (see the `ranged` enum). 


    
## Public Attributes Documentation




### variable bits 

```C++
uint8_t* wfm_source_t::bits;
```




<hr>



### variable f\_end 

```C++
double wfm_source_t::f_end;
```




<hr>



### variable f\_end\_hi 

```C++
double wfm_source_t::f_end_hi;
```




<hr>



### variable freq 

```C++
double wfm_source_t::freq;
```




<hr>



### variable freq\_hi 

```C++
double wfm_source_t::freq_hi;
```




<hr>



### variable level 

```C++
double wfm_source_t::level;
```




<hr>



### variable level\_hi 

```C++
double wfm_source_t::level_hi;
```




<hr>



### variable lfsr 

```C++
int wfm_source_t::lfsr;
```




<hr>



### variable modulation 

```C++
int wfm_source_t::modulation;
```




<hr>



### variable n\_bits 

```C++
size_t wfm_source_t::n_bits;
```




<hr>



### variable n\_symbols 

```C++
size_t wfm_source_t::n_symbols;
```




<hr>



### variable pn\_length 

```C++
int wfm_source_t::pn_length;
```




<hr>



### variable pn\_poly 

```C++
uint64_t wfm_source_t::pn_poly;
```




<hr>



### variable pulse 

```C++
int wfm_source_t::pulse;
```




<hr>



### variable ranged 

```C++
unsigned wfm_source_t::ranged;
```




<hr>



### variable rrc\_beta 

```C++
double wfm_source_t::rrc_beta;
```




<hr>



### variable rrc\_span 

```C++
int wfm_source_t::rrc_span;
```




<hr>



### variable seed 

```C++
uint32_t wfm_source_t::seed;
```




<hr>



### variable snr 

```C++
double wfm_source_t::snr;
```




<hr>



### variable snr\_hi 

```C++
double wfm_source_t::snr_hi;
```




<hr>



### variable snr\_mode 

```C++
int wfm_source_t::snr_mode;
```




<hr>



### variable sps 

```C++
int wfm_source_t::sps;
```




<hr>



### variable symbols 

```C++
float _Complex* wfm_source_t::symbols;
```




<hr>



### variable type 

```C++
int wfm_source_t::type;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_compose.h`

