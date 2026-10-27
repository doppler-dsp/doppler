

# Struct dsss\_receiver\_extra\_t



[**ClassList**](annotated.md) **>** [**dsss\_receiver\_extra\_t**](structdsss__receiver__extra__t.md)



_Receiver-level payload that follows the state envelope: the routing flag, cached hit estimates, the grid (layout key) and the carrier-carry depth._ 

* `#include <dsss_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**\_pad**](#variable-_pad)  <br> |
|  uint64\_t | [**car\_carry\_len**](#variable-car_carry_len)  <br> |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  double | [**doppler\_hz\_est**](#variable-doppler_hz_est)  <br> |
|  uint64\_t | [**n**](#variable-n)  <br> |
|  uint64\_t | [**segments**](#variable-segments)  <br> |
|  uint64\_t | [**sps**](#variable-sps)  <br> |
|  uint8\_t | [**tracking**](#variable-tracking)  <br> |












































## Public Attributes Documentation




### variable \_pad 

```C++
uint8_t dsss_receiver_extra_t::_pad[7];
```



Explicit padding to 8-byte alignment. 


        

<hr>



### variable car\_carry\_len 

```C++
uint64_t dsss_receiver_extra_t::car_carry_len;
```



Valid carry samples, &lt; tsamps. 


        

<hr>



### variable cn0\_dbhz\_est 

```C++
double dsss_receiver_extra_t::cn0_dbhz_est;
```



Cached hit C/N0 estimate, dB-Hz. 


        

<hr>



### variable doppler\_hz\_est 

```C++
double dsss_receiver_extra_t::doppler_hz_est;
```



Cached hit Doppler estimate, Hz. 


        

<hr>



### variable n 

```C++
uint64_t dsss_receiver_extra_t::n;
```



Layout key; must match the live engine. 


        

<hr>



### variable segments 

```C++
uint64_t dsss_receiver_extra_t::segments;
```



Layout key; must match the live engine. 


        

<hr>



### variable sps 

```C++
uint64_t dsss_receiver_extra_t::sps;
```



Layout key; must match the live engine. 


        

<hr>



### variable tracking 

```C++
uint8_t dsss_receiver_extra_t::tracking;
```



0 searching, 1 tracking. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dsss_receiver/dsss_receiver_core.h`

