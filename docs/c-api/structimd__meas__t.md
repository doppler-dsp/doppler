

# Struct imd\_meas\_t



[**ClassList**](annotated.md) **>** [**imd\_meas\_t**](structimd__meas__t.md)



_Two-tone intermodulation result (IMD2/IMD3/TOI)._ 

* `#include <measure_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**f1**](#variable-f1)  <br>_Lower tone frequency (Hz)._  |
|  double | [**f2**](#variable-f2)  <br>_Upper tone frequency (Hz)._  |
|  double | [**imd2\_dbc**](#variable-imd2_dbc)  <br>_2nd-order product (f2-f1) vs mean tone (dBc)._  |
|  double | [**imd2\_freq**](#variable-imd2_freq)  <br>_2nd-order product frequency (Hz)._  |
|  double | [**imd3\_dbc**](#variable-imd3_dbc)  <br>_Worst 3rd-order product vs mean tone (dBc)._  |
|  double | [**imd3\_hi\_freq**](#variable-imd3_hi_freq)  <br>_3rd-order (2f2-f1) product frequency (Hz)._  |
|  double | [**imd3\_lo\_freq**](#variable-imd3_lo_freq)  <br>_3rd-order (2f1-f2) product frequency (Hz)._  |
|  double | [**p1\_dbfs**](#variable-p1_dbfs)  <br>_Lower tone level (dBFS)._  |
|  double | [**p2\_dbfs**](#variable-p2_dbfs)  <br>_Upper tone level (dBFS)._  |
|  double | [**rbw\_hz**](#variable-rbw_hz)  <br>_Resolution bandwidth (Hz)._  |
|  double | [**soi\_dbfs**](#variable-soi_dbfs)  <br>_Second-order intercept (dBFS)._  |
|  double | [**toi\_dbfs**](#variable-toi_dbfs)  <br>_Third-order intercept (dBFS)._  |












































## Public Attributes Documentation




### variable f1 

_Lower tone frequency (Hz)._ 
```C++
double imd_meas_t::f1;
```




<hr>



### variable f2 

_Upper tone frequency (Hz)._ 
```C++
double imd_meas_t::f2;
```




<hr>



### variable imd2\_dbc 

_2nd-order product (f2-f1) vs mean tone (dBc)._ 
```C++
double imd_meas_t::imd2_dbc;
```




<hr>



### variable imd2\_freq 

_2nd-order product frequency (Hz)._ 
```C++
double imd_meas_t::imd2_freq;
```




<hr>



### variable imd3\_dbc 

_Worst 3rd-order product vs mean tone (dBc)._ 
```C++
double imd_meas_t::imd3_dbc;
```




<hr>



### variable imd3\_hi\_freq 

_3rd-order (2f2-f1) product frequency (Hz)._ 
```C++
double imd_meas_t::imd3_hi_freq;
```




<hr>



### variable imd3\_lo\_freq 

_3rd-order (2f1-f2) product frequency (Hz)._ 
```C++
double imd_meas_t::imd3_lo_freq;
```




<hr>



### variable p1\_dbfs 

_Lower tone level (dBFS)._ 
```C++
double imd_meas_t::p1_dbfs;
```




<hr>



### variable p2\_dbfs 

_Upper tone level (dBFS)._ 
```C++
double imd_meas_t::p2_dbfs;
```




<hr>



### variable rbw\_hz 

_Resolution bandwidth (Hz)._ 
```C++
double imd_meas_t::rbw_hz;
```




<hr>



### variable soi\_dbfs 

_Second-order intercept (dBFS)._ 
```C++
double imd_meas_t::soi_dbfs;
```




<hr>



### variable toi\_dbfs 

_Third-order intercept (dBFS)._ 
```C++
double imd_meas_t::toi_dbfs;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/measure/measure_core.h`

