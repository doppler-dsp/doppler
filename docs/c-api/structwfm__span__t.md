

# Struct wfm\_span\_t



[**ClassList**](annotated.md) **>** [**wfm\_span\_t**](structwfm__span__t.md)



_One rendered segment instance's exact timing: where it lands in the composed stream and how its_ `delay | on | off` _spans divide it._[More...](#detailed-description)

* `#include <wfm_compose.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**delay**](#variable-delay)  <br> |
|  size\_t | [**instance**](#variable-instance)  <br> |
|  size\_t | [**off**](#variable-off)  <br> |
|  size\_t | [**on**](#variable-on)  <br> |
|  size\_t | [**seg**](#variable-seg)  <br> |
|  size\_t | [**start**](#variable-start)  <br> |












































## Detailed Description


Produced by [**wfm\_compose\_spans()**](wfm__compose_8h.md#function-wfm_compose_spans) — the deterministic replay of the ranged draws (same hash, epoch 0), so the reported positions match the rendered capture sample-for-sample without rendering anything. This is the ground truth a detector-scoring pipeline or a SigMF annotation needs: the burst (on-time) of instance k starts at `start + delay` and runs `on` samples. 


    
## Public Attributes Documentation




### variable delay 

```C++
size_t wfm_span_t::delay;
```




<hr>



### variable instance 

```C++
size_t wfm_span_t::instance;
```




<hr>



### variable off 

```C++
size_t wfm_span_t::off;
```




<hr>



### variable on 

```C++
size_t wfm_span_t::on;
```




<hr>



### variable seg 

```C++
size_t wfm_span_t::seg;
```




<hr>



### variable start 

```C++
size_t wfm_span_t::start;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_compose.h`

