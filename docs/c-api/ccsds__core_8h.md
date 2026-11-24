

# File ccsds\_core.h



[**FileList**](files.md) **>** [**ccsds**](dir_c033bcb1c91e23f54b625bfe2cf0448c.md) **>** [**ccsds\_core.h**](ccsds__core_8h.md)

[Go to the source code of this file](ccsds__core_8h_source.md)

_CCSDS 131.0-B's published literals, as a Python-facing component._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_asm\_bits**](#function-dp_asm_bits) (uint8\_t \* out) <br>_The CCSDS Attached Sync Marker, 0x1ACFFC1D, as 32 unpacked bits._  |
|  double | [**dp\_ccsds415\_chip\_rate**](#function-dp_ccsds415_chip_rate) (double fc) <br>_Chips a second at carrier_ `fc` _at the CCSDS 415.1-B-1 point: F\_fwd x 31 / (221 x 96), table 4-1. IEEE arithmetic with fast math off, so every compiler, and the Python face, give the same bits._ |
|  double | [**dp\_ccsds415\_cn0**](#function-dp_ccsds415_cn0) (double esn0\_db) <br>_C/N0, dB-Hz, of an Es/N0 at the 415.1-B-1 point's 3,000 symbols a second:_ `esn0_db + 10 log10(3000)` _. An alias over_[_**dp\_ccsds\_415\_cn0()**_](ccsds__415_8h.md#function-dp_ccsds_415_cn0) _._ |




























## Detailed Description


The values one standard picked, kept BESIDE the general layer rather than inside it. `ccsds_tm` ([**native/inc/doppler/ccsds\_tm/ccsds\_tm.h**](ccsds__tm_8h.md)) is where those picks are defined and certified; this component is the thin face that carries them to Python, and holds no arithmetic of its own.


This is deliberately NOT a binding of `ccsds_tm`'s transforms, and must not become one. The outer code, the randomiser and the inner code are reached by DESCRIBING a CADU through `doppler.wfm.FrameDesc` — the general assembler runs the standard's kernels from an ops table, which is what keeps `wfm/wfm_frame.h` free of CCSDS. See docs/design/frame-description.md. 


    
## Public Functions Documentation




### function dp\_asm\_bits 

_The CCSDS Attached Sync Marker, 0x1ACFFC1D, as 32 unpacked bits._ 
```C++
void dp_asm_bits (
    uint8_t * out
) 
```



`out[0]` is the first bit on the wire — figure 9-1 of 131.0-B numbers the marker's bit 0 as the most significant bit of 0x1A. One bit per byte, the convention every frame path here passes around.


The thing a Python receiver ACQUIRES on: pair it with `doppler.detection.SyncFinder` to find where a CADU starts in a bit stream, then slice and `Frame.check()` it. The marker is deliberately NOT randomised (10.4's NOTE: "The ASM was not randomized and is not
derandomized"), so it reads the same in every frame and in exactly one polarity — which is what makes it the only thing in a CADU that can report a 180-degree carrier ambiguity.


A function rather than a constant a caller expands, because an MSB-first expansion written out twice is a transcription that can disagree with itself. This tree's own doctests were the second copy until doppler#900, and this alias exists so the third copy is not a Python one: it delegates to `dp_ccsds_tm_asm_bits`, which is where the expansion is written and where `test_ccsds_tm_asm` holds it to the published pattern.




**Parameters:**


* `out` Receives 32 bits, one per byte. 
```C++
>>> from doppler.ccsds import asm_bits
>>> b = asm_bits()
>>> b.size, b[:8].tolist()          # 0x1A, first bit at the top
(32, [0, 0, 0, 1, 1, 0, 1, 0])
>>> int("".join(map(str, b.tolist())), 2) == 0x1ACFFC1D
True
```
 




        

<hr>



### function dp\_ccsds415\_chip\_rate 

_Chips a second at carrier_ `fc` _at the CCSDS 415.1-B-1 point: F\_fwd x 31 / (221 x 96), table 4-1. IEEE arithmetic with fast math off, so every compiler, and the Python face, give the same bits._
```C++
double dp_ccsds415_chip_rate (
    double fc
) 
```



An alias over [**dp\_ccsds\_415\_chip\_rate()**](ccsds__415_8h.md#function-dp_ccsds_415_chip_rate), which owns the arithmetic; the point's other numbers and the scene helper are [**doppler/ccsds/ccsds\_415.h**](ccsds__415_8h.md).




**Parameters:**


* `fc` The carrier, F\_fwd, Hz. 



**Returns:**

The chip rate, chips/s. 
```C++
printf ("%.6f\n", dp_ccsds415_chip_rate (2.8e9));   // 4091251.885370
```
 





        

<hr>



### function dp\_ccsds415\_cn0 

_C/N0, dB-Hz, of an Es/N0 at the 415.1-B-1 point's 3,000 symbols a second:_ `esn0_db + 10 log10(3000)` _. An alias over_[_**dp\_ccsds\_415\_cn0()**_](ccsds__415_8h.md#function-dp_ccsds_415_cn0) _._
```C++
double dp_ccsds415_cn0 (
    double esn0_db
) 
```





**Parameters:**


* `esn0_db` Es/N0, dB. 



**Returns:**

C/N0, dB-Hz. 
```C++
printf ("%.4f\n", dp_ccsds415_cn0 (4.0));   // 38.7712
```
 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ccsds/ccsds_core.h`

