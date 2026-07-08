

# File mpsk\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**mpsk**](dir_ca9d413705226c109a44c5982d79aa0f.md) **>** [**mpsk\_core.h**](mpsk__core_8h.md)

[Go to the source code of this file](mpsk__core_8h_source.md)

_M-PSK constellation: Gray-coded map / demap for BPSK, QPSK, 8PSK._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include <complex.h>`
* `#include <math.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**mpsk\_bits\_per\_symbol**](#function-mpsk_bits_per_symbol) (int m) <br>_Bits per M-PSK symbol = log2(M)._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) int | [**mpsk\_bps**](#function-mpsk_bps) (int m) <br>_Bits per M-PSK symbol = log2(M); M in {2,4,8} -&gt; {1,2,3}, else 0._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float complex | [**mpsk\_constellation**](#function-mpsk_constellation) (unsigned g, int m) <br>_Constellation point for Gray label_ `g` _(M-PSK), unit amplitude._ |
|  void | [**mpsk\_demap**](#function-mpsk_demap) (const float complex \* x, size\_t x\_len, uint8\_t \* out, int m) <br>_Hard-decide M-PSK symbols to their Gray-coded label bytes._  |
|  void | [**mpsk\_diff\_demap**](#function-mpsk_diff_demap) (const float complex \* x, size\_t x\_len, uint8\_t \* out, int m) <br>_Differential M-PSK demap: decide from the phase DIFFERENCE._  |
|  void | [**mpsk\_diff\_map**](#function-mpsk_diff_map) (const uint8\_t \* sym, size\_t sym\_len, float complex \* out, int m) <br>_Differential M-PSK map: the label selects a phase INCREMENT._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) unsigned | [**mpsk\_gray\_decode**](#function-mpsk_gray_decode) (unsigned g) <br>_Gray code -&gt; binary index (inverse of mpsk\_gray\_encode)._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) unsigned | [**mpsk\_gray\_encode**](#function-mpsk_gray_encode) (unsigned k) <br>_Binary index -&gt; Gray code (k ^ k&gt;&gt;1)._  |
|  void | [**mpsk\_map**](#function-mpsk_map) (const uint8\_t \* sym, size\_t sym\_len, float complex \* out, int m) <br>_Map Gray-coded M-PSK labels to unit-amplitude constellation points._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**mpsk\_phi0**](#function-mpsk_phi0) (int m) <br>_Constellation phase offset (radians): pi/4 for QPSK, else 0._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) unsigned | [**mpsk\_slice**](#function-mpsk_slice) (float complex y, int m, float complex \* ahat) <br>_Hard-decide_ `y` _to the nearest M-PSK point; return its Gray label._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**MPSK\_PI**](mpsk__core_8h.md#define-mpsk_pi)  `3.14159265358979323846`<br> |

## Detailed Description


The receive-side decision primitive (and its transmit inverse) for M-ary phase-shift keying. A symbol carries `log2(M)` bits packed LSB-first into one byte (0..M-1); the byte IS the **Gray-coded** label, so a slip to an adjacent constellation point flips exactly one bit. Unit amplitude; constellation:



* BPSK (M=2): {+1, -1} (phi0 = 0)
* QPSK (M=4): (+-1 +- j)/sqrt(2) (phi0 = pi/4, axis-separable I/Q)
* 8PSK (M=8): exp(j\*k\*pi/4) (phi0 = 0)




The inline helpers (mpsk\_constellation, mpsk\_slice) are the C composition API a carrier loop / receiver inlines per symbol; the module free functions (mpsk\_map/mpsk\_demap and the differential variants) are the array Python face. Memoryless functions are element-wise (one byte &lt;-&gt; one cf32); the differential variants carry phase state across the array (info on phase _differences_, which removes the M-fold carrier ambiguity).



```C++
// C: round-trip one QPSK symbol through the inline slicer
float complex ahat;
unsigned g = mpsk_slice((1.0f + 1.0f*I) * 0.70710678f, 4, &ahat); // -> 0
```
 


    
## Public Functions Documentation




### function mpsk\_bits\_per\_symbol 

_Bits per M-PSK symbol = log2(M)._ 
```C++
int mpsk_bits_per_symbol (
    int m
) 
```





**Parameters:**


* `m` M in {2,4,8}. 



**Returns:**

1, 2, or 3 (0 for an unsupported M).



```C++
>>> from doppler.mpsk import mpsk_bits_per_symbol
>>> [mpsk_bits_per_symbol(m) for m in (2, 4, 8)]
[1, 2, 3]
```
 


        

<hr>



### function mpsk\_bps 

_Bits per M-PSK symbol = log2(M); M in {2,4,8} -&gt; {1,2,3}, else 0._ 
```C++
JM_FORCEINLINE int mpsk_bps (
    int m
) 
```




<hr>



### function mpsk\_constellation 

_Constellation point for Gray label_ `g` _(M-PSK), unit amplitude._
```C++
JM_FORCEINLINE float complex mpsk_constellation (
    unsigned g,
    int m
) 
```



Maps the Gray-coded label byte (0..M-1) to its complex point `exp(j*(2*pi*k/M + phi0))`, where `k = gray_decode(g)` is the constellation index. Inverse of [**mpsk\_slice()**](mpsk__core_8h.md#function-mpsk_slice)'s returned label.




**Parameters:**


* `g` Gray label, masked to the low log2(M) bits. 
* `m` M in {2,4,8}. 



**Returns:**

Unit-amplitude constellation point. 





        

<hr>



### function mpsk\_demap 

_Hard-decide M-PSK symbols to their Gray-coded label bytes._ 
```C++
void mpsk_demap (
    const float complex * x,
    size_t x_len,
    uint8_t * out,
    int m
) 
```



Element-wise inverse of [**mpsk\_map()**](mpsk__core_8h.md#function-mpsk_map): each cf32 symbol is sliced to the nearest constellation point and its Gray label (0..M-1) is written out. A slip to an adjacent point flips exactly one bit (Gray). `out` must hold `x_len` bytes.




**Parameters:**


* `x` Received symbols (any amplitude; phase only). 
* `x_len` Number of symbols. 
* `out` Out: `x_len` Gray label bytes. 
* `m` M in {2,4,8}.


```C++
>>> import numpy as np
>>> from doppler.mpsk import mpsk_demap
>>> x = np.array([1+0j, 1j, -1+0j, -1j], dtype=np.complex64)   # 8PSK points
>>> mpsk_demap(x, 8).tolist()   # Gray labels of indices 0, 2, 4, 6
[0, 3, 6, 5]
```
 


        

<hr>



### function mpsk\_diff\_demap 

_Differential M-PSK demap: decide from the phase DIFFERENCE._ 
```C++
void mpsk_diff_demap (
    const float complex * x,
    size_t x_len,
    uint8_t * out,
    int m
) 
```



Inverse of [**mpsk\_diff\_map()**](mpsk__core_8h.md#function-mpsk_diff_map): the Gray label of each symbol is decided from the phase difference between consecutive sliced indices (the first references an implicit zero-phase start). Invariant to an unknown constant carrier phase.




**Parameters:**


* `x` Received symbols (any amplitude; phase only). 
* `x_len` Number of symbols. 
* `out` Out: `x_len` Gray label bytes. 
* `m` M in {2,4,8}.


```C++
>>> import numpy as np
>>> from doppler.mpsk import mpsk_diff_demap, mpsk_diff_map
>>> sym = np.array([2, 2, 1, 0], dtype=np.uint8)
>>> np.array_equal(mpsk_diff_demap(mpsk_diff_map(sym, 8), 8), sym)
True
```
 


        

<hr>



### function mpsk\_diff\_map 

_Differential M-PSK map: the label selects a phase INCREMENT._ 
```C++
void mpsk_diff_map (
    const uint8_t * sym,
    size_t sym_len,
    float complex * out,
    int m
) 
```



Information rides on phase _differences_: the running constellation index accumulates `gray_decode(label)` each symbol (starting from an implicit zero-phase reference), so an unknown constant carrier phase cancels at the receiver (mpsk\_diff\_demap) — resolving the M-fold ambiguity, at ~2x the symbol-error rate of coherent map(). Sequential over the array.




**Parameters:**


* `sym` Gray label bytes (0..M-1), one per symbol. 
* `sym_len` Number of symbols. 
* `out` Out: `sym_len` constellation points. 
* `m` M in {2,4,8}.


```C++
>>> import numpy as np
>>> from doppler.mpsk import mpsk_diff_map, mpsk_diff_demap
>>> sym = np.array([1, 0, 3, 2, 1], dtype=np.uint8)
>>> pts = mpsk_diff_map(sym, 4)
>>> np.array_equal(mpsk_diff_demap(pts, 4), sym)   # exact round-trip
True
>>> rot = (pts * np.exp(1j * np.pi / 2)).astype(np.complex64)  # 90 deg slip
>>> np.array_equal(mpsk_diff_demap(rot, 4)[1:], sym[1:])   # rotation-invariant
True
```
 


        

<hr>



### function mpsk\_gray\_decode 

_Gray code -&gt; binary index (inverse of mpsk\_gray\_encode)._ 
```C++
JM_FORCEINLINE unsigned mpsk_gray_decode (
    unsigned g
) 
```




<hr>



### function mpsk\_gray\_encode 

_Binary index -&gt; Gray code (k ^ k&gt;&gt;1)._ 
```C++
JM_FORCEINLINE unsigned mpsk_gray_encode (
    unsigned k
) 
```




<hr>



### function mpsk\_map 

_Map Gray-coded M-PSK labels to unit-amplitude constellation points._ 
```C++
void mpsk_map (
    const uint8_t * sym,
    size_t sym_len,
    float complex * out,
    int m
) 
```



Element-wise inverse of [**mpsk\_demap()**](mpsk__core_8h.md#function-mpsk_demap): each input byte is one symbol's log2(M) Gray-coded bits (0..M-1), each output is its cf32 point. Memoryless (absolute phase). `out` must hold `sym_len` points.




**Parameters:**


* `sym` Gray label bytes (0..M-1), one per symbol. 
* `sym_len` Number of symbols. 
* `out` Out: `sym_len` constellation points. 
* `m` M in {2,4,8}.


```C++
>>> import numpy as np
>>> from doppler.mpsk import mpsk_map, mpsk_demap
>>> sym = np.array([0, 1, 2, 3], dtype=np.uint8)   # QPSK labels
>>> pts = mpsk_map(sym, 4)
>>> np.round(np.abs(pts), 5)
array([1., 1., 1., 1.], dtype=float32)
>>> np.array_equal(mpsk_demap(pts, 4), sym)
True
```
 


        

<hr>



### function mpsk\_phi0 

_Constellation phase offset (radians): pi/4 for QPSK, else 0._ 
```C++
JM_FORCEINLINE double mpsk_phi0 (
    int m
) 
```




<hr>



### function mpsk\_slice 

_Hard-decide_ `y` _to the nearest M-PSK point; return its Gray label._
```C++
JM_FORCEINLINE unsigned mpsk_slice (
    float complex y,
    int m,
    float complex * ahat
) 
```



Picks the constellation index nearest in phase, writes that unit-amplitude point to `ahat` (the decision, for a decision-directed carrier error `Im(y * conj(ahat))`), and returns the Gray-coded label byte. Inverse of [**mpsk\_constellation()**](mpsk__core_8h.md#function-mpsk_constellation). One atan2 per call (symbol-rate, not the sample loop).




**Parameters:**


* `y` Received symbol (any amplitude; only the phase is used). 
* `m` M in {2,4,8}. 
* `ahat` Out: the nearest unit-amplitude constellation point. 



**Returns:**

Gray-coded label (0..M-1). 





        

<hr>
## Macro Definition Documentation





### define MPSK\_PI 

```C++
#define MPSK_PI `3.14159265358979323846`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/mpsk/mpsk_core.h`

