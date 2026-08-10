

# Struct RateConverter\_state\_t



[**ClassList**](annotated.md) **>** [**RateConverter\_state\_t**](structRateConverter__state__t.md)



_Cascade state_  _owns all sub-stage C objects._[More...](#detailed-description)

* `#include <RateConverter_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**agc\_state\_t**](structagc__state__t.md) \* | [**agc**](#variable-agc)  <br> |
|  double | [**agc\_alpha**](#variable-agc_alpha)  <br> |
|  double | [**agc\_bn\_sym**](#variable-agc_bn_sym)  <br> |
|  double | [**agc\_ref\_db**](#variable-agc_ref_db)  <br> |
|  double | [**bank\_e0**](#variable-bank_e0)  <br> |
|  double | [**bank\_sps**](#variable-bank_sps)  <br> |
|  double | [**beta**](#variable-beta)  <br> |
|  size\_t | [**buf\_cap**](#variable-buf_cap)  <br> |
|  float \_Complex \* | [**bufs**](#variable-bufs)  <br> |
|  int | [**compensate**](#variable-compensate)  <br> |
|  int | [**n\_stages**](#variable-n_stages)  <br> |
|  bool | [**narrow\_pulse**](#variable-narrow_pulse)  <br> |
|  size\_t | [**num\_phases**](#variable-num_phases)  <br> |
|  int | [**pulse**](#variable-pulse)  <br> |
|  double | [**pulse\_sps**](#variable-pulse_sps)  <br> |
|  double | [**rate**](#variable-rate)  <br> |
|  size\_t | [**span**](#variable-span)  <br> |
|  void \* | [**stage\_ptrs**](#variable-stage_ptrs)  <br> |
|  [**rc\_stage\_t**](RateConverter__core_8h.md#enum-rc_stage_t) | [**stage\_types**](#variable-stage_types)  <br> |












































## Detailed Description


Do not initialise directly; use [**RateConverter\_create()**](RateConverter__core_8h.md#function-rateconverter_create). 


    
## Public Attributes Documentation




### variable agc 

```C++
agc_state_t* RateConverter_state_t::agc;
```



NULL when off — one branch per sample 
 


        

<hr>



### variable agc\_alpha 

```C++
double RateConverter_state_t::agc_alpha;
```



detector EMA coefficient 
 


        

<hr>



### variable agc\_bn\_sym 

```C++
double RateConverter_state_t::agc_bn_sym;
```



requested bandwidth, cycles/SYMBOL 
 


        

<hr>



### variable agc\_ref\_db 

```C++
double RateConverter_state_t::agc_ref_db;
```



derived: 10\*log10(bank\_e0 / bank\_sps) 
 


        

<hr>



### variable bank\_e0 

```C++
double RateConverter_state_t::bank_e0;
```



sum h(t)^2 on that grid; the bank's own normaliser, and the AGC's reference 


        

<hr>



### variable bank\_sps 

```C++
double RateConverter_state_t::bank_sps;
```



symbol period on the terminal's grid 
 


        

<hr>



### variable beta 

```C++
double RateConverter_state_t::beta;
```



RRC roll-off 
 


        

<hr>



### variable buf\_cap 

```C++
size_t RateConverter_state_t::buf_cap;
```




<hr>



### variable bufs 

```C++
float _Complex* RateConverter_state_t::bufs[2];
```



Ping-pong intermediate buffers, grown lazily on first execute. 


        

<hr>



### variable compensate 

```C++
int RateConverter_state_t::compensate;
```



CIC droop-comp flag 
 


        

<hr>



### variable n\_stages 

```C++
int RateConverter_state_t::n_stages;
```



active stage count 
 


        

<hr>



### variable narrow\_pulse 

```C++
bool RateConverter_state_t::narrow_pulse;
```



Set when a rectangular pulse was selected with fewer than four output samples per symbol, where its matched filter degenerates to a 2-3 tap sum. Read by the binding, which turns it into a UserWarning. 


        

<hr>



### variable num\_phases 

```C++
size_t RateConverter_state_t::num_phases;
```



terminal-stage arms (power of two) 
 


        

<hr>



### variable pulse 

```C++
int RateConverter_state_t::pulse;
```



rc\_pulse\_t; RC\_PULSE\_NONE when not matched 
 


        

<hr>



### variable pulse\_sps 

```C++
double RateConverter_state_t::pulse_sps;
```



symbol period in OUTPUT samples 
 


        

<hr>



### variable rate 

```C++
double RateConverter_state_t::rate;
```



current rate ratio 
 


        

<hr>



### variable span 

```C++
size_t RateConverter_state_t::span;
```



one-sided RRC span, symbols 
 


        

<hr>



### variable stage\_ptrs 

```C++
void* RateConverter_state_t::stage_ptrs[RC_MAX_STAGES];
```



sub-object per slot 
 


        

<hr>



### variable stage\_types 

```C++
rc_stage_t RateConverter_state_t::stage_types[RC_MAX_STAGES];
```



stage type per slot 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/RateConverter/RateConverter_core.h`

