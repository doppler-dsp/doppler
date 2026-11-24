

# Struct dp\_ccsds\_415\_scene\_t



[**ClassList**](annotated.md) **>** [**dp\_ccsds\_415\_scene\_t**](structdp__ccsds__415__scene__t.md)



_The numbers a scene at the point is built from, at one carrier: what_ [_**dp\_ccsds\_415\_scene()**_](ccsds__415_8h.md#function-dp_ccsds_415_scene) _returns._

* `#include <ccsds_415.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**chip\_rate**](#variable-chip_rate)  <br> |
|  double | [**rate\_ppm\_s**](#variable-rate_ppm_s)  <br> |
|  double | [**ratio**](#variable-ratio)  <br> |
|  double | [**start\_ppm**](#variable-start_ppm)  <br> |












































## Public Attributes Documentation




### variable chip\_rate 

```C++
double dp_ccsds_415_scene_t::chip_rate;
```



Chips a second, [**dp\_ccsds\_415\_chip\_rate()**](ccsds__415_8h.md#function-dp_ccsds_415_chip_rate). 


        

<hr>



### variable rate\_ppm\_s 

```C++
double dp_ccsds_415_scene_t::rate_ppm_s;
```



R\_env in ppm of the carrier a second: DP\_CCSDS\_415\_R\_ENV\_HZ\_S / fc x 1e6. 


        

<hr>



### variable ratio 

```C++
double dp_ccsds_415_scene_t::ratio;
```



The rate converter's ratio from an emitter at `spc` samples a chip to the capture: DP\_CCSDS\_415\_FS / (spc x chip\_rate). 


        

<hr>



### variable start\_ppm 

```C++
double dp_ccsds_415_scene_t::start_ppm;
```



The Doppler at the first sample in ppm of the carrier, the unit [**dp\_doppler\_channel\_create()**](doppler__channel__core_8h.md#function-dp_doppler_channel_create) takes: start\_hz / fc x 1e6. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ccsds/ccsds_415.h`

