

# Struct dp\_spectrogram\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_spectrogram\_state\_t**](structdp__spectrogram__state__t.md)



_Spectrogram state. Allocate with_ [_**dp\_spectrogram\_create()**_](spectrogram__core_8h.md#function-dp_spectrogram_create) _._[More...](#detailed-description)

* `#include <spectrogram_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float | [**beta**](#variable-beta)  <br> |
|  size\_t | [**consumed**](#variable-consumed)  <br> |
|  dp\_f32\_framer\_t | [**fr**](#variable-fr)  <br> |
|  size\_t | [**hop**](#variable-hop)  <br> |
|  [**dp\_psd\_kernel\_t**](structdp__psd__kernel__t.md) | [**k**](#variable-k)  <br> |
|  float \_Complex \* | [**last**](#variable-last)  <br> |
|  int | [**mode**](#variable-mode)  <br> |
|  size\_t | [**nfft**](#variable-nfft)  <br> |
|  dp\_f32\_t \* | [**ring**](#variable-ring)  <br> |
|  int | [**window**](#variable-window)  <br> |












































## Detailed Description


Every field is configuration (or `k's` scratch) except `fr's` stream position and `consumed`. The state blob carries the configuration a row depends on (mode, nfft, hop, window, beta) and the stream position, not `consumed`. 


    
## Public Attributes Documentation




### variable beta 

```C++
float dp_spectrogram_state_t::beta;
```



Kaiser beta (the Kaiser window only). 
 


        

<hr>



### variable consumed 

```C++
size_t dp_spectrogram_state_t::consumed;
```



Input samples the last push took. 
 


        

<hr>



### variable fr 

```C++
dp_f32_framer_t dp_spectrogram_state_t::fr;
```



Any chunk in, nfft-sample frames out. 
 


        

<hr>



### variable hop 

```C++
size_t dp_spectrogram_state_t::hop;
```



Samples between row starts. 
 


        

<hr>



### variable k 

```C++
dp_psd_kernel_t dp_spectrogram_state_t::k;
```



PSD's per-frame kernel: n = nfft, no pad. 
 


        

<hr>



### variable last 

```C++
float _Complex* dp_spectrogram_state_t::last;
```



flush()'s zero-padded frame, nfft samples. 


        

<hr>



### variable mode 

```C++
int dp_spectrogram_state_t::mode;
```



DP\_SPECTROGRAM\_POWER or DP\_SPECTROGRAM\_DB. 


        

<hr>



### variable nfft 

```C++
size_t dp_spectrogram_state_t::nfft;
```



Samples per frame, and bins per row. 
 


        

<hr>



### variable ring 

```C++
dp_f32_t* dp_spectrogram_state_t::ring;
```



The carry's storage, owned by `fr` alone. 
 


        

<hr>



### variable window 

```C++
int dp_spectrogram_state_t::window;
```



A DP\_PSD\_WINDOW\_\* (the kernel's window). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/spectrogram/spectrogram_core.h`

