

# Struct dp\_wlan\_rate\_t



[**ClassList**](annotated.md) **>** [**dp\_wlan\_rate\_t**](structdp__wlan__rate__t.md)



_A non-HT rate, by the two columns of Table 17-4 that do not change with the clocking: a constellation and a code rate._ [More...](#detailed-description)

* `#include <wlan_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_wlan\_code\_t**](wlan__core_8h.md#enum-dp_wlan_code_t) | [**code**](#variable-code)  <br> |
|  unsigned | [**n\_bpsc**](#variable-n_bpsc)  <br> |












































## Detailed Description


`n_bpsc` is bits per subcarrier: 1 BPSK, 2 QPSK, 4 16-QAM, 6 64-QAM. 


    
## Public Attributes Documentation




### variable code 

```C++
dp_wlan_code_t dp_wlan_rate_t::code;
```



the BCC rate 


        

<hr>



### variable n\_bpsc 

```C++
unsigned dp_wlan_rate_t::n_bpsc;
```



1, 2, 4 or 6 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wlan/wlan_core.h`

