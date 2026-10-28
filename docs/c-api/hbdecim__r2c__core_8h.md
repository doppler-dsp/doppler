

# File hbdecim\_r2c\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**hbdecim**](dir_29792a392a590bbead7fbef545faea73.md) **>** [**hbdecim\_r2c\_core.h**](hbdecim__r2c__core_8h.md)

[Go to the source code of this file](hbdecim__r2c__core_8h_source.md)

_Real-to-complex halfband 2:1 decimator (Architecture D2)._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct hbdecim\_r2c\_state | [**hbdecim\_r2c\_state\_t**](#typedef-hbdecim_r2c_state_t)  <br>_Opaque real-to-complex halfband decimator state._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_hbdecim\_r2c\_consumed**](#function-dp_hbdecim_r2c_consumed) (const [**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* r) <br>_Input samples the last_ [_**dp\_hbdecim\_r2c\_execute()**_](hbdecim__r2c__core_8h.md#function-dp_hbdecim_r2c_execute) _took._ |
|  [**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* | [**dp\_hbdecim\_r2c\_create**](#function-dp_hbdecim_r2c_create) (size\_t num\_taps, const float \* h) <br>_Allocate a real-to-complex halfband decimator._  |
|  void | [**dp\_hbdecim\_r2c\_destroy**](#function-dp_hbdecim_r2c_destroy) ([**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* r) <br>_Free the coefficients, both delay lines and the state._  |
|  size\_t | [**dp\_hbdecim\_r2c\_execute**](#function-dp_hbdecim_r2c_execute) ([**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* r, const float \* in, size\_t num\_in, float \_Complex \* out, size\_t max\_out) <br>_Decimate real float32 input by 2, producing CF32._  |
|  size\_t | [**dp\_hbdecim\_r2c\_get\_num\_taps**](#function-dp_hbdecim_r2c_get_num_taps) (const [**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* r) <br>_FIR branch length N._  |
|  double | [**dp\_hbdecim\_r2c\_get\_rate**](#function-dp_hbdecim_r2c_get_rate) (const [**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* r) <br>_Output-to-input sample-rate ratio (fixed by design)._  |
|  void | [**dp\_hbdecim\_r2c\_get\_state**](#function-dp_hbdecim_r2c_get_state) (const [**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* r, void \* blob) <br>_Serialize_ `r's` _mutable state into_`blob` _._ |
|  void | [**dp\_hbdecim\_r2c\_reset**](#function-dp_hbdecim_r2c_reset) ([**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* r) <br>_Zero history and output parity without freeing._  |
|  int | [**dp\_hbdecim\_r2c\_set\_state**](#function-dp_hbdecim_r2c_set_state) ([**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* r, const void \* blob) <br>_Restore mutable state from_ `blob` _(same num\_taps)._ |
|  size\_t | [**dp\_hbdecim\_r2c\_state\_bytes**](#function-dp_hbdecim_r2c_state_bytes) (const [**hbdecim\_r2c\_state\_t**](hbdecim__r2c__core_8h.md#typedef-hbdecim_r2c_state_t) \* r) <br>_Bytes_ [_**dp\_hbdecim\_r2c\_get\_state()**_](hbdecim__r2c__core_8h.md#function-dp_hbdecim_r2c_get_state) _writes (envelope + payload)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**HBDECIM\_R2C\_STATE\_MAGIC**](hbdecim__r2c__core_8h.md#define-hbdecim_r2c_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('H', 'B', 'R', '2')`<br> |
| define  | [**HBDECIM\_R2C\_STATE\_VERSION**](hbdecim__r2c__core_8h.md#define-hbdecim_r2c_state_version)  `1u`<br> |

## Detailed Description


Lifted from dp\_hbdecim\_r2cf32\_t (c/src/hbdecim.c). Accepts real float32 input and produces CF32 IQ at half the input rate, with an embedded fs/4 frequency shift (same effect as mixing with e^{jπn/2} before decimating, at zero extra multiplications).


The output at sample m is: y(m) = (FIR(even) + j·delay(odd)) · (-1)^m


where the sign pattern (-1)^m provides the fs/4 shift correction. That form is for an even FIR branch length N; for odd N the roles swap (I comes from the delay branch, Q from the FIR on the odd stream). Both rails are then scaled by 2 so a real tone of amplitude A comes out as a complex tone of amplitude A (the analytic-signal convention), matching the unity DC gain of the complex sibling ([**dp\_hbdecim\_dc\_gain()**](hbdecim__core_8h.md#function-dp_hbdecim_dc_gain)).


Lifecycle: 
```C++
hbdecim_r2c_state_t *r = dp_hbdecim_r2c_create(num_taps, h);
size_t n = dp_hbdecim_r2c_execute(r, in, num_in, out, max_out);
dp_hbdecim_r2c_destroy(r);
```
 


    
## Public Types Documentation




### typedef hbdecim\_r2c\_state\_t 

_Opaque real-to-complex halfband decimator state._ 
```C++
typedef struct hbdecim_r2c_state hbdecim_r2c_state_t;
```



Allocate with [**dp\_hbdecim\_r2c\_create()**](hbdecim__r2c__core_8h.md#function-dp_hbdecim_r2c_create); free with [**dp\_hbdecim\_r2c\_destroy()**](hbdecim__r2c__core_8h.md#function-dp_hbdecim_r2c_destroy). Holds the scaled, sign-modulated FIR coefficients, two dual-write delay rings (even and odd input samples), a held trailing sample and the output parity. 


        

<hr>
## Public Functions Documentation




### function dp\_hbdecim\_r2c\_consumed 

_Input samples the last_ [_**dp\_hbdecim\_r2c\_execute()**_](hbdecim__r2c__core_8h.md#function-dp_hbdecim_r2c_execute) _took._
```C++
size_t dp_hbdecim_r2c_consumed (
    const hbdecim_r2c_state_t * r
) 
```



Equal to its `num_in` unless `out` filled first; then the caller resumes at in + consumed. 0 after create, reset and set\_state.




**Parameters:**


* `r` Must be non-NULL. 



**Returns:**

Input samples taken by the last execute. 





        

<hr>



### function dp\_hbdecim\_r2c\_create 

_Allocate a real-to-complex halfband decimator._ 
```C++
hbdecim_r2c_state_t * dp_hbdecim_r2c_create (
    size_t num_taps,
    const float * h
) 
```



The coefficients are copied and stored as `h[k] * (-1)^k * 0.5` (the fs/4 modulation baked into the taps). As in [**hbdecim\_core.h**](hbdecim__core_8h.md), only the first num\_taps/2 are read, so the branch must be symmetric.




**Parameters:**


* `num_taps` FIR branch length N. Must be &gt; 0. 
* `h` FIR coefficients, float32, length num\_taps. Copied; the caller keeps ownership. Must be non-NULL. 



**Returns:**

A new state owned by the caller (free with [**dp\_hbdecim\_r2c\_destroy()**](hbdecim__r2c__core_8h.md#function-dp_hbdecim_r2c_destroy)), with zeroed history and parity 0; NULL if num\_taps is 0, h is NULL, or allocation fails. 





        

<hr>



### function dp\_hbdecim\_r2c\_destroy 

_Free the coefficients, both delay lines and the state._ 
```C++
void dp_hbdecim_r2c_destroy (
    hbdecim_r2c_state_t * r
) 
```





**Parameters:**


* `r` State from [**dp\_hbdecim\_r2c\_create()**](hbdecim__r2c__core_8h.md#function-dp_hbdecim_r2c_create); NULL is a no-op. 




        

<hr>



### function dp\_hbdecim\_r2c\_execute 

_Decimate real float32 input by 2, producing CF32._ 
```C++
size_t dp_hbdecim_r2c_execute (
    hbdecim_r2c_state_t * r,
    const float * in,
    size_t num_in,
    float _Complex * out,
    size_t max_out
) 
```



One complex output per (even, odd) input pair, counting a sample held from the previous call as the first of the block. A sample left unpaired at the end is held for the next call, so contiguous blocks give the same output as one large block. Size `out` for the whole block, (num\_in + 1) / 2 samples. A shorter `out` drops nothing: the call stops at the pair boundary after the last output it has room for, and [**dp\_hbdecim\_r2c\_consumed()**](hbdecim__r2c__core_8h.md#function-dp_hbdecim_r2c_consumed) says how much input it took, so the caller offers in + consumed again and the outputs resume exactly as one whole call gives (doppler#2643). A trailing sample is held only when it is the last input; with `max_out` 0 the call writes nothing and takes at most that one sample.




**Parameters:**


* `r` Must be non-NULL. 
* `in` Real input samples. 
* `num_in` Number of input samples. 
* `out` CF32 output buffer. 
* `max_out` Capacity of `out` in samples. 



**Returns:**

Number of output samples written: min((num\_in + held) / 2, max\_out), where held is 1 when a trailing sample was carried in from the previous call. 





        

<hr>



### function dp\_hbdecim\_r2c\_get\_num\_taps 

_FIR branch length N._ 
```C++
size_t dp_hbdecim_r2c_get_num_taps (
    const hbdecim_r2c_state_t * r
) 
```





**Parameters:**


* `r` State. Must be non-NULL. 



**Returns:**

The num\_taps passed to [**dp\_hbdecim\_r2c\_create()**](hbdecim__r2c__core_8h.md#function-dp_hbdecim_r2c_create). 





        

<hr>



### function dp\_hbdecim\_r2c\_get\_rate 

_Output-to-input sample-rate ratio (fixed by design)._ 
```C++
double dp_hbdecim_r2c_get_rate (
    const hbdecim_r2c_state_t * r
) 
```





**Parameters:**


* `r` State; ignored. 



**Returns:**

Always 0.5. 





        

<hr>



### function dp\_hbdecim\_r2c\_get\_state 

_Serialize_ `r's` _mutable state into_`blob` _._
```C++
void dp_hbdecim_r2c_get_state (
    const hbdecim_r2c_state_t * r,
    void * blob
) 
```





**Parameters:**


* `r` State. Must be non-NULL. Not modified. 
* `blob` Caller-owned buffer of at least dp\_hbdecim\_r2c\_state\_bytes(r) bytes. 




        

<hr>



### function dp\_hbdecim\_r2c\_reset 

_Zero history and output parity without freeing._ 
```C++
void dp_hbdecim_r2c_reset (
    hbdecim_r2c_state_t * r
) 
```



Clears both delay lines and their heads, drops any held trailing sample and sets the (-1)^m output parity back to +1.




**Parameters:**


* `r` State. Must be non-NULL. 




        

<hr>



### function dp\_hbdecim\_r2c\_set\_state 

_Restore mutable state from_ `blob` _(same num\_taps)._
```C++
int dp_hbdecim_r2c_set_state (
    hbdecim_r2c_state_t * r,
    const void * blob
) 
```



The envelope (magic, version, endianness, size) is validated before anything is written; on rejection `r` is unchanged.




**Parameters:**


* `r` State built with the same num\_taps as the source. 
* `blob` Blob from [**dp\_hbdecim\_r2c\_get\_state()**](hbdecim__r2c__core_8h.md#function-dp_hbdecim_r2c_get_state). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the blob's envelope rejects. 





        

<hr>



### function dp\_hbdecim\_r2c\_state\_bytes 

_Bytes_ [_**dp\_hbdecim\_r2c\_get\_state()**_](hbdecim__r2c__core_8h.md#function-dp_hbdecim_r2c_get_state) _writes (envelope + payload)._
```C++
size_t dp_hbdecim_r2c_state_bytes (
    const hbdecim_r2c_state_t * r
) 
```





**Parameters:**


* `r` State. Must be non-NULL. 



**Returns:**

Blob size in bytes; depends on num\_taps through the ring length, so a blob fits only a same-num\_taps instance. 





        

<hr>
## Macro Definition Documentation





### define HBDECIM\_R2C\_STATE\_MAGIC 

```C++
#define HBDECIM_R2C_STATE_MAGIC `DP_FOURCC ('H', 'B', 'R', '2')`
```



Envelope magic ("HBR2") that tags an hbdecim\_r2c state blob. 


        

<hr>



### define HBDECIM\_R2C\_STATE\_VERSION 

```C++
#define HBDECIM_R2C_STATE_VERSION `1u`
```



Payload layout version of an hbdecim\_r2c state blob. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/hbdecim/hbdecim_r2c_core.h`

