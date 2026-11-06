

# Struct dp\_correlator\_hit\_t



[**ClassList**](annotated.md) **>** [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md)



_One detection, from either detector flavor's_ [_**dp\_correlator\_push()**_](correlator__core_8h.md#function-dp_correlator_push) _._[More...](#detailed-description)

* `#include <correlator_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**col**](#variable-col)  <br> |
|  float | [**noise\_est**](#variable-noise_est)  <br> |
|  float | [**peak\_mag**](#variable-peak_mag)  <br> |
|  size\_t | [**row**](#variable-row)  <br> |
|  float | [**test\_stat**](#variable-test_stat)  <br> |












































## Detailed Description


The peak's flat index in the ny x nx correlation map, decomposed into (row, col) so that a caller does not need nx. At one row (the 1-D detector) row is 0 and col is the lag. docs/design/correlator.md section 4: the one hit record, which the search flavors widen. 


    
## Public Attributes Documentation




### variable col 

```C++
size_t dp_correlator_hit_t::col;
```



Column of the peak: the lag, at one row. 
 


        

<hr>



### variable noise\_est 

```C++
float dp_correlator_hit_t::noise_est;
```



Noise estimate aggregated over noise\_lo..noise\_hi. 


        

<hr>



### variable peak\_mag 

```C++
float dp_correlator_hit_t::peak_mag;
```



max \|R\| (linear magnitude, not power). 
 


        

<hr>



### variable row 

```C++
size_t dp_correlator_hit_t::row;
```



Row of the correlation peak (0 at one row). 
 


        

<hr>



### variable test\_stat 

```C++
float dp_correlator_hit_t::test_stat;
```



peak\_mag / noise\_est; 0 if noise\_est is not &gt; 0. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/correlator/correlator_core.h`

