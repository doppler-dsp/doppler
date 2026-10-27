

# File hbdecim\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**hbdecim**](dir_29792a392a590bbead7fbef545faea73.md) **>** [**hbdecim\_core.h**](hbdecim__core_8h.md)

[Go to the source code of this file](hbdecim__core_8h_source.md)

_Halfband 2:1 decimator for CF32 IQ samples._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**hbdecim\_state\_t**](structhbdecim__state__t.md) <br>_State of a CF32 halfband 2:1 decimator._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**hbdecim\_state\_t**](structhbdecim__state__t.md) \* | [**dp\_hbdecim\_create**](#function-dp_hbdecim_create) (size\_t num\_taps, const float \* h) <br>_Allocate and initialise a halfband 2:1 decimator._  |
|  double | [**dp\_hbdecim\_dc\_gain**](#function-dp_hbdecim_dc_gain) (const [**hbdecim\_state\_t**](structhbdecim__state__t.md) \* r) <br>_The filter's response to a constant input, from its own taps._  |
|  void | [**dp\_hbdecim\_destroy**](#function-dp_hbdecim_destroy) ([**hbdecim\_state\_t**](structhbdecim__state__t.md) \* r) <br>_Free the coefficient copy, both delay lines and the state._  |
|  size\_t | [**dp\_hbdecim\_execute**](#function-dp_hbdecim_execute) ([**hbdecim\_state\_t**](structhbdecim__state__t.md) \* r, const float \_Complex \* in, size\_t num\_in, float \_Complex \* out, size\_t max\_out) <br>_Decimate a block of CF32 samples by 2._  |
|  size\_t | [**dp\_hbdecim\_get\_num\_taps**](#function-dp_hbdecim_get_num_taps) (const [**hbdecim\_state\_t**](structhbdecim__state__t.md) \* r) <br>_FIR branch length N._  |
|  double | [**dp\_hbdecim\_get\_rate**](#function-dp_hbdecim_get_rate) (const [**hbdecim\_state\_t**](structhbdecim__state__t.md) \* r) <br>_Output-to-input sample-rate ratio (fixed by design)._  |
|  void | [**dp\_hbdecim\_get\_state**](#function-dp_hbdecim_get_state) (const [**hbdecim\_state\_t**](structhbdecim__state__t.md) \* r, void \* blob) <br>_Serialize_ `r's` _mutable state into_`blob` _._ |
|  void | [**dp\_hbdecim\_reset**](#function-dp_hbdecim_reset) ([**hbdecim\_state\_t**](structhbdecim__state__t.md) \* r) <br>_Zero both delay lines and drop any pending sample._  |
|  int | [**dp\_hbdecim\_set\_state**](#function-dp_hbdecim_set_state) ([**hbdecim\_state\_t**](structhbdecim__state__t.md) \* r, const void \* blob) <br>_Restore mutable state from_ `blob` _(same num\_taps)._ |
|  size\_t | [**dp\_hbdecim\_state\_bytes**](#function-dp_hbdecim_state_bytes) (const [**hbdecim\_state\_t**](structhbdecim__state__t.md) \* r) <br>_Bytes_ [_**dp\_hbdecim\_get\_state()**_](hbdecim__core_8h.md#function-dp_hbdecim_get_state) _writes for_`r` _(envelope + payload)._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**HBDECIM\_STATE\_MAGIC**](hbdecim__core_8h.md#define-hbdecim_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('H', 'B', 'D', 'C')`<br> |
| define  | [**HBDECIM\_STATE\_VERSION**](hbdecim__core_8h.md#define-hbdecim_state_version)  `1u`<br> |

## Detailed Description


Lifted from dp\_hbdecim\_cf32\_t (c/src/hbdecim.c). Only the cf32 variant lives here; the real-input sibling (Architecture D2) is [**hbdecim\_r2c\_core.h**](hbdecim__r2c__core_8h.md), and the jm-bound Python face is [**HalfbandDecimator\_core.h**](HalfbandDecimator__core_8h.md) (doppler.resample.HalfbandDecimator).


Algorithm: two dual-write circular delay lines hold even-indexed and odd-indexed input samples separately. Per output sample:
* Push x(2m) → even delay line.
* Push x(2m+1) → odd delay line.
* Compute symmetric FIR (N/2 paired multiplies) + scaled delay tap.




Which delay line carries the FIR is determined by N: N even (fir\_on\_even=1): FIR on even\_dl; delay from odd\_dl(centre). N odd (fir\_on\_even=0): FIR on odd\_dl at offset +1; delay from even\_dl(centre).


Coefficients are scaled by 0.5 inside dp\_hbdecim\_create — this is the polyphase identity normalisation; do not remove it.


Lifecycle: 
```C++
hbdecim_state_t *r = dp_hbdecim_create(num_taps, h_fir);
size_t n = dp_hbdecim_execute(r, in, num_in, out, max_out);
dp_hbdecim_destroy(r);
```
 


    
## Public Functions Documentation




### function dp\_hbdecim\_create 

_Allocate and initialise a halfband 2:1 decimator._ 
```C++
hbdecim_state_t * dp_hbdecim_create (
    size_t num_taps,
    const float * h
) 
```



The FIR branch must be symmetric (`h[k] == h[N-1-k]`): the filter reads only the first N/2 coefficients and applies each to a mirrored pair of delay-line samples. For odd N the centre coefficient `h[N/2]` is not read; the half-weight delay branch takes its place.




**Parameters:**


* `num_taps` Length N of the FIR branch (the row from kaiser\_prototype(phases=2) that has more than one significant coefficient). Must be &gt; 0. 
* `h` FIR coefficients, length num\_taps (float32). Copied internally and scaled by 0.5, so the caller keeps ownership. Must be non-NULL. 



**Returns:**

A new state owned by the caller (free with [**dp\_hbdecim\_destroy()**](hbdecim__core_8h.md#function-dp_hbdecim_destroy)), with zeroed delay lines and no pending sample; NULL if num\_taps is 0, h is NULL, or allocation fails. 





        

<hr>



### function dp\_hbdecim\_dc\_gain 

_The filter's response to a constant input, from its own taps._ 
```C++
double dp_hbdecim_dc_gain (
    const hbdecim_state_t * r
) 
```



A halfband decimator sums a symmetric FIR branch (each stored tap multiplies TWO delay-line samples) and a half-weight delay branch, so at DC the answer is `2*sum(h) + 0.5`. Computed, not measured: a caller — or a gate — can ask what gain the object contributes without running a signal through it.




**Parameters:**


* `r` State. Must be non-NULL. 



**Returns:**

The DC gain. 1.0 for a correctly normalised halfband.



```C++
hbdecim_state_t *d = dp_hbdecim_create (ntaps, h);
printf ("%.4f\n", dp_hbdecim_dc_gain (d));  // 1.0000
dp_hbdecim_destroy (d);
```
 


        

<hr>



### function dp\_hbdecim\_destroy 

_Free the coefficient copy, both delay lines and the state._ 
```C++
void dp_hbdecim_destroy (
    hbdecim_state_t * r
) 
```





**Parameters:**


* `r` State from [**dp\_hbdecim\_create()**](hbdecim__core_8h.md#function-dp_hbdecim_create); NULL is a no-op. 




        

<hr>



### function dp\_hbdecim\_execute 

_Decimate a block of CF32 samples by 2._ 
```C++
size_t dp_hbdecim_execute (
    hbdecim_state_t * r,
    const float _Complex * in,
    size_t num_in,
    float _Complex * out,
    size_t max_out
) 
```



Processes input pairs (even, odd); one output per pair. If num\_in is odd, the trailing even sample is buffered and consumed on the next call.


Output-buffer sizing: allocate at least (num\_in + 1) / 2 samples. Emission stops once `max_out` samples are written; complete input pairs left over at that point are discarded (only one trailing sample is carried), so size `out` for the whole block. If num\_in or max\_out is 0 the call returns 0 and consumes nothing.




**Parameters:**


* `r` Must be non-NULL. 
* `in` Input CF32 samples. 
* `num_in` Number of input samples. 
* `out` Output buffer. 
* `max_out` Capacity of out in samples. 



**Returns:**

Number of output samples written: min((num\_in + has\_pending) / 2, max\_out). 





        

<hr>



### function dp\_hbdecim\_get\_num\_taps 

_FIR branch length N._ 
```C++
size_t dp_hbdecim_get_num_taps (
    const hbdecim_state_t * r
) 
```





**Parameters:**


* `r` State. Must be non-NULL. 



**Returns:**

The num\_taps passed to [**dp\_hbdecim\_create()**](hbdecim__core_8h.md#function-dp_hbdecim_create). 





        

<hr>



### function dp\_hbdecim\_get\_rate 

_Output-to-input sample-rate ratio (fixed by design)._ 
```C++
double dp_hbdecim_get_rate (
    const hbdecim_state_t * r
) 
```





**Parameters:**


* `r` State; ignored. 



**Returns:**

Always 0.5. 





        

<hr>



### function dp\_hbdecim\_get\_state 

_Serialize_ `r's` _mutable state into_`blob` _._
```C++
void dp_hbdecim_get_state (
    const hbdecim_state_t * r,
    void * blob
) 
```





**Parameters:**


* `r` State. Must be non-NULL. Not modified. 
* `blob` Caller-owned buffer of at least dp\_hbdecim\_state\_bytes(r) bytes. 




        

<hr>



### function dp\_hbdecim\_reset 

_Zero both delay lines and drop any pending sample._ 
```C++
void dp_hbdecim_reset (
    hbdecim_state_t * r
) 
```



num\_taps and coefficients are preserved; the next execute behaves as on a freshly created decimator.




**Parameters:**


* `r` State. Must be non-NULL. 




        

<hr>



### function dp\_hbdecim\_set\_state 

_Restore mutable state from_ `blob` _(same num\_taps)._
```C++
int dp_hbdecim_set_state (
    hbdecim_state_t * r,
    const void * blob
) 
```



The envelope (magic, version, endianness, size) is validated before anything is written; on rejection `r` is unchanged. Coefficients are not in the blob and are kept from create.




**Parameters:**


* `r` State built with the same num\_taps as the source. 
* `blob` Blob from [**dp\_hbdecim\_get\_state()**](hbdecim__core_8h.md#function-dp_hbdecim_get_state). 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID if the blob's envelope rejects. 





        

<hr>



### function dp\_hbdecim\_state\_bytes 

_Bytes_ [_**dp\_hbdecim\_get\_state()**_](hbdecim__core_8h.md#function-dp_hbdecim_get_state) _writes for_`r` _(envelope + payload)._
```C++
size_t dp_hbdecim_state_bytes (
    const hbdecim_state_t * r
) 
```



The size depends on num\_taps (through the ring length even\_cap), so a blob only fits an instance built with the same num\_taps.




**Parameters:**


* `r` State. Must be non-NULL. 



**Returns:**

Blob size in bytes: header, both ring heads, the pending flag and sample, and both dual-write rings. 





        

<hr>
## Macro Definition Documentation





### define HBDECIM\_STATE\_MAGIC 

```C++
#define HBDECIM_STATE_MAGIC `DP_FOURCC ('H', 'B', 'D', 'C')`
```



Envelope magic ("HBDC") that tags an hbdecim state blob. 


        

<hr>



### define HBDECIM\_STATE\_VERSION 

```C++
#define HBDECIM_STATE_VERSION `1u`
```



Payload layout version of an hbdecim state blob. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/hbdecim/hbdecim_core.h`

