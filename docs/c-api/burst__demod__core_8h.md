

# File burst\_demod\_core.h



[**FileList**](files.md) **>** [**burst\_demod**](dir_96a22b0098c79a5049df57065c5b8df4.md) **>** [**burst\_demod\_core.h**](burst__demod__core_8h.md)

[Go to the source code of this file](burst__demod__core_8h_source.md)

_Feedforward BPSK DSSS frame demodulator._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "ppe/ppe_core.h"`
* `#include "fft/fft_core.h"`
* `#include "spectral/spectral_core.h"`
* `#include <complex.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**burst\_demod\_state\_t**](structburst__demod__state__t.md) <br>_BurstDemod state. Allocate with_ [_**burst\_demod\_create()**_](burst__demod__core_8h.md#function-burst_demod_create) _._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* | [**burst\_demod\_create**](#function-burst_demod_create) (const uint8\_t \* data\_code, size\_t data\_code\_len, size\_t spc, double chip\_rate, double carrier\_hz, double max\_rate, size\_t payload\_len, size\_t est\_segments) <br>_Create a burst demodulator._  |
|  size\_t | [**burst\_demod\_demod**](#function-burst_demod_demod) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, const float complex \* x, size\_t x\_len, uint8\_t \* out, size\_t max\_out) <br>_Demodulate a burst; write the payload bits to_ `out` _._ |
|  size\_t | [**burst\_demod\_demod\_max\_out**](#function-burst_demod_demod_max_out) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state) <br>_Max output bits = payload\_len (caller sizes the buffer)._  |
|  void | [**burst\_demod\_destroy**](#function-burst_demod_destroy) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state) <br>_Destroy a demodulator._  |
|  void | [**burst\_demod\_reset**](#function-burst_demod_reset) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state) <br>_Clear the read-backs (config is preserved)._  |
|  void | [**burst\_demod\_set\_preamble**](#function-burst_demod_set_preamble) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, const uint8\_t \* acq\_code, size\_t acq\_code\_len, size\_t reps) <br>_Set the unmodulated acq preamble code + repetition count._  |
|  void | [**burst\_demod\_set\_prior**](#function-burst_demod_set_prior) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, double f0\_coarse, size\_t start) <br>_Seed from acquisition: coarse Doppler + preamble start sample._  |
|  void | [**burst\_demod\_set\_sync**](#function-burst_demod_set_sync) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, const uint8\_t \* sync, size\_t sync\_len) <br>_Set the known frame-sync word (0/1 symbols)._  |




























## Detailed Description


The whole post-acquisition payload chain, in C, with no tracking loops:
* preamble estimate — segment-despread the unmodulated, repeated acq preamble into partial correlations and feed them to ppe, giving a coarse (frequency, chirp-rate);
* sample-rate dechirp by (f0, rate) — removes Doppler AND Doppler rate;
* despread the data section with the (short) data code -&gt; soft BPSK symbols;
* frame sync — correlate the symbols against the known sync word; the complex peak gives the frame offset and the residual phase (derotated);
* slice the payload to bits; verify the CRC-16 trailer -&gt; `frame_valid`.




Seed from acquisition with set\_prior(coarse Doppler, preamble start), set\_preamble(acq code, reps) and set\_sync(sync word), then demod(burst). One `max_rate` knob spans near-static Doppler (0) to severe LEO chirp. One-shot per burst. Composes ppe (which composes fft + spectral).



```C++
burst_demod_state_t *d = burst_demod_create(dcode, 50, 4, 1e6, 0, 0, 256, 10);
burst_demod_set_preamble(d, acode, 500, 5);
burst_demod_set_sync(d, sync, 31);
burst_demod_set_prior(d, f0_coarse, preamble_start);
size_t nbits = burst_demod_demod(d, x, n, bits, 256);   // d->frame_valid ...
```
 


    
## Public Functions Documentation




### function burst\_demod\_create 

_Create a burst demodulator._ 
```C++
burst_demod_state_t * burst_demod_create (
    const uint8_t * data_code,
    size_t data_code_len,
    size_t spc,
    double chip_rate,
    double carrier_hz,
    double max_rate,
    size_t payload_len,
    size_t est_segments
) 
```





**Parameters:**


* `data_code` Data spreading code (0/1); copied. 
* `data_code_len` Data spreading factor (chips/symbol). 
* `spc` Samples per chip. 
* `chip_rate` Chip rate (Hz). 
* `carrier_hz` RF carrier (Hz) for code-Doppler scaling; 0 = ignore. 
* `max_rate` Chirp-rate search half-span (cycles/sample^2 at the input rate); 0 = Doppler only (no rate search). 
* `payload_len` Number of payload data symbols (bits) in a frame. 
* `est_segments` Partial correlations per acq period (segmentation for the feedforward estimate; larger tolerates more rate). 



**Returns:**

Heap state, or NULL on bad args / allocation failure. 





        

<hr>



### function burst\_demod\_demod 

_Demodulate a burst; write the payload bits to_ `out` _._
```C++
size_t burst_demod_demod (
    burst_demod_state_t * state,
    const float complex * x,
    size_t x_len,
    uint8_t * out,
    size_t max_out
) 
```





**Returns:**

Number of bits written (0 on failure / too-short burst). The read-back fields (frame\_valid, est\_\*, frame\_offset) are updated. 





        

<hr>



### function burst\_demod\_demod\_max\_out 

_Max output bits = payload\_len (caller sizes the buffer)._ 
```C++
size_t burst_demod_demod_max_out (
    burst_demod_state_t * state
) 
```




<hr>



### function burst\_demod\_destroy 

_Destroy a demodulator._ 
```C++
void burst_demod_destroy (
    burst_demod_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function burst\_demod\_reset 

_Clear the read-backs (config is preserved)._ 
```C++
void burst_demod_reset (
    burst_demod_state_t * state
) 
```




<hr>



### function burst\_demod\_set\_preamble 

_Set the unmodulated acq preamble code + repetition count._ 
```C++
void burst_demod_set_preamble (
    burst_demod_state_t * state,
    const uint8_t * acq_code,
    size_t acq_code_len,
    size_t reps
) 
```




<hr>



### function burst\_demod\_set\_prior 

_Seed from acquisition: coarse Doppler + preamble start sample._ 
```C++
void burst_demod_set_prior (
    burst_demod_state_t * state,
    double f0_coarse,
    size_t start
) 
```




<hr>



### function burst\_demod\_set\_sync 

_Set the known frame-sync word (0/1 symbols)._ 
```C++
void burst_demod_set_sync (
    burst_demod_state_t * state,
    const uint8_t * sync,
    size_t sync_len
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/burst_demod/burst_demod_core.h`

