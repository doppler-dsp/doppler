

# Struct symsync\_state\_t



[**ClassList**](annotated.md) **>** [**symsync\_state\_t**](structsymsync__state__t.md)



_SymbolSync state._ [More...](#detailed-description)

* `#include <symsync_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**base\_inc**](#variable-base_inc)  <br> |
|  double | [**bn**](#variable-bn)  <br> |
|  [**farrow\_state\_t**](structfarrow__state__t.md) | [**farrow**](#variable-farrow)  <br> |
|  int | [**have\_ontime**](#variable-have_ontime)  <br> |
|  double | [**last\_error**](#variable-last_error)  <br> |
|  [**loop\_filter\_state\_t**](structloop__filter__state__t.md) | [**lf**](#variable-lf)  <br> |
|  float complex | [**mid**](#variable-mid)  <br> |
|  float complex | [**prev\_ontime**](#variable-prev_ontime)  <br> |
|  double | [**pwr\_avg**](#variable-pwr_avg)  <br> |
|  double | [**rate\_est**](#variable-rate_est)  <br> |
|  size\_t | [**sps**](#variable-sps)  <br> |
|  [**nco\_state\_t**](structnco__state__t.md) | [**timing**](#variable-timing)  <br> |
|  double | [**zeta**](#variable-zeta)  <br> |












































## Detailed Description


Allocate with [**symsync\_create()**](symsync__core_8h.md#function-symsync_create). Embeds the integer timing NCO, the Farrow interpolator and the PI loop filter by value; treat the Gardner history as internal. 


    
## Public Attributes Documentation




### variable base\_inc 

```C++
uint32_t symsync_state_t::base_inc;
```



nominal NCO inc (one wrap / symbol). 


        

<hr>



### variable bn 

```C++
double symsync_state_t::bn;
```



loop noise bandwidth (retained). 


        

<hr>



### variable farrow 

```C++
farrow_state_t symsync_state_t::farrow;
```



fractional interpolator. 


        

<hr>



### variable have\_ontime 

```C++
int symsync_state_t::have_ontime;
```



a previous on-time sample exists. 


        

<hr>



### variable last\_error 

```C++
double symsync_state_t::last_error;
```



last Gardner timing error. 


        

<hr>



### variable lf 

```C++
loop_filter_state_t symsync_state_t::lf;
```



2nd-order timing PI loop. 


        

<hr>



### variable mid 

```C++
float complex symsync_state_t::mid;
```



mid-symbol interpolant (Gardner). 


        

<hr>



### variable prev\_ontime 

```C++
float complex symsync_state_t::prev_ontime;
```



previous on-time interpolant. 


        

<hr>



### variable pwr\_avg 

```C++
double symsync_state_t::pwr_avg;
```



running symbol power (TED normaliser). 


        

<hr>



### variable rate\_est 

```C++
double symsync_state_t::rate_est;
```



smoothed tracked samples/symbol. 


        

<hr>



### variable sps 

```C++
size_t symsync_state_t::sps;
```



nominal samples per symbol. 


        

<hr>



### variable timing 

```C++
nco_state_t symsync_state_t::timing;
```



integer timing NCO (phase/phase\_inc). 


        

<hr>



### variable zeta 

```C++
double symsync_state_t::zeta;
```



damping factor (retained). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/symsync/symsync_core.h`

