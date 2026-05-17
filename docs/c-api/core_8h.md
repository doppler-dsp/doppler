

# File core.h



[**FileList**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**core.h**](core_8h.md)

[Go to the source code of this file](core_8h_source.md)

_Core DSP engine — initialisation and version._ [More...](#detailed-description)






































## Public Functions

| Type | Name |
| ---: | :--- |
|  const char \* | [**dp\_build\_info**](#function-dp_build_info) (void) <br>_Return a build-info string describing compile-time features._  |
|  void | [**dp\_cleanup**](#function-dp_cleanup) (void) <br>_Clean up global Doppler state._  |
|  int | [**dp\_init**](#function-dp_init) (void) <br>_Initialise the Doppler DSP engine._  |
|  const char \* | [**dp\_version**](#function-dp_version) (void) <br>_Return the library version string (e.g._ `"1.0.0"` _)._ |




























## Detailed Description


Call [**dp\_init()**](core_8h.md#function-dp_init) once before using any DSP function. Call [**dp\_cleanup()**](core_8h.md#function-dp_cleanup) at shutdown to release FFTW plans and global state.



```C++
#include <dp/core.h>
dp_init();
// ... use FFT, SIMD, etc. ...
dp_cleanup();
```




## Public Functions Documentation




### function dp\_build\_info

_Return a build-info string describing compile-time features._
```C++
const char * dp_build_info (
    void
)
```



Includes SIMD level, FFTW backend, and compiler details.




**Returns:**

Statically allocated, null-terminated string.







<hr>



### function dp\_cleanup

_Clean up global Doppler state._
```C++
void dp_cleanup (
    void
)
```



Releases FFTW plans and any other global resources. Safe to call even if [**dp\_init()**](core_8h.md#function-dp_init) was never called.




<hr>



### function dp\_init

_Initialise the Doppler DSP engine._
```C++
int dp_init (
    void
)
```



Must be called once before any other DSP function. Initialises FFTW thread support and performs SIMD feature detection.




**Returns:**

0 on success, non-zero on failure.







<hr>



### function dp\_version

_Return the library version string (e.g._ `"1.0.0"` _)._
```C++
const char * dp_version (
    void
)
```





**Returns:**

Statically allocated, null-terminated string.







<hr>

------------------------------
The documentation for this class was generated from the following file `c/include/dp/core.h`
