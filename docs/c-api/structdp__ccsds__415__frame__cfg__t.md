

# Struct dp\_ccsds\_415\_frame\_cfg\_t



[**ClassList**](annotated.md) **>** [**dp\_ccsds\_415\_frame\_cfg\_t**](structdp__ccsds__415__frame__cfg__t.md)



[More...](#detailed-description)

* `#include <ccsds_415_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**gap\_sym**](#variable-gap_sym)  <br> |
|  unsigned | [**scid**](#variable-scid)  <br> |
|  unsigned | [**seq**](#variable-seq)  <br> |
|  unsigned | [**vcid**](#variable-vcid)  <br> |












































## Detailed Description


The values §1.4 leaves arbitrary (a receiver at the point reads none of them), and the gap. 


    
## Public Attributes Documentation




### variable gap\_sym 

```C++
size_t dp_ccsds_415_frame_cfg_t::gap_sym;
```



Gap symbols ahead of the acquisition sequence: the code alone. 1 to [**WFM\_FIELD\_MAX\_BITS**](wfm__frame_8h.md#define-wfm_field_max_bits), since the gap is one field of that many bits. 


        

<hr>



### variable scid 

```C++
unsigned dp_ccsds_415_frame_cfg_t::scid;
```



Spacecraft identifier, 0 to 1,023. 


        

<hr>



### variable seq 

```C++
unsigned dp_ccsds_415_frame_cfg_t::seq;
```



Frame sequence number, 0 to 255 (0: the value the COP sets on a Type-B frame). 


        

<hr>



### variable vcid 

```C++
unsigned dp_ccsds_415_frame_cfg_t::vcid;
```



Virtual channel identifier, 0 to 63. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ccsds/ccsds_415_frame.h`

