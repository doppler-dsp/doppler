

# File wfm\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_core.h**](wfm__core_8h.md)

[Go to the source code of this file](wfm__core_8h_source.md)

_Wfmgen module — public C API._ 

* `#include "clib_common.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**bpsk\_map**](#function-bpsk_map) (const uint8\_t \* bits, size\_t bits\_len, float complex \* out) <br>_Map binary bits {0, 1} to BPSK constellation symbols (cf32). The mapping is: 0 -&gt; +1 + 0j, 1 -&gt; -1 + 0j. Output is unit-power (each symbol has magnitude 1). The imaginary component is always zero. Typically used before a carrier multiply and noise addition to build a BPSK burst without the full Synth engine._  |
|  void | [**dsss\_spread**](#function-dsss_spread) (const float complex \* syms, size\_t syms\_len, const uint8\_t \* code, size\_t code\_len, int sf, float complex \* out) <br> |
|  uint64\_t | [**mls\_poly**](#function-mls_poly) (uint32\_t n) <br>_Maximal-length-sequence primitive polynomial for a length-_ `n` _LFSR. Returns the tap mask (in the same bit convention the synth/PN engine uses for_`pn_poly = 0` _) that drives an_`n-stage` _Fibonacci LFSR through its full 2^n - 1 state period. Thin public alias over the synth engine's MLS table; valid for_`n` _in 2..64 and returns 0 otherwise._ |
|  void | [**qpsk\_map**](#function-qpsk_map) (const uint8\_t \* syms, size\_t syms\_len, float complex \* out) <br>_Map QPSK symbol indices {0, 1, 2, 3} to Gray-coded symbols (cf32). Gray coding: adjacent indices differ in exactly one bit, minimising BER at low SNR. Bit 0 (LSB) controls I, bit 1 controls Q: I = (1 - 2\*b\_i) / sqrt(2), Q = (1 - 2\*b\_q) / sqrt(2). Output is unit-power (\|sym\| = 1.0 exactly). The four constellation points lie at the cardinal diagonals of the IQ plane._  |
|  void | [**rrc\_taps**](#function-rrc_taps) (double beta, int sps, int span, float \* out) <br> |
|  float | [**wfm\_awgn\_amplitude**](#function-wfm_awgn_amplitude) (float snr\_db, float signal\_power) <br>_Compute the per-component AWGN amplitude for a target SNR. The AWGN engine uses equal-power I and Q noise: complex noise power is 2 \* amplitude². This function inverts that relationship for a given_ `signal_power` _and_`snr_db` _(measured over the full sample-rate bandwidth): amplitude = sqrt(signal\_power / (2 \* 10^(snr\_db / 10))). Pass the result directly to_`awgn_create` _to get the exact noise level that corresponds to the requested SNR._ |
|  float | [**wfm\_ebno\_to\_snr\_db**](#function-wfm_ebno_to_snr_db) (float ebno\_db, int bits\_per\_symbol, float samples\_per\_symbol) <br>_Convert Eb/No (dB) to SNR (dB) over the full sample-rate band. Digital communication systems are typically specified in Eb/No; doppler uses an fs-band SNR internally. The conversion is: SNR\_fs = Eb/No + 10 log10(bits\_per\_symbol) - 10 log10(samples\_per\_symbol) For BPSK (bits\_per\_symbol=1, sps=8) at Eb/No=10 dB this gives ~0.97 dB. For QPSK (bits\_per\_symbol=2, sps=8) at Eb/No=10 dB this gives ~3.98 dB._  |




























## Public Functions Documentation




### function bpsk\_map 

_Map binary bits {0, 1} to BPSK constellation symbols (cf32). The mapping is: 0 -&gt; +1 + 0j, 1 -&gt; -1 + 0j. Output is unit-power (each symbol has magnitude 1). The imaginary component is always zero. Typically used before a carrier multiply and noise addition to build a BPSK burst without the full Synth engine._ 
```C++
void bpsk_map (
    const uint8_t * bits,
    size_t bits_len,
    float complex * out
) 
```





**Parameters:**


* `bits` Array of uint8 values; only the LSB of each byte is used. 
* `bits_len` Number of elements in `bits`. 
* `out` Output buffer of at least `bits_len` cf32 elements. 
```C++
>>> from doppler.wfm import bpsk_map
>>> import numpy as np
>>> bits = np.array([0, 1, 0, 1], dtype=np.uint8)
>>> bpsk_map(bits).tolist()
[(1+0j), (-1+0j), (1+0j), (-1+0j)]
```
 




        

<hr>



### function dsss\_spread 

```C++
void dsss_spread (
    const float complex * syms,
    size_t syms_len,
    const uint8_t * code,
    size_t code_len,
    int sf,
    float complex * out
) 
```




<hr>



### function mls\_poly 

_Maximal-length-sequence primitive polynomial for a length-_ `n` _LFSR. Returns the tap mask (in the same bit convention the synth/PN engine uses for_`pn_poly = 0` _) that drives an_`n-stage` _Fibonacci LFSR through its full 2^n - 1 state period. Thin public alias over the synth engine's MLS table; valid for_`n` _in 2..64 and returns 0 otherwise._
```C++
uint64_t mls_poly (
    uint32_t n
) 
```





**Parameters:**


* `n` LFSR length in stages (2..64). 



**Returns:**

Primitive-polynomial tap mask, or 0 if `n` is out of range. 
```C++
>>> from doppler.wfm import mls_poly
>>> hex(mls_poly(7))
'0x41'
```
 





        

<hr>



### function qpsk\_map 

_Map QPSK symbol indices {0, 1, 2, 3} to Gray-coded symbols (cf32). Gray coding: adjacent indices differ in exactly one bit, minimising BER at low SNR. Bit 0 (LSB) controls I, bit 1 controls Q: I = (1 - 2\*b\_i) / sqrt(2), Q = (1 - 2\*b\_q) / sqrt(2). Output is unit-power (\|sym\| = 1.0 exactly). The four constellation points lie at the cardinal diagonals of the IQ plane._ 
```C++
void qpsk_map (
    const uint8_t * syms,
    size_t syms_len,
    float complex * out
) 
```





**Parameters:**


* `syms` Array of uint8 symbol indices; values must be in {0,1,2,3}. Bits above position 1 are ignored. 
* `syms_len` Number of elements in `syms`. 
* `out` Output buffer of at least `syms_len` cf32 elements. 
```C++
>>> from doppler.wfm import qpsk_map
>>> import numpy as np
>>> idx = np.array([0, 1, 2, 3], dtype=np.uint8)
>>> out = qpsk_map(idx)
>>> [round(float(v.real), 4) for v in out]
[0.7071, -0.7071, 0.7071, -0.7071]
>>> [round(float(v.imag), 4) for v in out]
[0.7071, 0.7071, -0.7071, -0.7071]
```
 




        

<hr>



### function rrc\_taps 

```C++
void rrc_taps (
    double beta,
    int sps,
    int span,
    float * out
) 
```




<hr>



### function wfm\_awgn\_amplitude 

_Compute the per-component AWGN amplitude for a target SNR. The AWGN engine uses equal-power I and Q noise: complex noise power is 2 \* amplitude². This function inverts that relationship for a given_ `signal_power` _and_`snr_db` _(measured over the full sample-rate bandwidth): amplitude = sqrt(signal\_power / (2 \* 10^(snr\_db / 10))). Pass the result directly to_`awgn_create` _to get the exact noise level that corresponds to the requested SNR._
```C++
float wfm_awgn_amplitude (
    float snr_db,
    float signal_power
) 
```





**Parameters:**


* `snr_db` Target SNR in dB, referenced to the full sample rate. 
* `signal_power` RMS power of the signal (e.g. 1.0 for unit-power complex tones or unit-energy BPSK/QPSK symbols). 



**Returns:**

Per-component AWGN amplitude (sigma for one I or Q channel). 
```C++
>>> from doppler.wfm import wfm_awgn_amplitude
>>> round(float(wfm_awgn_amplitude(10.0, 1.0)), 6)
0.223607
>>> round(float(wfm_awgn_amplitude(0.0, 1.0)), 6)
0.707107
```
 





        

<hr>



### function wfm\_ebno\_to\_snr\_db 

_Convert Eb/No (dB) to SNR (dB) over the full sample-rate band. Digital communication systems are typically specified in Eb/No; doppler uses an fs-band SNR internally. The conversion is: SNR\_fs = Eb/No + 10 log10(bits\_per\_symbol) - 10 log10(samples\_per\_symbol) For BPSK (bits\_per\_symbol=1, sps=8) at Eb/No=10 dB this gives ~0.97 dB. For QPSK (bits\_per\_symbol=2, sps=8) at Eb/No=10 dB this gives ~3.98 dB._ 
```C++
float wfm_ebno_to_snr_db (
    float ebno_db,
    int bits_per_symbol,
    float samples_per_symbol
) 
```





**Parameters:**


* `ebno_db` Eb/No in dB (energy per bit over noise spectral density). 
* `bits_per_symbol` Bits carried per modulation symbol: 1 for BPSK, 2 for QPSK. 
* `samples_per_symbol` Oversampling ratio (sps), e.g. 8.0. 



**Returns:**

SNR in dB measured over the full sample-rate bandwidth. 
```C++
>>> from doppler.wfm import wfm_ebno_to_snr_db
>>> round(float(wfm_ebno_to_snr_db(10.0, 2, 8.0)), 4)
3.9794
>>> round(float(wfm_ebno_to_snr_db(10.0, 1, 8.0)), 4)
0.9691
```
 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_core.h`

