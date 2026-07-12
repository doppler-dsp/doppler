

# File detection\_core.h



[**FileList**](files.md) **>** [**detection**](dir_3a1e0e8c534208cc3745b2f53a028862.md) **>** [**detection\_core.h**](detection__core_8h.md)

[Go to the source code of this file](detection__core_8h_source.md)

_Detection-theory utilities for the amplitude-ratio test statistic._ [More...](#detailed-description)






































## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**det\_dwell**](#function-det_dwell) (double snr, double pd\_min, double pfa, int max\_dwell) <br>_Minimum dwell such that Pd &gt;= pd\_min for the given SNR and Pfa._  |
|  int | [**det\_dwell\_power**](#function-det_dwell_power) (double snr\_power, double pd\_min, double pfa, int max\_dwell) <br>_Minimum dwell such that Pd &gt;= pd\_min for the power detector._  |
|  double | [**det\_ema\_alpha**](#function-det_ema_alpha) (double snr\_in\_db, double snr\_out\_db) <br>_EMA coefficient for a target estimator SNR (DC level in noise)._  |
|  int | [**det\_n\_noncoh**](#function-det_n_noncoh) (double snr, int n\_coh, double pd\_min, double pfa, int max\_n\_noncoh) <br>_Minimum non-coherent looks achieving Pd &gt;= pd\_min at fixed n\_coh._  |
|  double | [**det\_pd**](#function-det_pd) (double snr, int dwell, double threshold) <br>_Detection probability for given per-sample amplitude SNR and dwell._  |
|  double | [**det\_pd\_noncoherent**](#function-det_pd_noncoherent) (double snr, int n\_coh, int n\_noncoh, double threshold) <br>_Detection probability for n\_noncoh non-coherent looks._  |
|  double | [**det\_pd\_power**](#function-det_pd_power) (double snr\_power, int dwell, double power\_threshold) <br>_Detection probability for the power detector._  |
|  double | [**det\_snr**](#function-det_snr) (int dwell, double pd\_min, double pfa) <br>_Minimum per-sample amplitude SNR achieving Pd &gt;= pd\_min._  |
|  double | [**det\_snr\_power**](#function-det_snr_power) (int dwell, double pd\_min, double pfa) <br>_Minimum per-sample power SNR achieving Pd &gt;= pd\_min._  |
|  double | [**det\_threshold**](#function-det_threshold) (double pfa) <br>_Threshold eta for a given false-alarm probability._  |
|  double | [**det\_threshold\_f**](#function-det_threshold_f) (double pfa, int n) <br>_Upper quantile of F(n, n) — the exact H0 law for a ratio test whose noise reference is estimated from as many samples as the signal sum._  |
|  double | [**det\_threshold\_noncoherent**](#function-det_threshold_noncoherent) (double pfa, int n\_noncoh) <br>_CFAR threshold eta\_nc for a non-coherent detector of n\_noncoh looks._  |
|  double | [**det\_threshold\_power**](#function-det_threshold_power) (double pfa) <br>_Power threshold p from Pfa for the power detector._  |
|  int | [**det\_verify\_count**](#function-det_verify_count) (double p\_look, double p\_target) <br>_Verify count: consecutive looks needed to compound to a budget._  |
|  double | [**det\_verify\_delay**](#function-det_verify_delay) (double p\_look, int n) <br>_Expected looks until a run of n consecutive successes completes._  |
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



```C++
>>> from doppler.detection import det_dwell
>>> det_dwell(snr=0.5, pd_min=0.9, pfa=1e-6, max_dwell=256)
84
```
 


        

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



```C++
>>> from doppler.detection import det_dwell_power
>>> det_dwell_power(snr_power=0.25, pd_min=0.9, pfa=1e-6, max_dwell=256)
84
```
 


        

<hr>



### function det\_ema\_alpha 

_EMA coefficient for a target estimator SNR (DC level in noise)._ 
```C++
double det_ema_alpha (
    double snr_in_db,
    double snr_out_db
) 
```



Sizes a first-order EMA `y = (1-alpha)*y + alpha*x` that estimates a DC level from noisy i.i.d. measurements x. Per sample the estimator SNR (mean^2 / variance) is `snr_in`; the EMA improves it by its variance reduction `(2-alpha)/alpha`, so the output SNR is `snr_out = snr_in * (2-alpha)/alpha`. Solving for the coefficient:


alpha = 2 \* snr\_in / (snr\_in + snr\_out) (SNRs linear)


Returns 1.0 (no averaging) when snr\_out\_db &lt;= snr\_in\_db. Typical inputs: a signal-free power reference \|n\|^2 is exponential (0 dB per sample); a lock signal at known C/N0 has per-look SNR from its coherent integration (minus squaring loss), and this picks the smoothing bandwidth that makes the lock decision variable meet a chosen decision SNR.




**Parameters:**


* `snr_in_db` Per-sample estimator SNR, dB (mean^2 / variance). 
* `snr_out_db` Desired EMA-output estimator SNR, dB. 



**Returns:**

EMA coefficient alpha in (0, 1].



```C++
>>> from doppler.detection import det_ema_alpha
>>> det_ema_alpha(0.0, 0.0)      # no gain requested -> no averaging
1.0
>>> round(1 / det_ema_alpha(0.0, 20.0), 1)   # 20 dB gain ~ 50 looks
50.5
>>> round(1 / det_ema_alpha(10.0, 30.0), 1)  # same 20 dB gain, shifted
50.5
```
 


        

<hr>



### function det\_n\_noncoh 

_Minimum non-coherent looks achieving Pd &gt;= pd\_min at fixed n\_coh._ 
```C++
int det_n_noncoh (
    double snr,
    int n_coh,
    double pd_min,
    double pfa,
    int max_n_noncoh
) 
```



Iterates n\_noncoh = 1, 2, ..., max\_n\_noncoh, recomputing the threshold (det\_threshold\_noncoherent, which grows with the look count) at each step. Returns the first look count that meets the Pd requirement, or -1 if none does within max\_n\_noncoh. Used by the acquisition engine's (M, N\_nc) split.




**Parameters:**


* `snr` Per-sample amplitude SNR (linear). 
* `n_coh` Coherent integration length in samples (dwell \* N). 
* `pd_min` Required detection probability, e.g. 0.9. 
* `pfa` Per-test false-alarm probability. 
* `max_n_noncoh` Search upper bound on the look count. 



**Returns:**

Minimum n\_noncoh &gt;= 1, or -1 if not achievable.



```C++
>>> from doppler.detection import det_n_noncoh
>>> det_n_noncoh(snr=2.0, n_coh=16, pd_min=0.9, pfa=1e-3, max_n_noncoh=64)
1
```
 


        

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



```C++
>>> from doppler.detection import det_pd, det_threshold
>>> thr = det_threshold(pfa=1e-6)
>>> round(det_pd(snr=1.613, dwell=8, threshold=thr), 2)  # 8-dwell -> Pd~0.9
0.9
>>> round(det_pd(snr=0.0, dwell=8, threshold=thr), 6)    # snr=0 -> Pd=Pfa
1e-06
```
 


        

<hr>



### function det\_pd\_noncoherent 

_Detection probability for n\_noncoh non-coherent looks._ 
```C++
double det_pd_noncoherent (
    double snr,
    int n_coh,
    int n_noncoh,
    double threshold
) 
```



Computes Pd = Q\_{n\_noncoh}(a, threshold) with the non-centrality a = sqrt(2 \* n\_coh \* n\_noncoh) \* snr. At n\_noncoh = 1 this is exactly det\_pd(snr, n\_coh, threshold); at snr = 0 it returns the per-test Pfa.




**Parameters:**


* `snr` Per-sample amplitude SNR (signal / noise amplitude). 
* `n_coh` Coherent integration length in samples (dwell \* N). 
* `n_noncoh` Number of non-coherent looks; must be &gt;= 1. 
* `threshold` Threshold eta\_nc, e.g. from [**det\_threshold\_noncoherent()**](detection__core_8h.md#function-det_threshold_noncoherent). 



**Returns:**

Detection probability in &#91;0, 1&#93;.



```C++
>>> from doppler.detection import det_pd_noncoherent, det_pd, det_threshold
>>> from doppler.detection import det_threshold_noncoherent
>>> eta = det_threshold(pfa=1e-6)
>>> det_pd_noncoherent(snr=0.5, n_coh=8, n_noncoh=1, threshold=eta) \
...     == det_pd(snr=0.5, dwell=8, threshold=eta)        # reduces to coherent
True
>>> eta4 = det_threshold_noncoherent(pfa=1e-3, n_noncoh=4)
>>> round(det_pd_noncoherent(snr=0.3, n_coh=16, n_noncoh=4, threshold=eta4), 2)
0.19
```
 


        

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



```C++
>>> from doppler.detection import det_pd_power, det_threshold_power
>>> thr = det_threshold_power(pfa=1e-6)
>>> round(det_pd_power(snr_power=2.6017, dwell=8, power_threshold=thr), 2)
0.9
```
 The result equals [**det\_pd()**](detection__core_8h.md#function-det_pd) at the equivalent amplitude SNR: power SNR `s` corresponds to amplitude SNR `sqrt(s)`, and the Q\_1 arguments match. 


        

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



```C++
>>> from doppler.detection import det_snr, det_pd, det_threshold
>>> snr = det_snr(dwell=8, pd_min=0.9, pfa=1e-6)
>>> round(snr, 3)
1.613
>>> det_pd(snr=snr, dwell=8, threshold=det_threshold(pfa=1e-6)) >= 0.9
True
```
 


        

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





**Parameters:**


* `dwell` Coherent integration depth; must be &gt;= 1. 
* `pd_min` Required detection probability. 
* `pfa` False-alarm probability. 



**Returns:**

Minimum power SNR &gt;= 0.



```C++
>>> from doppler.detection import (det_snr_power, det_pd_power,
...                                det_threshold_power)
>>> sp = det_snr_power(dwell=8, pd_min=0.9, pfa=1e-6)
>>> round(sp, 4)
2.6017
>>> det_pd_power(snr_power=sp, dwell=8,
...              power_threshold=det_threshold_power(pfa=1e-6)) >= 0.9
True
```
 


        

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



```C++
>>> from doppler.detection import det_threshold
>>> round(det_threshold(pfa=1e-6), 4)
5.2565
```
 


        

<hr>



### function det\_threshold\_f 

_Upper quantile of F(n, n) — the exact H0 law for a ratio test whose noise reference is estimated from as many samples as the signal sum._ 
```C++
double det_threshold_f (
    double pfa,
    int n
) 
```



A chi-square threshold (det\_threshold\_noncoherent) prices a statistic normalised by a KNOWN noise power. When the noise power is instead estimated from n same-burst samples (the BurstDespreader lock test: sum Re^2 against sum Im^2), the ratio's tail fattens to F(n, n) and the chi-square gate realizes tens of times the priced pfa (41x at n = 16, pfa = 1e-3). This helper returns the exact gate: P(chi2\_n / chi2\_n &gt; g) = I\_{1/(1+g)}(n/2, n/2) = pfa, solved on the regularized incomplete beta — valid for every n &gt;= 1, odd included. As n grows the estimate hardens and g approaches the known-noise value. Threshold a BurstDespreader as `lock_stat > sqrt(stat_n * det_threshold_f(pfa, stat_n))`.




**Parameters:**


* `pfa` Tail probability budget, in (0, 1). 
* `n` Degrees of freedom on each side (&gt;= 1). 



**Returns:**

The F(n, n) upper-pfa quantile; 0 on invalid input.



```C++
>>> from doppler.detection import det_threshold_f
>>> round(det_threshold_f(1e-3, 2), 6)  # exact: (1 - pfa)/pfa
999.0
>>> round(det_threshold_f(1e-3, 4), 4)
53.4358
>>> round(det_threshold_f(1e-3, 64), 4)  # hardens toward known-noise
2.1931
```
 


        

<hr>



### function det\_threshold\_noncoherent 

_CFAR threshold eta\_nc for a non-coherent detector of n\_noncoh looks._ 
```C++
double det_threshold_noncoherent (
    double pfa,
    int n_noncoh
) 
```



Solves marcum\_q(n\_noncoh, 0, eta\_nc) = pfa (the order-M central tail, monotone decreasing in eta\_nc) by bisection. For n\_noncoh = 1 this is the exact closed form sqrt(-2 ln pfa) (== det\_threshold).




**Parameters:**


* `pfa` Per-test false-alarm probability in (0, 1). 
* `n_noncoh` Number of non-coherent looks; must be &gt;= 1. 



**Returns:**

Threshold eta\_nc on the normalized statistic R.



```C++
>>> from doppler.detection import det_threshold_noncoherent, det_threshold
>>> round(det_threshold_noncoherent(pfa=1e-3, n_noncoh=4), 3)
5.111
>>> det_threshold_noncoherent(pfa=1e-6, n_noncoh=1) == det_threshold(pfa=1e-6)
True
```
 


        

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



```C++
>>> from doppler.detection import det_threshold_power
>>> round(det_threshold_power(pfa=1e-6), 3)   # -ln(1e-6) = 6*ln(10)
13.816
```
 


        

<hr>



### function det\_verify\_count 

_Verify count: consecutive looks needed to compound to a budget._ 
```C++
int det_verify_count (
    double p_look,
    double p_target
) 
```



n consecutive independent looks at per-look probability p compound to p^n, so the smallest n with `p_look^n <= p_target` is `ceil(ln p_target / ln p_look)` (clamped to &gt;= 1). One function serves both sides of a lock detector ([**lockdet\_core.h**](lockdet__core_8h.md)): the declare count from (per-look pfa, false-declare budget) and the drop count from (per-look miss rate 1 - pd, false-drop budget). Degenerate inputs resolve naturally: a target already met by one look returns 1; p\_look &gt;= 1 can never compound below a smaller target and returns INT\_MAX.




**Parameters:**


* `p_look` Per-look probability (pfa or 1 - pd), in (0, 1). 
* `p_target` Compound probability budget, in (0, 1). 



**Returns:**

Smallest verify count n with p\_look^n &lt;= p\_target.



```C++
>>> from doppler.detection import det_verify_count
>>> det_verify_count(1e-3, 1e-6)   # two 1e-3 looks reach 1e-6
2
>>> det_verify_count(1e-3, 1e-9)
3
>>> det_verify_count(0.5, 1e-3)    # drop side: pd = 0.5 per look
10
>>> det_verify_count(1e-3, 0.5)    # budget already met -> 1
1
```
 


        

<hr>



### function det\_verify\_delay 

_Expected looks until a run of n consecutive successes completes._ 
```C++
double det_verify_delay (
    double p_look,
    int n
) 
```



The mean waiting time of the consecutive-run process a lockdet verify counter implements: at per-look success probability p, the first run of n straight successes takes on average


`E[T]` = (1 - p^n) / (p^n \* (1 - p)) looks,


which is the declare latency bought by a verify count of n (multiply by the look period for time). Limits are handled exactly: p = 1 gives n (the run completes immediately), p = 0 gives infinity.




**Parameters:**


* `p_look` Per-look success probability (e.g. pd), in &#91;0, 1&#93;. 
* `n` Run length (the verify count); clamped to &gt;= 1. 



**Returns:**

Expected number of looks to the first length-n run.



```C++
>>> from doppler.detection import det_verify_delay
>>> det_verify_delay(1.0, 8)             # certain hits: exactly n
8.0
>>> round(det_verify_delay(0.5, 2), 6)   # 2 straight coin heads: 6
6.0
>>> round(det_verify_delay(0.9, 8), 1)
13.2
```
 


        

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



```C++
>>> from doppler.detection import marcum_q
>>> round(marcum_q(m=1, a=0.0, b=1.0), 5)   # P(Rayleigh > 1) = exp(-0.5)
0.60653
>>> round(marcum_q(m=1, a=0.0, b=2.0), 5)   # exp(-2)
0.13534
>>> round(marcum_q(m=2, a=0.0, b=2.0), 5)   # 3*exp(-2)
0.40601
>>> round(marcum_q(m=1, a=2.0, b=1.0), 5)   # signal present (a=2)
0.91811
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/detection/detection_core.h`

