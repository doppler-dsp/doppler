

# Struct ber\_interval\_t



[**ClassList**](annotated.md) **>** [**ber\_interval\_t**](structber__interval__t.md)



_A rate with its exact interval. Assert on_ `lo` _, never on_`p_hat` _._[More...](#detailed-description)

* `#include <ber_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**conf**](#variable-conf)  <br> |
|  size\_t | [**errors**](#variable-errors)  <br> |
|  double | [**hi**](#variable-hi)  <br> |
|  double | [**lo**](#variable-lo)  <br> |
|  double | [**p\_hat**](#variable-p_hat)  <br> |
|  double | [**rel**](#variable-rel)  <br> |
|  size\_t | [**symbols**](#variable-symbols)  <br> |












































## Detailed Description


Under inverse binomial sampling the trials `N` are the random variable, not the errors, so the naive `r/N` is biased (the unbiased estimator is `(r-1)/(N-1)`) and the interval is the Gamma/chi-square one, `[chi2_{a/2}(2r)/2N, chi2_{1-a/2}(2r)/2N]`. Comparing `lo` against a spec is the form that cannot flake on counting noise; comparing `p_hat` will. 


    
## Public Attributes Documentation




### variable conf 

```C++
double ber_interval_t::conf;
```



Confidence level used. 
 


        

<hr>



### variable errors 

```C++
size_t ber_interval_t::errors;
```



`r`. 
 


        

<hr>



### variable hi 

```C++
double ber_interval_t::hi;
```



Upper confidence limit. 
 


        

<hr>



### variable lo 

```C++
double ber_interval_t::lo;
```



Lower confidence limit. 
 


        

<hr>



### variable p\_hat 

```C++
double ber_interval_t::p_hat;
```



Unbiased point estimate `(r-1)/(N-1)`. 
 


        

<hr>



### variable rel 

```C++
double ber_interval_t::rel;
```



Relative standard error `1/sqrt(r)`. 
 


        

<hr>



### variable symbols 

```C++
size_t ber_interval_t::symbols;
```



`N` (or bits, for a BER). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ber/ber_core.h`

