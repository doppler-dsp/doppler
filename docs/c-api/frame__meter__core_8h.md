

# File frame\_meter\_core.h



[**FileList**](files.md) **>** [**frame\_meter**](dir_7d049e2511dda4d27f50479ac6f6567b.md) **>** [**frame\_meter\_core.h**](frame__meter__core_8h.md)

[Go to the source code of this file](frame__meter__core_8h_source.md)

_Frame outcomes accumulated across a record: FER, and sync detection._ [More...](#detailed-description)

* `#include "ber/ber_core.h"`
* `#include "dp_state.h"`
* `#include <stddef.h>`
* `#include <stdint.h>`
* `#include "detection/detection_core.h"`
* `#include "ber_meter/ber_meter_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**frame\_meter\_state\_t**](structframe__meter__state__t.md) <br>_Frame-outcome accumulator. Allocate with_ [_**frame\_meter\_create()**_](frame__meter__core_8h.md#function-frame_meter_create) _._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**frame\_meter\_add**](#function-frame_meter_add) ([**frame\_meter\_state\_t**](structframe__meter__state__t.md) \* state, int sync\_ok, int crc) <br>_Record one frame's outcome._  |
|  [**frame\_meter\_state\_t**](structframe__meter__state__t.md) \* | [**frame\_meter\_create**](#function-frame_meter_create) (size\_t target\_errors, double conf) <br>_Create an accumulator._  |
|  void | [**frame\_meter\_destroy**](#function-frame_meter_destroy) ([**frame\_meter\_state\_t**](structframe__meter__state__t.md) \* state) <br>_Release the meter._  |
|  [**ber\_interval\_t**](structber__interval__t.md) | [**frame\_meter\_fer**](#function-frame_meter_fer) (const [**frame\_meter\_state\_t**](structframe__meter__state__t.md) \* state) <br>_Frame error rate with its exact interval._  |
|  size\_t | [**frame\_meter\_get\_crc\_passed**](#function-frame_meter_get_crc_passed) (const [**frame\_meter\_state\_t**](structframe__meter__state__t.md) \* state) <br>_Frames whose CRC checked._  |
|  int | [**frame\_meter\_get\_enough**](#function-frame_meter_get_enough) (const [**frame\_meter\_state\_t**](structframe__meter__state__t.md) \* state) <br>_Non-zero once_ `target_errors` _frame errors have accumulated._ |
|  size\_t | [**frame\_meter\_get\_errors**](#function-frame_meter_get_errors) (const [**frame\_meter\_state\_t**](structframe__meter__state__t.md) \* state) <br>_Frames not delivered: no sync, or a failed CRC._  |
|  size\_t | [**frame\_meter\_get\_frames**](#function-frame_meter_get_frames) (const [**frame\_meter\_state\_t**](structframe__meter__state__t.md) \* state) <br>_Frames attempted._  |
|  void | [**frame\_meter\_get\_state**](#function-frame_meter_get_state) (const [**frame\_meter\_state\_t**](structframe__meter__state__t.md) \* state, void \* blob) <br>_Serialize the running counters into_ `blob` _._ |
|  size\_t | [**frame\_meter\_get\_sync\_detected**](#function-frame_meter_get_sync_detected) (const [**frame\_meter\_state\_t**](structframe__meter__state__t.md) \* state) <br>_Frames whose sync word was detected._  |
|  void | [**frame\_meter\_reset**](#function-frame_meter_reset) ([**frame\_meter\_state\_t**](structframe__meter__state__t.md) \* state) <br>_Clear every counter; the configuration is untouched._  |
|  int | [**frame\_meter\_set\_state**](#function-frame_meter_set_state) ([**frame\_meter\_state\_t**](structframe__meter__state__t.md) \* state, const void \* blob) <br>_Restore; DP\_OK, or DP\_ERR\_INVALID if the blob is rejected._  |
|  size\_t | [**frame\_meter\_state\_bytes**](#function-frame_meter_state_bytes) (const [**frame\_meter\_state\_t**](structframe__meter__state__t.md) \* state) <br>_Serialized-state byte size._  |
|  [**ber\_interval\_t**](structber__interval__t.md) | [**frame\_meter\_sync\_miss**](#function-frame_meter_sync_miss) (const [**frame\_meter\_state\_t**](structframe__meter__state__t.md) \* state) <br>_Sync MISS rate with its exact interval._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**FRAME\_METER\_STATE\_MAGIC**](frame__meter__core_8h.md#define-frame_meter_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('F', 'R', 'M', 'M')`<br> |
| define  | [**FRAME\_METER\_STATE\_VERSION**](frame__meter__core_8h.md#define-frame_meter_state_version)  `1u`<br> |

## Detailed Description


The fourth metric, and the only one that needs NO TRUTH and still catches a false lock. `ber_evm_db` and `snr_m2m4_db` need no truth either, and a stationary-but-wrong constellation reads clean on both — measured across orders in `test_mpsk_receiver_performance.py`, with the penalty SHRINKING as M rises. BER sees it but needs truth and a trustworthy alignment. A CRC-checked frame needs no payload truth at all: it either checks or it does not, and a false lock fails it. That makes a frame error rate the one metric usable on a real capture that still detects the failure this receiver family is most prone to.


### What a frame outcome is



Two independent things can go wrong, and collapsing them loses the diagnosis: the sync word may not be FOUND, or the frame may be found and fail its CRC. Both are frame errors — a frame you did not detect is a frame you did not deliver — but "the sync is too short at this Es/N0" and "the
demodulator is making bit errors" are different repairs, so both counts come back separately.


A frame carrying no CRC (`crc = -1`, which is exactly what `wfm_frame_crc_ok()` returns for one) counts as delivered when its sync was detected. Counting it as an error instead would make every unprotected frame fail, which is a measurement of the frame format rather than the receiver.



### The stopping rule is the ERROR count, and that is not decoration



`ber_confidence()` is the exact Gamma/chi-square interval for INVERSE BINOMIAL sampling — fix the errors, let the trial count fall out. Its relative standard error is `1/sqrt(r)`, a function of the error count ALONE, which is why a run stopped on errors gives a consistent measurement and one stopped on a fixed count does not. This meter therefore uses the same rule as `ber_meter`, exposes the same `enough` read-back, and hands the same interval back. **Reusing that interval under a fixed-frame-count stopping rule would be the wrong sampling model** (that is binomial, and its exact interval is Clopper-Pearson), so the convention is stated here rather than left for a caller to assume.




**See also:** docs/design/rx-test.md section 2.5 




    
## Public Functions Documentation




### function frame\_meter\_add 

_Record one frame's outcome._ 
```C++
void frame_meter_add (
    frame_meter_state_t * state,
    int sync_ok,
    int crc
) 
```





**Parameters:**


* `state` the meter. 
* `sync_ok` non-zero when the frame's sync word was detected. Pass the detector's own decision — `ber_align_t::ok`, or `burst_demod`'s frame\_offset validity — never a threshold applied afterwards to a statistic. 
* `crc` `wfm_frame_crc_ok()`'s return, passed straight through: 1 pass, 0 fail, -1 the frame carries no CRC.

A frame counts as an error when its sync was not detected, or when it was and the CRC failed. With `crc = -1` a detected frame counts as delivered, because nothing about it can be checked.



```C++
>>> from doppler.ber import FrameMeter
>>> met = FrameMeter(target_errors=10)
>>> met.add(1, 1)    # found, and it checked
>>> met.add(1, 0)    # found, and the CRC failed
>>> met.add(0, 0)    # never found: still a frame you did not deliver
>>> met.add(1, -1)   # found, no CRC: delivered but not CHECKED
>>> met.frames, met.sync_detected, met.crc_passed, met.errors
(4, 3, 1, 2)
```
 


        

<hr>



### function frame\_meter\_create 

_Create an accumulator._ 
```C++
frame_meter_state_t * frame_meter_create (
    size_t target_errors,
    double conf
) 
```





**Parameters:**


* `target_errors` frame errors to accumulate before `enough`; 0 is taken as BER\_TARGET\_ERRORS. 
* `conf` confidence level in (0, 1); 0 is taken as BER\_CONF. 



**Returns:**

the meter, or NULL if `conf` is outside (0, 1). 





        

<hr>



### function frame\_meter\_destroy 

_Release the meter._ 
```C++
void frame_meter_destroy (
    frame_meter_state_t * state
) 
```




<hr>



### function frame\_meter\_fer 

_Frame error rate with its exact interval._ 
```C++
ber_interval_t frame_meter_fer (
    const frame_meter_state_t * state
) 
```



`ber_confidence(errors, frames, conf)` — the same interval `ber_meter` reports, which is generic over trials and therefore applies to frames unchanged. Assert on `lo`, never on `p_hat`.




**Parameters:**


* `state` the meter. 



**Returns:**

the rate with its exact interval. 
```C++
>>> from doppler.ber import FrameMeter
>>> met = FrameMeter(target_errors=4)
>>> for i in range(20):
...     met.add(1, 0 if i % 5 == 0 else 1)
>>> met.enough
1
>>> fer = met.fer()
>>> round(fer.p_hat, 3), fer.lo < fer.p_hat < fer.hi
(0.158, True)
```
 





        

<hr>



### function frame\_meter\_get\_crc\_passed 

_Frames whose CRC checked._ 
```C++
size_t frame_meter_get_crc_passed (
    const frame_meter_state_t * state
) 
```




<hr>



### function frame\_meter\_get\_enough 

_Non-zero once_ `target_errors` _frame errors have accumulated._
```C++
int frame_meter_get_enough (
    const frame_meter_state_t * state
) 
```



The stopping condition, so a caller loops records until the measurement has the precision it asked for rather than until a frame count someone guessed. 


        

<hr>



### function frame\_meter\_get\_errors 

_Frames not delivered: no sync, or a failed CRC._ 
```C++
size_t frame_meter_get_errors (
    const frame_meter_state_t * state
) 
```




<hr>



### function frame\_meter\_get\_frames 

_Frames attempted._ 
```C++
size_t frame_meter_get_frames (
    const frame_meter_state_t * state
) 
```




<hr>



### function frame\_meter\_get\_state 

_Serialize the running counters into_ `blob` _._
```C++
void frame_meter_get_state (
    const frame_meter_state_t * state,
    void * blob
) 
```




<hr>



### function frame\_meter\_get\_sync\_detected 

_Frames whose sync word was detected._ 
```C++
size_t frame_meter_get_sync_detected (
    const frame_meter_state_t * state
) 
```




<hr>



### function frame\_meter\_reset 

_Clear every counter; the configuration is untouched._ 
```C++
void frame_meter_reset (
    frame_meter_state_t * state
) 
```



The target and the confidence level are what the caller asked for, so resetting the accumulation must not silently re-negotiate them. Use it between records, or to discard a run that turned out to be measuring the wrong thing.




**Parameters:**


* `state` the meter. 
```C++
>>> from doppler.ber import FrameMeter
>>> met = FrameMeter(target_errors=10)
>>> met.add(1, 0)
>>> met.frames, met.errors
(1, 1)
>>> met.reset()
>>> met.frames, met.errors
(0, 0)
```
 




        

<hr>



### function frame\_meter\_set\_state 

_Restore; DP\_OK, or DP\_ERR\_INVALID if the blob is rejected._ 
```C++
int frame_meter_set_state (
    frame_meter_state_t * state,
    const void * blob
) 
```




<hr>



### function frame\_meter\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t frame_meter_state_bytes (
    const frame_meter_state_t * state
) 
```




<hr>



### function frame\_meter\_sync\_miss 

_Sync MISS rate with its exact interval._ 
```C++
ber_interval_t frame_meter_sync_miss (
    const frame_meter_state_t * state
) 
```



Reported as a miss rate rather than a detection rate so it is an ERROR rate like every other number here, and so the same interval applies without reinterpretation. **This is what turns "is this sync word long
enough at this Es/N0" into a measurement** — `ber_align_detect()` already returns `margin_db` and `runner_db` per attempt, and accumulating the decisions is what answers the question with a number.




**Parameters:**


* `state` the meter. 



**Returns:**

the miss rate with its exact interval. 
```C++
>>> from doppler.ber import FrameMeter
>>> met = FrameMeter()
>>> for i in range(50):
...     met.add(0 if i % 10 == 0 else 1, 1)
>>> met.frames, met.sync_detected
(50, 45)
>>> miss = met.sync_miss()
>>> round(miss.p_hat, 3), miss.hi > miss.p_hat
(0.082, True)
```
 





        

<hr>
## Macro Definition Documentation





### define FRAME\_METER\_STATE\_MAGIC 

```C++
#define FRAME_METER_STATE_MAGIC `DP_FOURCC ('F', 'R', 'M', 'M')`
```




<hr>



### define FRAME\_METER\_STATE\_VERSION 

```C++
#define FRAME_METER_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/frame_meter/frame_meter_core.h`

