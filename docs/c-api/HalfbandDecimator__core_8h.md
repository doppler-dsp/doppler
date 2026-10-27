

# File HalfbandDecimator\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**HalfbandDecimator**](dir_7d0e752fb42c448bafa7d80e7fc4aaf0.md) **>** [**HalfbandDecimator\_core.h**](HalfbandDecimator__core_8h.md)

[Go to the source code of this file](HalfbandDecimator__core_8h_source.md)

_Halfband 2:1 decimator for CF32 IQ (adapter over hbdecim\_core)._ [More...](#detailed-description)

* `#include "doppler/hbdecim/hbdecim_core.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef [**hbdecim\_state\_t**](structhbdecim__state__t.md) | [**dp\_HalfbandDecimator\_state\_t**](#typedef-dp_halfbanddecimator_state_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-dp_halfbanddecimator_state_t) \* | [**dp\_HalfbandDecimator\_create**](#function-dp_halfbanddecimator_create) (const float \* h, size\_t h\_len) <br>_Create a HalfbandDecimator with caller-supplied FIR taps. Implements a 2:1 polyphase halfband decimator over CF32 IQ. The caller provides the FIR branch coefficient array h; use_ `doppler.resample.kaiser_num_taps(2, atten, pb, sb)` _to size it and scipy or the built-in bank helper to design the prototype. Output length is approximately x\_len / 2 per execute() call._ |
|  void | [**dp\_HalfbandDecimator\_destroy**](#function-dp_halfbanddecimator_destroy) ([**dp\_HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-dp_halfbanddecimator_state_t) \* state) <br>_Free the coefficients, delay lines and the state._  |
|  size\_t | [**dp\_HalfbandDecimator\_execute**](#function-dp_halfbanddecimator_execute) ([**dp\_HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-dp_halfbanddecimator_state_t) \* state, const float \_Complex \* x, size\_t x\_len, float \_Complex \* out, size\_t max\_out) <br>_Decimate x by 2 using the polyphase halfband FIR filter. Each (even, odd) input pair yields one output: one phase runs through the symmetric FIR branch and the other through a pure delay weighted 0.5. State persists between calls: a sample left unpaired at the end of a block (counting one held from the previous call) is held for the next, so contiguous blocks give identical output to one large block._  |
|  size\_t | [**dp\_HalfbandDecimator\_execute\_max\_out**](#function-dp_halfbanddecimator_execute_max_out) ([**dp\_HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-dp_halfbanddecimator_state_t) \* state) <br>_Default output capacity, used by the binding to size buffers._  |
|  size\_t | [**dp\_HalfbandDecimator\_get\_num\_taps**](#function-dp_halfbanddecimator_get_num_taps) (const [**dp\_HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-dp_halfbanddecimator_state_t) \* state) <br>_Number of FIR branch taps as passed to create. The pure-delay branch has no taps; only the FIR branch has length num\_taps (it runs on the even-indexed inputs when num\_taps is even, the odd-indexed ones when it is odd). The total prototype length is 2 \* num\_taps - 1._  |
|  double | [**dp\_HalfbandDecimator\_get\_rate**](#function-dp_halfbanddecimator_get_rate) (const [**dp\_HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-dp_halfbanddecimator_state_t) \* state) <br>_Fixed decimation rate — always 0.5. The halfband decimator is structurally 2:1; this property exists for API parity with Resampler and RateConverter._  |
|  void | [**dp\_HalfbandDecimator\_get\_state**](#function-dp_halfbanddecimator_get_state) (const [**dp\_HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-dp_halfbanddecimator_state_t) \* state, void \* blob) <br>_Serialize the decimator's delay-line state into_ `blob` _._ |
|  void | [**dp\_HalfbandDecimator\_reset**](#function-dp_halfbanddecimator_reset) ([**dp\_HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-dp_halfbanddecimator_state_t) \* state) <br>_Zero all delay lines. Coefficients and num\_taps preserved. Call between signal bursts to suppress transient ringing from prior filter state. The next execute() after reset produces the same output as a freshly created decimator fed the same input. Any sample held for the next call is dropped too._  |
|  int | [**dp\_HalfbandDecimator\_set\_state**](#function-dp_halfbanddecimator_set_state) ([**dp\_HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-dp_halfbanddecimator_state_t) \* state, const void \* blob) <br>_Restore state from_ `blob` _; DP\_OK, or DP\_ERR\_INVALID if rejected._ |
|  size\_t | [**dp\_HalfbandDecimator\_state\_bytes**](#function-dp_halfbanddecimator_state_bytes) (const [**dp\_HalfbandDecimator\_state\_t**](HalfbandDecimator__core_8h.md#typedef-dp_halfbanddecimator_state_t) \* state) <br>_Serialized-state byte size (forwarded to the hbdecim leaf)._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**HBDECIM\_MAX\_OUT**](HalfbandDecimator__core_8h.md#define-hbdecim_max_out)  `32768`<br> |

## Detailed Description


Thin adapter over [**hbdecim\_state\_t**](structhbdecim__state__t.md). The caller supplies a FIR coefficient array at construction; use resample.build\_bank(2, ...) from Python to generate a suitable halfband prototype.


Lifecycle: 
```C++
float h[] = { ... };  // num_taps FIR branch coefficients
dp_HalfbandDecimator_state_t *r =
    dp_HalfbandDecimator_create(h, num_taps);
float _Complex out[512];
size_t n = dp_HalfbandDecimator_execute(r, in, 1024, out, 1024);
dp_HalfbandDecimator_destroy(r);
```
 


    
## Public Types Documentation




### typedef dp\_HalfbandDecimator\_state\_t 

```C++
typedef hbdecim_state_t dp_HalfbandDecimator_state_t;
```



HalfbandDecimator state: an alias of [**hbdecim\_state\_t**](structhbdecim__state__t.md), so every function here forwards to the dp\_hbdecim\_\* leaf unchanged. 


        

<hr>
## Public Functions Documentation




### function dp\_HalfbandDecimator\_create 

_Create a HalfbandDecimator with caller-supplied FIR taps. Implements a 2:1 polyphase halfband decimator over CF32 IQ. The caller provides the FIR branch coefficient array h; use_ `doppler.resample.kaiser_num_taps(2, atten, pb, sb)` _to size it and scipy or the built-in bank helper to design the prototype. Output length is approximately x\_len / 2 per execute() call._
```C++
dp_HalfbandDecimator_state_t * dp_HalfbandDecimator_create (
    const float * h,
    size_t h_len
) 
```





**Parameters:**


* `h` Float32 FIR branch coefficients (the branch of the halfband prototype with more than one nonzero tap, not the whole prototype). Must be symmetric, `h[k] == h[h_len-1-k]`: only the first h\_len/2 are read, and for odd h\_len the centre tap is ignored in favor of the fixed 0.5 delay branch. Copied and scaled by 0.5; must be non-NULL. 
* `h_len` Number of coefficients in h (the FIR branch length reported by num\_taps). Must be &gt; 0. 



**Returns:**

A new state owned by the caller (free with [**dp\_HalfbandDecimator\_destroy()**](HalfbandDecimator__core_8h.md#function-dp_halfbanddecimator_destroy)); NULL if h is NULL, h\_len is 0, or allocation fails.



```C++
>>> from doppler.resample import HalfbandDecimator
>>> import numpy as np
>>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
...              dtype=np.float32)
>>> hb = HalfbandDecimator(h=h)
>>> hb.num_taps, hb.rate
(5, 0.5)
```
 


        

<hr>



### function dp\_HalfbandDecimator\_destroy 

_Free the coefficients, delay lines and the state._ 
```C++
void dp_HalfbandDecimator_destroy (
    dp_HalfbandDecimator_state_t * state
) 
```





**Parameters:**


* `state` State from [**dp\_HalfbandDecimator\_create()**](HalfbandDecimator__core_8h.md#function-dp_halfbanddecimator_create); NULL is a no-op. 




        

<hr>



### function dp\_HalfbandDecimator\_execute 

_Decimate x by 2 using the polyphase halfband FIR filter. Each (even, odd) input pair yields one output: one phase runs through the symmetric FIR branch and the other through a pure delay weighted 0.5. State persists between calls: a sample left unpaired at the end of a block (counting one held from the previous call) is held for the next, so contiguous blocks give identical output to one large block._ 
```C++
size_t dp_HalfbandDecimator_execute (
    dp_HalfbandDecimator_state_t * state,
    const float _Complex * x,
    size_t x_len,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Pointer to a valid dp\_HalfbandDecimator\_state\_t. 
* `x` CF32 input array. A sample left unpaired at the end is held for the next call. 
* `x_len` Number of input samples. 
* `out` Output buffer; must hold at least (x\_len + 1) / 2 samples. If it fills first, the first unconsumed sample is held and every sample after it is dropped (doppler#2643). 
* `max_out` Capacity of `out` in elements. Emission stops there, so the return value is the number actually written. 



**Returns:**

Number of CF32 samples written to `out:` min((x\_len + held) / 2, max\_out), where held is 1 if a trailing sample was carried in from the previous call. 0 if x\_len or max\_out is 0.



```C++
>>> from doppler.resample import HalfbandDecimator
>>> import numpy as np
>>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
...              dtype=np.float32)
>>> hb = HalfbandDecimator(h=h)
>>> y = hb.execute(np.zeros(100, dtype=np.complex64))
>>> y.shape, y.dtype
((50,), dtype('complex64'))
```
 


        

<hr>



### function dp\_HalfbandDecimator\_execute\_max\_out 

_Default output capacity, used by the binding to size buffers._ 
```C++
size_t dp_HalfbandDecimator_execute_max_out (
    dp_HalfbandDecimator_state_t * state
) 
```





**Parameters:**


* `state` State; ignored. 



**Returns:**

Always HBDECIM\_MAX\_OUT (32768). 





        

<hr>



### function dp\_HalfbandDecimator\_get\_num\_taps 

_Number of FIR branch taps as passed to create. The pure-delay branch has no taps; only the FIR branch has length num\_taps (it runs on the even-indexed inputs when num\_taps is even, the odd-indexed ones when it is odd). The total prototype length is 2 \* num\_taps - 1._ 
```C++
size_t dp_HalfbandDecimator_get_num_taps (
    const dp_HalfbandDecimator_state_t * state
) 
```





**Parameters:**


* `state` State. Must be non-NULL. 



**Returns:**

The h\_len passed to [**dp\_HalfbandDecimator\_create()**](HalfbandDecimator__core_8h.md#function-dp_halfbanddecimator_create).



```C++
>>> from doppler.resample import HalfbandDecimator
>>> import numpy as np
>>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
...              dtype=np.float32)
>>> HalfbandDecimator(h=h).num_taps
5
```
 


        

<hr>



### function dp\_HalfbandDecimator\_get\_rate 

_Fixed decimation rate — always 0.5. The halfband decimator is structurally 2:1; this property exists for API parity with Resampler and RateConverter._ 
```C++
double dp_HalfbandDecimator_get_rate (
    const dp_HalfbandDecimator_state_t * state
) 
```





**Parameters:**


* `state` State; ignored. 



**Returns:**

Always 0.5 (output rate / input rate).



```C++
>>> from doppler.resample import HalfbandDecimator
>>> import numpy as np
>>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
...              dtype=np.float32)
>>> HalfbandDecimator(h=h).rate
0.5
```
 


        

<hr>



### function dp\_HalfbandDecimator\_get\_state 

_Serialize the decimator's delay-line state into_ `blob` _._
```C++
void dp_HalfbandDecimator_get_state (
    const dp_HalfbandDecimator_state_t * state,
    void * blob
) 
```



The blob is the hbdecim leaf's (HBDECIM\_STATE\_MAGIC envelope): both delay rings, their heads and any held trailing sample.




**Parameters:**


* `state` State. Must be non-NULL. Not modified. 
* `blob` Caller-owned buffer of at least dp\_HalfbandDecimator\_state\_bytes(state) bytes. 




        

<hr>



### function dp\_HalfbandDecimator\_reset 

_Zero all delay lines. Coefficients and num\_taps preserved. Call between signal bursts to suppress transient ringing from prior filter state. The next execute() after reset produces the same output as a freshly created decimator fed the same input. Any sample held for the next call is dropped too._ 
```C++
void dp_HalfbandDecimator_reset (
    dp_HalfbandDecimator_state_t * state
) 
```





**Parameters:**


* `state` State. Must be non-NULL.


```C++
>>> from doppler.resample import HalfbandDecimator
>>> import numpy as np
>>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
...              dtype=np.float32)
>>> hb = HalfbandDecimator(h=h)
>>> _ = hb.execute(np.ones(64, dtype=np.complex64))
>>> hb.reset()
>>> hb.num_taps
5
```
 


        

<hr>



### function dp\_HalfbandDecimator\_set\_state 

_Restore state from_ `blob` _; DP\_OK, or DP\_ERR\_INVALID if rejected._
```C++
int dp_HalfbandDecimator_set_state (
    dp_HalfbandDecimator_state_t * state,
    const void * blob
) 
```





**Parameters:**


* `state` State built with the same num\_taps as the source; unchanged on rejection. 
* `blob` Blob from [**dp\_HalfbandDecimator\_get\_state()**](HalfbandDecimator__core_8h.md#function-dp_halfbanddecimator_get_state) (or [**dp\_hbdecim\_get\_state()**](hbdecim__core_8h.md#function-dp_hbdecim_get_state)). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the envelope (magic, version, endianness, size) rejects. 





        

<hr>



### function dp\_HalfbandDecimator\_state\_bytes 

_Serialized-state byte size (forwarded to the hbdecim leaf)._ 
```C++
size_t dp_HalfbandDecimator_state_bytes (
    const dp_HalfbandDecimator_state_t * state
) 
```





**Parameters:**


* `state` State. Must be non-NULL. 



**Returns:**

dp\_hbdecim\_state\_bytes(state); depends on num\_taps. 





        

<hr>
## Macro Definition Documentation





### define HBDECIM\_MAX\_OUT 

```C++
#define HBDECIM_MAX_OUT `32768`
```



Output capacity reported by [**dp\_HalfbandDecimator\_execute\_max\_out()**](HalfbandDecimator__core_8h.md#function-dp_halfbanddecimator_execute_max_out). The C execute takes the caller's real max\_out, so this is not a limit there; the Python binding allocates max(HBDECIM\_MAX\_OUT, len(x)) output samples per call. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/HalfbandDecimator/HalfbandDecimator_core.h`

