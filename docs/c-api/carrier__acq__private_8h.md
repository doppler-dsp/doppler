

# File carrier\_acq\_private.h



[**FileList**](files.md) **>** [**carrier\_acq**](dir_92c8a71a0b308d7c304bd53eb19bc933.md) **>** [**carrier\_acq\_private.h**](carrier__acq__private_8h.md)

[Go to the source code of this file](carrier__acq__private_8h_source.md)

_The carrier acquisition's derived detection threshold._ [More...](#detailed-description)

* `#include "doppler/carrier_acq/carrier_acq_core.h"`
* `#include <stddef.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_\_carrier\_acq\_derive\_threshold**](#function-dp__carrier_acq_derive_threshold) ([**dp\_carrier\_acq\_state\_t**](structdp__carrier__acq__state__t.md) \* ca) <br>_Test this estimator against its derived threshold from now on._  |
|  double | [**dp\_\_carrier\_acq\_derived\_threshold**](#function-dp__carrier_acq_derived_threshold) (const [**dp\_carrier\_acq\_state\_t**](structdp__carrier__acq__state__t.md) \* ca, size\_t n\_blocks) <br>_The derived ratio threshold at_ `n_blocks` _blocks (&gt; 0)._ |




























## Detailed Description


Not part of the public API. A composition that knows its template's statistic is the one the derivation covers (AsyncDsssReceiver's squared tone estimator, docs/design/async-dsss-receiver.md §4.7) switches its CarrierAcquisition to it; every other caller keeps the default rule. 


    
## Public Functions Documentation




### function dp\_\_carrier\_acq\_derive\_threshold 

_Test this estimator against its derived threshold from now on._ 
```C++
void dp__carrier_acq_derive_threshold (
    dp_carrier_acq_state_t * ca
) 
```



The threshold is the ratio eta at which the null probability that the correlation's peak over its median exceeds eta equals the configured pfa: each lag a chi-square of thr\_nu1 \* n\_blocks degrees of freedom, the peak over the nfft circular lags by Rice's upcrossing formula, the median a sample over thr\_neff independent lags. Every input is the window's and the template's, derived at create; nothing is fitted. The rule travels in the blob, so a restored estimator keeps it.




**Parameters:**


* `ca` Must be non-NULL. 




        

<hr>



### function dp\_\_carrier\_acq\_derived\_threshold 

_The derived ratio threshold at_ `n_blocks` _blocks (&gt; 0)._
```C++
double dp__carrier_acq_derived_threshold (
    const dp_carrier_acq_state_t * ca,
    size_t n_blocks
) 
```



What the derived rule tests against after that many blocks, exposed for the tests that measure its realized false-alarm rate.




**Parameters:**


* `ca` Must be non-NULL; its null statistics are computed at create. 
* `n_blocks` Blocks integrated, &gt; 0. 



**Returns:**

The peak-over-median ratio a dwell of `n_blocks` must exceed to declare at the configured pfa. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/carrier_acq/carrier_acq_private.h`

