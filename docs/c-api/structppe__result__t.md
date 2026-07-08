

# Struct ppe\_result\_t



[**ClassList**](annotated.md) **>** [**ppe\_result\_t**](structppe__result__t.md)



_Polynomial-phase estimate (one search)._ 

* `#include <ppe_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**freq\_norm**](#variable-freq_norm)  <br> |
|  double | [**rate\_norm**](#variable-rate_norm)  <br> |
|  double | [**snr\_db**](#variable-snr_db)  <br> |












































## Public Attributes Documentation




### variable freq\_norm 

```C++
double ppe_result_t::freq_norm;
```



frequency, cycles/sample, in [-0.5, 0.5). 


        

<hr>



### variable rate\_norm 

```C++
double ppe_result_t::rate_norm;
```



chirp rate, cycles/sample^2. 


        

<hr>



### variable snr\_db 

```C++
double ppe_result_t::snr_db;
```



winning-row peak-to-mean (rough confidence). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ppe/ppe_core.h`

