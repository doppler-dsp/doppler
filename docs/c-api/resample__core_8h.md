

# File resample\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**resample**](dir_430486ea22038fad478027f2dc6550c6.md) **>** [**resample\_core.h**](resample__core_8h.md)

[Go to the source code of this file](resample__core_8h_source.md)

_Resample module — public C API._ 

* `#include "clib_common.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**ciccompmf**](#function-ciccompmf) (double \* h, uint32\_t N, uint32\_t R, uint32\_t M) <br> |
|  double | [**kaiser\_beta**](#function-kaiser_beta) (double atten) <br> |
|  int | [**kaiser\_num\_taps**](#function-kaiser_num_taps) (int num\_phases, double atten, double pb, double sb) <br> |




























## Public Functions Documentation




### function ciccompmf 

```C++
void ciccompmf (
    double * h,
    uint32_t N,
    uint32_t R,
    uint32_t M
) 
```



Design a CIC passband-droop compensator FIR filter.


Implements the closed-form Bernoulli-series (maximally-flat error) method described in:


Molnar & Vucic, "Closed-Form Design of CIC Compensators Based on
  Maximally Flat Error Criterion," IEEE TCAS-II, 58(12):926-930, 2011. DOI: 10.1109/TCSII.2011.2172522


The compensator runs at the _decimated_ (output) rate and should be applied after the CIC decimator.


Bernoulli table has 9 entries (B\_2 ... B\_18). Valid tap counts: odd M: 1, 3, 5, ... 19 (half = (M-1)/2 &lt;= 9) even M: 2, 4, 6, ... 18 (half = M/2 &lt;= 9) M outside these ranges -&gt; h is left unmodified.




**Parameters:**


* `h` Output buffer, caller-allocated, M elements. DC gain = 1.0. 
* `N` CIC filter order (number of integrator/comb stages, &gt;= 1). 
* `R` CIC decimation factor (&gt;= 2). 
* `M` Number of compensator taps. Odd M -&gt; symmetric linear-phase; even M -&gt; half-sample-shifted linear-phase. 




        

<hr>



### function kaiser\_beta 

```C++
double kaiser_beta (
    double atten
) 
```




<hr>



### function kaiser\_num\_taps 

```C++
int kaiser_num_taps (
    int num_phases,
    double atten,
    double pb,
    double sb
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/resample/resample_core.h`

