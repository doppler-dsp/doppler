

# File snr\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**snr**](dir_a0dc77cb6789ae5cf19b2d0651b00ce2.md) **>** [**snr\_core.h**](snr__core_8h.md)

[Go to the source code of this file](snr__core_8h_source.md)

_Stateless SNR / Es-N0 estimators, data-aided and non-data-aided._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include <complex.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  double | [**snr\_data\_aided\_db**](#function-snr_data_aided_db) (const float complex \* soft, size\_t soft\_len, const uint8\_t \* sign\_bits, size\_t sign\_bits\_len) <br>_Data-aided Es/N0 (dB) over a block of despread symbols._  |
|  void | [**snr\_data\_aided\_db\_series**](#function-snr_data_aided_db_series) (const float complex \* soft, size\_t soft\_len, const uint8\_t \* sign\_bits, size\_t sign\_bits\_len, size\_t window, double \* out) <br>_Sliding-window data-aided Es/N0 (dB), one estimate per index._  |
|  double | [**snr\_m2m4\_db**](#function-snr_m2m4_db) (const float complex \* x, size\_t x\_len) <br>_Non-data-aided (blind) moment-based Es/N0 (dB) over a block._  |
|  void | [**snr\_m2m4\_db\_series**](#function-snr_m2m4_db_series) (const float complex \* x, size\_t x\_len, size\_t window, double \* out) <br>_Sliding-window blind (M2M4) Es/N0 (dB), one estimate per index._  |




























## Detailed Description


Two independent, pure (no persistent state) estimators over a block of complex baseband samples:



* [**snr\_data\_aided\_db()**](snr__core_8h.md#function-snr_data_aided_db): known-symbol estimator. Strip the known transmitted sign, then Es/N0 = (mean signal amplitude)^2 / (mean residual power)  the classic pilot/known-sequence SNR estimate. Needs ground truth (or trusted decisions), but is simple and unbiased.
* [**snr\_m2m4\_db()**](snr__core_8h.md#function-snr_m2m4_db): moment-based (M2M4) blind estimator (Pauluzzi & Beaulieu, "A comparison of SNR estimation techniques for the AWGN
  channel", IEEE Trans. Commun. 48(10), 2000) for a constant-modulus signal (BPSK/QPSK/M-PSK) in circular complex AWGN. No known symbols needed. SNR = sqrt(2\*M2^2 - M4) / (M2 - sqrt(2\*M2^2 - M4)), where M2/M4 are the 2nd/4th moments of \|x\|. Degenerates to 0 dB-equivalent (linear 0) for pure noise and +inf for a noiseless constant-modulus signal.




Each has a \*\_db\_series() sliding-window sibling, for visualizing SNR drift vs time/index rather than reading one block-average scalar.



```C++
double snr = snr_data_aided_db(soft, n_soft, sign_bits, n_bits);
double blind = snr_m2m4_db(x, n);
```
 


    
## Public Functions Documentation




### function snr\_data\_aided\_db 

_Data-aided Es/N0 (dB) over a block of despread symbols._ 
```C++
double snr_data_aided_db (
    const float complex * soft,
    size_t soft_len,
    const uint8_t * sign_bits,
    size_t sign_bits_len
) 
```



Strips the known transmitted sign (`soft[i] * (sign_bits[i] ? -1 : 1)`), then Es/N0 = (mean signal amplitude)^2 / (mean residual power). Scale-invariant (works regardless of the caller's symbol normalization) and polarity-invariant (a global sign flip in `soft` changes nothing, since the amplitude is squared)  so it needs no resolution of an absolute-phase ambiguity a tracking loop may carry.




**Parameters:**


* `soft` Despread complex symbols. 
* `soft_len` Length of `soft`. 
* `sign_bits` Known transmitted bits (0/1; 0 -&gt; +1, 1 -&gt; -1). 
* `sign_bits_len` Length of `sign_bits`. 



**Returns:**

Es/N0 in dB over `min(soft_len, sign_bits_len)` paired samples, or NaN if that count is 0 or the residual power is exactly 0. 
```C++
>>> import numpy as np
>>> from doppler.snr import snr_data_aided_db
>>> rng = np.random.default_rng(0)
>>> bits = (rng.random(2000) > 0.5).astype(np.uint8)
>>> sign = np.where(bits, -1.0, 1.0).astype(np.complex64)
>>> noise = (0.1 * (rng.standard_normal(2000)
...          + 1j * rng.standard_normal(2000))).astype(np.complex64)
>>> soft = (sign + noise).astype(np.complex64)
>>> round(float(snr_data_aided_db(soft, bits)), 1)
17.1
```
 





        

<hr>



### function snr\_data\_aided\_db\_series 

_Sliding-window data-aided Es/N0 (dB), one estimate per index._ 
```C++
void snr_data_aided_db_series (
    const float complex * soft,
    size_t soft_len,
    const uint8_t * sign_bits,
    size_t sign_bits_len,
    size_t window,
    double * out
) 
```



Same estimator as [**snr\_data\_aided\_db()**](snr__core_8h.md#function-snr_data_aided_db), applied to a `[i - window/2, i + window/2]` window centered (clamped at the edges) on each output index  for visualizing SNR drift vs time/index rather than reading one block-average scalar.




**Parameters:**


* `soft` Despread complex symbols. 
* `soft_len` Length of `soft`; also the output length. 
* `sign_bits` Known transmitted bits (0/1). 
* `sign_bits_len` Length of `sign_bits`; indices at or beyond this length have no known sign and are set to NaN. 
* `window` Window width in samples. 
* `out` Output, length `soft_len`. 




        

<hr>



### function snr\_m2m4\_db 

_Non-data-aided (blind) moment-based Es/N0 (dB) over a block._ 
```C++
double snr_m2m4_db (
    const float complex * x,
    size_t x_len
) 
```



M2M4 estimator (Pauluzzi & Beaulieu 2000) for a constant-modulus signal (BPSK/QPSK/M-PSK) in circular complex AWGN: no known symbols required.




**Parameters:**


* `x` Complex baseband samples (post-carrier-lock; residual phase does not bias the moment-based estimate). 
* `x_len` Length of `x`. 



**Returns:**

Es/N0 in dB, 0-linear for pure noise, +inf for a noiseless constant-modulus signal, or NaN if `x_len` is 0 or the block has zero power. 
```C++
>>> import numpy as np
>>> from doppler.snr import snr_m2m4_db
>>> rng = np.random.default_rng(0)
>>> bits = (rng.random(2000) > 0.5).astype(np.uint8)
>>> sign = np.where(bits, -1.0, 1.0).astype(np.complex64)
>>> noise = (0.1 * (rng.standard_normal(2000)
...          + 1j * rng.standard_normal(2000))).astype(np.complex64)
>>> x = (sign + noise).astype(np.complex64)
>>> round(float(snr_m2m4_db(x)), 1)
17.1
```
 





        

<hr>



### function snr\_m2m4\_db\_series 

_Sliding-window blind (M2M4) Es/N0 (dB), one estimate per index._ 
```C++
void snr_m2m4_db_series (
    const float complex * x,
    size_t x_len,
    size_t window,
    double * out
) 
```



Same estimator as [**snr\_m2m4\_db()**](snr__core_8h.md#function-snr_m2m4_db), applied to a `[i - window/2, i + window/2]` window centered (clamped at the edges) on each output index.




**Parameters:**


* `x` Complex baseband samples. 
* `x_len` Length of `x`; also the output length. 
* `window` Window width in samples. 
* `out` Output, length `x_len`. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/snr/snr_core.h`

