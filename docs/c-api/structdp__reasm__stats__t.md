

# Struct dp\_reasm\_stats\_t



[**ClassList**](annotated.md) **>** [**dp\_reasm\_stats\_t**](structdp__reasm__stats__t.md)



_What reassembling chunked frames has lost on a receiving context._ [More...](#detailed-description)

* `#include <stream.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**abandoned**](#variable-abandoned)  <br> |
|  uint64\_t | [**mid\_frame\_timeouts**](#variable-mid_frame_timeouts)  <br> |
|  uint64\_t | [**rejected**](#variable-rejected)  <br> |












































## Detailed Description


A PUB frame above the server's max\_payload arrives as chunks ([**dp\_chunk\_t**](structdp__chunk__t.md)). The receiver rebuilds one frame at a time and gives up a frame it can no longer complete instead of hiding the loss: these are the counts, from the context's creation, never reset. 


    
## Public Attributes Documentation




### variable abandoned 

```C++
uint64_t dp_reasm_stats_t::abandoned;
```



Frames given up part-assembled because a chunk of a DIFFERENT frame arrived first: a lost chunk, a subscriber that joined mid-frame, or a second publisher interleaving on the subject. That chunk starts the next frame; it is never discarded with the old one. 


        

<hr>



### variable mid\_frame\_timeouts 

```C++
uint64_t dp_reasm_stats_t::mid_frame_timeouts;
```



Receives that timed out with a frame part-assembled. The frame is kept, and the next receive resumes it. 


        

<hr>



### variable rejected 

```C++
uint64_t dp_reasm_stats_t::rejected;
```



Chunks no frame could hold, dropped: off the chunk grid, out of range, or overlapping. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/stream/stream.h`

