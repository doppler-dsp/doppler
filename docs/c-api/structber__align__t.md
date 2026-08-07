

# Struct ber\_align\_t



[**ClassList**](annotated.md) **>** [**ber\_align\_t**](structber__align__t.md)



_Where the recovered stream sits against truth, and how sure._ 

* `#include <ber_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**lag**](#variable-lag)  <br> |
|  double | [**margin\_db**](#variable-margin_db)  <br> |
|  size\_t | [**occurrences**](#variable-occurrences)  <br> |
|  int | [**ok**](#variable-ok)  <br> |
|  double | [**phase**](#variable-phase)  <br> |
|  double | [**runner\_db**](#variable-runner_db)  <br> |
|  int | [**saturated**](#variable-saturated)  <br> |
|  size\_t | [**slips**](#variable-slips)  <br> |
|  double | [**stat**](#variable-stat)  <br> |
|  double | [**threshold**](#variable-threshold)  <br> |












































## Public Attributes Documentation




### variable lag 

```C++
int ber_align_t::lag;
```



`rx[i]` carries `truth[i + lag]`. 
 


        

<hr>



### variable margin\_db 

```C++
double ber_align_t::margin_db;
```



`20*log10(stat/threshold)` — headroom. 
 


        

<hr>



### variable occurrences 

```C++
size_t ber_align_t::occurrences;
```



Marker occurrences combined. 
 


        

<hr>



### variable ok 

```C++
int ber_align_t::ok;
```



Detected, unambiguous, unsaturated. 
 


        

<hr>



### variable phase 

```C++
double ber_align_t::phase;
```



Absolute residual constellation rotation. 
 


        

<hr>



### variable runner\_db 

```C++
double ber_align_t::runner_db;
```



Peak over runner-up, dB; ambiguity check. 
 


        

<hr>



### variable saturated 

```C++
int ber_align_t::saturated;
```



Peak on a search edge: lag\_span too small. 
 


        

<hr>



### variable slips 

```C++
size_t ber_align_t::slips;
```



Occurrences whose phase disagreed: slips. 
 


        

<hr>



### variable stat 

```C++
double ber_align_t::stat;
```



Detection statistic at the peak. 
 


        

<hr>



### variable threshold 

```C++
double ber_align_t::threshold;
```



Pfa-derived threshold it had to beat. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ber/ber_core.h`

