

# File cic\_core.h



[**FileList**](files.md) **>** [**cic**](dir_cf560077cc62991e7289ea57a3d930a1.md) **>** [**cic\_core.h**](cic__core_8h.md)

[Go to the source code of this file](cic__core_8h_source.md)

_Cascaded Integrator-Comb (CIC) decimation filter for CF32 IQ._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**cic\_state\_t**](structcic__state__t.md) <br>_CIC filter state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**cic\_state\_t**](structcic__state__t.md) \* | [**cic\_create**](#function-cic_create) (uint32\_t R, uint32\_t N, uint32\_t M) <br>_Create a CIC decimation filter._  |
|  size\_t | [**cic\_decimate**](#function-cic_decimate) ([**cic\_state\_t**](structcic__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Decimate n\_in CF32 samples; write output to out._  |
|  size\_t | [**cic\_decimate\_max\_out**](#function-cic_decimate_max_out) ([**cic\_state\_t**](structcic__state__t.md) \* state) <br>_Upper bound on decimate output for the lazy-alloc ext path._  |
|  void | [**cic\_destroy**](#function-cic_destroy) ([**cic\_state\_t**](structcic__state__t.md) \* state) <br> |
|  void | [**cic\_reconfigure**](#function-cic_reconfigure) ([**cic\_state\_t**](structcic__state__t.md) \* state, uint32\_t R, uint32\_t N, uint32\_t M) <br>_Reconfigure R, N, M in place; resets all filter state._  |
|  void | [**cic\_reset**](#function-cic_reset) ([**cic\_state\_t**](structcic__state__t.md) \* state) <br>_Zero integrators and comb delay lines; preserve R, N, M._  |




























## Detailed Description


Internally runs two identical uint64\_t pipelines — one for the real part and one for the imaginary part. All arithmetic is unsigned, so C-guaranteed wrap-around handles intermediate overflow automatically. The final output is correct as long as the true (infinite-precision) result fits in 63 bits after applying input\_scale.


Structure (decimating by R, N stages, differential delay M):


`x[n]` → INT\_1 → … → INT\_N → ↓R → COMB\_1 → … → COMB\_N → `y[n]`


input\_scale is chosen to maximise dynamic range for ±1.0 input:


input\_scale = floor((2^63 − 1) / (R × M)^N)


output\_scale is the reciprocal normalisation so the float output is back in ±1.0 range for a ±1.0 input signal at full CIC passband.


Dynamic range (bits) for M=1: 



```C++
cic_state_t *cic = cic_create(32, 4, 1);
// stream processing — block size must stay consistent after first call
size_t n_out = cic_decimate(cic, in, 1024, out);
cic_destroy(cic);
```
 


    
## Public Functions Documentation




### function cic\_create 

_Create a CIC decimation filter._ 
```C++
cic_state_t * cic_create (
    uint32_t R,
    uint32_t N,
    uint32_t M
) 
```



input\_scale is computed as floor((2^63−1) / (R×M)^N), which fills every available bit and leaves no headroom — correct for ±1.0 inputs.




**Parameters:**


* `R` Decimation ratio (≥ 1). 
* `N` Number of stages (1–6). 
* `M` Differential delay (1 or 2). 



**Returns:**

Heap-allocated state, or NULL on invalid args or OOM. 





        

<hr>



### function cic\_decimate 

_Decimate n\_in CF32 samples; write output to out._ 
```C++
size_t cic_decimate (
    cic_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
) 
```



Each input sample is converted to a uint64\_t (two's complement) via input\_scale, run through N integrators, and tested against the decimation phase. Every R-th sample is passed through N comb stages and converted back to CF32 via output\_scale.




**Parameters:**


* `state` Must be non-NULL. 
* `in` CF32 input array, length n\_in. 
* `n_in` Number of input samples. 
* `out` Output buffer; must hold at least ceil((state-&gt;phase + n\_in) / state-&gt;R) elements. 



**Returns:**

Number of output samples written. 





        

<hr>



### function cic\_decimate\_max\_out 

_Upper bound on decimate output for the lazy-alloc ext path._ 
```C++
size_t cic_decimate_max_out (
    cic_state_t * state
) 
```



Returns 0 so the Python extension allocates the output buffer on the first call (sized to n\_in, which is always ≥ the actual output count n\_in/R). Block size must stay consistent after the first call. 


        

<hr>



### function cic\_destroy 

```C++
void cic_destroy (
    cic_state_t * state
) 
```



Free all resources. NULL is a no-op. 


        

<hr>



### function cic\_reconfigure 

_Reconfigure R, N, M in place; resets all filter state._ 
```C++
void cic_reconfigure (
    cic_state_t * state,
    uint32_t R,
    uint32_t N,
    uint32_t M
) 
```



Reallocates the comb delay lines if N×M changes. Silently ignores invalid parameters (R=0, N=0 or N&gt;6, M=0 or M&gt;2, or OOM). 


        

<hr>



### function cic\_reset 

_Zero integrators and comb delay lines; preserve R, N, M._ 
```C++
void cic_reset (
    cic_state_t * state
) 
```



Resets the phase counter so the first output sample after reset is produced on input sample R-1 (same as after cic\_create). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/cic/cic_core.h`

