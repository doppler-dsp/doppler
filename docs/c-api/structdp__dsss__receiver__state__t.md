

# Struct dp\_dsss\_receiver\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_dsss\_receiver\_state\_t**](structdp__dsss__receiver__state__t.md)



_Composed receiver state._ [More...](#detailed-description)

* `#include <dsss_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_acq\_state\_t**](structdp__acq__state__t.md) \* | [**acq**](#variable-acq)  <br> |
|  [**dp\_costas\_state\_t**](structdp__costas__state__t.md) | [**car**](#variable-car)  <br> |
|  float \_Complex \* | [**car\_carry\_buf**](#variable-car_carry_buf)  <br> |
|  size\_t | [**car\_carry\_len**](#variable-car_carry_len)  <br> |
|  float \_Complex \* | [**car\_wiped\_buf**](#variable-car_wiped_buf)  <br> |
|  double | [**chip\_rate**](#variable-chip_rate)  <br> |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  uint8\_t \* | [**code**](#variable-code)  <br> |
|  size\_t | [**code\_len**](#variable-code_len)  <br> |
|  int | [**differential**](#variable-differential)  <br> |
|  [**dp\_dll\_state\_t**](structdp__dll__state__t.md) \* | [**dll**](#variable-dll)  <br> |
|  double | [**doppler\_hz\_est**](#variable-doppler_hz_est)  <br> |
|  int | [**m**](#variable-m)  <br> |
|  int | [**n**](#variable-n)  <br> |
|  [**dp\_RateConverter\_state\_t**](structdp__RateConverter__state__t.md) \* | [**rc**](#variable-rc)  <br> |
|  [**dp\_mpsk\_receiver\_state\_t**](structdp__mpsk__receiver__state__t.md) \* | [**rx**](#variable-rx)  <br> |
|  size\_t | [**segments**](#variable-segments)  <br> |
|  size\_t | [**spc**](#variable-spc)  <br> |
|  size\_t | [**sps**](#variable-sps)  <br> |
|  double | [**symbol\_rate**](#variable-symbol_rate)  <br> |
|  int | [**tracking**](#variable-tracking)  <br> |
|  float \_Complex \* | [**trk\_dll\_buf**](#variable-trk_dll_buf)  <br> |
|  size\_t | [**trk\_dll\_cap**](#variable-trk_dll_cap)  <br> |
|  float \_Complex \* | [**trk\_rc\_buf**](#variable-trk_rc_buf)  <br> |
|  size\_t | [**trk\_rc\_cap**](#variable-trk_rc_cap)  <br> |
|  size\_t | [**tsamps**](#variable-tsamps)  <br> |












































## Detailed Description


Owns all four children for the object's entire lifetime  `dll`/ `rc`/`rx` are allocated at create() with a placeholder seed (phase 0, no Doppler) and REBUILT (not freed to NULL) the moment a real hit fires, on every configure\_chain\_raw(), and back to the placeholder seed on reset(). This keeps every child's pointer always valid and the serialized blob's layout independent of `tracking`  a fixed shape, not a conditionally-present one. `tracking` is purely a routing flag for steps() (search vs. the despread/resample/demod chain), not a lifecycle state. Treat all fields as internal (use the getters). 


    
## Public Attributes Documentation




### variable acq 

```C++
dp_acq_state_t* dp_dsss_receiver_state_t::acq;
```



Owned continuous acquisition engine; fed while `tracking` is 0. 


        

<hr>



### variable car 

```C++
dp_costas_state_t dp_dsss_receiver_state_t::car;
```



Pre-despread carrier loop  embedded by value, same pattern [**dp\_despreader\_state\_t**](structdp__despreader__state__t.md)'s own `car` uses. Rebuilt alongside dll/rc/rx (create-time placeholder, every real hit, configure\_chain\_raw(), reset()). One update per code period (`tsamps` samples). 


        

<hr>



### variable car\_carry\_buf 

```C++
float _Complex* dp_dsss_receiver_state_t::car_carry_buf;
```



Genuinely stateful: raw samples handed to steps() that didn't fill a whole code period yet, carried to the NEXT call so the carrier loop always wipes exact, complete periods regardless of how a caller chunks its input (this object's own "any block size, state
carries across calls" contract). Capacity `tsamps`; only the first `car_carry_len` samples are valid. Must be serialized (see below) or a resumed instance silently loses its period alignment. 


        

<hr>



### variable car\_carry\_len 

```C++
size_t dp_dsss_receiver_state_t::car_carry_len;
```



Valid samples in `car_carry_buf`, 0..tsamps-1; running state. 


        

<hr>



### variable car\_wiped\_buf 

```C++
float _Complex* dp_dsss_receiver_state_t::car_wiped_buf;
```



Scratch, not state: sized `tsamps`, allocated once (never per steps() call  no allocation in the hot streaming path). 


        

<hr>



### variable chip\_rate 

```C++
double dp_dsss_receiver_state_t::chip_rate;
```



Chip rate, Hz; config. 


        

<hr>



### variable cn0\_dbhz\_est 

```C++
double dp_dsss_receiver_state_t::cn0_dbhz_est;
```



Cached from the winning acquisition hit. 


        

<hr>



### variable code 

```C++
uint8_t* dp_dsss_receiver_state_t::code;
```



Own copy of the spreading code  [**dp\_acq\_create\_continuous()**](acq__core_8h.md#function-dp_acq_create_continuous)/ [**dp\_dll\_create()**](dll__core_8h.md#function-dp_dll_create)'s own borrow-vs-copy semantics aren't part of either's public contract, so this object keeps a persistent copy rather than depend on being able to read it back out of a child, or on the caller's original buffer outliving construction. 


        

<hr>



### variable code\_len 

```C++
size_t dp_dsss_receiver_state_t::code_len;
```



Chips in `code` (spreading factor); config. 


        

<hr>



### variable differential 

```C++
int dp_dsss_receiver_state_t::differential;
```



Nonzero = differential demap; config. 


        

<hr>



### variable dll 

```C++
dp_dll_state_t* dp_dsss_receiver_state_t::dll;
```



Owned code loop (despreader); always allocated, rebuilt on each hit, reset() and configure\_chain\_raw(). 


        

<hr>



### variable doppler\_hz\_est 

```C++
double dp_dsss_receiver_state_t::doppler_hz_est;
```



Cached from the winning acquisition hit. 


        

<hr>



### variable m 

```C++
int dp_dsss_receiver_state_t::m;
```



PSK order (2, 4 or 8); config. 


        

<hr>



### variable n 

```C++
int dp_dsss_receiver_state_t::n;
```



MpskReceiver's terminal outputs per symbol (`m_out`). 
 


        

<hr>



### variable rc 

```C++
dp_RateConverter_state_t* dp_dsss_receiver_state_t::rc;
```



Owned resampler from the Dll's partial rate to sps\*symbol\_rate; rebuilt with `dll`. 


        

<hr>



### variable rx 

```C++
dp_mpsk_receiver_state_t* dp_dsss_receiver_state_t::rx;
```



Owned demodulator at `sps` samples/symbol; rebuilt with `dll`. 


        

<hr>



### variable segments 

```C++
size_t dp_dsss_receiver_state_t::segments;
```



Dll's own tracking parameter. 
 


        

<hr>



### variable spc 

```C++
size_t dp_dsss_receiver_state_t::spc;
```



Samples per chip at the input; config. 


        

<hr>



### variable sps 

```C++
size_t dp_dsss_receiver_state_t::sps;
```



MpskReceiver's own samples/symbol. 
 


        

<hr>



### variable symbol\_rate 

```C++
double dp_dsss_receiver_state_t::symbol_rate;
```



Data-symbol rate, Hz; config. 


        

<hr>



### variable tracking 

```C++
int dp_dsss_receiver_state_t::tracking;
```



0 = searching, 1 = locked and demodulating. 
 


        

<hr>



### variable trk\_dll\_buf 

```C++
float _Complex* dp_dsss_receiver_state_t::trk_dll_buf;
```



Scratch, not state: the tracking chain's Dll output, grown on demand to the largest block seen and kept, so a tracking steps() allocates nothing once warm (doppler#2650). Freed in destroy(). 


        

<hr>



### variable trk\_dll\_cap 

```C++
size_t dp_dsss_receiver_state_t::trk_dll_cap;
```



Capacity of `trk_dll_buf`, samples. 


        

<hr>



### variable trk\_rc\_buf 

```C++
float _Complex* dp_dsss_receiver_state_t::trk_rc_buf;
```



Scratch, not state: the tracking chain's RateConverter output, grown on demand and kept like `trk_dll_buf`. 


        

<hr>



### variable trk\_rc\_cap 

```C++
size_t dp_dsss_receiver_state_t::trk_rc_cap;
```



Capacity of `trk_rc_buf`, samples. 


        

<hr>



### variable tsamps 

```C++
size_t dp_dsss_receiver_state_t::tsamps;
```



code\_len\*spc  one code period, samples; the carrier loop's own fixed chunk size. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dsss_receiver/dsss_receiver_core.h`

