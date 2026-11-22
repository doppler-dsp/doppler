

# Struct async\_dsss\_receiver\_fine\_t



[**ClassList**](annotated.md) **>** [**async\_dsss\_receiver\_fine\_t**](structasync__dsss__receiver__fine__t.md)



[More...](#detailed-description)

* `#include <async_dsss_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**blocks**](#variable-blocks)  <br> |
|  double | [**g**](#variable-g)  <br> |
|  double | [**gamma\_sq**](#variable-gamma_sq)  <br> |
|  int | [**k**](#variable-k)  <br> |
|  size\_t | [**n\_s**](#variable-n_s)  <br> |
|  double | [**p\_miss**](#variable-p_miss)  <br> |
|  double | [**p\_w**](#variable-p_w)  <br> |
|  double | [**rho**](#variable-rho)  <br> |
|  double | [**sd\_hz**](#variable-sd_hz)  <br> |
|  double | [**thr\_db**](#variable-thr_db)  <br> |












































## Detailed Description


The fine hand-over's design: the refine's squared pair sums, located by PolynomialPhaseEstimator, and the dwell that hands over inside the carrier loop's edge with probability `p_ho`. Every field is derived by [**dp\_async\_dsss\_receiver\_fine\_design()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_fine_design) (docs/design/async-dsss-receiver.md, "The hand-over: the M-th power of the refine's partial dumps"). 


    
## Public Attributes Documentation




### variable blocks 

```C++
size_t async_dsss_receiver_fine_t::blocks;
```



the dwell, refine blocks. 
 


        

<hr>



### variable g 

```C++
double async_dsss_receiver_fine_t::g;
```



one sum's SNR, linear, with every loss. 
 


        

<hr>



### variable gamma\_sq 

```C++
double async_dsss_receiver_fine_t::gamma_sq;
```



one squared sum's SNR, linear. 
 


        

<hr>



### variable k 

```C++
int async_dsss_receiver_fine_t::k;
```



sums a code period: the smallest divisor of refine\_segments whose range K/(2MT) covers e. 


        

<hr>



### variable n\_s 

```C++
size_t async_dsss_receiver_fine_t::n_s;
```



sums in the dwell. 
 


        

<hr>



### variable p\_miss 

```C++
double async_dsss_receiver_fine_t::p_miss;
```



the check refuses the right one. 
 


        

<hr>



### variable p\_w 

```C++
double async_dsss_receiver_fine_t::p_w;
```



a wrong peak, the union bound. 
 


        

<hr>



### variable rho 

```C++
double async_dsss_receiver_fine_t::rho;
```



the estimate's output SNR, linear: n\_s gamma\_sq. 


        

<hr>



### variable sd\_hz 

```C++
double async_dsss_receiver_fine_t::sd_hz;
```



the estimate's sd, Hz, once divided by M. 
 


        

<hr>



### variable thr\_db 

```C++
double async_dsss_receiver_fine_t::thr_db;
```



the check on snr\_db: 10 log10(ln(n\_s/pfa)) + 10 gamma\_E / ln 10 (the exponential bin's mean dB under the dB of its mean). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/async_dsss_receiver/async_dsss_receiver_core.h`

