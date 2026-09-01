

# Struct burst\_capture\_state\_t



[**ClassList**](annotated.md) **>** [**burst\_capture\_state\_t**](structburst__capture__state__t.md)



_BurstCapture state._ [More...](#detailed-description)

* `#include <burst_capture_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**burst\_acq\_state\_t**](structburst__acq__state__t.md) \* | [**acq**](#variable-acq)  <br> |
|  size\_t | [**acq\_blob\_max**](#variable-acq_blob_max)  <br> |
|  uint8\_t \* | [**acq\_code**](#variable-acq_code)  <br> |
|  size\_t | [**acq\_code\_len**](#variable-acq_code_len)  <br> |
|  int | [**backed**](#variable-backed)  <br> |
|  size\_t | [**burst\_len**](#variable-burst_len)  <br> |
|  double | [**chip\_rate**](#variable-chip_rate)  <br> |
|  size\_t | [**chunk\_max**](#variable-chunk_max)  <br> |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  size\_t | [**code\_period**](#variable-code_period)  <br> |
|  float \_Complex \* | [**corr\_buf**](#variable-corr_buf)  <br> |
|  size\_t | [**corr\_len**](#variable-corr_len)  <br> |
|  [**burst\_capture\_detection\_t**](structburst__capture__detection__t.md) \* | [**det**](#variable-det)  <br> |
|  size\_t | [**det\_cap**](#variable-det_cap)  <br> |
|  size\_t | [**det\_len**](#variable-det_len)  <br> |
|  double | [**doppler\_hz\_est**](#variable-doppler_hz_est)  <br> |
|  double | [**doppler\_res\_hz**](#variable-doppler_res_hz)  <br> |
|  uint64\_t | [**dropped**](#variable-dropped)  <br> |
|  [**burst\_capture\_event\_t**](structburst__capture__event__t.md) \* | [**ev**](#variable-ev)  <br> |
|  size\_t | [**ev\_cap**](#variable-ev_cap)  <br> |
|  size\_t | [**ev\_len**](#variable-ev_len)  <br> |
|  dp\_f32\_t \* | [**hist**](#variable-hist)  <br> |
|  size\_t | [**k\_hi**](#variable-k_hi)  <br> |
|  size\_t | [**k\_lo**](#variable-k_lo)  <br> |
|  size\_t | [**min\_gap**](#variable-min_gap)  <br> |
|  uint64\_t | [**n\_bursts**](#variable-n_bursts)  <br> |
|  size\_t | [**pending**](#variable-pending)  <br> |
|  uint64\_t | [**preamble\_start**](#variable-preamble_start)  <br> |
|  [**burst\_capture\_pending\_t**](structburst__capture__pending__t.md) \* | [**q**](#variable-q)  <br> |
|  size\_t | [**q\_cap**](#variable-q_cap)  <br> |
|  size\_t | [**q\_head**](#variable-q_head)  <br> |
|  int | [**recovered**](#variable-recovered)  <br> |
|  float \* | [**ref\_sign**](#variable-ref_sign)  <br> |
|  double | [**refine\_margin**](#variable-refine_margin)  <br> |
|  size\_t | [**refine\_span**](#variable-refine_span)  <br> |
|  uint8\_t \* | [**released**](#variable-released)  <br> |
|  size\_t | [**reps**](#variable-reps)  <br> |
|  size\_t | [**retain\_span**](#variable-retain_span)  <br> |
|  uint64\_t | [**samples\_fed**](#variable-samples_fed)  <br> |
|  size\_t | [**spc**](#variable-spc)  <br> |
|  uint64\_t | [**suppress\_base**](#variable-suppress_base)  <br> |
|  uint64\_t | [**suppress\_until**](#variable-suppress_until)  <br> |
|  int | [**underpowered**](#variable-underpowered)  <br> |
|  float \_Complex \* | [**win**](#variable-win)  <br> |
|  size\_t | [**win\_cap**](#variable-win_cap)  <br> |












































## Detailed Description


Allocate with [**burst\_capture\_create()**](burst__capture__core_8h.md#function-burst_capture_create). 


    
## Public Attributes Documentation




### variable acq 

```C++
burst_acq_state_t* burst_capture_state_t::acq;
```



Search stage, certified separately. 
 


        

<hr>



### variable acq\_blob\_max 

```C++
size_t burst_capture_state_t::acq_blob_max;
```



Fixed upper bound on the acquisition child's blob. state\_bytes() must be a pure function of CONFIGURATION  jm's binding compares an incoming blob's length against it  yet both the retained look-back and acq's own unconsumed ring vary with the stream. Both are therefore written into fixed-size regions with a length prefix. 
 


        

<hr>



### variable acq\_code 

```C++
uint8_t* burst_capture_state_t::acq_code;
```



Preamble code, owned copy. 
 


        

<hr>



### variable acq\_code\_len 

```C++
size_t burst_capture_state_t::acq_code_len;
```



Preamble code length, chips. 
 


        

<hr>



### variable backed 

```C++
int burst_capture_state_t::backed;
```



Non-zero when the ring's pages are a FILE's. Fixed at create(), so state\_bytes() stays a pure function of configuration  a backed blob and an in-RAM one are different sizes on purpose, and neither restores into the other. 
 


        

<hr>



### variable burst\_len 

```C++
size_t burst_capture_state_t::burst_len;
```



Samples in one emitted window. Acquisition has no notion of this  [**acq\_create\_burst()**](acq__core_8h.md#function-acq_create_burst) takes search parameters only  which is exactly why it is a parameter HERE: for a capture, the burst length is what gets captured. 
 


        

<hr>



### variable chip\_rate 

```C++
double burst_capture_state_t::chip_rate;
```



Chip rate, Hz. 
 


        

<hr>



### variable chunk\_max 

```C++
size_t burst_capture_state_t::chunk_max;
```



Largest slice of one push processed at a time, so any block size is accepted without the ring overrunning its own retention. 
 


        

<hr>



### variable cn0\_dbhz\_est 

```C++
double burst_capture_state_t::cn0_dbhz_est;
```



C/N0 lower bound, dB-Hz (saturating). 
 


        

<hr>



### variable code\_period 

```C++
size_t burst_capture_state_t::code_period;
```



One preamble repetition, in SAMPLES. The modulus acq's code\_phase is a residue of, so every epoch ambiguity in this object is stated against it. 
 


        

<hr>



### variable corr\_buf 

```C++
float _Complex* burst_capture_state_t::corr_buf;
```



Per-offset code-period correlations, reused across the candidate sweep so the sliding correlation is computed once and the non-coherent combine just indexes it. 
 


        

<hr>



### variable corr\_len 

```C++
size_t burst_capture_state_t::corr_len;
```



Entries in corr\_buf. 
 


        

<hr>



### variable det 

```C++
burst_capture_detection_t* burst_capture_state_t::det;
```



Raw hits of the LAST push  what the SEARCH found, before the claim rule and the suppression window. 
 


        

<hr>



### variable det\_cap 

```C++
size_t burst_capture_state_t::det_cap;
```



Allocated records. 
 


        

<hr>



### variable det\_len 

```C++
size_t burst_capture_state_t::det_len;
```



Records the last push wrote. 
 


        

<hr>



### variable doppler\_hz\_est 

```C++
double burst_capture_state_t::doppler_hz_est;
```



Signed coarse Doppler, Hz. 
 


        

<hr>



### variable doppler\_res\_hz 

```C++
double burst_capture_state_t::doppler_res_hz;
```



Width of that estimate. 
 


        

<hr>



### variable dropped 

```C++
uint64_t burst_capture_state_t::dropped;
```



Samples the ring refused. A LOST BURST each, not a statistic  lifetime, survives reset(). 
 


        

<hr>



### variable ev 

```C++
burst_capture_event_t* burst_capture_state_t::ev;
```



One record per window returned. 
 


        

<hr>



### variable ev\_cap 

```C++
size_t burst_capture_state_t::ev_cap;
```



Allocated records. 
 


        

<hr>



### variable ev\_len 

```C++
size_t burst_capture_state_t::ev_len;
```



Records the last push() wrote. 
 


        

<hr>



### variable hist 

```C++
dp_f32_t* burst_capture_state_t::hist;
```



History ring. Double-mapped, so a window that spans the wrap is ONE contiguous pointer. This object keeps its own rather than borrowing acq's, which consumes every frame it processes and has therefore released what is still needed. 
 


        

<hr>



### variable k\_hi 

```C++
size_t burst_capture_state_t::k_hi;
```



...and AFTER: `reps`. The detecting frame can start before the preamble, so the anchor can be up to `coherent_bins - 1` periods early. It was 2, and at reps=10 refine returned one period early with a resolved-looking margin (doppler#1181). 
 


        

<hr>



### variable k\_lo 

```C++
size_t burst_capture_state_t::k_lo;
```



Whole code periods searched BEFORE the anchor: `3*reps + 2`, the detection lag's bound. 
 


        

<hr>



### variable min\_gap 

```C++
size_t burst_capture_state_t::min_gap;
```



Dead air a caller must leave BETWEEN bursts, in samples  edge to edge, not start to start.


DERIVED, and the derivation is the point. A detection's anchor is the code epoch of whichever frame detected, and acquisition's framing is not aligned to the preamble, so the last frame that can detect sits up to `reps * code_period` past the true start (the detection lag, docs/design/dsss-burst-receiver.md §7.1). CLAIM merges two anchors closer than `refine_span`, so with the first burst detected LATE and the second EARLY the pair survives only when


gap &gt;= refine\_span + reps\*P - burst\_len


ZERO is a real answer  a burst longer than `refine_span + reps*P` needs no gap for the claim rule's sake. It does not mean zero is wise: a zero gap is a continuous stream rather than a burst link (the design's own non-goal), and it measures 88% at a geometry where this reads 0.


The prose this replaces said `max(0, refine_span - burst_len)` and was short by the whole detection-lag term  32 samples against 528 at the C suite's geometry (doppler#1172). 
 


        

<hr>



### variable n\_bursts 

```C++
uint64_t burst_capture_state_t::n_bursts;
```



Windows emitted, lifetime. 
 


        

<hr>



### variable pending 

```C++
size_t burst_capture_state_t::pending;
```



Detections held because their burst window has NOT fully arrived  the caller-facing "there is not
enough data yet" read-back. push() deliberately emits nothing for these: a window is returned when it is complete, not when it is guessed at. What it exists for is the other end  a caller closing a file while this is non-zero is discarding a burst that would have been captured. 
 


        

<hr>



### variable preamble\_start 

```C++
uint64_t burst_capture_state_t::preamble_start;
```



Stream-absolute preamble start. Never late. 


        

<hr>



### variable q 

```C++
burst_capture_pending_t* burst_capture_state_t::q;
```



Detections, oldest first; `q_cap` long. 
 


        

<hr>



### variable q\_cap 

```C++
size_t burst_capture_state_t::q_cap;
```



DERIVED, not a constant. Entries sit at least `refine_span` apart within `retain_span` of the head, so the count scales with burst\_len/refine\_span  about 1 at a short-burst test geometry but 5.5x at a real link. A fixed 8 silently dropped the hit AND the rest of the batch on anything else. 
 


        

<hr>



### variable q\_head 

```C++
size_t burst_capture_state_t::q_head;
```



Index of the oldest entry. 
 


        

<hr>



### variable recovered 

```C++
int burst_capture_state_t::recovered;
```



Non-zero when create() found the backing file already holding a ring of exactly this geometry, so its samples ARE the look-back. Zero when the file was created or resized, which zeroes it  and then a blob claiming retained history has nothing to reach back into, which set\_state() refuses rather than resuming into silence. 
 


        

<hr>



### variable ref\_sign 

```C++
float* burst_capture_state_t::ref_sign;
```



One code period of +-1 chip signs, spc-expanded. Real, so the per-period correlation is a signed sum rather than a complex multiply. 
 


        

<hr>



### variable refine\_margin 

```C++
double burst_capture_state_t::refine_margin;
```



Winning preamble correlation over its nearest whole-period competitor. Near 1 means the period was NOT resolved. 
 


        

<hr>



### variable refine\_span 

```C++
size_t burst_capture_state_t::refine_span;
```



Candidate offsets searched, in samples: `(k_lo + k_hi + reps) * code_period`. Read it rather than restating the formula  the design doc's own prose for it was 2.4x low at reps=5 until it was measured.


The merge test compares two resolved code epochs  burst START against burst START  so it bounds start-to-start separation, NOT the dead air between bursts (doppler#1085). The gap actually required is NOT `max(0, refine_span - burst_len)` either: swept, a pair needs about two code periods of dead air, against the 32 samples that formula gives at the test geometry (doppler#1172). 
 


        

<hr>



### variable released 

```C++
uint8_t* burst_capture_state_t::released;
```



Per row of `ev`: the consumer said "not a
 burst". Scratch, like the rows. 
 


        

<hr>



### variable reps 

```C++
size_t burst_capture_state_t::reps;
```



Preamble code repetitions. 
 


        

<hr>



### variable retain\_span 

```C++
size_t burst_capture_state_t::retain_span;
```



Samples that must stay reachable: refine span + one whole burst. Also the caller-facing minimum TRAILING context  a burst closer than this to the end of what has been pushed is not emitted until more samples arrive. 
 


        

<hr>



### variable samples\_fed 

```C++
uint64_t burst_capture_state_t::samples_fed;
```



Stream position: total samples ever pushed. What makes an epoch stream-ABSOLUTE, and the reason preamble\_start is a quantity only this object can compute. 
 


        

<hr>



### variable spc 

```C++
size_t burst_capture_state_t::spc;
```



Samples per chip. 
 


        

<hr>



### variable suppress\_base 

```C++
uint64_t burst_capture_state_t::suppress_base;
```



`suppress_until` as the last push() began: what EARLIER pushes' windows own, which a release() of this push's window must not give back. 
 


        

<hr>



### variable suppress\_until 

```C++
uint64_t burst_capture_state_t::suppress_until;
```



Detections below this stream position fall inside a burst already EMITTED, so they are the payload firing against the acquisition code rather than new bursts. Armed when a window is emitted  refine resolved a start here and this object handed out the whole span, which is the fact it owns. Arming it on every DETECTION instead let one spurious hit blind the search for a whole burst and discard the next real one (doppler#1004). Coalescing the several frames of ONE preamble is a separate job, done by `refine_span` proximity plus a greatest-of tie-break. 
 


        

<hr>



### variable underpowered 

```C++
int burst_capture_state_t::underpowered;
```



The search cannot meet the requested pd at this cn0\_dbhz and geometry. It still builds a best-effort grid, so the symptom is bursts that are never captured rather than an error. 
 


        

<hr>



### variable win 

```C++
float _Complex* burst_capture_state_t::win;
```



Emitted windows, burst\_len apart. 
 


        

<hr>



### variable win\_cap 

```C++
size_t burst_capture_state_t::win_cap;
```



Allocated samples. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/burst_capture/burst_capture_core.h`

