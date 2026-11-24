

# Struct dp\_ccsds\_415\_rx\_t



[**ClassList**](annotated.md) **>** [**dp\_ccsds\_415\_rx\_t**](structdp__ccsds__415__rx__t.md)



_The async DSSS receiver's create arguments at the point, every one but the code: what_ [_**dp\_ccsds\_415\_rx()**_](ccsds__415_8h.md#function-dp_ccsds_415_rx) _fills._[More...](#detailed-description)

* `#include <ccsds_415.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**carrier\_freq\_hz**](#variable-carrier_freq_hz)  <br> |
|  double | [**chip\_rate**](#variable-chip_rate)  <br> |
|  double | [**cn0\_dbhz**](#variable-cn0_dbhz)  <br> |
|  double | [**code\_only\_s**](#variable-code_only_s)  <br> |
|  int | [**differential**](#variable-differential)  <br> |
|  double | [**doppler\_rate**](#variable-doppler_rate)  <br> |
|  double | [**doppler\_uncertainty**](#variable-doppler_uncertainty)  <br> |
|  double | [**lost\_confirm\_s**](#variable-lost_confirm_s)  <br> |
|  int | [**m**](#variable-m)  <br> |
|  double | [**pd**](#variable-pd)  <br> |
|  double | [**pfa**](#variable-pfa)  <br> |
|  bool | [**reacquire**](#variable-reacquire)  <br> |
|  bool | [**refine**](#variable-refine)  <br> |
|  double | [**refine\_design\_margin\_db**](#variable-refine_design_margin_db)  <br> |
|  double | [**refine\_max\_error\_db**](#variable-refine_max_error_db)  <br> |
|  size\_t | [**refine\_max\_n\_blocks**](#variable-refine_max_n_blocks)  <br> |
|  size\_t | [**refine\_n\_fft**](#variable-refine_n_fft)  <br> |
|  size\_t | [**refine\_samples\_per\_symbol**](#variable-refine_samples_per_symbol)  <br> |
|  bool | [**refine\_sequential**](#variable-refine_sequential)  <br> |
|  size\_t | [**refine\_zero\_pad**](#variable-refine_zero_pad)  <br> |
|  double | [**sample\_rate**](#variable-sample_rate)  <br> |
|  size\_t | [**segments**](#variable-segments)  <br> |
|  size\_t | [**sps**](#variable-sps)  <br> |
|  double | [**symbol\_rate**](#variable-symbol_rate)  <br> |












































## Detailed Description


One field per parameter of [**dp\_async\_dsss\_receiver\_create()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_create), in its order and under its name; that function's documentation says what each does. The receiver view ([**dp\_async\_dsss\_receiver\_create\_ccsds415()**](async__dsss__receiver__core_8h.md#function-dp_async_dsss_receiver_create_ccsds415)), the pool view ([**dp\_async\_dsss\_pool\_create\_ccsds415()**](async__dsss__pool__core_8h.md#function-dp_async_dsss_pool_create_ccsds415)) and the receiver's test all read this one record, so the point's arguments are written once. 


    
## Public Attributes Documentation




### variable carrier\_freq\_hz 

```C++
double dp_ccsds_415_rx_t::carrier_freq_hz;
```



The caller's carrier. 


        

<hr>



### variable chip\_rate 

```C++
double dp_ccsds_415_rx_t::chip_rate;
```



[**dp\_ccsds\_415\_chip\_rate()**](ccsds__415_8h.md#function-dp_ccsds_415_chip_rate) of the carrier. 


        

<hr>



### variable cn0\_dbhz 

```C++
double dp_ccsds_415_rx_t::cn0_dbhz;
```



[**dp\_ccsds\_415\_cn0()**](ccsds__415_8h.md#function-dp_ccsds_415_cn0) of the Es/N0 floor. 


        

<hr>



### variable code\_only\_s 

```C++
double dp_ccsds_415_rx_t::code_only_s;
```



DP\_CCSDS\_415\_CODE\_ONLY\_SYM / DP\_CCSDS\_415\_SYM. 


        

<hr>



### variable differential 

```C++
int dp_ccsds_415_rx_t::differential;
```



0: coherent demap. 


        

<hr>



### variable doppler\_rate 

```C++
double dp_ccsds_415_rx_t::doppler_rate;
```



DP\_CCSDS\_415\_R\_ENV\_HZ\_S. 


        

<hr>



### variable doppler\_uncertainty 

```C++
double dp_ccsds_415_rx_t::doppler_uncertainty;
```



DP\_CCSDS\_415\_SPAN\_HZ. 


        

<hr>



### variable lost\_confirm\_s 

```C++
double dp_ccsds_415_rx_t::lost_confirm_s;
```



The caller's. 


        

<hr>



### variable m 

```C++
int dp_ccsds_415_rx_t::m;
```



2: BPSK. 


        

<hr>



### variable pd 

```C++
double dp_ccsds_415_rx_t::pd;
```



0.9. 


        

<hr>



### variable pfa 

```C++
double dp_ccsds_415_rx_t::pfa;
```



1e-3. 


        

<hr>



### variable reacquire 

```C++
bool dp_ccsds_415_rx_t::reacquire;
```



false. 


        

<hr>



### variable refine 

```C++
bool dp_ccsds_415_rx_t::refine;
```



The caller's. 


        

<hr>



### variable refine\_design\_margin\_db 

```C++
double dp_ccsds_415_rx_t::refine_design_margin_db;
```



14.0. 


        

<hr>



### variable refine\_max\_error\_db 

```C++
double dp_ccsds_415_rx_t::refine_max_error_db;
```



0.5. 


        

<hr>



### variable refine\_max\_n\_blocks 

```C++
size_t dp_ccsds_415_rx_t::refine_max_n_blocks;
```




* 




        

<hr>



### variable refine\_n\_fft 

```C++
size_t dp_ccsds_415_rx_t::refine_n_fft;
```




* 




        

<hr>



### variable refine\_samples\_per\_symbol 

```C++
size_t dp_ccsds_415_rx_t::refine_samples_per_symbol;
```




* 




        

<hr>



### variable refine\_sequential 

```C++
bool dp_ccsds_415_rx_t::refine_sequential;
```



false. 


        

<hr>



### variable refine\_zero\_pad 

```C++
size_t dp_ccsds_415_rx_t::refine_zero_pad;
```




* 




        

<hr>



### variable sample\_rate 

```C++
double dp_ccsds_415_rx_t::sample_rate;
```



DP\_CCSDS\_415\_FS. 


        

<hr>



### variable segments 

```C++
size_t dp_ccsds_415_rx_t::segments;
```




* 




        

<hr>



### variable sps 

```C++
size_t dp_ccsds_415_rx_t::sps;
```




* 




        

<hr>



### variable symbol\_rate 

```C++
double dp_ccsds_415_rx_t::symbol_rate;
```



DP\_CCSDS\_415\_SYM. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ccsds/ccsds_415.h`

