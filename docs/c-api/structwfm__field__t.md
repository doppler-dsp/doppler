

# Struct wfm\_field\_t



[**ClassList**](annotated.md) **>** [**wfm\_field\_t**](structwfm__field__t.md)



_One field of a frame — a run of bits that appears on the wire._ [More...](#detailed-description)

* `#include <wfm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**bits**](#variable-bits)  <br> |
|  unsigned | [**derived\_by**](#variable-derived_by)  <br> |
|  size\_t | [**reps**](#variable-reps)  <br> |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**seq**](#variable-seq)  <br> |












































## Detailed Description


Either the caller supplies the bits (`seq`, any [**wfm\_seq\_kind\_t**](wfm__frame_8h.md#enum-wfm_seq_kind_t)) or a stage produces them (`derived_by` non-zero: a CRC trailer, a block of Reed-Solomon check symbols). Both are fields, because both are on the wire, and making the second one a field is what removes the need for a stage to expand the field it covers. 


    
## Public Attributes Documentation




### variable bits 

```C++
size_t wfm_field_t::bits;
```



derived only: length in bits, sized by its stage 


        

<hr>



### variable derived\_by 

```C++
unsigned wfm_field_t::derived_by;
```



0 when the caller supplies this field; otherwise the index of the producing stage, plus one. The `+1` is so a zero-initialised field is a caller-supplied one rather than silently the output of stage 0. 


        

<hr>



### variable reps 

```C++
size_t wfm_field_t::reps;
```



repetitions of `seq`, verbatim; 0 means one 
 


        

<hr>



### variable seq 

```C++
wfm_seq_t wfm_field_t::seq;
```



the bits, when the caller supplies them 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

