

# Struct mpsk\_receiver\_state\_t



[**ClassList**](annotated.md) **>** [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md)



_M-PSK receiver state._ [More...](#detailed-description)

* `#include <mpsk_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**auto\_handover**](#variable-auto_handover)  <br> |
|  [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) | [**car**](#variable-car)  <br> |
|  int | [**differential**](#variable-differential)  <br> |
|  int | [**have\_prev\_idx**](#variable-have_prev_idx)  <br> |
|  double | [**lock\_thresh**](#variable-lock_thresh)  <br> |
|  int | [**m**](#variable-m)  <br> |
|  [**fir\_state\_t**](structfir__state__t.md) \* | [**mf**](#variable-mf)  <br> |
|  float \* | [**mf\_taps**](#variable-mf_taps)  <br> |
|  int | [**n**](#variable-n)  <br> |
|  unsigned | [**prev\_idx**](#variable-prev_idx)  <br> |
|  int | [**pulse**](#variable-pulse)  <br> |
|  double | [**rrc\_beta**](#variable-rrc_beta)  <br> |
|  int | [**rrc\_span**](#variable-rrc_span)  <br> |
|  size\_t | [**sps**](#variable-sps)  <br> |
|  size\_t | [**sym\_count**](#variable-sym_count)  <br> |
|  float complex | [**sym\_rot**](#variable-sym_rot)  <br> |
|  [**symsync\_state\_t**](structsymsync__state__t.md) | [**sync**](#variable-sync)  <br> |
|  int | [**tracking**](#variable-tracking)  <br> |
|  size\_t | [**warmup\_syms**](#variable-warmup_syms)  <br> |












































## Detailed Description


Allocate with [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create). Embeds the carrier loop (`car`) and symbol-timing loop (`sync`) by value and owns the matched-filter FIR; the carrier NCO and lock metric live in `car`. Treat all fields as internal (use the getters); they are exposed for the inline sample loop. 


    
## Public Attributes Documentation




### variable auto\_handover 

```C++
int mpsk_receiver_state_t::auto_handover;
```



opt-in NDA-&gt;decision handover. 


        

<hr>



### variable car 

```C++
carrier_nda_state_t mpsk_receiver_state_t::car;
```



carrier loop: wipe NCO + arm I/D + NDA. 


        

<hr>



### variable differential 

```C++
int mpsk_receiver_state_t::differential;
```



bits(): differential demap. 


        

<hr>



### variable have\_prev\_idx 

```C++
int mpsk_receiver_state_t::have_prev_idx;
```



differential: prev\_idx valid. 


        

<hr>



### variable lock\_thresh 

```C++
double mpsk_receiver_state_t::lock_thresh;
```



handover lock-metric threshold. 


        

<hr>



### variable m 

```C++
int mpsk_receiver_state_t::m;
```



constellation order M (2, 4, 8). 


        

<hr>



### variable mf 

```C++
fir_state_t* mpsk_receiver_state_t::mf;
```



matched filter on the de-rotated stream. 


        

<hr>



### variable mf\_taps 

```C++
float* mpsk_receiver_state_t::mf_taps;
```



owned real MF taps. 


        

<hr>



### variable n 

```C++
int mpsk_receiver_state_t::n;
```



arm dumps per symbol. 


        

<hr>



### variable prev\_idx 

```C++
unsigned mpsk_receiver_state_t::prev_idx;
```



differential: prev sliced index. 


        

<hr>



### variable pulse 

```C++
int mpsk_receiver_state_t::pulse;
```



MPSK\_RX\_PULSE\_IANDD / \_RRC. 


        

<hr>



### variable rrc\_beta 

```C++
double mpsk_receiver_state_t::rrc_beta;
```



RRC roll-off (pulse == RRC). 


        

<hr>



### variable rrc\_span 

```C++
int mpsk_receiver_state_t::rrc_span;
```



RRC one-sided span, symbols. 


        

<hr>



### variable sps 

```C++
size_t mpsk_receiver_state_t::sps;
```



samples per symbol. 


        

<hr>



### variable sym\_count 

```C++
size_t mpsk_receiver_state_t::sym_count;
```



symbols emitted (warmup counter). 


        

<hr>



### variable sym\_rot 

```C++
float complex mpsk_receiver_state_t::sym_rot;
```



exp(j\*phi0): NDA-grid -&gt; slicer. 


        

<hr>



### variable sync 

```C++
symsync_state_t mpsk_receiver_state_t::sync;
```



Gardner symbol-timing loop (by value). 


        

<hr>



### variable tracking 

```C++
int mpsk_receiver_state_t::tracking;
```



0 = NDA acquire, 1 = decision. 


        

<hr>



### variable warmup\_syms 

```C++
size_t mpsk_receiver_state_t::warmup_syms;
```



symbols before handover allowed. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/mpsk_receiver/mpsk_receiver_core.h`

