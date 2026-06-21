

# File hbdecim\_q15\_core.h



[**FileList**](files.md) **>** [**hbdecim\_q15**](dir_93499f550a23db63d09661ee916a0767.md) **>** [**hbdecim\_q15\_core.h**](hbdecim__q15__core_8h.md)

[Go to the source code of this file](hbdecim__q15__core_8h_source.md)

_Fixed-point halfband 2:1 decimator for interleaved IQ int16 samples._ [More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**hbdecim\_q15\_state\_t**](structhbdecim__q15__state__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**hbdecim\_q15\_state\_t**](structhbdecim__q15__state__t.md) \* | [**hbdecim\_q15\_create**](#function-hbdecim_q15_create) (size\_t num\_taps, const float \* h) <br>_Allocate and initialise a fixed-point halfband 2:1 decimator. The FIR branch coefficients are supplied as float and converted internally to Q15 with a x0.5 polyphase rate scaling. The full halfband prototype is sparse (every other tap is zero); supply only the non-zero FIR branch taps, not the full sparse prototype._  |
|  void | [**hbdecim\_q15\_destroy**](#function-hbdecim_q15_destroy) ([**hbdecim\_q15\_state\_t**](structhbdecim__q15__state__t.md) \* r) <br>_Free all heap resources owned by the decimator state. Releases the Q15 coefficient buffer, all four delay rings, and the state struct itself. Passing NULL is a no-op. The Python wrapper calls this in_ **del** _and_**exit** _; call it explicitly only for deterministic release before GC reclaims the object._ |
|  size\_t | [**hbdecim\_q15\_execute**](#function-hbdecim_q15_execute) ([**hbdecim\_q15\_state\_t**](structhbdecim__q15__state__t.md) \* r, const int16\_t \* in, size\_t n\_in, int16\_t \* out, size\_t max\_out) <br>_Decimate a block of interleaved IQ int16 samples by 2. Input must be interleaved int16\_t IQ pairs (I₀ Q₀ I₁ Q₁ …); pass a 1-D array of 2\*n\_complex elements. Each pair of complex input samples produces one complex output sample, so an array of length 2N yields at most N output pairs (2N int16 output values). If n\_in is odd the trailing IQ pair is buffered and consumed on the next call._  |
|  size\_t | [**hbdecim\_q15\_execute\_max\_out**](#function-hbdecim_q15_execute_max_out) ([**hbdecim\_q15\_state\_t**](structhbdecim__q15__state__t.md) \* r) <br>_Maximum output samples for a given input length._  |
|  size\_t | [**hbdecim\_q15\_get\_num\_taps**](#function-hbdecim_q15_get_num_taps) (const [**hbdecim\_q15\_state\_t**](structhbdecim__q15__state__t.md) \* r) <br>_FIR branch length as supplied to the constructor. This is the count of non-zero symmetric taps in the FIR branch, not the full sparse halfband prototype length. Useful for introspection when chaining multiple stages with programmatically computed filter banks._  |
|  double | [**hbdecim\_q15\_get\_rate**](#function-hbdecim_q15_get_rate) (const [**hbdecim\_q15\_state\_t**](structhbdecim__q15__state__t.md) \* r) <br>_The sample-rate reduction factor; always 0.5 for 2:1 decimation. Exposed as a read-only property so pipelines can query the rate of each stage programmatically without hard-coding the 2:1 assumption._  |
|  void | [**hbdecim\_q15\_reset**](#function-hbdecim_q15_reset) ([**hbdecim\_q15\_state\_t**](structhbdecim__q15__state__t.md) \* r) <br>_Zero all delay rings and clear the pending-sample flag. After a reset the decimator behaves identically to a freshly constructed instance: the four dual-write delay rings are zeroed and has\_pending is cleared, so no partial IQ pair carries over. Call this between unrelated signal segments to prevent inter-segment leakage._  |




























## Detailed Description


## Public Functions Documentation




### function hbdecim\_q15\_create 

_Allocate and initialise a fixed-point halfband 2:1 decimator. The FIR branch coefficients are supplied as float and converted internally to Q15 with a x0.5 polyphase rate scaling. The full halfband prototype is sparse (every other tap is zero); supply only the non-zero FIR branch taps, not the full sparse prototype._ 
```C++
hbdecim_q15_state_t * hbdecim_q15_create (
    size_t num_taps,
    const float * h
) 
```





**Parameters:**


* `num_taps` Number of FIR branch coefficients in h (&gt;= 1). 
* `h` Float FIR branch coefficients of length num\_taps. Must be symmetric (`h[k]` == `h[num_taps-1-k]`). 



**Returns:**

HBDecimQ15 instance. 
```C++
>>> import numpy as np
>>> from doppler.filter import HBDecimQ15
>>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
>>> dec = HBDecimQ15(h)
>>> dec.num_taps
3
>>> dec.rate
0.5
```
 





        

<hr>



### function hbdecim\_q15\_destroy 

_Free all heap resources owned by the decimator state. Releases the Q15 coefficient buffer, all four delay rings, and the state struct itself. Passing NULL is a no-op. The Python wrapper calls this in_ **del** _and_**exit** _; call it explicitly only for deterministic release before GC reclaims the object._
```C++
void hbdecim_q15_destroy (
    hbdecim_q15_state_t * r
) 
```




```C++
>>> import numpy as np
>>> from doppler.filter import HBDecimQ15
>>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
>>> with HBDecimQ15(h) as dec:
...     y = dec.execute(
...         np.array([1000, 0, 1000, 0, 1000, 0, 1000, 0],
...                  dtype=np.int16))
...     y.dtype
dtype('int16')
```
 


        

<hr>



### function hbdecim\_q15\_execute 

_Decimate a block of interleaved IQ int16 samples by 2. Input must be interleaved int16\_t IQ pairs (I₀ Q₀ I₁ Q₁ …); pass a 1-D array of 2\*n\_complex elements. Each pair of complex input samples produces one complex output sample, so an array of length 2N yields at most N output pairs (2N int16 output values). If n\_in is odd the trailing IQ pair is buffered and consumed on the next call._ 
```C++
size_t hbdecim_q15_execute (
    hbdecim_q15_state_t * r,
    const int16_t * in,
    size_t n_in,
    int16_t * out,
    size_t max_out
) 
```





**Parameters:**


* `r` Decimator state. 
* `in` Interleaved int16\_t IQ input array of 2\*n\_in elements (I₀ Q₀ I₁ Q₁ …). 
* `n_in` Number of complex input pairs (half the int16 element count). 
* `out` Output buffer; caller must provide space for max\_out int16\_t values. 
* `max_out` Capacity of out in int16\_t elements (&gt;= n\_in). 



**Returns:**

Number of int16\_t values written to out. 
```C++
>>> import numpy as np
>>> from doppler.filter import HBDecimQ15
>>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
>>> dec = HBDecimQ15(h)
>>> x = np.array([1000, 0, 1000, 0, 1000, 0, 1000, 0], dtype=np.int16)
>>> y = dec.execute(x)
>>> y.dtype
dtype('int16')
>>> y.shape
(4,)
>>> y.tolist()
[0, 0, 625, 0]
```
 





        

<hr>



### function hbdecim\_q15\_execute\_max\_out 

_Maximum output samples for a given input length._ 
```C++
size_t hbdecim_q15_execute_max_out (
    hbdecim_q15_state_t * r
) 
```



Returns 0 to trigger the lazy-alloc path in the Python glue: the output buffer is sized to n\_in on first call (always sufficient for 2:1). 


        

<hr>



### function hbdecim\_q15\_get\_num\_taps 

_FIR branch length as supplied to the constructor. This is the count of non-zero symmetric taps in the FIR branch, not the full sparse halfband prototype length. Useful for introspection when chaining multiple stages with programmatically computed filter banks._ 
```C++
size_t hbdecim_q15_get_num_taps (
    const hbdecim_q15_state_t * r
) 
```




```C++
>>> import numpy as np
>>> from doppler.filter import HBDecimQ15
>>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
>>> HBDecimQ15(h).num_taps
3
```
 


        

<hr>



### function hbdecim\_q15\_get\_rate 

_The sample-rate reduction factor; always 0.5 for 2:1 decimation. Exposed as a read-only property so pipelines can query the rate of each stage programmatically without hard-coding the 2:1 assumption._ 
```C++
double hbdecim_q15_get_rate (
    const hbdecim_q15_state_t * r
) 
```




```C++
>>> import numpy as np
>>> from doppler.filter import HBDecimQ15
>>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
>>> HBDecimQ15(h).rate
0.5
```
 


        

<hr>



### function hbdecim\_q15\_reset 

_Zero all delay rings and clear the pending-sample flag. After a reset the decimator behaves identically to a freshly constructed instance: the four dual-write delay rings are zeroed and has\_pending is cleared, so no partial IQ pair carries over. Call this between unrelated signal segments to prevent inter-segment leakage._ 
```C++
void hbdecim_q15_reset (
    hbdecim_q15_state_t * r
) 
```




```C++
>>> import numpy as np
>>> from doppler.filter import HBDecimQ15
>>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
>>> dec = HBDecimQ15(h)
>>> x = np.array([1000, 0, 1000, 0, 1000, 0, 1000, 0], dtype=np.int16)
>>> _ = dec.execute(x)
>>> dec.reset()
>>> y = dec.execute(x)
>>> y.tolist()
[0, 0, 625, 0]
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/hbdecim_q15/hbdecim_q15_core.h`

