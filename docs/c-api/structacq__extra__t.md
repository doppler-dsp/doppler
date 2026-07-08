

# Struct acq\_extra\_t



[**ClassList**](annotated.md) **>** [**acq\_extra\_t**](structacq__extra__t.md)



_Per-object extra header for an engine's cross-call state._ [More...](#detailed-description)

* `#include <acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint16\_t | [**\_pad**](#variable-_pad)  <br> |
|  uint16\_t | [**has\_nc**](#variable-has_nc)  <br> |
|  uint64\_t | [**n**](#variable-n)  <br> |
|  uint32\_t | [**n\_noncoh**](#variable-n_noncoh)  <br> |
|  uint32\_t | [**n\_unconsumed**](#variable-n_unconsumed)  <br> |
|  uint32\_t | [**nc\_count**](#variable-nc_count)  <br> |
|  uint64\_t | [**samples\_consumed**](#variable-samples_consumed)  <br> |












































## Detailed Description


The state blob is the _only_ thing a fresh engine needs to continue a stream from `(descriptor, state, input)` — it makes the engine a pure transducer for the elastic fan-out (thread / process / pod). Standard bytes interface (see [**dp\_state.h**](dp__state_8h.md)); layout, contiguous and flat:


[ [**dp\_state\_hdr\_t**](structdp__state__hdr__t.md) ] [ [**acq\_extra\_t**](structacq__extra__t.md) ] [ float complex unconsumed[n\_unconsumed] ] (partial frame, &lt; n samples) [ float nc\_surface[n] ] (only when n\_noncoh &gt; 1)


Build the byte buffer with [**acq\_state\_bytes()**](acq__core_8h.md#function-acq_state_bytes); set\_state validates the envelope (magic/version/size) plus n / n\_noncoh below, rejecting a mismatch rather than reinterpreting it. 


    
## Public Attributes Documentation




### variable \_pad 

```C++
uint16_t acq_extra_t::_pad;
```




<hr>



### variable has\_nc 

```C++
uint16_t acq_extra_t::has_nc;
```



1 if nc\_surface[n] follows the samples. 


        

<hr>



### variable n 

```C++
uint64_t acq_extra_t::n;
```



Frame size; must equal engine's n. 


        

<hr>



### variable n\_noncoh 

```C++
uint32_t acq_extra_t::n_noncoh;
```



Non-coherent looks (consistency). 


        

<hr>



### variable n\_unconsumed 

```C++
uint32_t acq_extra_t::n_unconsumed;
```



Partial-frame samples that follow (&lt; n). 


        

<hr>



### variable nc\_count 

```C++
uint32_t acq_extra_t::nc_count;
```



Looks accumulated in the current dump. 


        

<hr>



### variable samples\_consumed 

```C++
uint64_t acq_extra_t::samples_consumed;
```



Stream offset framed so far. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acq/acq_core.h`

