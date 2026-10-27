

# Struct dp\_hbdecim\_q15\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_hbdecim\_q15\_state\_t**](structdp__hbdecim__q15__state__t.md)



_HalfbandDecimatorQ15 state: Q15 FIR-branch coefficients, four dual-write int16 delay rings and a one-sample carry._ [More...](#detailed-description)

* `#include <hbdecim_q15_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**K**](#variable-k)  <br> |
|  size\_t | [**K\_pad**](#variable-k_pad)  <br> |
|  size\_t | [**cap**](#variable-cap)  <br> |
|  size\_t | [**centre**](#variable-centre)  <br> |
|  int16\_t \* | [**coeffs**](#variable-coeffs)  <br> |
|  int16\_t \* | [**even\_I**](#variable-even_i)  <br> |
|  int16\_t \* | [**even\_Q**](#variable-even_q)  <br> |
|  size\_t | [**even\_head**](#variable-even_head)  <br> |
|  int | [**fir\_on\_even**](#variable-fir_on_even)  <br> |
|  int | [**has\_pending**](#variable-has_pending)  <br> |
|  size\_t | [**mask**](#variable-mask)  <br> |
|  size\_t | [**num\_taps**](#variable-num_taps)  <br> |
|  int16\_t \* | [**odd\_I**](#variable-odd_i)  <br> |
|  int16\_t \* | [**odd\_Q**](#variable-odd_q)  <br> |
|  size\_t | [**odd\_head**](#variable-odd_head)  <br> |
|  int16\_t | [**pending\_I**](#variable-pending_i)  <br> |
|  int16\_t | [**pending\_Q**](#variable-pending_q)  <br> |












































## Detailed Description


Config fields are set by [**dp\_hbdecim\_q15\_create()**](hbdecim__q15__core_8h.md#function-dp_hbdecim_q15_create); the ring contents, heads and pending pair are running state. Allocate with [**dp\_hbdecim\_q15\_create()**](hbdecim__q15__core_8h.md#function-dp_hbdecim_q15_create). 


    
## Public Attributes Documentation




### variable K 

```C++
size_t dp_hbdecim_q15_state_t::K;
```



Symmetric pair count, num\_taps / 2 (config). 


        

<hr>



### variable K\_pad 

```C++
size_t dp_hbdecim_q15_state_t::K_pad;
```



K rounded up to a multiple of 16 for AVX2 loads (config). 
 


        

<hr>



### variable cap 

```C++
size_t dp_hbdecim_q15_state_t::cap;
```



Ring capacity: next power of 2 &gt;= num\_taps (config). 
 


        

<hr>



### variable centre 

```C++
size_t dp_hbdecim_q15_state_t::centre;
```



num\_taps / 2: delay-branch tap index into its ring (config). 
 


        

<hr>



### variable coeffs 

```C++
int16_t* dp_hbdecim_q15_state_t::coeffs;
```



Owned, 32-byte aligned, K\_pad Q15 taps: the first K of h scaled by 0.5, zero-padded. 
 


        

<hr>



### variable even\_I 

```C++
int16_t* dp_hbdecim_q15_state_t::even_I;
```



Owned ring, 2\*cap: I of even-index inputs. 


        

<hr>



### variable even\_Q 

```C++
int16_t* dp_hbdecim_q15_state_t::even_Q;
```



Owned ring, 2\*cap: Q of even-index inputs. 


        

<hr>



### variable even\_head 

```C++
size_t dp_hbdecim_q15_state_t::even_head;
```



Newest-sample index in the even rings, [0, cap) (running state). 
 


        

<hr>



### variable fir\_on\_even 

```C++
int dp_hbdecim_q15_state_t::fir_on_even;
```



1 (num\_taps even): FIR branch runs on the even-indexed ring; 0: on the odd ring. 
 


        

<hr>



### variable has\_pending 

```C++
int dp_hbdecim_q15_state_t::has_pending;
```



1 when a trailing even IQ pair is buffered for the next execute() (running state). 
 


        

<hr>



### variable mask 

```C++
size_t dp_hbdecim_q15_state_t::mask;
```



cap - 1, the ring index mask (config). 
 


        

<hr>



### variable num\_taps 

```C++
size_t dp_hbdecim_q15_state_t::num_taps;
```



FIR branch length from the caller (config). 


        

<hr>



### variable odd\_I 

```C++
int16_t* dp_hbdecim_q15_state_t::odd_I;
```



Owned ring, 2\*cap: I of odd-index inputs. 
 


        

<hr>



### variable odd\_Q 

```C++
int16_t* dp_hbdecim_q15_state_t::odd_Q;
```



Owned ring, 2\*cap: Q of odd-index inputs. 
 


        

<hr>



### variable odd\_head 

```C++
size_t dp_hbdecim_q15_state_t::odd_head;
```



Newest-sample index in the odd rings, [0, cap) (running state). 
 


        

<hr>



### variable pending\_I 

```C++
int16_t dp_hbdecim_q15_state_t::pending_I;
```



I of the buffered pair; valid if has\_pending. 


        

<hr>



### variable pending\_Q 

```C++
int16_t dp_hbdecim_q15_state_t::pending_Q;
```



Q of the buffered pair; valid if has\_pending. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/hbdecim_q15/hbdecim_q15_core.h`

