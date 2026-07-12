

# Struct burst\_despreader\_state\_t



[**ClassList**](annotated.md) **>** [**burst\_despreader\_state\_t**](structburst__despreader__state__t.md)



_BurstDespreader state._ [More...](#detailed-description)

* `#include <burst_despreader_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float complex | [**acc\_e**](#variable-acc_e)  <br> |
|  float complex | [**acc\_l**](#variable-acc_l)  <br> |
|  float complex | [**acc\_p**](#variable-acc_p)  <br> |
|  uint8\_t \* | [**acq\_code**](#variable-acq_code)  <br> |
|  size\_t | [**acq\_reps**](#variable-acq_reps)  <br> |
|  size\_t | [**acq\_sf**](#variable-acq_sf)  <br> |
|  double | [**car\_phase**](#variable-car_phase)  <br> |
|  double | [**car\_w**](#variable-car_w)  <br> |
|  double | [**chip\_pos**](#variable-chip_pos)  <br> |
|  uint8\_t \* | [**code**](#variable-code)  <br> |
|  double | [**code\_rate**](#variable-code_rate)  <br> |
|  [**loop\_filter\_state\_t**](structloop__filter__state__t.md) | [**lf\_car**](#variable-lf_car)  <br> |
|  [**loop\_filter\_state\_t**](structloop__filter__state__t.md) | [**lf\_code**](#variable-lf_code)  <br> |
|  double | [**lock\_metric**](#variable-lock_metric)  <br> |
|  size\_t | [**preamble\_left**](#variable-preamble_left)  <br> |
|  double | [**seed\_chip**](#variable-seed_chip)  <br> |
|  double | [**seed\_w**](#variable-seed_w)  <br> |
|  size\_t | [**sf**](#variable-sf)  <br> |
|  double | [**snr\_est**](#variable-snr_est)  <br> |
|  size\_t | [**sps**](#variable-sps)  <br> |
|  size\_t | [**stat\_n**](#variable-stat_n)  <br> |
|  double | [**sum\_im2**](#variable-sum_im2)  <br> |
|  double | [**sum\_lock**](#variable-sum_lock)  <br> |
|  double | [**sum\_re2**](#variable-sum_re2)  <br> |
|  size\_t | [**tsamps**](#variable-tsamps)  <br> |












































## Detailed Description


Allocate with [**burst\_despreader\_create()**](burst__despreader__core_8h.md#function-burst_despreader_create). 


    
## Public Attributes Documentation




### variable acc\_e 

```C++
float complex burst_despreader_state_t::acc_e;
```



early correlator accumulator. 


        

<hr>



### variable acc\_l 

```C++
float complex burst_despreader_state_t::acc_l;
```



late correlator accumulator. 


        

<hr>



### variable acc\_p 

```C++
float complex burst_despreader_state_t::acc_p;
```



prompt correlator accumulator. 


        

<hr>



### variable acq\_code 

```C++
uint8_t* burst_despreader_state_t::acq_code;
```



owned acq code, NULL if payload-only. 


        

<hr>



### variable acq\_reps 

```C++
size_t burst_despreader_state_t::acq_reps;
```



preamble periods to track before payload. 


        

<hr>



### variable acq\_sf 

```C++
size_t burst_despreader_state_t::acq_sf;
```



acq code length, chips/period. 


        

<hr>



### variable car\_phase 

```C++
double burst_despreader_state_t::car_phase;
```



current carrier phase, radians. 


        

<hr>



### variable car\_w 

```C++
double burst_despreader_state_t::car_w;
```



current carrier angular freq, rad/sample. 


        

<hr>



### variable chip\_pos 

```C++
double burst_despreader_state_t::chip_pos;
```



prompt code position within symbol, chips. 


        

<hr>



### variable code 

```C++
uint8_t* burst_despreader_state_t::code;
```



owned spreading code, 0/1, length sf. 


        

<hr>



### variable code\_rate 

```C++
double burst_despreader_state_t::code_rate;
```



chips advanced per nominal chip (~1.0). 


        

<hr>



### variable lf\_car 

```C++
loop_filter_state_t burst_despreader_state_t::lf_car;
```



carrier (Costas) loop. 


        

<hr>



### variable lf\_code 

```C++
loop_filter_state_t burst_despreader_state_t::lf_code;
```



code (DLL) loop. 


        

<hr>



### variable lock\_metric 

```C++
double burst_despreader_state_t::lock_metric;
```



mean of \|Re P\|/\|P\| over the burst (~1 locked, ~2/pi with no carrier). 


        

<hr>



### variable preamble\_left 

```C++
size_t burst_despreader_state_t::preamble_left;
```



preamble periods still to consume. 


        

<hr>



### variable seed\_chip 

```C++
double burst_despreader_state_t::seed_chip;
```



create-time code phase, chips, for reset. 


        

<hr>



### variable seed\_w 

```C++
double burst_despreader_state_t::seed_w;
```



create-time carrier angular freq, rad/sample. 


        

<hr>



### variable sf 

```C++
size_t burst_despreader_state_t::sf;
```



spreading factor = code length, chips/symbol. 


        

<hr>



### variable snr\_est 

```C++
double burst_despreader_state_t::snr_est;
```



accumulate-then-ratio post-despread SNR: (sum Re^2 - sum Im^2)/sum Im^2, clamped &gt;= 0. 


        

<hr>



### variable sps 

```C++
size_t burst_despreader_state_t::sps;
```



samples per chip (&gt;= 2). 


        

<hr>



### variable stat\_n 

```C++
size_t burst_despreader_state_t::stat_n;
```



prompts folded into the burst statistics. 


        

<hr>



### variable sum\_im2 

```C++
double burst_despreader_state_t::sum_im2;
```



running sum of Im(P)^2. 


        

<hr>



### variable sum\_lock 

```C++
double burst_despreader_state_t::sum_lock;
```



running sum of \|Re P\|/\|P\|. 


        

<hr>



### variable sum\_re2 

```C++
double burst_despreader_state_t::sum_re2;
```



running sum of Re(P)^2. 


        

<hr>



### variable tsamps 

```C++
size_t burst_despreader_state_t::tsamps;
```



sf\*sps, symbol period in samples. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/burst_despreader/burst_despreader_core.h`

