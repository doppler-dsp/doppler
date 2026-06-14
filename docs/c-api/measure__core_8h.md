

# File measure\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**measure**](dir_4f61a452d1df39cf8c2e8be27f29f1f2.md) **>** [**measure\_core.h**](measure__core_8h.md)

[Go to the source code of this file](measure__core_8h_source.md)

_Measure module — shared result structs and module-level helpers._ [More...](#detailed-description)

* `#include "clib_common.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**time\_stats\_t**](structtime__stats__t.md) <br>_Time-domain capture statistics (AC-coupled crest/PAPR)._  |
| struct | [**tone\_meas\_t**](structtone__meas__t.md) <br>_Single-tone dynamic-measurement bag._  |


















































## Detailed Description


The `doppler.measure` objects (ToneMeasure, …) each own a window + FFT and analyse a time-domain capture, returning one of these plain-C result bags by out-parameter. Every spectral metric integrates a component's power over its window MAIN LOBE (IEEE Std 1241) rather than reading a single peak bin, so the reading is independent of where the tone falls between FFT bins. 


    

------------------------------
The documentation for this class was generated from the following file `native/inc/measure/measure_core.h`

