

# Struct wfm\_segment\_t



[**ClassList**](annotated.md) **>** [**wfm\_segment\_t**](structwfm__segment__t.md)



_One composer segment: one or more sources summed over the same span, then a trailing off-time gap._ [More...](#detailed-description)

* `#include <wfm_compose.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**delay\_samples**](#variable-delay_samples)  <br> |
|  size\_t | [**delay\_samples\_hi**](#variable-delay_samples_hi)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  int | [**gap\_noise**](#variable-gap_noise)  <br> |
|  size\_t | [**n\_sources**](#variable-n_sources)  <br> |
|  size\_t | [**num\_samples**](#variable-num_samples)  <br> |
|  size\_t | [**num\_samples\_hi**](#variable-num_samples_hi)  <br> |
|  size\_t | [**off\_samples**](#variable-off_samples)  <br> |
|  size\_t | [**off\_samples\_hi**](#variable-off_samples_hi)  <br> |
|  unsigned | [**ranged**](#variable-ranged)  <br> |
|  size\_t | [**repeats**](#variable-repeats)  <br> |
|  [**wfm\_source\_t**](structwfm__source__t.md) \* | [**sources**](#variable-sources)  <br> |












































## Detailed Description


A 1-source segment is byte-identical to driving that source's `synth` directly. `num_samples` is the on-time; `off_samples` is a trailing gap of zeros. Durations in seconds are `round(duration * fs)` — the caller resolves. 


    
## Public Attributes Documentation




### variable delay\_samples 

```C++
size_t wfm_segment_t::delay_samples;
```



Leading gap before the on-time, in samples: the burst arrives after this delay, and the gap carries the noise floor like off\_samples. Ranged like off\_samples and re-drawn per repeats instance, so a (lo, hi) delay is per-burst arrival jitter. Use off\_samples for inter-burst spacing, delay\_samples for arrival jitter. 


        

<hr>



### variable delay\_samples\_hi 

```C++
size_t wfm_segment_t::delay_samples_hi;
```



upper bound when WFM\_RANGE\_DELAY\_SAMPLES 


        

<hr>



### variable fs 

```C++
double wfm_segment_t::fs;
```



Sample rate in Hz, one per segment and shared by all its sources. At the default 1.0 every frequency is normalised (cycles per sample); state it whenever a scene is in real Hz. 


        

<hr>



### variable gap\_noise 

```C++
int wfm_segment_t::gap_noise;
```



Gap policy for this segment's delay and trailing gap. auto: gaps carry the segment's noise floor  the sources' AWGN keeps running while the signal stops (clean scenes still get exact-zero gaps). off: gaps are hard zeros. 


        

<hr>



### variable n\_sources 

```C++
size_t wfm_segment_t::n_sources;
```



number of entries in sources 


        

<hr>



### variable num\_samples 

```C++
size_t wfm_segment_t::num_samples;
```



Segment on-time in samples, before the trailing gap: 0 derives it from the sources, or 1024 when they set none. A finite data source sets its frames, a lone dsss burst one burst, and a stream runs to its end. A count beside a finite data source or a lone dsss burst is refused, since they set the length; give repeats for more. 


        

<hr>



### variable num\_samples\_hi 

```C++
size_t wfm_segment_t::num_samples_hi;
```



upper bound when WFM\_RANGE\_NUM\_SAMPLES is set 


        

<hr>



### variable off\_samples 

```C++
size_t wfm_segment_t::off_samples;
```



Trailing gap after the on-time, in samples. It carries the noise floor or hard zeros, per gap\_noise. 


        

<hr>



### variable off\_samples\_hi 

```C++
size_t wfm_segment_t::off_samples_hi;
```



upper bound when WFM\_RANGE\_OFF\_SAMPLES is set 


        

<hr>



### variable ranged 

```C++
unsigned wfm_segment_t::ranged;
```



WFM\_RANGE\_{NUM,OFF}\_SAMPLES bitmask 


        

<hr>



### variable repeats 

```C++
size_t wfm_segment_t::repeats;
```



Play the segment this many times back-to-back (each instance = delay
* on-time + trailing gap) before advancing. Ranged fields re-draw and the AWGN is fresh per instance; the signal (codes, payload, PN phase) stays fixed. 0 and 1 both mean one instance. 




        

<hr>



### variable sources 

```C++
wfm_source_t* wfm_segment_t::sources;
```



n\_sources sources summed at the same time 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_compose.h`

