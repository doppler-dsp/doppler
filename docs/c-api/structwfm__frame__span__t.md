

# Struct wfm\_frame\_span\_t



[**ClassList**](annotated.md) **>** [**wfm\_frame\_span\_t**](structwfm__frame__span__t.md)



_A run of bits inside the assembled frame,_ `[first, first + n)` _._[More...](#detailed-description)

* `#include <wfm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**first**](#variable-first)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |












































## Detailed Description


A stage that did not run reports `n == 0`, and its `first` is then zero as well; `first` is meaningful only for a stage that ran. 


    
## Public Attributes Documentation




### variable first 

```C++
size_t wfm_frame_span_t::first;
```



first frame-bit index the stage covers 


        

<hr>



### variable n 

```C++
size_t wfm_frame_span_t::n;
```



bits covered, or 0 if the stage did not run 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

