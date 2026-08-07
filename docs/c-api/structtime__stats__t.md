

# Struct time\_stats\_t



[**ClassList**](annotated.md) **>** [**time\_stats\_t**](structtime__stats__t.md)



_Time-domain capture statistics (AC-coupled crest/PAPR)._ 

* `#include <measure_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**crest\_db**](#variable-crest_db)  <br>_Crest factor, 20log10(peak\_ac / rms\_ac) (dB)._  |
|  double | [**dc\_offset**](#variable-dc_offset)  <br>_DC offset, mean(x)._  |
|  double | [**fs\_util\_pct**](#variable-fs_util_pct)  <br>_Full-scale use, 100\*max\|x\|/full\_scale (%)._  |
|  double | [**papr\_db**](#variable-papr_db)  <br>_Peak-to-average power ratio (= crest) (dB)._  |
|  double | [**peak**](#variable-peak)  <br>_Peak deviation, max\|x - DC\|._  |
|  double | [**rms**](#variable-rms)  <br>_Root-mean-square amplitude (DC included)._  |












































## Public Attributes Documentation




### variable crest\_db 

_Crest factor, 20log10(peak\_ac / rms\_ac) (dB)._ 
```C++
double time_stats_t::crest_db;
```




<hr>



### variable dc\_offset 

_DC offset, mean(x)._ 
```C++
double time_stats_t::dc_offset;
```




<hr>



### variable fs\_util\_pct 

_Full-scale use, 100\*max\|x\|/full\_scale (%)._ 
```C++
double time_stats_t::fs_util_pct;
```




<hr>



### variable papr\_db 

_Peak-to-average power ratio (= crest) (dB)._ 
```C++
double time_stats_t::papr_db;
```




<hr>



### variable peak 

_Peak deviation, max\|x - DC\|._ 
```C++
double time_stats_t::peak;
```




<hr>



### variable rms 

_Root-mean-square amplitude (DC included)._ 
```C++
double time_stats_t::rms;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/measure/measure_core.h`

