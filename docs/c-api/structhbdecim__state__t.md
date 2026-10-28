

# Struct hbdecim\_state\_t



[**ClassList**](annotated.md) **>** [**hbdecim\_state\_t**](structhbdecim__state__t.md)



_State of a CF32 halfband 2:1 decimator._ [More...](#detailed-description)

* `#include <hbdecim_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**centre**](#variable-centre)  <br> |
|  size\_t | [**consumed**](#variable-consumed)  <br> |
|  float \_Complex \* | [**even\_buf**](#variable-even_buf)  <br> |
|  size\_t | [**even\_cap**](#variable-even_cap)  <br> |
|  size\_t | [**even\_head**](#variable-even_head)  <br> |
|  size\_t | [**even\_mask**](#variable-even_mask)  <br> |
|  int | [**fir\_on\_even**](#variable-fir_on_even)  <br> |
|  float \* | [**h**](#variable-h)  <br> |
|  int | [**has\_pending**](#variable-has_pending)  <br> |
|  size\_t | [**num\_taps**](#variable-num_taps)  <br> |
|  float \_Complex \* | [**odd\_buf**](#variable-odd_buf)  <br> |
|  size\_t | [**odd\_head**](#variable-odd_head)  <br> |
|  float \_Complex | [**pending**](#variable-pending)  <br> |












































## Detailed Description


Allocate with [**dp\_hbdecim\_create()**](hbdecim__core_8h.md#function-dp_hbdecim_create) and free with [**dp\_hbdecim\_destroy()**](hbdecim__core_8h.md#function-dp_hbdecim_destroy). The layout is public because [**HalfbandDecimator\_core.h**](HalfbandDecimator__core_8h.md) aliases it, but callers should treat every field as read-only: the config fields are set once by create, and the running fields are owned by execute, reset and set\_state. 


    
## Public Attributes Documentation




### variable centre 

```C++
size_t hbdecim_state_t::centre;
```



num\_taps / 2 (integer division): the delay-line index the half-weight delay branch reads (config) 


        

<hr>



### variable consumed 

```C++
size_t hbdecim_state_t::consumed;
```



Input samples the last execute took; per-call, not in the blob (0 after create, reset and set\_state) 


        

<hr>



### variable even\_buf 

```C++
float _Complex* hbdecim_state_t::even_buf;
```



Owned even-sample ring of 2\*even\_cap samples; each push writes index head and head+even\_cap, so even\_cap samples from head are always contiguous (running state) 


        

<hr>



### variable even\_cap 

```C++
size_t hbdecim_state_t::even_cap;
```



Ring length: the smallest power of two &gt;= num\_taps (config; shared by both rings) 


        

<hr>



### variable even\_head 

```C++
size_t hbdecim_state_t::even_head;
```



Index of the newest sample in even\_buf; moves down by one per push (running state) 


        

<hr>



### variable even\_mask 

```C++
size_t hbdecim_state_t::even_mask;
```



even\_cap - 1, the wrap mask for both rings (config) 


        

<hr>



### variable fir\_on\_even 

```C++
int hbdecim_state_t::fir_on_even;
```



1 if N is even (FIR runs on the even-sample line), 0 if N is odd (FIR on the odd line) (config) 


        

<hr>



### variable h 

```C++
float* hbdecim_state_t::h;
```



Owned copy of the N FIR coefficients, each scaled by 0.5 (polyphase normalization); only the first N/2 are read, the branch being symmetric (config) 


        

<hr>



### variable has\_pending 

```C++
int hbdecim_state_t::has_pending;
```



1 when an unpaired sample is held for the next execute call, else 0 (running state) 


        

<hr>



### variable num\_taps 

```C++
size_t hbdecim_state_t::num_taps;
```



FIR branch length N, as passed to create (config) 


        

<hr>



### variable odd\_buf 

```C++
float _Complex* hbdecim_state_t::odd_buf;
```



Owned odd-sample ring of 2\*even\_cap samples, dual-write like even\_buf (running state) 


        

<hr>



### variable odd\_head 

```C++
size_t hbdecim_state_t::odd_head;
```



Index of the newest sample in odd\_buf (running state) 


        

<hr>



### variable pending 

```C++
float _Complex hbdecim_state_t::pending;
```



The held even sample; 0 when none is held (running state) 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/hbdecim/hbdecim_core.h`

