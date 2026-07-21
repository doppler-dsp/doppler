

# Struct dsss\_receiver\_state\_t



[**ClassList**](annotated.md) **>** [**dsss\_receiver\_state\_t**](structdsss__receiver__state__t.md)



_Composed receiver state._ [More...](#detailed-description)

* `#include <dsss_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**acq\_state\_t**](structacq__state__t.md) \* | [**acq**](#variable-acq)  <br> |
|  double | [**chip\_rate**](#variable-chip_rate)  <br> |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  uint8\_t \* | [**code**](#variable-code)  <br> |
|  size\_t | [**code\_len**](#variable-code_len)  <br> |
|  int | [**differential**](#variable-differential)  <br> |
|  [**dll\_state\_t**](structdll__state__t.md) \* | [**dll**](#variable-dll)  <br> |
|  double | [**doppler\_hz\_est**](#variable-doppler_hz_est)  <br> |
|  int | [**m**](#variable-m)  <br> |
|  int | [**n**](#variable-n)  <br> |
|  [**RateConverter\_state\_t**](structRateConverter__state__t.md) \* | [**rc**](#variable-rc)  <br> |
|  [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) \* | [**rx**](#variable-rx)  <br> |
|  uint64\_t | [**samples\_fed**](#variable-samples_fed)  <br> |
|  size\_t | [**segments**](#variable-segments)  <br> |
|  size\_t | [**spc**](#variable-spc)  <br> |
|  size\_t | [**sps**](#variable-sps)  <br> |
|  double | [**symbol\_rate**](#variable-symbol_rate)  <br> |
|  int | [**tracking**](#variable-tracking)  <br> |












































## Detailed Description


Owns all four children for the object's entire lifetime  `dll`/ `rc`/`rx` are allocated at create() with a placeholder seed (phase 0, no Doppler) and REBUILT (not freed to NULL) the moment a real hit fires, on every configure\_chain\_raw(), and back to the placeholder seed on reset(). This keeps every child's pointer always valid and the serialized blob's layout independent of `tracking`  a fixed shape, not a conditionally-present one. `tracking` is purely a routing flag for steps() (search vs. the despread/resample/demod chain), not a lifecycle state. Treat all fields as internal (use the getters). 


    
## Public Attributes Documentation




### variable acq 

```C++
acq_state_t* dsss_receiver_state_t::acq;
```




<hr>



### variable chip\_rate 

```C++
double dsss_receiver_state_t::chip_rate;
```




<hr>



### variable cn0\_dbhz\_est 

```C++
double dsss_receiver_state_t::cn0_dbhz_est;
```



Cached from the winning acquisition hit. 


        

<hr>



### variable code 

```C++
uint8_t* dsss_receiver_state_t::code;
```




<hr>



### variable code\_len 

```C++
size_t dsss_receiver_state_t::code_len;
```




<hr>



### variable differential 

```C++
int dsss_receiver_state_t::differential;
```




<hr>



### variable dll 

```C++
dll_state_t* dsss_receiver_state_t::dll;
```




<hr>



### variable doppler\_hz\_est 

```C++
double dsss_receiver_state_t::doppler_hz_est;
```



Cached from the winning acquisition hit. 


        

<hr>



### variable m 

```C++
int dsss_receiver_state_t::m;
```




<hr>



### variable n 

```C++
int dsss_receiver_state_t::n;
```



MpskReceiver's own carrier-arm count. 


        

<hr>



### variable rc 

```C++
RateConverter_state_t* dsss_receiver_state_t::rc;
```




<hr>



### variable rx 

```C++
mpsk_receiver_state_t* dsss_receiver_state_t::rx;
```




<hr>



### variable samples\_fed 

```C++
uint64_t dsss_receiver_state_t::samples_fed;
```



Running total handed to [**acq\_push()**](acq__core_8h.md#function-acq_push) so far — diffed against acq-&gt;samples\_consumed right after a hit to find the exact unconsumed tail of the current call. 


        

<hr>



### variable segments 

```C++
size_t dsss_receiver_state_t::segments;
```



Dll's own tracking parameter. 


        

<hr>



### variable spc 

```C++
size_t dsss_receiver_state_t::spc;
```




<hr>



### variable sps 

```C++
size_t dsss_receiver_state_t::sps;
```



MpskReceiver's own samples/symbol. 


        

<hr>



### variable symbol\_rate 

```C++
double dsss_receiver_state_t::symbol_rate;
```




<hr>



### variable tracking 

```C++
int dsss_receiver_state_t::tracking;
```



0 = searching, 1 = locked and demodulating. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dsss_receiver/dsss_receiver_core.h`

