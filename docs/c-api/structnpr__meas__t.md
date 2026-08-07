

# Struct npr\_meas\_t



[**ClassList**](annotated.md) **>** [**npr\_meas\_t**](structnpr__meas__t.md)



_Noise Power Ratio (notched-noise loading) result._ 

* `#include <measure_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**inband\_psd\_dbfs**](#variable-inband_psd_dbfs)  <br>_Mean in-band noise power per bin (dBFS)._  |
|  size\_t | [**n\_inband\_bins**](#variable-n_inband_bins)  <br>_Bins averaged in the active band._  |
|  size\_t | [**n\_notch\_bins**](#variable-n_notch_bins)  <br>_Bins averaged inside the notch._  |
|  double | [**notch\_psd\_dbfs**](#variable-notch_psd_dbfs)  <br>_Mean power folded into the notch (dBFS)._  |
|  double | [**npr\_db**](#variable-npr_db)  <br>_NPR = 10log10(in-band PSD / notch PSD) (dB)._  |
|  double | [**rbw\_hz**](#variable-rbw_hz)  <br>_Resolution bandwidth (Hz)._  |












































## Public Attributes Documentation




### variable inband\_psd\_dbfs 

_Mean in-band noise power per bin (dBFS)._ 
```C++
double npr_meas_t::inband_psd_dbfs;
```




<hr>



### variable n\_inband\_bins 

_Bins averaged in the active band._ 
```C++
size_t npr_meas_t::n_inband_bins;
```




<hr>



### variable n\_notch\_bins 

_Bins averaged inside the notch._ 
```C++
size_t npr_meas_t::n_notch_bins;
```




<hr>



### variable notch\_psd\_dbfs 

_Mean power folded into the notch (dBFS)._ 
```C++
double npr_meas_t::notch_psd_dbfs;
```




<hr>



### variable npr\_db 

_NPR = 10log10(in-band PSD / notch PSD) (dB)._ 
```C++
double npr_meas_t::npr_db;
```




<hr>



### variable rbw\_hz 

_Resolution bandwidth (Hz)._ 
```C++
double npr_meas_t::rbw_hz;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/measure/measure_core.h`

