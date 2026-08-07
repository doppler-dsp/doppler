

# Struct tone\_meas\_t



[**ClassList**](annotated.md) **>** [**tone\_meas\_t**](structtone__meas__t.md)



_Single-tone dynamic-measurement bag._ [More...](#detailed-description)

* `#include <measure_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**amp\_uncert\_db**](#variable-amp_uncert_db)  <br>_Amplitude-read uncertainty bound (dB)._  |
|  double | [**bin\_hz**](#variable-bin_hz)  <br>_FFT bin spacing = fs/nfft (Hz)._  |
|  double | [**enbw\_hz**](#variable-enbw_hz)  <br>_Equivalent noise bandwidth (Hz) (= rbw\_hz)._  |
|  double | [**enob**](#variable-enob)  <br>_ENOB = (SINAD - 1.76)/6.02._  |
|  double | [**enob\_fs**](#variable-enob_fs)  <br>_Full-scale-corrected ENOB._  |
|  double | [**floor\_uncert\_db**](#variable-floor_uncert_db)  <br>_Noise-floor standard error (dB)._  |
|  double | [**fund\_dbfs**](#variable-fund_dbfs)  <br>_Fundamental level (dBFS)._  |
|  double | [**fund\_freq**](#variable-fund_freq)  <br>_Fundamental frequency (Hz)._  |
|  size\_t | [**lobe\_bins**](#variable-lobe_bins)  <br>_Window main-lobe half-width L (bins)._  |
|  size\_t | [**n\_noise\_bins**](#variable-n_noise_bins)  <br>_Number of bins counted as noise._  |
|  double | [**noise\_floor\_dbfs**](#variable-noise_floor_dbfs)  <br>_Mean per-bin noise power (dBFS)._  |
|  double | [**proc\_gain\_db**](#variable-proc_gain_db)  <br>_FFT processing gain = 10log10(nfft/2) (dB)._  |
|  double | [**rbw\_hz**](#variable-rbw_hz)  <br>_Resolution bandwidth = enbw\*fs/n (Hz)._  |
|  double | [**sfdr\_dbc**](#variable-sfdr_dbc)  <br>_SFDR: fundamental - worst spur (dBc)._  |
|  double | [**sfdr\_dbfs**](#variable-sfdr_dbfs)  <br>_SFDR: full scale - worst spur (dBFS)._  |
|  double | [**sinad**](#variable-sinad)  <br>_SINAD = 10log10(fund/(noise+harm)) (dB)._  |
|  double | [**snr**](#variable-snr)  <br>_SNR = 10log10(P\_fund / P\_noise) (dB)._  |
|  double | [**thd**](#variable-thd)  <br>_THD = 10log10(P\_harm / P\_fund) (dBc)._  |
|  double | [**thd\_n**](#variable-thd_n)  <br>_THD+N = 10log10((noise+harm)/fund) = -SINAD._  |
|  double | [**thd\_pct**](#variable-thd_pct)  <br>_THD = 100 sqrt(P\_harm / P\_fund) (%)._  |
|  double | [**worst\_spur\_dbc**](#variable-worst_spur_dbc)  <br>_Worst spur level vs the fundamental (dBc)._  |
|  double | [**worst\_spur\_freq**](#variable-worst_spur_freq)  <br>_Worst spur frequency (Hz)._  |
|  int | [**worst\_spur\_is\_harm**](#variable-worst_spur_is_harm)  <br>_1 if the worst spur is a harmonic, else 0._  |












































## Detailed Description


All ratios (SNR/SINAD/THD/THD+N) are dimensionless dB and independent of the dBFS reference; the absolute `*_dbfs` levels reference a full-scale tone to 0 dBFS (real captures: a peak-`full_scale` sine; complex: a `full_scale` exponential). Accuracy fields describe the analysis grid that produced them. 


    
## Public Attributes Documentation




### variable amp\_uncert\_db 

_Amplitude-read uncertainty bound (dB)._ 
```C++
double tone_meas_t::amp_uncert_db;
```




<hr>



### variable bin\_hz 

_FFT bin spacing = fs/nfft (Hz)._ 
```C++
double tone_meas_t::bin_hz;
```




<hr>



### variable enbw\_hz 

_Equivalent noise bandwidth (Hz) (= rbw\_hz)._ 
```C++
double tone_meas_t::enbw_hz;
```




<hr>



### variable enob 

_ENOB = (SINAD - 1.76)/6.02._ 
```C++
double tone_meas_t::enob;
```




<hr>



### variable enob\_fs 

_Full-scale-corrected ENOB._ 
```C++
double tone_meas_t::enob_fs;
```




<hr>



### variable floor\_uncert\_db 

_Noise-floor standard error (dB)._ 
```C++
double tone_meas_t::floor_uncert_db;
```




<hr>



### variable fund\_dbfs 

_Fundamental level (dBFS)._ 
```C++
double tone_meas_t::fund_dbfs;
```




<hr>



### variable fund\_freq 

_Fundamental frequency (Hz)._ 
```C++
double tone_meas_t::fund_freq;
```




<hr>



### variable lobe\_bins 

_Window main-lobe half-width L (bins)._ 
```C++
size_t tone_meas_t::lobe_bins;
```




<hr>



### variable n\_noise\_bins 

_Number of bins counted as noise._ 
```C++
size_t tone_meas_t::n_noise_bins;
```




<hr>



### variable noise\_floor\_dbfs 

_Mean per-bin noise power (dBFS)._ 
```C++
double tone_meas_t::noise_floor_dbfs;
```




<hr>



### variable proc\_gain\_db 

_FFT processing gain = 10log10(nfft/2) (dB)._ 
```C++
double tone_meas_t::proc_gain_db;
```




<hr>



### variable rbw\_hz 

_Resolution bandwidth = enbw\*fs/n (Hz)._ 
```C++
double tone_meas_t::rbw_hz;
```




<hr>



### variable sfdr\_dbc 

_SFDR: fundamental - worst spur (dBc)._ 
```C++
double tone_meas_t::sfdr_dbc;
```




<hr>



### variable sfdr\_dbfs 

_SFDR: full scale - worst spur (dBFS)._ 
```C++
double tone_meas_t::sfdr_dbfs;
```




<hr>



### variable sinad 

_SINAD = 10log10(fund/(noise+harm)) (dB)._ 
```C++
double tone_meas_t::sinad;
```




<hr>



### variable snr 

_SNR = 10log10(P\_fund / P\_noise) (dB)._ 
```C++
double tone_meas_t::snr;
```




<hr>



### variable thd 

_THD = 10log10(P\_harm / P\_fund) (dBc)._ 
```C++
double tone_meas_t::thd;
```




<hr>



### variable thd\_n 

_THD+N = 10log10((noise+harm)/fund) = -SINAD._ 
```C++
double tone_meas_t::thd_n;
```




<hr>



### variable thd\_pct 

_THD = 100 sqrt(P\_harm / P\_fund) (%)._ 
```C++
double tone_meas_t::thd_pct;
```




<hr>



### variable worst\_spur\_dbc 

_Worst spur level vs the fundamental (dBc)._ 
```C++
double tone_meas_t::worst_spur_dbc;
```




<hr>



### variable worst\_spur\_freq 

_Worst spur frequency (Hz)._ 
```C++
double tone_meas_t::worst_spur_freq;
```




<hr>



### variable worst\_spur\_is\_harm 

_1 if the worst spur is a harmonic, else 0._ 
```C++
int tone_meas_t::worst_spur_is_harm;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/measure/measure_core.h`

