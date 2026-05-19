

# Struct agc\_state\_t



[**ClassList**](annotated.md) **>** [**agc\_state\_t**](structagc__state__t.md)



_AGC state._ [More...](#detailed-description)

* `#include <agc_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**alpha**](#variable-alpha)  <br> |
|  double | [**clip\_db**](#variable-clip_db)  <br> |
|  size\_t | [**decim**](#variable-decim)  <br> |
|  double | [**g\_last**](#variable-g_last)  <br> |
|  double | [**gain\_db**](#variable-gain_db)  <br> |
|  double | [**loop\_bw**](#variable-loop_bw)  <br> |
|  double | [**p\_avg**](#variable-p_avg)  <br> |
|  double | [**ref\_db**](#variable-ref_db)  <br> |












































## Detailed Description


Allocate with [**agc\_create()**](agc__core_8h.md#function-agc_create). `ref_db`, `loop_bw`, `alpha`, `decim` and `clip_db` are configuration (readable and writable at runtime); `gain_db`, `p_avg` and `g_last` are the loop's internal memory. 


    
## Public Attributes Documentation




### variable alpha 

```C++
double agc_state_t::alpha;
```




<hr>



### variable clip\_db 

```C++
double agc_state_t::clip_db;
```




<hr>



### variable decim 

```C++
size_t agc_state_t::decim;
```




<hr>



### variable g\_last 

```C++
double agc_state_t::g_last;
```




<hr>



### variable gain\_db 

```C++
double agc_state_t::gain_db;
```




<hr>



### variable loop\_bw 

```C++
double agc_state_t::loop_bw;
```




<hr>



### variable p\_avg 

```C++
double agc_state_t::p_avg;
```




<hr>



### variable ref\_db 

```C++
double agc_state_t::ref_db;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/agc/agc_core.h`

