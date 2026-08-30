

# Struct wfm\_draw\_t



[**ClassList**](annotated.md) **>** [**wfm\_draw\_t**](structwfm__draw__t.md)



_One rendered source instance: its timing AND the values it was actually rendered with._ [More...](#detailed-description)

* `#include <wfm_compose.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**delay**](#variable-delay)  <br> |
|  double | [**f\_end**](#variable-f_end)  <br> |
|  double | [**freq**](#variable-freq)  <br> |
|  size\_t | [**instance**](#variable-instance)  <br> |
|  double | [**level**](#variable-level)  <br> |
|  size\_t | [**off**](#variable-off)  <br> |
|  size\_t | [**on**](#variable-on)  <br> |
|  size\_t | [**seg**](#variable-seg)  <br> |
|  double | [**snr**](#variable-snr)  <br> |
|  size\_t | [**src**](#variable-src)  <br> |
|  size\_t | [**start**](#variable-start)  <br> |












































## Detailed Description


A `wfm_span_t` answers _when_; this answers _when and what_, for one source of one instance. The distinction is not academic. The SigMF sidecar used to build each annotation from two provenances  timing replayed through [**wfm\_compose\_spans()**](wfm__compose_8h.md#function-wfm_compose_spans), frequency and SNR read straight off the source struct, which for a ranged field still holds `lo`  so every annotation of a `--freq 11200:12800 --snr 8:14` scene claimed 11200 Hz and 8 dB beside a sample-accurate start. Measured against the capture itself: up to 1224 Hz and 6.0 dB out (doppler#1086). A 6 dB error is a different operating point, and nothing in the file revealed it.


An un-ranged field reports its scalar, so a consumer never branches on the `ranged` bitmask.


The spec keeps storing `(lo, hi)`: replay is guaranteed by re-deriving the draw hash, not by recording the draw. "What does this spec permit" and "what did this run do" are different questions and one field cannot answer both  which is why this is a separate call rather than a resolved spec. 


    
## Public Attributes Documentation




### variable delay 

```C++
size_t wfm_draw_t::delay;
```




<hr>



### variable f\_end 

```C++
double wfm_draw_t::f_end;
```




<hr>



### variable freq 

```C++
double wfm_draw_t::freq;
```




<hr>



### variable instance 

```C++
size_t wfm_draw_t::instance;
```




<hr>



### variable level 

```C++
double wfm_draw_t::level;
```




<hr>



### variable off 

```C++
size_t wfm_draw_t::off;
```




<hr>



### variable on 

```C++
size_t wfm_draw_t::on;
```




<hr>



### variable seg 

```C++
size_t wfm_draw_t::seg;
```




<hr>



### variable snr 

```C++
double wfm_draw_t::snr;
```




<hr>



### variable src 

```C++
size_t wfm_draw_t::src;
```




<hr>



### variable start 

```C++
size_t wfm_draw_t::start;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_compose.h`

