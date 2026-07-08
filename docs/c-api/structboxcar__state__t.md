

# Struct boxcar\_state\_t



[**ClassList**](annotated.md) **>** [**boxcar\_state\_t**](structboxcar__state__t.md)



_Boxcar moving-average state (cf32)._ [More...](#detailed-description)

* `#include <boxcar_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float complex | [**acc**](#variable-acc)  <br> |
|  double | [**gain**](#variable-gain)  <br> |
|  double | [**inv\_len**](#variable-inv_len)  <br> |
|  size\_t | [**len**](#variable-len)  <br> |
|  size\_t | [**pos**](#variable-pos)  <br> |
|  float complex | [**ring**](#variable-ring)  <br> |
|  float | [**scale**](#variable-scale)  <br> |












































## Detailed Description


Pointer-free POD. Allocate with [**boxcar\_create()**](boxcar__core_8h.md#function-boxcar_create), or embed by value and [**boxcar\_init()**](boxcar__core_8h.md#function-boxcar_init). The accumulator and ring are internal; read `len`/`gain` for the configured window and output gain. 


    
## Public Attributes Documentation




### variable acc 

```C++
float complex boxcar_state_t::acc;
```



running sum over the window. 


        

<hr>



### variable gain 

```C++
double boxcar_state_t::gain;
```



output gain applied to the mean. 


        

<hr>



### variable inv\_len 

```C++
double boxcar_state_t::inv_len;
```



cached 1 / len. 


        

<hr>



### variable len 

```C++
size_t boxcar_state_t::len;
```



window length (1 .. BOXCAR\_MAX\_LEN). 


        

<hr>



### variable pos 

```C++
size_t boxcar_state_t::pos;
```



ring write index (0 .. len-1). 


        

<hr>



### variable ring 

```C++
float complex boxcar_state_t::ring[BOXCAR_MAX_LEN];
```



delay line. 


        

<hr>



### variable scale 

```C++
float boxcar_state_t::scale;
```



cached (float)(gain / len) — the per-sample applied multiply. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/boxcar/boxcar_core.h`

