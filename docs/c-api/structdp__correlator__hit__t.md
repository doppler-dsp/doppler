

# Struct dp\_correlator\_hit\_t



[**ClassList**](annotated.md) **>** [**dp\_correlator\_hit\_t**](structdp__correlator__hit__t.md)



_One detection, from any detector or search flavor's_ [_**dp\_correlator\_push()**_](correlator__core_8h.md#function-dp_correlator_push) _._[More...](#detailed-description)

* `#include <correlator_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**\_pad**](#variable-_pad)  <br> |
|  double | [**chip\_phase**](#variable-chip_phase)  <br> |
|  float | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  size\_t | [**col**](#variable-col)  <br> |
|  double | [**doppler\_rate\_hz\_s**](#variable-doppler_rate_hz_s)  <br> |
|  float | [**noise\_est**](#variable-noise_est)  <br> |
|  float | [**peak\_mag**](#variable-peak_mag)  <br> |
|  size\_t | [**row**](#variable-row)  <br> |
|  uint64\_t | [**samples\_consumed**](#variable-samples_consumed)  <br> |
|  float | [**test\_stat**](#variable-test_stat)  <br> |
|  float | [**test\_stat\_peak**](#variable-test_stat_peak)  <br> |












































## Detailed Description


docs/design/correlator.md section 4: the one hit record. The peak's flat index in the correlation map, decomposed into (row, col) so that a caller does not need the map's width. At one row (the 1-D detector) row is 0 and col is the lag. A search flavor's row is the Doppler bin and its col the code phase, and it fills every field; a detector flavor fills row through test\_stat and writes the rest as 0. 64 bytes, with no unnamed padding. 


    
## Public Attributes Documentation




### variable \_pad 

```C++
uint32_t dp_correlator_hit_t::_pad;
```



Alignment padding, named so the record has none unnamed; written as 0 (every builder's designated initializer zeroes it). A new field goes before it, which moves it, and test\_correlator\_search.c's layout assert goes red until the comparison names the field. 


        

<hr>



### variable chip\_phase 

```C++
double dp_correlator_hit_t::chip_phase;
```



The code's own instantaneous phase at the hand-off instant, in chips, Dll's convention  the seed a code loop wants for this hit. The same value [**dp\_correlator\_build\_handoff()**](correlator__core_8h.md#function-dp_correlator_build_handoff) reports as [**dp\_correlator\_handoff\_t::chip\_phase**](structdp__correlator__handoff__t.md#variable-chip_phase): the inverted lag advanced by the dwell's dilation when a carrier is set ([**dp\_correlator\_set\_carrier\_freq\_hz()**](correlator__core_8h.md#function-dp_correlator_set_carrier_freq_hz)). For an engine built by [**dp\_correlator\_create\_burst()**](correlator__core_8h.md#function-dp_correlator_create_burst), one chip is one template sample, so chip\_phase is in template samples over code\_bins: a caller at s samples per chip divides by s. The burst search refuses a carrier setter, so its hits carry no dwell advance. Appended last so positional initialisers do not shift. 


        

<hr>



### variable cn0\_dbhz\_est 

```C++
float dp_correlator_hit_t::cn0_dbhz_est;
```



Estimated carrier-to-noise density (dB-Hz), backed out of test\_stat via the same C/N0 &lt;-&gt; per-sample-amplitude-SNR relationship used to size the engine (see [**dp\_correlator\_create\_burst()**](correlator__core_8h.md#function-dp_correlator_create_burst)/ [**dp\_correlator\_create\_continuous()**](correlator__core_8h.md#function-dp_correlator_create_continuous)). Tracks the true C/N0 while receiver AWGN dominates the CFAR noise estimate; saturates at the code's own autocorrelation-sidelobe floor once the true C/N0 exceeds what this code/geometry can resolve — a real ceiling, not a fault. 
 


        

<hr>



### variable col 

```C++
size_t dp_correlator_hit_t::col;
```



Column of the peak: the lag, at one row; a search flavor's code phase (0 … lag\_bins-1), a lag of col \* code\_bins / lag\_bins samples. 


        

<hr>



### variable doppler\_rate\_hz\_s 

```C++
double dp_correlator_hit_t::doppler_rate_hz_s;
```



The Doppler rate, Hz/s, of the rate hypothesis that won this hit's dwell ([**dp\_correlator\_create\_burst()**](correlator__core_8h.md#function-dp_correlator_create_burst)'s rate\_hypotheses); 0 without hypotheses, and 0 when the dwell's block is one or two epochs, which cannot see a rate (every hypothesis is then the same surface). A hypothesis, not a fit, and a reliable one only from a block WHOLLY inside the preamble and well above the design point. Then it names the hypothesis nearest the carrier's rate  within half a spacing, `doppler_rate/(n - 1)`  every time (validate\_acq\_rate\_pd's reported-rate rows, N = 9, D = 10, design -13 dB, 23 dB over it: 119/119 at true rate 0, 121/121 at +R). Below that it is not: at the design point only 30% (rate 0) and 66% (+R) of the full-block hits name it, adjacent hypotheses differing by at most pi/4 of phase at the block's edges so that noise picks among neighbours; and about three hits in four come from dwells STRADDLING the preamble's edges, which often name +-R (39% of all hits at +10 dB-Hz name the nearest at true rate 0, 51% at +R). DETECTION is unaffected  the surface that wins is a good one whichever neighbour it is. Treat the value as a seed, not a measurement, at the C/N0 the engine was sized for. With `max_peaks > 1` one hypothesis's surface serves the whole dwell's list, so every hit of a dwell wears the winner's rate. The Doppler the hit reports is the carrier's at the middle of the dwell's block. Appended last so positional initialisers do not shift. 


        

<hr>



### variable noise\_est 

```C++
float dp_correlator_hit_t::noise_est;
```



Noise estimate aggregated over noise\_lo..noise\_hi. 


        

<hr>



### variable peak\_mag 

```C++
float dp_correlator_hit_t::peak_mag;
```



max \|R\| (linear magnitude, not power). 
 


        

<hr>



### variable row 

```C++
size_t dp_correlator_hit_t::row;
```



Row of the peak: 0 at one row; a search flavor's Doppler bin (0 … coherent\_bins-1), or, in wideband mode, a bin of the uniform grid of window\_bins\*coherent\_bins bins (window\_bins tiles of coherent\_bins each) — see the search flavors' section. 


        

<hr>



### variable samples\_consumed 

```C++
uint64_t dp_correlator_hit_t::samples_consumed;
```



st-&gt;samples\_consumed at the exact moment this hit was appended — the raw sample offset (since this engine's own stream start) this detection's epoch ended at. The per-hit anchor a caller needs to derive a precise timestamp ([**dp\_sample\_clock\_stamp\_at()**](timing__core_8h.md#function-dp_sample_clock_stamp_at), [**timing/timing\_core.h**](timing__core_8h.md)) instead of reusing one message-level timestamp for every hit — a single push() call spanning multiple epochs can emit several hits at different sample offsets. Acquisition itself stays wall-clock-agnostic; this is the raw material a caller with a real clock converts. 
 


        

<hr>



### variable test\_stat 

```C++
float dp_correlator_hit_t::test_stat;
```



peak\_mag / noise\_est; 0 if noise\_est is not &gt; 0. A search flavor's is scaled by sqrt(2\*n\_noncoh) once non-coherent looks are combined. 
 


        

<hr>



### variable test\_stat\_peak 

```C++
float dp_correlator_hit_t::test_stat_peak;
```



test\_stat as the emitter would read on its own lag: the straddle a delay between two lag columns costs, put back. Estimated from the larger of the hit's two neighbouring columns under the mainlobe the engine models (a triangle `shape.zone` samples wide, as for a chip code): a centre c and neighbour n one column, s samples, apart give (c + n) / (2 - s / zone), exact on that triangle, clamped between c and c / (1 - s / (2 zone))  never below what was read, never past the worst straddle. Noise lifts the neighbour, so at low statistics it reads high, within that clamp. For a bound on what the emitter's sidelobes can reach (a sidelobe does not lose its straddle with the peak); detection reads test\_stat. Appended last, like the field above. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/correlator/correlator_core.h`

