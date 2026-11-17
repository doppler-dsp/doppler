

# File detection\_private.h



[**FileList**](files.md) **>** [**detection**](dir_c7528e0bd68524c48f260a564c045102.md) **>** [**detection\_private.h**](detection__private_8h.md)

[Go to the source code of this file](detection__private_8h_source.md)

_The detection module's numeric kernels other modules reuse._ [More...](#detailed-description)






































## Public Functions

| Type | Name |
| ---: | :--- |
|  double | [**dp\_\_gammaq**](#function-dp__gammaq) (double a, double x) <br>_The regularized upper incomplete gamma, Q(a, x) = Gamma(a, x) / Gamma(a), for any real shape a &gt; 0._  |




























## Detailed Description


Not part of the public API. One copy of each kernel, defined in the detection module and called from wherever a statistic needs it (the carrier acquisition's derived threshold), never re-derived there. 


    
## Public Functions Documentation




### function dp\_\_gammaq 

_The regularized upper incomplete gamma, Q(a, x) = Gamma(a, x) / Gamma(a), for any real shape a &gt; 0._ 
```C++
double dp__gammaq (
    double a,
    double x
) 
```



The survival function of a Gamma(a, 1) variable at x: P(G &gt; x). The series / continued-fraction pair with a log-domain prefactor (marcum\_q.c), so it does not underflow where exp(-x) would. [**dp\_marcum\_q()**](detection__core_8h.md#function-dp_marcum_q) calls it at integer shapes.




**Parameters:**


* `a` Shape, &gt; 0. 
* `x` The point; x &lt;= 0 returns 1. 



**Returns:**

Q(a, x), from 0 to 1 inclusive. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/detection/detection_private.h`

