

# Struct dp\_ofdm\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md)



_Ofdm state: the layout (configuration) and the position in it (state)._ [More...](#detailed-description)

* `#include <ofdm_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**cap\_cls**](#variable-cap_cls)  <br> |
|  uint32\_t | [**cap\_runs**](#variable-cap_runs)  <br> |
|  [**ofdm\_class\_t**](structofdm__class__t.md) \* | [**cls**](#variable-cls)  <br> |
|  float \_Complex \* | [**grid**](#variable-grid)  <br> |
|  uint32\_t | [**max\_window**](#variable-max_window)  <br> |
|  uint32\_t | [**n\_cls**](#variable-n_cls)  <br> |
|  uint32\_t | [**n\_plans**](#variable-n_plans)  <br> |
|  uint32\_t | [**n\_runs**](#variable-n_runs)  <br> |
|  uint32\_t \* | [**plan\_of**](#variable-plan_of)  <br> |
|  [**dp\_fft\_state\_t**](structdp__fft__state__t.md) \*\* | [**plans**](#variable-plans)  <br> |
|  uint32\_t | [**run\_index**](#variable-run_index)  <br> |
|  [**ofdm\_run\_t**](structofdm__run__t.md) \* | [**runs**](#variable-runs)  <br> |
|  int | [**sealed**](#variable-sealed)  <br> |
|  uint32\_t | [**sym\_in\_run**](#variable-sym_in_run)  <br> |
|  float \_Complex \* | [**tail**](#variable-tail)  <br> |
|  uint32\_t | [**tail\_cap**](#variable-tail_cap)  <br> |
|  uint32\_t | [**tail\_len**](#variable-tail_len)  <br> |
|  uint8\_t \* | [**taken**](#variable-taken)  <br> |
|  float \_Complex \* | [**xform**](#variable-xform)  <br> |












































## Detailed Description


Allocate with [**dp\_ofdm\_create()**](ofdm__core_8h.md#function-dp_ofdm_create). 


    
## Public Attributes Documentation




### variable cap\_cls 

```C++
uint32_t dp_ofdm_state_t::cap_cls;
```



Allocated class slots. 
 


        

<hr>



### variable cap\_runs 

```C++
uint32_t dp_ofdm_state_t::cap_runs;
```



Allocated run slots. 
 


        

<hr>



### variable cls 

```C++
ofdm_class_t* dp_ofdm_state_t::cls;
```



Classes, `n_cls` of them. 
 


        

<hr>



### variable grid 

```C++
float _Complex* dp_ofdm_state_t::grid;
```



Scratch: one grid, n\_fft\_max. 
 


        

<hr>



### variable max\_window 

```C++
uint32_t dp_ofdm_state_t::max_window;
```



Largest class window; sizes the blob. 
 


        

<hr>



### variable n\_cls 

```C++
uint32_t dp_ofdm_state_t::n_cls;
```



Classes held. 
 


        

<hr>



### variable n\_plans 

```C++
uint32_t dp_ofdm_state_t::n_plans;
```



Distinct inverse plans held. 
 


        

<hr>



### variable n\_runs 

```C++
uint32_t dp_ofdm_state_t::n_runs;
```



Runs held. 
 


        

<hr>



### variable plan\_of 

```C++
uint32_t* dp_ofdm_state_t::plan_of;
```



Class -&gt; its plan. 
 


        

<hr>



### variable plans 

```C++
dp_fft_state_t** dp_ofdm_state_t::plans;
```



One inverse plan per distinct n\_fft. 
 


        

<hr>



### variable run\_index 

```C++
uint32_t dp_ofdm_state_t::run_index;
```



The run the next symbol belongs to. 
 


        

<hr>



### variable runs 

```C++
ofdm_run_t* dp_ofdm_state_t::runs;
```



Runs, `n_runs` of them. 
 


        

<hr>



### variable sealed 

```C++
int dp_ofdm_state_t::sealed;
```



Checked; adding to the layout clears it. 
 


        

<hr>



### variable sym\_in\_run 

```C++
uint32_t dp_ofdm_state_t::sym_in_run;
```



Symbols already rendered of that run. 


        

<hr>



### variable tail 

```C++
float _Complex* dp_ofdm_state_t::tail;
```



The held tail, `tail_cap` slots. 
 


        

<hr>



### variable tail\_cap 

```C++
uint32_t dp_ofdm_state_t::tail_cap;
```



Allocated tail slots, &gt;= max\_window. 
 


        

<hr>



### variable tail\_len 

```C++
uint32_t dp_ofdm_state_t::tail_len;
```



Samples of window tail held. 
 


        

<hr>



### variable taken 

```C++
uint8_t* dp_ofdm_state_t::taken;
```



Scratch: subcarriers a fixed entry owns. 
 


        

<hr>



### variable xform 

```C++
float _Complex* dp_ofdm_state_t::xform;
```



Scratch: one transform, n\_fft\_max. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ofdm/ofdm_core.h`

