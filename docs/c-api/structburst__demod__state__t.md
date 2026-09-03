

# Struct burst\_demod\_state\_t



[**ClassList**](annotated.md) **>** [**burst\_demod\_state\_t**](structburst__demod__state__t.md)



_BurstDemod state. Allocate with_ [_**burst\_demod\_create()**_](burst__demod__core_8h.md#function-burst_demod_create) _._

* `#include <burst_demod_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t \* | [**acq\_code**](#variable-acq_code)  <br> |
|  size\_t | [**acq\_reps**](#variable-acq_reps)  <br> |
|  size\_t | [**acq\_sf**](#variable-acq_sf)  <br> |
|  double | [**carrier\_hz**](#variable-carrier_hz)  <br> |
|  double | [**chip\_rate**](#variable-chip_rate)  <br> |
|  uint8\_t \* | [**data\_code**](#variable-data_code)  <br> |
|  size\_t | [**data\_sf**](#variable-data_sf)  <br> |
|  double | [**est\_freq\_hz**](#variable-est_freq_hz)  <br> |
|  double | [**est\_n0**](#variable-est_n0)  <br> |
|  double | [**est\_rate\_hz**](#variable-est_rate_hz)  <br> |
|  size\_t | [**est\_segments**](#variable-est_segments)  <br> |
|  double | [**est\_snr\_db**](#variable-est_snr_db)  <br> |
|  double | [**f0\_prior**](#variable-f0_prior)  <br> |
|  size\_t | [**frame\_offset**](#variable-frame_offset)  <br> |
|  size\_t | [**frame\_syms**](#variable-frame_syms)  <br> |
|  float \* | [**llr**](#variable-llr)  <br> |
|  double | [**max\_rate**](#variable-max_rate)  <br> |
|  size\_t | [**n\_llr**](#variable-n_llr)  <br> |
|  size\_t | [**n\_part**](#variable-n_part)  <br> |
|  size\_t | [**n\_sym**](#variable-n_sym)  <br> |
|  size\_t | [**n\_symbols**](#variable-n_symbols)  <br> |
|  float \_Complex \* | [**part**](#variable-part)  <br> |
|  [**ppe\_state\_t**](structppe__state__t.md) \* | [**ppe**](#variable-ppe)  <br> |
|  size\_t | [**spc**](#variable-spc)  <br> |
|  size\_t | [**start**](#variable-start)  <br> |
|  float \_Complex \* | [**sym**](#variable-sym)  <br> |
|  int8\_t \* | [**sync**](#variable-sync)  <br> |
|  size\_t | [**sync\_len**](#variable-sync_len)  <br> |












































## Public Attributes Documentation




### variable acq\_code 

```C++
uint8_t* burst_demod_state_t::acq_code;
```



owned acq preamble code (0/1), length acq\_sf. 
 


        

<hr>



### variable acq\_reps 

```C++
size_t burst_demod_state_t::acq_reps;
```



acq preamble repetitions. 
 


        

<hr>



### variable acq\_sf 

```C++
size_t burst_demod_state_t::acq_sf;
```



acq code length (chips). 
 


        

<hr>



### variable carrier\_hz 

```C++
double burst_demod_state_t::carrier_hz;
```



RF carrier (Hz) for code-Doppler; 0 = ignore. 


        

<hr>



### variable chip\_rate 

```C++
double burst_demod_state_t::chip_rate;
```



chip rate (Hz). 
 


        

<hr>



### variable data\_code 

```C++
uint8_t* burst_demod_state_t::data_code;
```



owned data spreading code (0/1), length data\_sf. 


        

<hr>



### variable data\_sf 

```C++
size_t burst_demod_state_t::data_sf;
```



data spreading factor (chips/symbol). 
 


        

<hr>



### variable est\_freq\_hz 

```C++
double burst_demod_state_t::est_freq_hz;
```



estimated residual Doppler (Hz). 
 


        

<hr>



### variable est\_n0 

```C++
double burst_demod_state_t::est_n0;
```



Noise power the LLRs are scaled by, referred to unit symbol amplitude. Published so a caller can undo the scaling, or compare bursts by it. 
 


        

<hr>



### variable est\_rate\_hz 

```C++
double burst_demod_state_t::est_rate_hz;
```



estimated Doppler rate (Hz/s). 
 


        

<hr>



### variable est\_segments 

```C++
size_t burst_demod_state_t::est_segments;
```



partials per acq period for the estimate. 
 


        

<hr>



### variable est\_snr\_db 

```C++
double burst_demod_state_t::est_snr_db;
```



estimator confidence (dB). 
 


        

<hr>



### variable f0\_prior 

```C++
double burst_demod_state_t::f0_prior;
```



coarse Doppler prior (cycles/sample). 
 


        

<hr>



### variable frame\_offset 

```C++
size_t burst_demod_state_t::frame_offset;
```



symbol offset of the sync word. 
 


        

<hr>



### variable frame\_syms 

```C++
size_t burst_demod_state_t::frame_syms;
```



symbols the frame occupies AFTER the sync word — a number the caller states. What they MEAN is the frame description's business, one layer up. 
 


        

<hr>



### variable llr 

```C++
float* burst_demod_state_t::llr;
```



The frame's soft bits, `mpsk_soft_demap`'s convention: positive means bit 0, so `L < 0` is the hard decision demod() returned. Valid until the next demod(); n\_llr of them. 
 


        

<hr>



### variable max\_rate 

```C++
double burst_demod_state_t::max_rate;
```



chirp-rate search half-span (cycles/sample^2). 


        

<hr>



### variable n\_llr 

```C++
size_t burst_demod_state_t::n_llr;
```



LLRs the last demod() wrote (the frame's length). 
 


        

<hr>



### variable n\_part 

```C++
size_t burst_demod_state_t::n_part;
```




<hr>



### variable n\_sym 

```C++
size_t burst_demod_state_t::n_sym;
```



Symbols the last demod() wrote (the frame's length). 


        

<hr>



### variable n\_symbols 

```C++
size_t burst_demod_state_t::n_symbols;
```



despread data symbols produced. 
 


        

<hr>



### variable part 

```C++
float _Complex* burst_demod_state_t::part;
```



preamble partials scratch (acq\_reps\*est\_seg). 


        

<hr>



### variable ppe 

```C++
ppe_state_t* burst_demod_state_t::ppe;
```



feedforward (rate x freq) estimator. 
 


        

<hr>



### variable spc 

```C++
size_t burst_demod_state_t::spc;
```



samples per chip. 
 


        

<hr>



### variable start 

```C++
size_t burst_demod_state_t::start;
```



preamble start sample in the burst. 
 


        

<hr>



### variable sym 

```C++
float _Complex* burst_demod_state_t::sym;
```



The frame's DEROTATED, unit-normalised complex symbols  the constellation the LLRs are the real part of. Built either way to compute the projection and the noise estimate, and freed unread until doppler#1087: for BPSK a residual phase error and a genuine amplitude loss are indistinguishable in every real-only statistic (a rotation scales Re by cos(phi) without adding noise), so \|LLR\|, its spread and the BER all agree while only the quadrature separates them. Valid until the next demod(); n\_sym of them. 
 


        

<hr>



### variable sync 

```C++
int8_t* burst_demod_state_t::sync;
```



owned sync word as +/-1, length sync\_len — the correlation template, and the only thing this object knows about the frame's CONTENT. 
 


        

<hr>



### variable sync\_len 

```C++
size_t burst_demod_state_t::sync_len;
```



sync word length (symbols). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/burst_demod/burst_demod_core.h`

