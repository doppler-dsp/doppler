

# Struct dp\_wlan\_signal\_rate\_t



[**ClassList**](annotated.md) **>** [**dp\_wlan\_signal\_rate\_t**](structdp__wlan__signal__rate__t.md)



_A (rate, SIGNAL RATE bits) pair of_ [_**dp\_wlan\_tables\_t**_](structdp__wlan__tables__t.md) _._

* `#include <wlan_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**bits**](#variable-bits)  <br> |
|  [**dp\_wlan\_rate\_t**](structdp__wlan__rate__t.md) | [**rate**](#variable-rate)  <br> |












































## Public Attributes Documentation




### variable bits 

```C++
uint8_t dp_wlan_signal_rate_t::bits;
```



the four RATE bits, as an integer 0..15 


        

<hr>



### variable rate 

```C++
dp_wlan_rate_t dp_wlan_signal_rate_t::rate;
```



the rate this entry names 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wlan/wlan_core.h`

