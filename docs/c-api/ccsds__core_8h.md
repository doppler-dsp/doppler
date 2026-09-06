

# File ccsds\_core.h



[**FileList**](files.md) **>** [**ccsds**](dir_dc3b6b0faf00f348f137e45d38cdc7fa.md) **>** [**ccsds\_core.h**](ccsds__core_8h.md)

[Go to the source code of this file](ccsds__core_8h_source.md)

_CCSDS 131.0-B's published literals, as a Python-facing component._ [More...](#detailed-description)

* `#include "clib_common.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**asm\_bits**](#function-asm_bits) (uint8\_t \* out) <br>_The CCSDS Attached Sync Marker, 0x1ACFFC1D, as 32 unpacked bits._  |




























## Detailed Description


The values one standard picked, kept BESIDE the general layer rather than inside it. `ccsds_tm` ([**native/inc/ccsds\_tm/ccsds\_tm.h**](ccsds__tm_8h.md)) is where those picks are defined and certified; this component is the thin face that carries them to Python, and holds no arithmetic of its own.


This is deliberately NOT a binding of `ccsds_tm`'s transforms, and must not become one. The outer code, the randomiser and the inner code are reached by DESCRIBING a CADU through `doppler.wfm.FrameDesc` — the general assembler runs the standard's kernels from an ops table, which is what keeps `wfm/wfm_frame.h` free of CCSDS. See docs/design/frame-description.md. 


    
## Public Functions Documentation




### function asm\_bits 

_The CCSDS Attached Sync Marker, 0x1ACFFC1D, as 32 unpacked bits._ 
```C++
void asm_bits (
    uint8_t * out
) 
```



`out[0]` is the first bit on the wire — figure 9-1 of 131.0-B numbers the marker's bit 0 as the most significant bit of 0x1A. One bit per byte, the convention every frame path here passes around.


The thing a Python receiver ACQUIRES on: pair it with `doppler.detection.SyncFinder` to find where a CADU starts in a bit stream, then slice and `Frame.check()` it. The marker is deliberately NOT randomised (10.4's NOTE: "The ASM was not randomized and is not
derandomized"), so it reads the same in every frame and in exactly one polarity — which is what makes it the only thing in a CADU that can report a 180-degree carrier ambiguity.


A function rather than a constant a caller expands, because an MSB-first expansion written out twice is a transcription that can disagree with itself. This tree's own doctests were the second copy until doppler#900, and this alias exists so the third copy is not a Python one: it delegates to `ccsds_tm_asm_bits`, which is where the expansion is written and where `test_ccsds_tm_asm` holds it to the published pattern.




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

------------------------------
The documentation for this class was generated from the following file `native/inc/ccsds/ccsds_core.h`

