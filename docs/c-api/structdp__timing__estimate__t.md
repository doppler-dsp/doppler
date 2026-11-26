

# Struct dp\_timing\_estimate\_t



[**ClassList**](annotated.md) **>** [**dp\_timing\_estimate\_t**](structdp__timing__estimate__t.md)



_One feedforward symbol-timing estimate._ 

* `#include <timing_est_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**has\_rate**](#variable-has_rate)  <br> |
|  double | [**power**](#variable-power)  <br> |
|  double | [**quality**](#variable-quality)  <br> |
|  double | [**rate**](#variable-rate)  <br> |
|  double | [**symbols**](#variable-symbols)  <br> |
|  double | [**tau**](#variable-tau)  <br> |












































## Public Attributes Documentation




### variable has\_rate 

```C++
int dp_timing_estimate_t::has_rate;
```



1 when a previous block exists to take `rate` against, else 0. 
 


        

<hr>



### variable power 

```C++
double dp_timing_estimate_t::power;
```



Mean `|x|^2` over the block. 
 


        

<hr>



### variable quality 

```C++
double dp_timing_estimate_t::quality;
```



`|c| / sum |x|^2`, in [0, 1]: how much of the block's power is the symbol-rate line. Near 0 with no signal, with no transitions, or with a clock error that turns the line through a full cycle inside the block. 0 when the block has no power. 
 


        

<hr>



### variable rate 

```C++
double dp_timing_estimate_t::rate;
```



How far the eye center moves on the clock each symbol of the clock, in symbols: for symbols that last `sps * (1 + eps)` samples it is `eps / (1 + eps)`. From how far `tau` moved since the previous block, so 0 until `has_rate` is 1, and right only while the timing moves under half a symbol between the two blocks' middles. 
 


        

<hr>



### variable symbols 

```C++
double dp_timing_estimate_t::symbols;
```



The block's length in symbols of the clock: samples / sps. 
 


        

<hr>



### variable tau 

```C++
double dp_timing_estimate_t::tau;
```



Where the eye center sits on the estimator's symbol clock, in symbols, in (-1/2, 1/2]: the clock's phase at the instants the stream's power peaks. With a symbol-rate error it is the value at the middle of the block. 0 when the block has no power. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/timing_est/timing_est_core.h`

