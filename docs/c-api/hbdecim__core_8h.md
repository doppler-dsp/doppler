

# File hbdecim\_core.h



[**FileList**](files.md) **>** [**hbdecim**](dir_3828151286b0ff520a0d701b39db5af1.md) **>** [**hbdecim\_core.h**](hbdecim__core_8h.md)

[Go to the source code of this file](hbdecim__core_8h_source.md)

_Halfband 2:1 decimator for CF32 IQ samples._ [More...](#detailed-description)

* `#include "clib_common.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**hbdecim\_state\_t**](structhbdecim__state__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**hbdecim\_state\_t**](structhbdecim__state__t.md) \* | [**hbdecim\_create**](#function-hbdecim_create) (size\_t num\_taps, const float \* h) <br>_Allocate and initialise a halfband 2:1 decimator._  |
|  void | [**hbdecim\_destroy**](#function-hbdecim_destroy) ([**hbdecim\_state\_t**](structhbdecim__state__t.md) \* r) <br> |
|  size\_t | [**hbdecim\_execute**](#function-hbdecim_execute) ([**hbdecim\_state\_t**](structhbdecim__state__t.md) \* r, const float \_Complex \* in, size\_t num\_in, float \_Complex \* out, size\_t max\_out) <br>_Decimate a block of CF32 samples by 2._  |
|  size\_t | [**hbdecim\_get\_num\_taps**](#function-hbdecim_get_num_taps) (const [**hbdecim\_state\_t**](structhbdecim__state__t.md) \* r) <br> |
|  double | [**hbdecim\_get\_rate**](#function-hbdecim_get_rate) (const [**hbdecim\_state\_t**](structhbdecim__state__t.md) \* r) <br> |
|  void | [**hbdecim\_reset**](#function-hbdecim_reset) ([**hbdecim\_state\_t**](structhbdecim__state__t.md) \* r) <br> |




























## Detailed Description


Lifted from dp\_hbdecim\_cf32\_t (c/src/hbdecim.c). Only the cf32 variant is ported; dp\_hbdecim\_r2cf32 (real-input D2) is a separate object and a separate session.


Algorithm: two dual-write circular delay lines hold even-indexed and odd-indexed input samples separately. Per output sample:
* Push x(2m) → even delay line.
* Push x(2m+1) → odd delay line.
* Compute symmetric FIR (N/2 paired multiplies) + scaled delay tap.




Which delay line carries the FIR is determined by N: N even (fir\_on\_even=1): FIR on even\_dl; delay from odd\_dl(centre). N odd (fir\_on\_even=0): FIR on odd\_dl at offset +1; delay from even\_dl(centre).


Coefficients are scaled by 0.5 inside hbdecim\_create — this is the polyphase identity normalisation; do not remove it.


Lifecycle: 
```C++
hbdecim_state_t *r = hbdecim_create(num_taps, h_fir);
size_t n = hbdecim_execute(r, in, num_in, out, max_out);
hbdecim_destroy(r);
```
 


    
## Public Functions Documentation




### function hbdecim\_create 

_Allocate and initialise a halfband 2:1 decimator._ 
```C++
hbdecim_state_t * hbdecim_create (
    size_t num_taps,
    const float * h
) 
```





**Parameters:**


* `num_taps` Length of the FIR branch (the row from kaiser\_prototype(phases=2) that has more than one significant coefficient). 
* `h` FIR coefficients, length num\_taps (float32). Copied internally and scaled by 0.5. 



**Returns:**

Non-NULL on success, NULL on invalid args or OOM. 





        

<hr>



### function hbdecim\_destroy 

```C++
void hbdecim_destroy (
    hbdecim_state_t * r
) 
```



Free all resources. NULL is a no-op. 


        

<hr>



### function hbdecim\_execute 

_Decimate a block of CF32 samples by 2._ 
```C++
size_t hbdecim_execute (
    hbdecim_state_t * r,
    const float _Complex * in,
    size_t num_in,
    float _Complex * out,
    size_t max_out
) 
```



Processes input pairs (even, odd); one output per pair. If num\_in is odd, the trailing even sample is buffered and consumed on the next call.


Output-buffer sizing: allocate at least (num\_in + 1) / 2 samples.




**Parameters:**


* `r` Must be non-NULL. 
* `in` Input CF32 samples. 
* `num_in` Number of input samples. 
* `out` Output buffer. 
* `max_out` Capacity of out in samples. 



**Returns:**

Number of output samples written. 





        

<hr>



### function hbdecim\_get\_num\_taps 

```C++
size_t hbdecim_get_num_taps (
    const hbdecim_state_t * r
) 
```



Returns the FIR branch length passed to hbdecim\_create. 


        

<hr>



### function hbdecim\_get\_rate 

```C++
double hbdecim_get_rate (
    const hbdecim_state_t * r
) 
```



Always returns 0.5 (rate is fixed by design). 


        

<hr>



### function hbdecim\_reset 

```C++
void hbdecim_reset (
    hbdecim_state_t * r
) 
```



Zero both delay lines and clear the pending-sample flag. num\_taps and coefficients are preserved. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/hbdecim/hbdecim_core.h`

