

# Struct wfm\_stage\_t



[**ClassList**](annotated.md) **>** [**wfm\_stage\_t**](structwfm__stage__t.md)



_One transform, and — the whole point — the fields it covers._ [More...](#detailed-description)

* `#include <wfm_frame.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  unsigned | [**depth**](#variable-depth)  <br> |
|  unsigned | [**emit\_den**](#variable-emit_den)  <br> |
|  unsigned | [**emit\_num**](#variable-emit_num)  <br> |
|  unsigned | [**first\_field**](#variable-first_field)  <br> |
|  [**wfm\_stage\_kind\_t**](wfm__frame_8h.md#enum-wfm_stage_kind_t) | [**kind**](#variable-kind)  <br> |
|  unsigned | [**n\_fields**](#variable-n_fields)  <br> |












































## Detailed Description


**`n_fields` is load-bearing, not a refinement.** `ccsds_tm_frame.h` states the failure this prevents: \*"any chain of optional transforms
applied to 'the frame' is right at three stage boundaries and wrong at
the fourth, and wrong in the direction that still encodes, still decodes
against itself, and syncs to nothing."\* A stage that inherited "whatever
ran before me" would be that chain. CCSDS is the case that proves it — the marker is covered by the inner code and by neither the outer code nor the randomiser — and any frame with a sync word has the same shape.


The cover is what the stage OCCUPIES on the wire, so for a code it is the information _and_ the check symbols it derives. What the stage reads is the cover minus the fields it derives, which is why both are one declaration rather than two that can disagree. 


    
## Public Attributes Documentation




### variable depth 

```C++
unsigned wfm_stage_t::depth;
```



RS: interleaving depth 
 


        

<hr>



### variable emit\_den 

```C++
unsigned wfm_stage_t::emit_den;
```




<hr>



### variable emit\_num 

```C++
unsigned wfm_stage_t::emit_num;
```



A stage that consumes the assembled frame and emits a DIFFERENT stream sets these: the output is `n * emit_num / emit_den` bits. `emit_num == 0` means the stage stays inside the frame. Only the inner code does the former today, and it is exactly why [**wfm\_frame\_desc\_layout\_t**](structwfm__frame__desc__layout__t.md) reports `frame_bits` and `out_bits` as two numbers rather than one. 


        

<hr>



### variable first\_field 

```C++
unsigned wfm_stage_t::first_field;
```



first field covered 
 


        

<hr>



### variable kind 

```C++
wfm_stage_kind_t wfm_stage_t::kind;
```




<hr>



### variable n\_fields 

```C++
unsigned wfm_stage_t::n_fields;
```



fields covered; 0 = does not run 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfm/wfm_frame.h`

