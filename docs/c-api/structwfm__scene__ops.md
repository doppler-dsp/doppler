

# Struct wfm\_scene\_ops



[**ClassList**](annotated.md) **>** [**wfm\_scene\_ops**](structwfm__scene__ops.md)



_The composer's own entry points, as a table its state starts with._ [More...](#detailed-description)

* `#include <wfm_compose.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  const [**wfm\_segment\_t**](structwfm__segment__t.md) \*(\* | [**segments**](#variable-segments)  <br> |












































## Detailed Description


Every composer state's first member points at one of these, set at create. A component that holds a composer but does not link the composer engine (the SigMF writer, doppler#2564) reads the segments through it: dp\_wfm\_scene\_segments() calls the composer's OWN [**dp\_wfm\_compose\_segments()**](wfm__compose_8h.md#function-dp_wfm_compose_segments), so the code that runs is the code that built the scene, even when the two live in different extension modules. 


    
## Public Attributes Documentation




### variable segments 

```C++
const wfm_segment_t *(* wfm_scene_ops::segments) (const dp_wfm_compose_state_t *state, size_t *n_out, int *repeat, int *continuous);
```



[**dp\_wfm\_compose\_segments()**](wfm__compose_8h.md#function-dp_wfm_compose_segments), as the composer that owns the state has it. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_compose.h`

