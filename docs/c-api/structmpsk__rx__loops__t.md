

# Struct mpsk\_rx\_loops\_t



[**ClassList**](annotated.md) **>** [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md)



_The receiver's loops: timing, carrier, handover, demapper._ 

* `#include <mpsk_rx_loops.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**acq\_to\_track**](#variable-acq_to_track)  <br> |
|  [**boxcar\_state\_t**](structboxcar__state__t.md) | [**arm**](#variable-arm)  <br> |
|  double | [**bn\_agc\_ratio**](#variable-bn_agc_ratio)  <br> |
|  double | [**bn\_carrier**](#variable-bn_carrier)  <br> |
|  double | [**car\_error**](#variable-car_error)  <br> |
|  [**loop\_filter\_state\_t**](structloop__filter__state__t.md) | [**car\_lf**](#variable-car_lf)  <br> |
|  [**lockdet\_state\_t**](structlockdet__state__t.md) | [**car\_lock**](#variable-car_lock)  <br> |
|  int | [**differential**](#variable-differential)  <br> |
|  double | [**freq\_ctrl**](#variable-freq_ctrl)  <br> |
|  double | [**freq\_scale**](#variable-freq_scale)  <br> |
|  [**lockdet\_state\_t**](structlockdet__state__t.md) | [**handover**](#variable-handover)  <br> |
|  int | [**have\_prev\_idx**](#variable-have_prev_idx)  <br> |
|  double | [**lo\_sps**](#variable-lo_sps)  <br> |
|  double | [**lock**](#variable-lock)  <br> |
|  int | [**m**](#variable-m)  <br> |
|  size\_t | [**m\_out**](#variable-m_out)  <br> |
|  int | [**nda\_tap**](#variable-nda_tap)  <br> |
|  double | [**pre\_sps**](#variable-pre_sps)  <br> |
|  unsigned | [**prev\_idx**](#variable-prev_idx)  <br> |
|  double | [**sps**](#variable-sps)  <br> |
|  size\_t | [**sym\_count**](#variable-sym_count)  <br> |
|  float complex | [**sym\_rot**](#variable-sym_rot)  <br> |
|  int | [**tap\_timed**](#variable-tap_timed)  <br> |
|  [**ratesync\_loop\_t**](structratesync__loop__t.md) | [**timing**](#variable-timing)  <br> |
|  [**mpsk\_rx\_tlm\_t**](structmpsk__rx__tlm__t.md) | [**tlm**](#variable-tlm)  <br> |
|  int | [**tracking**](#variable-tracking)  <br> |
|  size\_t | [**warmup\_syms**](#variable-warmup_syms)  <br> |
|  double | [**zeta**](#variable-zeta)  <br> |












































## Public Attributes Documentation




### variable acq\_to\_track 

```C++
int mpsk_rx_loops_t::acq_to_track;
```



opt-in two-way handover. 
 


        

<hr>



### variable arm 

```C++
boxcar_state_t mpsk_rx_loops_t::arm;
```



free-running arm filter; LO\_ARM only. 


        

<hr>



### variable bn\_agc\_ratio 

```C++
double mpsk_rx_loops_t::bn_agc_ratio;
```



AGC bandwidth as a fraction of the slowest loop; see [**mpsk\_rx\_agc\_bn()**](mpsk__rx__loops_8h.md#function-mpsk_rx_agc_bn). 
 


        

<hr>



### variable bn\_carrier 

```C++
double mpsk_rx_loops_t::bn_carrier;
```



carrier loop noise bandwidth (per symbol). 
 


        

<hr>



### variable car\_error 

```C++
double mpsk_rx_loops_t::car_error;
```



last carrier phase discriminator (stress). 
 


        

<hr>



### variable car\_lf 

```C++
loop_filter_state_t mpsk_rx_loops_t::car_lf;
```



2nd-order carrier PI loop. 
 


        

<hr>



### variable car\_lock 

```C++
lockdet_state_t mpsk_rx_loops_t::car_lock;
```



de-chattered binary carrier lock. 
 


        

<hr>



### variable differential 

```C++
int mpsk_rx_loops_t::differential;
```



bits(): differential demap. 
 


        

<hr>



### variable freq\_ctrl 

```C++
double mpsk_rx_loops_t::freq_ctrl;
```



carrier command now applied, cycles/sample at the LO's own rate. 
 


        

<hr>



### variable freq\_scale 

```C++
double mpsk_rx_loops_t::freq_scale;
```



loop-filter output -&gt; freq\_ctrl; rad/symbol to cycles per LO sample, set once at init. 
 


        

<hr>



### variable handover 

```C++
lockdet_state_t mpsk_rx_loops_t::handover;
```



verify-counted declare/drop rule. 
 


        

<hr>



### variable have\_prev\_idx 

```C++
int mpsk_rx_loops_t::have_prev_idx;
```



differential: prev\_idx valid. 
 


        

<hr>



### variable lo\_sps 

```C++
double mpsk_rx_loops_t::lo_sps;
```



samples per symbol at the LO's own rate. 
 


        

<hr>



### variable lock 

```C++
double mpsk_rx_loops_t::lock;
```



EMA of the carrier lock signal. 
 


        

<hr>



### variable m 

```C++
int mpsk_rx_loops_t::m;
```



constellation order M (2, 4, 8). 
 


        

<hr>



### variable m\_out 

```C++
size_t mpsk_rx_loops_t::m_out;
```



terminal outputs per symbol. 
 


        

<hr>



### variable nda\_tap 

```C++
int mpsk_rx_loops_t::nda_tap;
```



MPSK\_RX\_NDA\_TAP\_\* — where the NDA disc reads. 


        

<hr>



### variable pre\_sps 

```C++
double mpsk_rx_loops_t::pre_sps;
```



samples per symbol pre-terminal (`bank_sps`); PRETERM only. A planner outcome, read from the cascade rather than configured. 
 


        

<hr>



### variable prev\_idx 

```C++
unsigned mpsk_rx_loops_t::prev_idx;
```



differential: prev sliced index. 
 


        

<hr>



### variable sps 

```C++
double mpsk_rx_loops_t::sps;
```



samples per symbol at the receiver's input. 
 


        

<hr>



### variable sym\_count 

```C++
size_t mpsk_rx_loops_t::sym_count;
```



symbols emitted (warmup counter). 
 


        

<hr>



### variable sym\_rot 

```C++
float complex mpsk_rx_loops_t::sym_rot;
```



exp(j\*phi0): NDA grid -&gt; slicer. 
 


        

<hr>



### variable tap\_timed 

```C++
int mpsk_rx_loops_t::tap_timed;
```



tap depends on symbol timing (STROBE only). 
 


        

<hr>



### variable timing 

```C++
ratesync_loop_t mpsk_rx_loops_t::timing;
```



the shared timing loop -&gt; rate\_ctrl. 
 


        

<hr>



### variable tlm 

```C++
mpsk_rx_tlm_t mpsk_rx_loops_t::tlm;
```



live attachment; zeroed in state blobs. 
 


        

<hr>



### variable tracking 

```C++
int mpsk_rx_loops_t::tracking;
```



0 = NDA acquire, 1 = decision. 
 


        

<hr>



### variable warmup\_syms 

```C++
size_t mpsk_rx_loops_t::warmup_syms;
```



symbols before the switch is allowed. 
 


        

<hr>



### variable zeta 

```C++
double mpsk_rx_loops_t::zeta;
```



damping factor for both loops. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/mpsk_receiver/mpsk_rx_loops.h`

