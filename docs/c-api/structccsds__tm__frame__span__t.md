

# Struct ccsds\_tm\_frame\_span\_t



[**ClassList**](annotated.md) **>** [**ccsds\_tm\_frame\_span\_t**](structccsds__tm__frame__span__t.md)



_A run of CADU bits, as a half-open range_ `[first, first + n)` _._[More...](#detailed-description)

* `#include <ccsds_tm_frame.h>`





















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
size_t ccsds_tm_frame_span_t::first;
```



First CADU bit index the stage covers 


        

<hr>



### variable n 

```C++
size_t ccsds_tm_frame_span_t::n;
```



Bits covered, or 0 if the stage did not run 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ccsds_tm/ccsds_tm_frame.h`

