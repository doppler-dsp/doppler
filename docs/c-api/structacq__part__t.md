

# Struct acq\_part\_t



[**ClassList**](annotated.md) **>** [**acq\_part\_t**](structacq__part__t.md)



_One tile's share of a decided surface (design §2.3): the surface is cut into_ `window_bins` _chunks of whole rows, and the per-cell passes after the fan_ _the magnitude, the CFAR reference, the mask copy, each scan of the peak list_ _run per chunk into one of these, merged serially in tile order. The merge is bit-identical at any thread count because the chunks never move._

* `#include <acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**best**](#variable-best)  <br> |
|  float | [**ref**](#variable-ref)  <br> |












































## Public Attributes Documentation




### variable best 

```C++
size_t acq_part_t::best;
```



The chunk's first maximum among the candidate cells; `n_surf` when it has none. 
 


        

<hr>



### variable ref 

```C++
float acq_part_t::ref;
```



det\_noise\_estimate() over the chunk (MEAN/MIN/MAX). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acq/acq_core.h`

