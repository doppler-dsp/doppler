

# Struct dp\_psd\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_psd\_state\_t**](structdp__psd__state__t.md)



_PSD state. Allocate with_ [_**dp\_psd\_create()**_](psd__core_8h.md#function-dp_psd_create) _._

* `#include <psd_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_acc\_trace\_state\_t**](structdp__acc__trace__state__t.md) \* | [**avg**](#variable-avg)  <br> |
|  size\_t | [**bits**](#variable-bits)  <br> |
|  float \* | [**dbbuf**](#variable-dbbuf)  <br> |
|  double | [**enbw**](#variable-enbw)  <br> |
|  dp\_f32\_framer\_t | [**fr**](#variable-fr)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  size\_t | [**hop**](#variable-hop)  <br> |
|  [**dp\_psd\_kernel\_t**](structdp__psd__kernel__t.md) | [**k**](#variable-k)  <br> |
|  dp\_f32\_t \* | [**ring**](#variable-ring)  <br> |












































## Public Attributes Documentation




### variable avg 

```C++
dp_acc_trace_state_t* dp_psd_state_t::avg;
```



Per-bin power averager, length nfft. 
 


        

<hr>



### variable bits 

```C++
size_t dp_psd_state_t::bits;
```



ADC depth that set full\_scale, else 0. 


        

<hr>



### variable dbbuf 

```C++
float* dp_psd_state_t::dbbuf;
```



dB-trace scratch, length nfft. 
 


        

<hr>



### variable enbw 

```C++
double dp_psd_state_t::enbw;
```



Equivalent noise bandwidth, bins (the kernel's, copied at create). 


        

<hr>



### variable fr 

```C++
dp_f32_framer_t dp_psd_state_t::fr;
```



Splits the stream into n-sample frames; carries the partial. 


        

<hr>



### variable fs 

```C++
double dp_psd_state_t::fs;
```



Sample rate, Hz. 
 


        

<hr>



### variable hop 

```C++
size_t dp_psd_state_t::hop;
```



Samples between frame starts, 1..n. 
 


        

<hr>



### variable k 

```C++
dp_psd_kernel_t dp_psd_state_t::k;
```



The per-frame kernel; owns the window. 


        

<hr>



### variable ring 

```C++
dp_f32_t* dp_psd_state_t::ring;
```



The framer's ring, 2n, owned by the framer. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/psd/psd_core.h`

