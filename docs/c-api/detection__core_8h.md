

# File detection\_core.h



[**FileList**](files.md) **>** [**detection**](dir_3a1e0e8c534208cc3745b2f53a028862.md) **>** [**detection\_core.h**](detection__core_8h.md)

[Go to the source code of this file](detection__core_8h_source.md)

_Detection-theory utilities for the amplitude-ratio test statistic._ [More...](#detailed-description)






































## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**det\_dwell**](#function-det_dwell) (double snr, double pd\_min, double pfa, int max\_dwell) <br>_Minimum dwell such that Pd &gt;= pd\_min for the given SNR and Pfa._  |
|  int | [**det\_dwell\_power**](#function-det_dwell_power) (double snr\_power, double pd\_min, double pfa, int max\_dwell) <br>_Minimum dwell such that Pd &gt;= pd\_min for the power detector._  |
|  double | [**det\_pd**](#function-det_pd) (double snr, int dwell, double threshold) <br>_Detection probability for given per-sample amplitude SNR and dwell._  |
|  double | [**det\_pd\_power**](#function-det_pd_power) (double snr\_power, int dwell, double power\_threshold) <br>_Detection probability for the power detector._  |
|  double | [**det\_snr**](#function-det_snr) (int dwell, double pd\_min, double pfa) <br>_Minimum per-sample amplitude SNR achieving Pd &gt;= pd\_min._  |
|  double | [**det\_snr\_power**](#function-det_snr_power) (int dwell, double pd\_min, double pfa) <br>_Minimum per-sample power SNR achieving Pd &gt;= pd\_min._  |
|  double | [**det\_threshold**](#function-det_threshold) (double pfa) <br>_Threshold eta for a given false-alarm probability._  |
|  double | [**det\_threshold\_power**](#function-det_threshold_power) (double pfa) <br>_Power threshold p from Pfa for the power detector._  |
|  double | [**marcum\_q**](#function-marcum_q) (int m, double a, double b) <br>_Marcum Q function Q\_M(a, b) for integer M &gt;= 1._  |




























## Detailed Description


The doppler detector forms the test statistic:


test\_stat = peak\_mag / noise\_est


With M-point coherent integration (dwell = M) and per-sample amplitude SNR `snr` (signal amplitude / noise amplitude, linear):


Under H0 (noise only): test\_stat ~ Rayleigh(1) Under H1 (signal+noise): test\_stat ~ Rice(a, 1), a = sqrt(2\*M) \* snr


False-alarm probability (threshold-only, M-independent):


Pfa = exp(-eta^2/2) =&gt; eta = sqrt(-2 ln Pfa) (exact)


Detection probability:


Pd = Q\_1(a, eta) (Marcum Q function, order 1)


All functions are stateless and thread-safe. 


    
## Public Functions Documentation




### function det\_dwell 

_Minimum dwell such that Pd &gt;= pd\_min for the given SNR and Pfa._ 
```C++
int det_dwell (
    double snr,
    double pd_min,
    double pfa,
    int max_dwell
) 
```



Iterates dwell = 1, 2, ..., max\_dwell, computing [**det\_pd()**](detection__core_8h.md#function-det_pd) at each step. Returns the first dwell that satisfies the Pd requirement, or -1 if none is found within max\_dwell iterations.




**Parameters:**


* `snr` Per-sample amplitude SNR (linear). 
* `pd_min` Required detection probability, e.g. 0.9. 
* `pfa` False-alarm probability; used to derive eta. 
* `max_dwell` Search upper bound; prevents infinite loops for low SNR. 



**Returns:**

Minimum dwell &gt;= 1, or -1 if not achievable.


Example: det\_dwell(0.5, 0.9, 1e-6, 256) returns the coherent integration depth needed to detect a 0.5 amplitude-SNR signal with Pd = 0.9 and Pfa = 1e-6. 


        

<hr>



### function det\_dwell\_power 

_Minimum dwell such that Pd &gt;= pd\_min for the power detector._ 
```C++
int det_dwell_power (
    double snr_power,
    double pd_min,
    double pfa,
    int max_dwell
) 
```





**Parameters:**


* `snr_power` Per-sample power SNR (linear). 
* `pd_min` Required detection probability. 
* `pfa` False-alarm probability; used to derive p. 
* `max_dwell` Search upper bound. 



**Returns:**

Minimum dwell &gt;= 1, or -1 if not achievable. 





        

<hr>



### function det\_pd 

_Detection probability for given per-sample amplitude SNR and dwell._ 
```C++
double det_pd (
    double snr,
    int dwell,
    double threshold
) 
```



Computes Pd = Q\_1(a, eta) where a = sqrt(2 \* dwell) \* snr.


At snr = 0, det\_pd returns Pfa (the false-alarm rate, as expected for a noise-only input). As snr or dwell increase, Pd approaches 1.




**Parameters:**


* `snr` Per-sample amplitude SNR (signal / noise amplitude, linear). snr = 0 gives Pd = Pfa. 
* `dwell` Coherent integration depth; must be &gt;= 1. 
* `threshold` Test-stat threshold eta, e.g. from [**det\_threshold()**](detection__core_8h.md#function-det_threshold). 



**Returns:**

Detection probability in &#91;0, 1&#93;.


Example: det\_pd(1.0, 4, det\_threshold(1e-6)) ~ 0.78 


        

<hr>



### function det\_pd\_power 

_Detection probability for the power detector._ 
```C++
double det_pd_power (
    double snr_power,
    int dwell,
    double power_threshold
) 
```



Pd = Q\_1(sqrt(2·dwell·snr\_power), sqrt(2·power\_threshold))




**Parameters:**


* `snr_power` Per-sample power SNR (signal power / noise power at the correlator output, linear). 0 gives Pd = Pfa. 
* `dwell` Coherent integration depth; must be &gt;= 1. 
* `power_threshold` Threshold p, e.g. from [**det\_threshold\_power()**](detection__core_8h.md#function-det_threshold_power). 



**Returns:**

Detection probability in &#91;0, 1&#93;.


Example: det\_pd\_power(1.0, 4, det\_threshold\_power(1e-6)) ~ 0.78 (same result as det\_pd(1.0, 4, det\_threshold(1e-6)) since snr\_power=1 corresponds to snr\_amplitude=1 and the Q\_1 arguments are identical) 


        

<hr>



### function det\_snr 

_Minimum per-sample amplitude SNR achieving Pd &gt;= pd\_min._ 
```C++
double det_snr (
    int dwell,
    double pd_min,
    double pfa
) 
```



Binary search over SNR in &#91;0, hi&#93; where hi is doubled from 1.0 until det\_pd(hi, dwell, threshold) &gt;= pd\_min. 64 bisection iterations yield ~1e-19 relative precision on the final interval.




**Parameters:**


* `dwell` Coherent integration depth; must be &gt;= 1. 
* `pd_min` Required detection probability. 
* `pfa` False-alarm probability; used to derive eta. 



**Returns:**

Minimum amplitude SNR &gt;= 0.


Roundtrip invariant: det\_pd(det\_snr(M, pd, pfa), M, det\_threshold(pfa)) &gt;= pd 


        

<hr>



### function det\_snr\_power 

_Minimum per-sample power SNR achieving Pd &gt;= pd\_min._ 
```C++
double det_snr_power (
    int dwell,
    double pd_min,
    double pfa
) 
```



Roundtrip invariant: det\_pd\_power(det\_snr\_power(M,pd,pfa), M, det\_threshold\_power(pfa)) &gt;= pd




**Parameters:**


* `dwell` Coherent integration depth; must be &gt;= 1. 
* `pd_min` Required detection probability. 
* `pfa` False-alarm probability. 



**Returns:**

Minimum power SNR &gt;= 0. 





        

<hr>



### function det\_threshold 

_Threshold eta for a given false-alarm probability._ 
```C++
double det_threshold (
    double pfa
) 
```



Exact closed-form inversion of Pfa = exp(-eta^2/2):


eta = sqrt(-2 \* ln(pfa))


The threshold is independent of dwell and SNR; it depends only on the desired Pfa.




**Parameters:**


* `pfa` Desired false-alarm probability; must be in (0, 1). 



**Returns:**

Threshold eta &gt; 0.


Example: det\_threshold(1e-6) ~ 5.2565 


        

<hr>



### function det\_threshold\_power 

_Power threshold p from Pfa for the power detector._ 
```C++
double det_threshold_power (
    double pfa
) 
```



Exact closed-form: P(Exponential(1) &gt; p) = exp(-p) = Pfa, so


p = -ln(Pfa)




**Parameters:**


* `pfa` Desired false-alarm probability; must be in (0, 1). 



**Returns:**

Threshold p &gt; 0.


Example: det\_threshold\_power(1e-6) = 6·ln(10) ~ 13.816 


        

<hr>



### function marcum\_q 

_Marcum Q function Q\_M(a, b) for integer M &gt;= 1._ 
```C++
double marcum_q (
    int m,
    double a,
    double b
) 
```



Probability that a Rice(a, sigma=1) random variable exceeds b. For M=1: Q\_1(a, b) = P(Rice(a,1) &gt; b). General integer M relates to the noncentral chi-squared CDF with 2M degrees of freedom.


Computed via the Poisson-weighted chi-squared series (exact for M=1, converges in ~60 terms for practical a, b &lt;= 15):


Q\_M(a, b) = sum\_{k=0}^inf w\_k \* Q\_{M+k}(0, b)


where: w\_k = exp(-u) \* u^k/k! (u = a^2/2) Q\_n(0,b) = exp(-v) \* sum\_{j=0}^{n-1} v^j/j! (v = b^2/2)


Each iteration advances both the Poisson weight and the chi-sum in O(1) using the recurrences w\_{k+1} = w\_k \* u/(k+1) and Q\_{n+1}(0,b) = Q\_n(0,b) + exp(-v)\*v^n/n!. Total cost: O(K) where K ~ max(u, M) + safety margin.


Special cases:
* a = 0: Q\_M(0, b) = exp(-b^2/2) \* sum\_{j=0}^{M-1} (b^2/2)^j/j!
* b &lt;= 0: Q\_M(a, b) = 1.0






**Parameters:**


* `m` Integration order; must be &gt;= 1. 
* `a` Non-centrality parameter (signal strength). a = 0 for H0. 
* `b` Threshold (same units as test\_stat). 



**Returns:**

Q\_M(a, b) in &#91;0, 1&#93;.


Verifiable reference values (scipy.special.marcum\_q): marcum\_q(1, 0.0, 1.0) = exp(-0.5) ~ 0.60653 marcum\_q(1, 0.0, 2.0) = exp(-2.0) ~ 0.13534 marcum\_q(2, 0.0, 2.0) = 3\*exp(-2) ~ 0.40601 marcum\_q(1, 2.0, 1.0) ~ 0.91441 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/detection/detection_core.h`

