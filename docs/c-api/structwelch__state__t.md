

# Struct welch\_state\_t



[**ClassList**](annotated.md) **>** [**welch\_state\_t**](structwelch__state__t.md)



_Welch state. Allocate with_ [_**welch\_create()**_](welch__core_8h.md#function-welch_create) _._

* `#include <welch_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**acc\_trace\_state\_t**](structacc__trace__state__t.md) \* | [**avg**](#variable-avg)  <br> |
|  double | [**cg**](#variable-cg)  <br> |
|  float \* | [**dbbuf**](#variable-dbbuf)  <br> |
|  double | [**enbw**](#variable-enbw)  <br> |
|  [**fft\_state\_t**](structfft__state__t.md) \* | [**fft**](#variable-fft)  <br> |
|  float complex \* | [**frame**](#variable-frame)  <br> |
|  double | [**fs**](#variable-fs)  <br> |
|  size\_t | [**n**](#variable-n)  <br> |
|  float \* | [**pwr**](#variable-pwr)  <br> |
|  double | [**s2**](#variable-s2)  <br> |
|  float complex \* | [**spec**](#variable-spec)  <br> |
|  float \* | [**w**](#variable-w)  <br> |












































## Public Attributes Documentation




### variable avg 

```C++
acc_trace_state_t* welch_state_t::avg;
```



Per-bin power averager (AccTrace). 


        

<hr>



### variable cg 

```C++
double welch_state_t::cg;
```



Window coherent gain, sum(w). 


        

<hr>



### variable dbbuf 

```C++
float* welch_state_t::dbbuf;
```



dB-trace scratch, length n. 


        

<hr>



### variable enbw 

```C++
double welch_state_t::enbw;
```



Equivalent noise bandwidth, bins. 


        

<hr>



### variable fft 

```C++
fft_state_t* welch_state_t::fft;
```



Forward cf32 plan, size n. 


        

<hr>



### variable frame 

```C++
float complex* welch_state_t::frame;
```



Windowed input scratch, length n. 


        

<hr>



### variable fs 

```C++
double welch_state_t::fs;
```



Sample rate, Hz. 


        

<hr>



### variable n 

```C++
size_t welch_state_t::n;
```



FFT length / frame size. 


        

<hr>



### variable pwr 

```C++
float* welch_state_t::pwr;
```



DC-centred power scratch, length n. 


        

<hr>



### variable s2 

```C++
double welch_state_t::s2;
```



Window power, sum(w^2). 


        

<hr>



### variable spec 

```C++
float complex* welch_state_t::spec;
```



FFT output scratch, length n. 


        

<hr>



### variable w 

```C++
float* welch_state_t::w;
```



Window, length n. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/welch/welch_core.h`

