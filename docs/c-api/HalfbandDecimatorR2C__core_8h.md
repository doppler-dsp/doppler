

# File HalfbandDecimatorR2C\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**HalfbandDecimatorR2C**](dir_de85e40507cd80906a5f603b94b719b5.md) **>** [**HalfbandDecimatorR2C\_core.h**](HalfbandDecimatorR2C__core_8h.md)

[Go to the source code of this file](HalfbandDecimatorR2C__core_8h_source.md)

_Real-input halfband 2:1 decimator with an fs/4 shift: F32 in, CF32 out (adapter over hbdecim\_r2c\_core)._ [More...](#detailed-description)

* `#include "doppler/hbdecim/hbdecim_r2c_core.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef [**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) | [**dp\_HalfbandDecimatorR2C\_state\_t**](#typedef-dp_halfbanddecimatorr2c_state_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_HalfbandDecimatorR2C\_state\_t**](HalfbandDecimatorR2C__core_8h.md#typedef-dp_halfbanddecimatorr2c_state_t) \* | [**dp\_HalfbandDecimatorR2C\_create**](#function-dp_halfbanddecimatorr2c_create) (const float \* h, size\_t h\_len) <br>_Create a real-input halfband decimator with caller-supplied FIR taps. Implements a 2:1 polyphase halfband decimator from real F32 at fs to CF32 at fs/2, with an embedded fs/4 shift: a tone at f\_in leaves at f\_in - fs/4 (input-normalised 0.25 lands at DC). The FIR branch is the same one HalfbandDecimator takes; size it with_ `doppler.resample.kaiser_num_taps(2, atten, pb, sb)` _._ |
|  void | [**dp\_HalfbandDecimatorR2C\_destroy**](#function-dp_halfbanddecimatorr2c_destroy) ([**dp\_HalfbandDecimatorR2C\_state\_t**](HalfbandDecimatorR2C__core_8h.md#typedef-dp_halfbanddecimatorr2c_state_t) \* state) <br>_Free the coefficients, delay lines and the state._  |
|  size\_t | [**dp\_HalfbandDecimatorR2C\_execute**](#function-dp_halfbanddecimatorr2c_execute) ([**dp\_HalfbandDecimatorR2C\_state\_t**](HalfbandDecimatorR2C__core_8h.md#typedef-dp_halfbanddecimatorr2c_state_t) \* state, const float \* x, size\_t x\_len, float \_Complex \* out, size\_t max\_out) <br>_Decimate real x by 2 to CF32, shifting fs/4 to DC. Each (even, odd) input pair yields one complex output. State persists between calls: a sample left unpaired at the end of a block (counting one held from the previous call) is held for the next, so contiguous blocks give identical output to one large block._  |
|  size\_t | [**dp\_HalfbandDecimatorR2C\_execute\_max\_out**](#function-dp_halfbanddecimatorr2c_execute_max_out) ([**dp\_HalfbandDecimatorR2C\_state\_t**](HalfbandDecimatorR2C__core_8h.md#typedef-dp_halfbanddecimatorr2c_state_t) \* state, size\_t x\_len) <br>_The most outputs one_ [_**dp\_HalfbandDecimatorR2C\_execute()**_](HalfbandDecimatorR2C__core_8h.md#function-dp_halfbanddecimatorr2c_execute) _call over_`x_len` _inputs can write:_`(x_len + 1) / 2` _._ |
|  size\_t | [**dp\_HalfbandDecimatorR2C\_get\_num\_taps**](#function-dp_halfbanddecimatorr2c_get_num_taps) (const [**dp\_HalfbandDecimatorR2C\_state\_t**](HalfbandDecimatorR2C__core_8h.md#typedef-dp_halfbanddecimatorr2c_state_t) \* state) <br>_Number of FIR branch taps as passed to create._  |
|  double | [**dp\_HalfbandDecimatorR2C\_get\_rate**](#function-dp_halfbanddecimatorr2c_get_rate) (const [**dp\_HalfbandDecimatorR2C\_state\_t**](HalfbandDecimatorR2C__core_8h.md#typedef-dp_halfbanddecimatorr2c_state_t) \* state) <br>_Fixed decimation rate — always 0.5._  |
|  void | [**dp\_HalfbandDecimatorR2C\_get\_state**](#function-dp_halfbanddecimatorr2c_get_state) (const [**dp\_HalfbandDecimatorR2C\_state\_t**](HalfbandDecimatorR2C__core_8h.md#typedef-dp_halfbanddecimatorr2c_state_t) \* state, void \* blob) <br>_Serialize the decimator's running state into_ `blob` _._ |
|  void | [**dp\_HalfbandDecimatorR2C\_reset**](#function-dp_halfbanddecimatorr2c_reset) ([**dp\_HalfbandDecimatorR2C\_state\_t**](HalfbandDecimatorR2C__core_8h.md#typedef-dp_halfbanddecimatorr2c_state_t) \* state) <br>_Zero the delay lines and the fs/4 phase. Coefficients and num\_taps preserved. The next execute() after reset produces the same output as a freshly created decimator fed the same input. Any sample held for the next call is dropped too._  |
|  int | [**dp\_HalfbandDecimatorR2C\_set\_state**](#function-dp_halfbanddecimatorr2c_set_state) ([**dp\_HalfbandDecimatorR2C\_state\_t**](HalfbandDecimatorR2C__core_8h.md#typedef-dp_halfbanddecimatorr2c_state_t) \* state, const void \* blob) <br>_Restore state from_ `blob` _; DP\_OK, or DP\_ERR\_INVALID if rejected._ |
|  size\_t | [**dp\_HalfbandDecimatorR2C\_state\_bytes**](#function-dp_halfbanddecimatorr2c_state_bytes) (const [**dp\_HalfbandDecimatorR2C\_state\_t**](HalfbandDecimatorR2C__core_8h.md#typedef-dp_halfbanddecimatorr2c_state_t) \* state) <br>_Serialized-state byte size (forwarded to the hbdecim\_r2c leaf)._  |




























## Detailed Description


Thin adapter over hbdecim\_r2c\_state\_t, the real-input sibling of HalfbandDecimator: one step takes a real ADC stream at fs to complex baseband at fs/2, with the band centred at fs/4 landing at DC (Architecture D2, docs/design/ddc.md). The fs/4 mix is folded into the polyphase arithmetic, so it costs no multiplies.


Lifecycle: 
```C++
static const float h[] = { 0.25f, 0.5f, 0.25f };  // the FIR branch
static float in[1024];                            // real ADC samples
static float _Complex out[512];
dp_HalfbandDecimatorR2C_state_t *r = dp_HalfbandDecimatorR2C_create (h, 3);
size_t n = dp_HalfbandDecimatorR2C_execute (r, in, 1024, out, 512);
dp_HalfbandDecimatorR2C_destroy (r);
return n == 512 ? 0 : 1;
```
 


    
## Public Types Documentation




### typedef dp\_HalfbandDecimatorR2C\_state\_t 

```C++
typedef hbdecim_r2c_state_t dp_HalfbandDecimatorR2C_state_t;
```



HalfbandDecimatorR2C state: an alias of hbdecim\_r2c\_state\_t, so every function here forwards to the dp\_hbdecim\_r2c\_\* leaf unchanged. 


        

<hr>
## Public Functions Documentation




### function dp\_HalfbandDecimatorR2C\_create 

_Create a real-input halfband decimator with caller-supplied FIR taps. Implements a 2:1 polyphase halfband decimator from real F32 at fs to CF32 at fs/2, with an embedded fs/4 shift: a tone at f\_in leaves at f\_in - fs/4 (input-normalised 0.25 lands at DC). The FIR branch is the same one HalfbandDecimator takes; size it with_ `doppler.resample.kaiser_num_taps(2, atten, pb, sb)` _._
```C++
dp_HalfbandDecimatorR2C_state_t * dp_HalfbandDecimatorR2C_create (
    const float * h,
    size_t h_len
) 
```





**Parameters:**


* `h` Float32 FIR branch coefficients (the branch of the halfband prototype with more than one nonzero tap, not the whole prototype). Copied; must be non-NULL. 
* `h_len` Number of coefficients in h (the FIR branch length reported by num\_taps). Must be &gt; 0. 



**Returns:**

A new state owned by the caller (free with [**dp\_HalfbandDecimatorR2C\_destroy()**](HalfbandDecimatorR2C__core_8h.md#function-dp_halfbanddecimatorr2c_destroy)); NULL if h is NULL, h\_len is 0, or allocation fails.



```C++
>>> from doppler.resample import HalfbandDecimatorR2C
>>> import numpy as np
>>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
...              dtype=np.float32)
>>> hb = HalfbandDecimatorR2C(h=h)
>>> hb.num_taps, hb.rate
(5, 0.5)
```
 


        

<hr>



### function dp\_HalfbandDecimatorR2C\_destroy 

_Free the coefficients, delay lines and the state._ 
```C++
void dp_HalfbandDecimatorR2C_destroy (
    dp_HalfbandDecimatorR2C_state_t * state
) 
```





**Parameters:**


* `state` State from [**dp\_HalfbandDecimatorR2C\_create()**](HalfbandDecimatorR2C__core_8h.md#function-dp_halfbanddecimatorr2c_create); NULL is a no-op. 




        

<hr>



### function dp\_HalfbandDecimatorR2C\_execute 

_Decimate real x by 2 to CF32, shifting fs/4 to DC. Each (even, odd) input pair yields one complex output. State persists between calls: a sample left unpaired at the end of a block (counting one held from the previous call) is held for the next, so contiguous blocks give identical output to one large block._ 
```C++
size_t dp_HalfbandDecimatorR2C_execute (
    dp_HalfbandDecimatorR2C_state_t * state,
    const float * x,
    size_t x_len,
    float _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Pointer to a valid dp\_HalfbandDecimatorR2C\_state\_t. 
* `x` Real F32 input array. 
* `x_len` Number of input samples. 
* `out` CF32 output buffer; must hold at least (x\_len + 1) / 2 samples, which is [**dp\_HalfbandDecimatorR2C\_execute\_max\_out()**](HalfbandDecimatorR2C__core_8h.md#function-dp_halfbanddecimatorr2c_execute_max_out). Python refuses a shorter `out=`. In C a shorter one drops nothing: the call stops at a pair boundary and [**dp\_hbdecim\_r2c\_consumed()**](hbdecim__r2c__core_8h.md#function-dp_hbdecim_r2c_consumed) says how much input it took. 
* `max_out` Capacity of `out` in elements. 



**Returns:**

Number of CF32 samples written to `out:` min((x\_len + held) / 2, max\_out). 0 if x\_len or max\_out is 0.



```C++
>>> from doppler.resample import HalfbandDecimatorR2C
>>> import numpy as np
>>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
...              dtype=np.float32)
>>> hb = HalfbandDecimatorR2C(h=h)
>>> y = hb.execute(np.zeros(100, dtype=np.float32))
>>> y.shape, y.dtype
((50,), dtype('complex64'))
```
 


        

<hr>



### function dp\_HalfbandDecimatorR2C\_execute\_max\_out 

_The most outputs one_ [_**dp\_HalfbandDecimatorR2C\_execute()**_](HalfbandDecimatorR2C__core_8h.md#function-dp_halfbanddecimatorr2c_execute) _call over_`x_len` _inputs can write:_`(x_len + 1) / 2` _._
```C++
size_t dp_HalfbandDecimatorR2C_execute_max_out (
    dp_HalfbandDecimatorR2C_state_t * state,
    size_t x_len
) 
```



One output per input pair, and at most one sample held over from the previous call, so `(x_len + held) / 2` never exceeds this.




**Parameters:**


* `state` State; ignored. 
* `x_len` Number of inputs the call will be given. 



**Returns:**

`(x_len + 1) / 2`; 0 for an empty call. 





        

<hr>



### function dp\_HalfbandDecimatorR2C\_get\_num\_taps 

_Number of FIR branch taps as passed to create._ 
```C++
size_t dp_HalfbandDecimatorR2C_get_num_taps (
    const dp_HalfbandDecimatorR2C_state_t * state
) 
```





**Parameters:**


* `state` State. Must be non-NULL. 



**Returns:**

The h\_len passed to [**dp\_HalfbandDecimatorR2C\_create()**](HalfbandDecimatorR2C__core_8h.md#function-dp_halfbanddecimatorr2c_create).



```C++
>>> from doppler.resample import HalfbandDecimatorR2C
>>> import numpy as np
>>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
...              dtype=np.float32)
>>> HalfbandDecimatorR2C(h=h).num_taps
5
```
 


        

<hr>



### function dp\_HalfbandDecimatorR2C\_get\_rate 

_Fixed decimation rate — always 0.5._ 
```C++
double dp_HalfbandDecimatorR2C_get_rate (
    const dp_HalfbandDecimatorR2C_state_t * state
) 
```





**Parameters:**


* `state` State; ignored. 



**Returns:**

Always 0.5 (output rate / input rate).



```C++
>>> from doppler.resample import HalfbandDecimatorR2C
>>> import numpy as np
>>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
...              dtype=np.float32)
>>> HalfbandDecimatorR2C(h=h).rate
0.5
```
 


        

<hr>



### function dp\_HalfbandDecimatorR2C\_get\_state 

_Serialize the decimator's running state into_ `blob` _._
```C++
void dp_HalfbandDecimatorR2C_get_state (
    const dp_HalfbandDecimatorR2C_state_t * state,
    void * blob
) 
```



The blob is the hbdecim\_r2c leaf's envelope: the delay lines, the fs/4 phase and any held trailing sample.




**Parameters:**


* `state` State. Must be non-NULL. Not modified. 
* `blob` Caller-owned buffer of at least dp\_HalfbandDecimatorR2C\_state\_bytes(state) bytes. 




        

<hr>



### function dp\_HalfbandDecimatorR2C\_reset 

_Zero the delay lines and the fs/4 phase. Coefficients and num\_taps preserved. The next execute() after reset produces the same output as a freshly created decimator fed the same input. Any sample held for the next call is dropped too._ 
```C++
void dp_HalfbandDecimatorR2C_reset (
    dp_HalfbandDecimatorR2C_state_t * state
) 
```





**Parameters:**


* `state` State. Must be non-NULL.


```C++
>>> from doppler.resample import HalfbandDecimatorR2C
>>> import numpy as np
>>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
...              dtype=np.float32)
>>> hb = HalfbandDecimatorR2C(h=h)
>>> x = np.ones(64, dtype=np.float32)
>>> a = hb.execute(x)
>>> hb.reset()
>>> bool(np.array_equal(hb.execute(x), a))
True
```
 


        

<hr>



### function dp\_HalfbandDecimatorR2C\_set\_state 

_Restore state from_ `blob` _; DP\_OK, or DP\_ERR\_INVALID if rejected._
```C++
int dp_HalfbandDecimatorR2C_set_state (
    dp_HalfbandDecimatorR2C_state_t * state,
    const void * blob
) 
```





**Parameters:**


* `state` State built with the same num\_taps as the source; unchanged on rejection. 
* `blob` Blob from [**dp\_HalfbandDecimatorR2C\_get\_state()**](HalfbandDecimatorR2C__core_8h.md#function-dp_halfbanddecimatorr2c_get_state) (or [**dp\_hbdecim\_r2c\_get\_state()**](hbdecim__r2c__core_8h.md#function-dp_hbdecim_r2c_get_state)). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the envelope (magic, version, endianness, size) rejects. 





        

<hr>



### function dp\_HalfbandDecimatorR2C\_state\_bytes 

_Serialized-state byte size (forwarded to the hbdecim\_r2c leaf)._ 
```C++
size_t dp_HalfbandDecimatorR2C_state_bytes (
    const dp_HalfbandDecimatorR2C_state_t * state
) 
```





**Parameters:**


* `state` State. Must be non-NULL. 



**Returns:**

dp\_hbdecim\_r2c\_state\_bytes(state); depends on num\_taps. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/HalfbandDecimatorR2C/HalfbandDecimatorR2C_core.h`

