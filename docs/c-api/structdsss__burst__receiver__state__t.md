

# Struct dsss\_burst\_receiver\_state\_t



[**ClassList**](annotated.md) **>** [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md)



_DsssBurstReceiver state._ [More...](#detailed-description)

* `#include <dsss_burst_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t \* | [**acq\_code**](#variable-acq_code)  <br> |
|  size\_t | [**acq\_code\_len**](#variable-acq_code_len)  <br> |
|  size\_t | [**burst\_len**](#variable-burst_len)  <br> |
|  [**burst\_capture\_state\_t**](structburst__capture__state__t.md) \* | [**cap**](#variable-cap)  <br> |
|  double | [**chip\_rate**](#variable-chip_rate)  <br> |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  size\_t | [**code\_period**](#variable-code_period)  <br> |
|  uint8\_t \* | [**data\_code**](#variable-data_code)  <br> |
|  size\_t | [**data\_code\_len**](#variable-data_code_len)  <br> |
|  [**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* | [**demod**](#variable-demod)  <br> |
|  double | [**doppler\_hz\_est**](#variable-doppler_hz_est)  <br> |
|  double | [**doppler\_res\_hz**](#variable-doppler_res_hz)  <br> |
|  double | [**est\_freq\_hz**](#variable-est_freq_hz)  <br> |
|  double | [**est\_rate\_hz**](#variable-est_rate_hz)  <br> |
|  double | [**est\_snr\_db**](#variable-est_snr_db)  <br> |
|  [**dsss\_br\_event\_t**](structdsss__br__event__t.md) \* | [**ev**](#variable-ev)  <br> |
|  size\_t | [**ev\_cap**](#variable-ev_cap)  <br> |
|  size\_t | [**ev\_len**](#variable-ev_len)  <br> |
|  size\_t | [**frame\_bits**](#variable-frame_bits)  <br> |
|  size\_t | [**frame\_syms**](#variable-frame_syms)  <br> |
|  float \* | [**llr**](#variable-llr)  <br> |
|  size\_t | [**llr\_cap**](#variable-llr_cap)  <br> |
|  size\_t | [**llr\_len**](#variable-llr_len)  <br> |
|  uint64\_t | [**n\_bursts**](#variable-n_bursts)  <br> |
|  uint64\_t | [**preamble\_start**](#variable-preamble_start)  <br> |
|  double | [**refine\_margin**](#variable-refine_margin)  <br> |
|  size\_t | [**reps**](#variable-reps)  <br> |
|  size\_t | [**spc**](#variable-spc)  <br> |
|  uint8\_t \* | [**sync**](#variable-sync)  <br> |
|  size\_t | [**sync\_len**](#variable-sync_len)  <br> |












































## Detailed Description


Allocate with [**dsss\_burst\_receiver\_create()**](dsss__burst__receiver__core_8h.md#function-dsss_burst_receiver_create). 


    
## Public Attributes Documentation




### variable acq\_code 

```C++
uint8_t* dsss_burst_receiver_state_t::acq_code;
```



Preamble code, owned copy. 
 


        

<hr>



### variable acq\_code\_len 

```C++
size_t dsss_burst_receiver_state_t::acq_code_len;
```



Preamble code length, chips. 
 


        

<hr>



### variable burst\_len 

```C++
size_t dsss_burst_receiver_state_t::burst_len;
```



Preamble + spread frame, in samples. 
 


        

<hr>



### variable cap 

```C++
burst_capture_state_t* dsss_burst_receiver_state_t::cap;
```



Search, refine, retain, emit. Owns the acquisition engine, the history ring and the claim rule  everything about FINDING a burst. This object owns what to DO with one. 
 


        

<hr>



### variable chip\_rate 

```C++
double dsss_burst_receiver_state_t::chip_rate;
```



Chip rate, Hz. 
 


        

<hr>



### variable cn0\_dbhz\_est 

```C++
double dsss_burst_receiver_state_t::cn0_dbhz_est;
```



C/N0 lower bound, dB-Hz (saturating). 
 


        

<hr>



### variable code\_period 

```C++
size_t dsss_burst_receiver_state_t::code_period;
```



One preamble repetition, in SAMPLES. The modulus acq's code\_phase is a residue of, so every epoch ambiguity in this object is stated against it. 
 


        

<hr>



### variable data\_code 

```C++
uint8_t* dsss_burst_receiver_state_t::data_code;
```



Payload spreading code, owned copy. 
 


        

<hr>



### variable data\_code\_len 

```C++
size_t dsss_burst_receiver_state_t::data_code_len;
```



Data code length, chips. 
 


        

<hr>



### variable demod 

```C++
burst_demod_state_t* dsss_burst_receiver_state_t::demod;
```



Demod stage, re-seeded per burst. 
 


        

<hr>



### variable doppler\_hz\_est 

```C++
double dsss_burst_receiver_state_t::doppler_hz_est;
```



Signed coarse Doppler, Hz. 
 


        

<hr>



### variable doppler\_res\_hz 

```C++
double dsss_burst_receiver_state_t::doppler_res_hz;
```



Width of that estimate. 
 


        

<hr>



### variable est\_freq\_hz 

```C++
double dsss_burst_receiver_state_t::est_freq_hz;
```



Demod's own residual estimate, Hz. 
 


        

<hr>



### variable est\_rate\_hz 

```C++
double dsss_burst_receiver_state_t::est_rate_hz;
```



Demod's own chirp-rate estimate. 
 


        

<hr>



### variable est\_snr\_db 

```C++
double dsss_burst_receiver_state_t::est_snr_db;
```



Demod's own post-decode SNR estimate. 
 


        

<hr>



### variable ev 

```C++
dsss_br_event_t* dsss_burst_receiver_state_t::ev;
```



One record per burst returned. 
 


        

<hr>



### variable ev\_cap 

```C++
size_t dsss_burst_receiver_state_t::ev_cap;
```



Allocated records. 
 


        

<hr>



### variable ev\_len 

```C++
size_t dsss_burst_receiver_state_t::ev_len;
```



Records the last push() wrote. 
 


        

<hr>



### variable frame\_bits 

```C++
size_t dsss_burst_receiver_state_t::frame_bits;
```



The frame's length, from the description  the stride of a row in `llr`. 
 


        

<hr>



### variable frame\_syms 

```C++
size_t dsss_burst_receiver_state_t::frame_syms;
```



Symbols the frame occupies after the sync word, and so bits per burst out of push(). What they MEAN is a frame description's business, one layer up (doppler#1022). 
 


        

<hr>



### variable llr 

```C++
float* dsss_burst_receiver_state_t::llr;
```



The soft bits of every burst the last push returned, concatenated: burst i starts at i\*frame\_bits. Scratch, like `ev`  it describes one call and is never serialized. 
 


        

<hr>



### variable llr\_cap 

```C++
size_t dsss_burst_receiver_state_t::llr_cap;
```



Allocated floats. 
 


        

<hr>



### variable llr\_len 

```C++
size_t dsss_burst_receiver_state_t::llr_len;
```



Floats written by the last push. 
 


        

<hr>



### variable n\_bursts 

```C++
uint64_t dsss_burst_receiver_state_t::n_bursts;
```



Bursts DEMODULATED, lifetime. Distinct from the capture's own count, which is windows EMITTED: they differ by any window the demodulator refused, and that difference is the thing worth seeing. 
 


        

<hr>



### variable preamble\_start 

```C++
uint64_t dsss_burst_receiver_state_t::preamble_start;
```



Stream-absolute preamble start. Never late. 


        

<hr>



### variable refine\_margin 

```C++
double dsss_burst_receiver_state_t::refine_margin;
```



Winning preamble correlation over its nearest whole-period competitor. Near 1 means the period was NOT resolved. 
 


        

<hr>



### variable reps 

```C++
size_t dsss_burst_receiver_state_t::reps;
```



Preamble code repetitions. 
 


        

<hr>



### variable spc 

```C++
size_t dsss_burst_receiver_state_t::spc;
```



Samples per chip. 
 


        

<hr>



### variable sync 

```C++
uint8_t* dsss_burst_receiver_state_t::sync;
```



Frame sync word, owned copy. 
 


        

<hr>



### variable sync\_len 

```C++
size_t dsss_burst_receiver_state_t::sync_len;
```



Sync word length, symbols. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dsss_burst_receiver/dsss_burst_receiver_core.h`

