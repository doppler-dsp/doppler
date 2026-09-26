

# File acquire\_core.h



[**FileList**](files.md) **>** [**acquire**](dir_684079cd19bcefcd0ee51c221517a041.md) **>** [**acquire\_core.h**](acquire__core_8h.md)

[Go to the source code of this file](acquire__core_8h_source.md)

_Acquire module — public C API._ 

* `#include "doppler/clib_common.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_bin\_to\_signed**](#function-dp_bin_to_signed) (size\_t bin, size\_t n\_bins) <br>_Map an FFT bin index to its SIGNED frequency index._  |




























## Public Functions Documentation




### function dp\_bin\_to\_signed 

_Map an FFT bin index to its SIGNED frequency index._ 
```C++
int dp_bin_to_signed (
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
>>> from doppler.acquire import bin_to_signed
>>> [bin_to_signed(b, 8) for b in range(8)]
[0, 1, 2, 3, -4, -3, -2, -1]
>>> (np.fft.fftfreq(8) * 8).astype(int).tolist()   # same convention
[0, 1, 2, 3, -4, -3, -2, -1]
>>> bin_to_signed(4, 7)                         # odd grid: no ambiguity
-3
```
 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/acquire/acquire_core.h`

