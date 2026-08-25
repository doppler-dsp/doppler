

# File dsss\_core.h



[**FileList**](files.md) **>** [**dsss**](dir_8b18bfb9a64167292d2c60acbfcb2ae1.md) **>** [**dsss\_core.h**](dsss__core_8h.md)

[Go to the source code of this file](dsss__core_8h_source.md)

_Dsss module — public C API._ 

* `#include "clib_common.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**bin\_to\_signed**](#function-bin_to_signed) (size\_t bin, size\_t n\_bins) <br>_Map an FFT bin index to its SIGNED frequency index._  |




























## Public Functions Documentation




### function bin\_to\_signed 

_Map an FFT bin index to its SIGNED frequency index._ 
```C++
int bin_to_signed (
    size_t bin,
    size_t n_bins
) 
```





**Parameters:**


* `bin` Bin index in `[0, n_bins)`. 
* `n_bins` Grid size. 



**Returns:**

Signed index in `[-(n_bins/2), +((n_bins-1)/2)]`. 
```C++
>>> import numpy as np
>>> from doppler.dsss import bin_to_signed
>>> [bin_to_signed(b, 8) for b in range(8)]
[0, 1, 2, 3, -4, -3, -2, -1]
>>> (np.fft.fftfreq(8) * 8).astype(int).tolist()   # same convention
[0, 1, 2, 3, -4, -3, -2, -1]
>>> bin_to_signed(4, 7)                         # odd grid: no ambiguity
-3
```
 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dsss/dsss_core.h`

