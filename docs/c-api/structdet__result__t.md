

# Struct det\_result\_t



[**ClassList**](annotated.md) **>** [**det\_result\_t**](structdet__result__t.md)



_Detection event returned by_ [_**detector\_push()**_](detector__core_8h.md#function-detector_push) _._[More...](#detailed-description)

* `#include <detector_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**lag**](#variable-lag)  <br> |
|  float | [**noise\_est**](#variable-noise_est)  <br> |
|  float | [**peak\_mag**](#variable-peak_mag)  <br> |
|  float | [**test\_stat**](#variable-test_stat)  <br> |












































## Detailed Description


Fields are filled on every int-dump that passes the threshold test. 


    
## Public Attributes Documentation




### variable lag 

```C++
size_t det_result_t::lag;
```



argmax \|R&#91;τ&#93;\| — lag index of the correlation peak. 


        

<hr>



### variable noise\_est 

```C++
float det_result_t::noise_est;
```



Noise estimate (aggregated \|R\| in &#91;noise\_lo,hi&#93;). 


        

<hr>



### variable peak\_mag 

```C++
float det_result_t::peak_mag;
```



max \|R&#91;τ&#93;\| (linear magnitude, not power). 


        

<hr>



### variable test\_stat 

```C++
float det_result_t::test_stat;
```



peak\_mag / noise\_est; 0 if noise\_est == 0. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/detector/detector_core.h`

