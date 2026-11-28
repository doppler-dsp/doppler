

# File correlation\_core.h



[**FileList**](files.md) **>** [**correlation**](dir_9360b0d8b0ed26b6dcce04fb43b6327f.md) **>** [**correlation\_core.h**](correlation__core_8h.md)

[Go to the source code of this file](correlation__core_8h_source.md)

_Correlation module — public C API._ 

* `#include "doppler/clib_common.h"`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_correlator\_same\_code\_phase**](#function-dp_correlator_same_code_phase) (double a, double b, double code\_len) <br>_One signal's code phase: 1 when code phases_ `a` _and_`b` _, chips, are within DP\_CORRELATOR\_SAME\_PHASE\_CHIPS of each other, circularly over a code of_`code_len` _chips; else 0._ |




























## Public Functions Documentation




### function dp\_correlator\_same\_code\_phase 

_One signal's code phase: 1 when code phases_ `a` _and_`b` _, chips, are within DP\_CORRELATOR\_SAME\_PHASE\_CHIPS of each other, circularly over a code of_`code_len` _chips; else 0._
```C++
int dp_correlator_same_code_phase (
    double a,
    double b,
    double code_len
) 
```



The one key the receiver's pick and the pool's duplicate test share, and the one the peak zone is sized from. The receiver's pick drops a peak within it of one already taken or of an excluded phase (rules 1 and 3 of docs/design/async-dsss-receiver.md section 7.2, keyed on code phase alone, never carrier: async-dsss-receiver-measurements.md section 12.14); the pool resets the second of two holders within it of each other; and the zone the receiver asks its search for ([**dp\_correlator\_set\_peak\_zone()**](correlator__core_8h.md#function-dp_correlator_set_peak_zone)) is half of it, so the zone stays inside the key (#3071: a zone as wide as the key masked pairs two lags apart that the key keeps). An example that names which emitter a receiver holds calls this, so it identifies an emitter by exactly the test the pool does. A C caller's example is on DP\_CORRELATOR\_SAME\_PHASE\_CHIPS ([**correlator\_core.h**](correlator__core_8h.md)).


The distance is \|a - b\| folded over the code, so phases either side of chip 0 are near: 0.4 and 1022.8 are 0.6 chips apart on a 1023-chip code. The bound is inclusive: phases exactly DP\_CORRELATOR\_SAME\_PHASE\_CHIPS apart are one signal. Phases are taken as given, each in [0, code\_len); a NaN phase compares as two signals.




**Parameters:**


* `a` A code phase, chips. 
* `b` Another, chips. 
* `code_len` The code's length, chips. 



**Returns:**

1 when `a` and `b` are one signal's code phase, else 0. 
```C++
>>> from doppler.correlation import correlator_same_code_phase
>>> correlator_same_code_phase(100.0, 101.0, 1023.0)
1
>>> correlator_same_code_phase(100.0, 101.5, 1023.0)
0
>>> correlator_same_code_phase(0.4, 1022.8, 1023.0)  # across chip 0
1
```
 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/correlation/correlation_core.h`

