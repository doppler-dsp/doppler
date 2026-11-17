

# Struct dp\_wlan\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_wlan\_state\_t**](structdp__wlan__state__t.md)



_A non-HT rate, held with what follows from it._ [More...](#detailed-description)

* `#include <wlan_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**code**](#variable-code)  <br> |
|  uint32\_t | [**n\_bpsc**](#variable-n_bpsc)  <br> |
|  uint32\_t | [**n\_cbps**](#variable-n_cbps)  <br> |
|  uint32\_t | [**n\_dbps**](#variable-n_dbps)  <br> |












































## Detailed Description


Immutable after [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create): the rate is configuration, so there is no running state and no state blob. 


    
## Public Attributes Documentation




### variable code 

```C++
uint32_t dp_wlan_state_t::code;
```



the dp\_wlan\_code\_t, as an integer 
 


        

<hr>



### variable n\_bpsc 

```C++
uint32_t dp_wlan_state_t::n_bpsc;
```



bits per subcarrier: 1, 2, 4 or 6 
 


        

<hr>



### variable n\_cbps 

```C++
uint32_t dp_wlan_state_t::n_cbps;
```



coded bits per OFDM symbol, 48 \* n\_bpsc 
 


        

<hr>



### variable n\_dbps 

```C++
uint32_t dp_wlan_state_t::n_dbps;
```



data bits per OFDM symbol 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wlan/wlan_core.h`

