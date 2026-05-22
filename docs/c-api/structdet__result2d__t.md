

# Struct det\_result2d\_t



[**ClassList**](annotated.md) **>** [**det\_result2d\_t**](structdet__result2d__t.md)



_Detection event returned by_ [_**detector2d\_push()**_](detector2d__core_8h.md#function-detector2d_push) _._[More...](#detailed-description)

* `#include <detector2d_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**col**](#variable-col)  <br> |
|  float | [**noise\_est**](#variable-noise_est)  <br> |
|  float | [**peak\_mag**](#variable-peak_mag)  <br> |
|  size\_t | [**row**](#variable-row)  <br> |
|  float | [**test\_stat**](#variable-test_stat)  <br> |












































## Detailed Description


The peak index in the flat ny×nx correlation map is decomposed into (row, col) so that callers do not need to know nx. 


    
## Public Attributes Documentation




### variable col 

```C++
size_t det_result2d_t::col;
```



Column of the correlation peak (0-indexed). 


        

<hr>



### variable noise\_est 

```C++
float det_result2d_t::noise_est;
```



Noise estimate aggregated over &#91;noise\_lo, hi&#93;. 


        

<hr>



### variable peak\_mag 

```C++
float det_result2d_t::peak_mag;
```



max \|R&#91;i,j&#93;\| (linear magnitude). 


        

<hr>



### variable row 

```C++
size_t det_result2d_t::row;
```



Row of the correlation peak (0-indexed). 


        

<hr>



### variable test\_stat 

```C++
float det_result2d_t::test_stat;
```



peak\_mag / noise\_est; 0 if noise\_est == 0. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/detector2d/detector2d_core.h`

