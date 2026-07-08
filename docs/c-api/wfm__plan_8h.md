

# File wfm\_plan.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_plan.h**](wfm__plan_8h.md)

[Go to the source code of this file](wfm__plan_8h_source.md)



* `#include <complex.h>`
* `#include <stddef.h>`
* `#include <stdint.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct wfm\_plan | [**wfm\_plan\_t**](#typedef-wfm_plan_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**wfm\_plan\_anchor\_seed**](#function-wfm_plan_anchor_seed) (const [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p) <br>_The noise seed that reproduces a full compose._  |
|  size\_t | [**wfm\_plan\_at**](#function-wfm_plan_at) (const [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p, double snr, uint64\_t seed, float \_Complex \* out) <br>_Scalar fast-path for the hot Monte-Carlo/SNR loop (no JSON parse)._  |
|  void | [**wfm\_plan\_destroy**](#function-wfm_plan_destroy) ([**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p) <br>_Destroy a Plan and free its caches. NULL is a no-op._  |
|  size\_t | [**wfm\_plan\_len**](#function-wfm_plan_len) (const [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p) <br>_Cached length in samples (the jm binding's out\_len\_fn)._  |
|  size\_t | [**wfm\_plan\_n\_sources**](#function-wfm_plan_n_sources) (const [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p) <br>_Number of cached signal sources (excludes the noise floor)._  |
|  [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* | [**wfm\_plan\_prepare**](#function-wfm_plan_prepare) (const char \* spec\_json) <br>_Prepare a Plan from a composer spec JSON (Composer.to\_json())._  |
|  size\_t | [**wfm\_plan\_render**](#function-wfm_plan_render) (const [**wfm\_plan\_t**](wfm__plan_8h.md#typedef-wfm_plan_t) \* p, const char \* overrides\_json, float \_Complex \* out) <br>_General render: apply a JSON override spec, return a cf32 array._  |




























## Public Types Documentation




### typedef wfm\_plan\_t 

```C++
typedef struct wfm_plan wfm_plan_t;
```



Opaque prepared-plan state. 


        

<hr>
## Public Functions Documentation




### function wfm\_plan\_anchor\_seed 

_The noise seed that reproduces a full compose._ 
```C++
uint64_t wfm_plan_anchor_seed (
    const wfm_plan_t * p
) 
```



Passing this as `wfm_plan_at`'s seed (with the scene's base SNR) yields the byte-identical output of `wfm_compose`. Varying the seed draws independent Monte-Carlo noise realizations over the same signal. 


        

<hr>



### function wfm\_plan\_at 

_Scalar fast-path for the hot Monte-Carlo/SNR loop (no JSON parse)._ 
```C++
size_t wfm_plan_at (
    const wfm_plan_t * p,
    double snr,
    uint64_t seed,
    float _Complex * out
) 
```



`out = Σ gain_k·cache_k + gain(snr)·noise(seed)`; writes `wfm_plan_len(p)` samples. Equivalent to `render` with only `{"snr":snr,"seed":seed}`.




**Returns:**

Samples written (== wfm\_plan\_len(p)). 





        

<hr>



### function wfm\_plan\_destroy 

_Destroy a Plan and free its caches. NULL is a no-op._ 
```C++
void wfm_plan_destroy (
    wfm_plan_t * p
) 
```




<hr>



### function wfm\_plan\_len 

_Cached length in samples (the jm binding's out\_len\_fn)._ 
```C++
size_t wfm_plan_len (
    const wfm_plan_t * p
) 
```




<hr>



### function wfm\_plan\_n\_sources 

_Number of cached signal sources (excludes the noise floor)._ 
```C++
size_t wfm_plan_n_sources (
    const wfm_plan_t * p
) 
```




<hr>



### function wfm\_plan\_prepare 

_Prepare a Plan from a composer spec JSON (Composer.to\_json())._ 
```C++
wfm_plan_t * wfm_plan_prepare (
    const char * spec_json
) 
```



Parses + resolves the scene, validates the v1 scope, then renders and caches each signal source at gain 1. Returns NULL on parse failure or an out-of-scope spec (multi-segment, continuous/repeat, ranged, a non-trailing or multiple noise source, or a lone bundled noisy source).




**Parameters:**


* `spec_json` A NUL-terminated composer spec JSON string. 



**Returns:**

Heap Plan (caller [**wfm\_plan\_destroy()**](wfm__plan_8h.md#function-wfm_plan_destroy)s it), or NULL. 





        

<hr>



### function wfm\_plan\_render 

_General render: apply a JSON override spec, return a cf32 array._ 
```C++
size_t wfm_plan_render (
    const wfm_plan_t * p,
    const char * overrides_json,
    float _Complex * out
) 
```



`overrides_json` is a small JSON object, all keys optional: `{"gains":[dB…], "phases":[rad…], "enable":[bool…], "snr":dB, "seed":u}` (`gains`/`phases`/`enable` are per-source, length = [**wfm\_plan\_n\_sources()**](wfm__plan_8h.md#function-wfm_plan_n_sources)). An empty object (or NULL) renders the baseline — bit-identical to `Composer(scene).compose()`. Writes `wfm_plan_len(p)` samples to `out`.




**Returns:**

Samples written (== wfm\_plan\_len(p)). 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_plan.h`

