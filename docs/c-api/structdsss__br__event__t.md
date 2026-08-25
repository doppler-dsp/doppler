

# Struct dsss\_br\_event\_t



[**ClassList**](annotated.md) **>** [**dsss\_br\_event\_t**](structdsss__br__event__t.md)





* `#include <dsss_burst_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  double | [**doppler\_hz\_est**](#variable-doppler_hz_est)  <br> |
|  double | [**doppler\_res\_hz**](#variable-doppler_res_hz)  <br> |
|  double | [**est\_freq\_hz**](#variable-est_freq_hz)  <br> |
|  double | [**est\_rate\_hz**](#variable-est_rate_hz)  <br> |
|  double | [**est\_snr\_db**](#variable-est_snr_db)  <br> |
|  uint64\_t | [**frame\_valid**](#variable-frame_valid)  <br> |
|  uint64\_t | [**preamble\_start**](#variable-preamble_start)  <br> |
|  double | [**refine\_margin**](#variable-refine_margin)  <br> |












































## Public Attributes Documentation




### variable cn0\_dbhz\_est 

```C++
double dsss_br_event_t::cn0_dbhz_est;
```



C/N0 lower bound from the hit, dB-Hz. 
 


        

<hr>



### variable doppler\_hz\_est 

```C++
double dsss_br_event_t::doppler_hz_est;
```



Signed coarse Doppler, Hz. 
 


        

<hr>



### variable doppler\_res\_hz 

```C++
double dsss_br_event_t::doppler_res_hz;
```



Acquisition's native bin width, Hz. 
 


        

<hr>



### variable est\_freq\_hz 

```C++
double dsss_br_event_t::est_freq_hz;
```



Demod's residual-frequency estimate. 
 


        

<hr>



### variable est\_rate\_hz 

```C++
double dsss_br_event_t::est_rate_hz;
```



Demod's chirp-rate estimate. 
 


        

<hr>



### variable est\_snr\_db 

```C++
double dsss_br_event_t::est_snr_db;
```



Demod's post-decode SNR estimate. 
 


        

<hr>



### variable frame\_valid 

```C++
uint64_t dsss_br_event_t::frame_valid;
```



Non-zero if the CRC-16 checked out. 
 


        

<hr>



### variable preamble\_start 

```C++
uint64_t dsss_br_event_t::preamble_start;
```



Exact stream position of the preamble. 
 


        

<hr>



### variable refine\_margin 

```C++
double dsss_br_event_t::refine_margin;
```



Runner-up period over the winner. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dsss_burst_receiver/dsss_burst_receiver_core.h`

