

# Struct dp\_wlan\_tables\_t



[**ClassList**](annotated.md) **>** [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md)



_What the standard supplies: the Clause 17 literals._ [More...](#detailed-description)

* `#include <wlan_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  const float \_Complex \* | [**ltf**](#variable-ltf)  <br> |
|  size\_t | [**n\_signal\_rates**](#variable-n_signal_rates)  <br> |
|  [**dp\_wlan\_perm2\_fn**](wlan__core_8h.md#typedef-dp_wlan_perm2_fn) | [**perm2**](#variable-perm2)  <br> |
|  const uint16\_t \* | [**relabel**](#variable-relabel)  <br> |
|  double | [**scale\_data**](#variable-scale_data)  <br> |
|  double | [**scale\_ltf**](#variable-scale_ltf)  <br> |
|  double | [**scale\_stf**](#variable-scale_stf)  <br> |
|  const [**dp\_wlan\_signal\_rate\_t**](structdp__wlan__signal__rate__t.md) \* | [**signal\_rates**](#variable-signal_rates)  <br> |
|  const float \_Complex \* | [**stf**](#variable-stf)  <br> |












































## Detailed Description


[**dp\_wlan\_tables\_standard()**](wlan__core_8h.md#function-dp_wlan_tables_standard) returns it filled, transcribed from the text (the file comment says which members the test holds against a printed form); [**dp\_wlan\_tables\_empty()**](wlan__core_8h.md#function-dp_wlan_tables_empty) returns it with every member empty (NULL, 0), and a builder that needs an empty member refuses. See the file comment. Every pointer is borrowed and must outlive its use. 


    
## Public Attributes Documentation




### variable ltf 

```C++
const float _Complex* dp_wlan_tables_t::ltf;
```



53 L-LTF values, k = -26..26 
 


        

<hr>



### variable n\_signal\_rates 

```C++
size_t dp_wlan_tables_t::n_signal_rates;
```



entries above 
 


        

<hr>



### variable perm2 

```C++
dp_wlan_perm2_fn dp_wlan_tables_t::perm2;
```



second interleaver permutation; NULL = absent 


        

<hr>



### variable relabel 

```C++
const uint16_t* dp_wlan_tables_t::relabel[7];
```



Per constellation (index n\_bpsc, 1..6): bit-group value -&gt; the library's symbol index, or NULL for the library's own labels. 


        

<hr>



### variable scale\_data 

```C++
double dp_wlan_tables_t::scale_data;
```



SIGNAL and Data scale; 0 = absent 
 


        

<hr>



### variable scale\_ltf 

```C++
double dp_wlan_tables_t::scale_ltf;
```



L-LTF scale; 0 = absent 
 


        

<hr>



### variable scale\_stf 

```C++
double dp_wlan_tables_t::scale_stf;
```



L-STF time-domain scale; 0 = absent 
 


        

<hr>



### variable signal\_rates 

```C++
const dp_wlan_signal_rate_t* dp_wlan_tables_t::signal_rates;
```



RATE bits per rate 
 


        

<hr>



### variable stf 

```C++
const float _Complex* dp_wlan_tables_t::stf;
```



53 L-STF values, k = -26..26 (index k+26) 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wlan/wlan_core.h`

