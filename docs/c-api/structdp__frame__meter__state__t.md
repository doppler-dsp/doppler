

# Struct dp\_frame\_meter\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_frame\_meter\_state\_t**](structdp__frame__meter__state__t.md)



_Frame-outcome accumulator. Allocate with_ [_**dp\_frame\_meter\_create()**_](frame__meter__core_8h.md#function-dp_frame_meter_create) _._

* `#include <frame_meter_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**conf**](#variable-conf)  <br> |
|  size\_t | [**crc\_passed**](#variable-crc_passed)  <br> |
|  size\_t | [**errors**](#variable-errors)  <br> |
|  size\_t | [**frames**](#variable-frames)  <br> |
|  size\_t | [**sync\_detected**](#variable-sync_detected)  <br> |
|  size\_t | [**target\_errors**](#variable-target_errors)  <br> |












































## Public Attributes Documentation




### variable conf 

```C++
double dp_frame_meter_state_t::conf;
```



config: confidence level for the interval 


        

<hr>



### variable crc\_passed 

```C++
size_t dp_frame_meter_state_t::crc_passed;
```



running: frames whose CRC checked 
 


        

<hr>



### variable errors 

```C++
size_t dp_frame_meter_state_t::errors;
```



running: frames not delivered 
 


        

<hr>



### variable frames 

```C++
size_t dp_frame_meter_state_t::frames;
```



running: frames attempted 
 


        

<hr>



### variable sync\_detected 

```C++
size_t dp_frame_meter_state_t::sync_detected;
```



running: frames whose sync was found 
 


        

<hr>



### variable target\_errors 

```C++
size_t dp_frame_meter_state_t::target_errors;
```



config: stop-on-errors target 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/frame_meter/frame_meter_core.h`

