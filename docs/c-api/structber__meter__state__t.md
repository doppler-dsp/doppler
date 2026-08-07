

# Struct ber\_meter\_state\_t



[**ClassList**](annotated.md) **>** [**ber\_meter\_state\_t**](structber__meter__state__t.md)



_BerMeter state._ [More...](#detailed-description)

* `#include <ber_meter_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**bit\_errors**](#variable-bit_errors)  <br> |
|  size\_t | [**bits**](#variable-bits)  <br> |
|  int | [**bps**](#variable-bps)  <br> |
|  size\_t | [**bursts**](#variable-bursts)  <br> |
|  double | [**conf**](#variable-conf)  <br> |
|  size\_t | [**errors**](#variable-errors)  <br> |
|  [**ber\_align\_t**](structber__align__t.md) | [**last**](#variable-last)  <br> |
|  int | [**m**](#variable-m)  <br> |
|  size\_t | [**mk\_n**](#variable-mk_n)  <br> |
|  size\_t | [**mk\_period**](#variable-mk_period)  <br> |
|  size\_t | [**mk\_t0**](#variable-mk_t0)  <br> |
|  size\_t | [**skipped**](#variable-skipped)  <br> |
|  size\_t | [**symbols**](#variable-symbols)  <br> |
|  size\_t | [**target\_errors**](#variable-target_errors)  <br> |
|  uint8\_t \* | [**truth**](#variable-truth)  <br> |
|  size\_t | [**truth\_len**](#variable-truth_len)  <br> |












































## Detailed Description


`m` / `target_errors` / `conf` and the truth sequence are CONFIGURATION, restored by create() and set\_truth(); only the running counters are packed into a state blob. That keeps a blob small and independent of however many symbols the reference happens to be. 


    
## Public Attributes Documentation




### variable bit\_errors 

```C++
size_t ber_meter_state_t::bit_errors;
```



Gray-coded bit errors counted. 
 


        

<hr>



### variable bits 

```C++
size_t ber_meter_state_t::bits;
```



Bits scored. 
 


        

<hr>



### variable bps 

```C++
int ber_meter_state_t::bps;
```



Bits per symbol = log2(m). 
 


        

<hr>



### variable bursts 

```C++
size_t ber_meter_state_t::bursts;
```



score() calls that scored anything. 
 


        

<hr>



### variable conf 

```C++
double ber_meter_state_t::conf;
```



Two-sided confidence level. 
 


        

<hr>



### variable errors 

```C++
size_t ber_meter_state_t::errors;
```



Symbol errors counted. 
 


        

<hr>



### variable last 

```C++
ber_align_t ber_meter_state_t::last;
```



Result of the last [**ber\_meter\_align()**](ber__meter__core_8h.md#function-ber_meter_align). 
 


        

<hr>



### variable m 

```C++
int ber_meter_state_t::m;
```



Constellation order (2, 4, 8). 
 


        

<hr>



### variable mk\_n 

```C++
size_t ber_meter_state_t::mk_n;
```



Marker length used. 
 


        

<hr>



### variable mk\_period 

```C++
size_t ber_meter_state_t::mk_period;
```



Marker period used. 
 


        

<hr>



### variable mk\_t0 

```C++
size_t ber_meter_state_t::mk_t0;
```



Marker start used by that align. 
 


        

<hr>



### variable skipped 

```C++
size_t ber_meter_state_t::skipped;
```



Skipped: marker-covered or out of range. 
 


        

<hr>



### variable symbols 

```C++
size_t ber_meter_state_t::symbols;
```



Symbols scored. 
 


        

<hr>



### variable target\_errors 

```C++
size_t ber_meter_state_t::target_errors;
```



Inverse-binomial stop condition. 
 


        

<hr>



### variable truth 

```C++
uint8_t* ber_meter_state_t::truth;
```



Transmitted symbol indices (owned copy). 
 


        

<hr>



### variable truth\_len 

```C++
size_t ber_meter_state_t::truth_len;
```



How many. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ber_meter/ber_meter_core.h`

