

# Struct wfm\_source\_t



[**ClassList**](annotated.md) **>** [**wfm\_source\_t**](structwfm__source__t.md)



_One additive source within a segment: a_ `synth` _config + its level._[More...](#detailed-description)

* `#include <wfm_compose.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**acq\_code**](#variable-acq_code)  <br> |
|  size\_t | [**acq\_reps**](#variable-acq_reps)  <br> |
|  int | [**attach\_asm**](#variable-attach_asm)  <br> |
|  int | [**background**](#variable-background)  <br> |
|  double | [**carrier\_hz**](#variable-carrier_hz)  <br> |
|  int | [**convolutional**](#variable-convolutional)  <br> |
|  int | [**crc**](#variable-crc)  <br> |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**data\_code**](#variable-data_code)  <br> |
|  double | [**doppler**](#variable-doppler)  <br> |
|  double | [**doppler\_hi**](#variable-doppler_hi)  <br> |
|  int | [**doppler\_lifetime**](#variable-doppler_lifetime)  <br> |
|  double | [**doppler\_rate**](#variable-doppler_rate)  <br> |
|  double | [**doppler\_rate\_hi**](#variable-doppler_rate_hi)  <br> |
|  int | [**dsss\_code\_only**](#variable-dsss_code_only)  <br> |
|  double | [**f\_end**](#variable-f_end)  <br> |
|  double | [**f\_end\_hi**](#variable-f_end_hi)  <br> |
|  double | [**freq**](#variable-freq)  <br> |
|  double | [**freq\_hi**](#variable-freq_hi)  <br> |
|  unsigned | [**interleave\_depth**](#variable-interleave_depth)  <br> |
|  unsigned | [**interleave\_unit\_bits**](#variable-interleave_unit_bits)  <br> |
|  double | [**level**](#variable-level)  <br> |
|  double | [**level\_hi**](#variable-level_hi)  <br> |
|  int | [**lfsr**](#variable-lfsr)  <br> |
|  int | [**modulation**](#variable-modulation)  <br> |
|  size\_t | [**n\_symbols**](#variable-n_symbols)  <br> |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**payload**](#variable-payload)  <br> |
|  int | [**pn\_length**](#variable-pn_length)  <br> |
|  uint64\_t | [**pn\_poly**](#variable-pn_poly)  <br> |
|  int | [**pulse**](#variable-pulse)  <br> |
|  int | [**randomise**](#variable-randomise)  <br> |
|  unsigned | [**ranged**](#variable-ranged)  <br> |
|  double | [**rrc\_beta**](#variable-rrc_beta)  <br> |
|  int | [**rrc\_span**](#variable-rrc_span)  <br> |
|  unsigned | [**rs\_depth**](#variable-rs_depth)  <br> |
|  uint32\_t | [**seed**](#variable-seed)  <br> |
|  double | [**snr**](#variable-snr)  <br> |
|  double | [**snr\_hi**](#variable-snr_hi)  <br> |
|  int | [**snr\_mode**](#variable-snr_mode)  <br> |
|  int | [**sps**](#variable-sps)  <br> |
|  double | [**symbol\_rate**](#variable-symbol_rate)  <br> |
|  float \_Complex \* | [**symbols**](#variable-symbols)  <br> |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**sync**](#variable-sync)  <br> |
|  int | [**type**](#variable-type)  <br> |












































## Detailed Description


The nine synth fields mirror `wfm_synth_create()` (minus `fs`, which is the segment's — one receiver, one sample rate). `level` is the source's average power in dBFS (≤0); the segment sums its sources, each scaled by `10^(level/20)`.


Any of `freq`/`snr`/`level`/`f_end` may be a per-repeat uniform draw: set the matching `WFM_RANGE_*` bit in `ranged`, leave the scalar as `lo`, and put `hi` in the `*_hi` companion (see the `ranged` enum). 


    
## Public Attributes Documentation




### variable acq\_code 

```C++
wfm_seq_t wfm_source_t::acq_code;
```




<hr>



### variable acq\_reps 

```C++
size_t wfm_source_t::acq_reps;
```




<hr>



### variable attach\_asm 

```C++
int wfm_source_t::attach_asm;
```




<hr>



### variable background 

```C++
int wfm_source_t::background;
```




<hr>



### variable carrier\_hz 

```C++
double wfm_source_t::carrier_hz;
```




<hr>



### variable convolutional 

```C++
int wfm_source_t::convolutional;
```




<hr>



### variable crc 

```C++
int wfm_source_t::crc;
```




<hr>



### variable data\_code 

```C++
wfm_seq_t wfm_source_t::data_code;
```




<hr>



### variable doppler 

```C++
double wfm_source_t::doppler;
```




<hr>



### variable doppler\_hi 

```C++
double wfm_source_t::doppler_hi;
```




<hr>



### variable doppler\_lifetime 

```C++
int wfm_source_t::doppler_lifetime;
```




<hr>



### variable doppler\_rate 

```C++
double wfm_source_t::doppler_rate;
```




<hr>



### variable doppler\_rate\_hi 

```C++
double wfm_source_t::doppler_rate_hi;
```




<hr>



### variable dsss\_code\_only 

```C++
int wfm_source_t::dsss_code_only;
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



### variable interleave\_depth 

```C++
unsigned wfm_source_t::interleave_depth;
```




<hr>



### variable interleave\_unit\_bits 

```C++
unsigned wfm_source_t::interleave_unit_bits;
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



### variable n\_symbols 

```C++
size_t wfm_source_t::n_symbols;
```




<hr>



### variable payload 

```C++
wfm_seq_t wfm_source_t::payload;
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



### variable randomise 

```C++
int wfm_source_t::randomise;
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



### variable rs\_depth 

```C++
unsigned wfm_source_t::rs_depth;
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



### variable symbol\_rate 

```C++
double wfm_source_t::symbol_rate;
```




<hr>



### variable symbols 

```C++
float _Complex* wfm_source_t::symbols;
```




<hr>



### variable sync 

```C++
wfm_seq_t wfm_source_t::sync;
```




<hr>



### variable type 

```C++
int wfm_source_t::type;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_compose.h`

