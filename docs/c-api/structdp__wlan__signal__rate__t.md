

# Struct dp\_wlan\_signal\_rate\_t



[**ClassList**](annotated.md) **>** [**dp\_wlan\_signal\_rate\_t**](structdp__wlan__signal__rate__t.md)



_A row of_ [_**dp\_wlan\_tables\_t**_](structdp__wlan__tables__t.md) _: Table 17-6 as printed, the data rate and the SIGNAL RATE bits of one pair._

* `#include <wlan_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**bits**](#variable-bits)  <br> |
|  uint8\_t | [**mbps**](#variable-mbps)  <br> |
|  [**dp\_wlan\_rate\_t**](structdp__wlan__rate__t.md) | [**rate**](#variable-rate)  <br> |












































## Public Attributes Documentation




### variable bits 

```C++
uint8_t dp_wlan_signal_rate_t::bits;
```



the four RATE bits, as an integer 0..15 


        

<hr>



### variable mbps 

```C++
uint8_t dp_wlan_signal_rate_t::mbps;
```



the data rate at 20 MHz in Mb/s, Table 17-6's first column; 0 = not given (synthetic tables) 


        

<hr>



### variable rate 

```C++
dp_wlan_rate_t dp_wlan_signal_rate_t::rate;
```



the rate this entry names 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wlan/wlan_core.h`

