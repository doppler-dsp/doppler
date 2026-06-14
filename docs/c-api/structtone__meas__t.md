

# Struct tone\_meas\_t



[**ClassList**](annotated.md) **>** [**tone\_meas\_t**](structtone__meas__t.md)



_Single-tone dynamic-measurement bag._ [More...](#detailed-description)

* `#include <measure_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**amp\_uncert\_db**](#variable-amp_uncert_db)  <br> |
|  double | [**bin\_hz**](#variable-bin_hz)  <br> |
|  double | [**enbw\_hz**](#variable-enbw_hz)  <br> |
|  double | [**enob**](#variable-enob)  <br> |
|  double | [**enob\_fs**](#variable-enob_fs)  <br> |
|  double | [**floor\_uncert\_db**](#variable-floor_uncert_db)  <br> |
|  double | [**fund\_dbfs**](#variable-fund_dbfs)  <br> |
|  double | [**fund\_freq**](#variable-fund_freq)  <br> |
|  size\_t | [**lobe\_bins**](#variable-lobe_bins)  <br> |
|  size\_t | [**n\_noise\_bins**](#variable-n_noise_bins)  <br> |
|  double | [**noise\_floor\_dbfs**](#variable-noise_floor_dbfs)  <br> |
|  double | [**proc\_gain\_db**](#variable-proc_gain_db)  <br> |
|  double | [**rbw\_hz**](#variable-rbw_hz)  <br> |
|  double | [**sfdr\_dbc**](#variable-sfdr_dbc)  <br> |
|  double | [**sfdr\_dbfs**](#variable-sfdr_dbfs)  <br> |
|  double | [**sinad**](#variable-sinad)  <br> |
|  double | [**snr**](#variable-snr)  <br> |
|  double | [**thd**](#variable-thd)  <br> |
|  double | [**thd\_n**](#variable-thd_n)  <br> |
|  double | [**thd\_pct**](#variable-thd_pct)  <br> |
|  double | [**worst\_spur\_dbc**](#variable-worst_spur_dbc)  <br> |
|  double | [**worst\_spur\_freq**](#variable-worst_spur_freq)  <br> |
|  int | [**worst\_spur\_is\_harm**](#variable-worst_spur_is_harm)  <br> |












































## Detailed Description


All ratios (SNR/SINAD/THD/THD+N) are dimensionless dB and independent of the dBFS reference; the absolute `*_dbfs` levels reference a full-scale tone to 0 dBFS (real captures: a peak-`full_scale` sine; complex: a `full_scale` exponential). Accuracy fields describe the analysis grid that produced them. 


    
## Public Attributes Documentation




### variable amp\_uncert\_db 

```C++
double tone_meas_t::amp_uncert_db;
```




<hr>



### variable bin\_hz 

```C++
double tone_meas_t::bin_hz;
```




<hr>



### variable enbw\_hz 

```C++
double tone_meas_t::enbw_hz;
```




<hr>



### variable enob 

```C++
double tone_meas_t::enob;
```




<hr>



### variable enob\_fs 

```C++
double tone_meas_t::enob_fs;
```




<hr>



### variable floor\_uncert\_db 

```C++
double tone_meas_t::floor_uncert_db;
```




<hr>



### variable fund\_dbfs 

```C++
double tone_meas_t::fund_dbfs;
```




<hr>



### variable fund\_freq 

```C++
double tone_meas_t::fund_freq;
```




<hr>



### variable lobe\_bins 

```C++
size_t tone_meas_t::lobe_bins;
```




<hr>



### variable n\_noise\_bins 

```C++
size_t tone_meas_t::n_noise_bins;
```




<hr>



### variable noise\_floor\_dbfs 

```C++
double tone_meas_t::noise_floor_dbfs;
```




<hr>



### variable proc\_gain\_db 

```C++
double tone_meas_t::proc_gain_db;
```




<hr>



### variable rbw\_hz 

```C++
double tone_meas_t::rbw_hz;
```




<hr>



### variable sfdr\_dbc 

```C++
double tone_meas_t::sfdr_dbc;
```




<hr>



### variable sfdr\_dbfs 

```C++
double tone_meas_t::sfdr_dbfs;
```




<hr>



### variable sinad 

```C++
double tone_meas_t::sinad;
```




<hr>



### variable snr 

```C++
double tone_meas_t::snr;
```




<hr>



### variable thd 

```C++
double tone_meas_t::thd;
```




<hr>



### variable thd\_n 

```C++
double tone_meas_t::thd_n;
```




<hr>



### variable thd\_pct 

```C++
double tone_meas_t::thd_pct;
```




<hr>



### variable worst\_spur\_dbc 

```C++
double tone_meas_t::worst_spur_dbc;
```




<hr>



### variable worst\_spur\_freq 

```C++
double tone_meas_t::worst_spur_freq;
```




<hr>



### variable worst\_spur\_is\_harm 

```C++
int tone_meas_t::worst_spur_is_harm;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/measure/measure_core.h`

