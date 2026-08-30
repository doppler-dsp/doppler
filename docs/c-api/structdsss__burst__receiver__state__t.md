

# Struct dsss\_burst\_receiver\_state\_t



[**ClassList**](annotated.md) **>** [**dsss\_burst\_receiver\_state\_t**](structdsss__burst__receiver__state__t.md)



_DsssBurstReceiver state._ [More...](#detailed-description)

* `#include <dsss_burst_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**burst\_acq\_state\_t**](structburst__acq__state__t.md) \* | [**acq**](#variable-acq)  <br> |
|  size\_t | [**acq\_blob\_max**](#variable-acq_blob_max)  <br> |
|  uint8\_t \* | [**acq\_code**](#variable-acq_code)  <br> |
|  size\_t | [**acq\_code\_len**](#variable-acq_code_len)  <br> |
|  size\_t | [**burst\_len**](#variable-burst_len)  <br> |
|  double | [**chip\_rate**](#variable-chip_rate)  <br> |
|  size\_t | [**chunk\_max**](#variable-chunk_max)  <br> |
|  double | [**cn0\_dbhz\_est**](#variable-cn0_dbhz_est)  <br> |
|  size\_t | [**code\_period**](#variable-code_period)  <br> |
|  float \_Complex \* | [**corr\_buf**](#variable-corr_buf)  <br> |
|  size\_t | [**corr\_len**](#variable-corr_len)  <br> |
|  uint8\_t \* | [**data\_code**](#variable-data_code)  <br> |
|  size\_t | [**data\_code\_len**](#variable-data_code_len)  <br> |
|  [**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* | [**demod**](#variable-demod)  <br> |
|  double | [**doppler\_hz\_est**](#variable-doppler_hz_est)  <br> |
|  double | [**doppler\_res\_hz**](#variable-doppler_res_hz)  <br> |
|  uint64\_t | [**dropped**](#variable-dropped)  <br> |
|  double | [**est\_freq\_hz**](#variable-est_freq_hz)  <br> |
|  double | [**est\_rate\_hz**](#variable-est_rate_hz)  <br> |
|  double | [**est\_snr\_db**](#variable-est_snr_db)  <br> |
|  [**dsss\_br\_event\_t**](structdsss__br__event__t.md) \* | [**ev**](#variable-ev)  <br> |
|  size\_t | [**ev\_cap**](#variable-ev_cap)  <br> |
|  size\_t | [**ev\_len**](#variable-ev_len)  <br> |
|  size\_t | [**frame\_bits**](#variable-frame_bits)  <br> |
|  size\_t | [**frame\_syms**](#variable-frame_syms)  <br> |
|  dp\_f32\_t \* | [**hist**](#variable-hist)  <br> |
|  size\_t | [**k\_hi**](#variable-k_hi)  <br> |
|  size\_t | [**k\_lo**](#variable-k_lo)  <br> |
|  float \* | [**llr**](#variable-llr)  <br> |
|  size\_t | [**llr\_cap**](#variable-llr_cap)  <br> |
|  size\_t | [**llr\_len**](#variable-llr_len)  <br> |
|  uint64\_t | [**n\_bursts**](#variable-n_bursts)  <br> |
|  size\_t | [**pending**](#variable-pending)  <br> |
|  uint64\_t | [**preamble\_start**](#variable-preamble_start)  <br> |
|  [**dsss\_br\_pending\_t**](structdsss__br__pending__t.md) \* | [**q**](#variable-q)  <br> |
|  size\_t | [**q\_cap**](#variable-q_cap)  <br> |
|  size\_t | [**q\_head**](#variable-q_head)  <br> |
|  float \* | [**ref\_sign**](#variable-ref_sign)  <br> |
|  double | [**refine\_margin**](#variable-refine_margin)  <br> |
|  size\_t | [**refine\_span**](#variable-refine_span)  <br> |
|  size\_t | [**reps**](#variable-reps)  <br> |
|  size\_t | [**retain\_span**](#variable-retain_span)  <br> |
|  uint64\_t | [**samples\_fed**](#variable-samples_fed)  <br> |
|  size\_t | [**spc**](#variable-spc)  <br> |
|  uint64\_t | [**suppress\_until**](#variable-suppress_until)  <br> |
|  uint8\_t \* | [**sync**](#variable-sync)  <br> |
|  size\_t | [**sync\_len**](#variable-sync_len)  <br> |












































## Detailed Description


Allocate with [**dsss\_burst\_receiver\_create()**](dsss__burst__receiver__core_8h.md#function-dsss_burst_receiver_create). 


    
## Public Attributes Documentation




### variable acq 

```C++
burst_acq_state_t* dsss_burst_receiver_state_t::acq;
```



Search stage. 
 


        

<hr>



### variable acq\_blob\_max 

```C++
size_t dsss_burst_receiver_state_t::acq_blob_max;
```



Fixed upper bound on the acquisition child's blob. `state_bytes()` must be a pure function of CONFIGURATION  jm's binding compares an incoming blob's length against it  yet both the retained look-back and acq's own unconsumed ring vary with the stream. Both are therefore written into fixed-size regions with a length prefix. 
 


        

<hr>



### variable acq\_code 

```C++
uint8_t* dsss_burst_receiver_state_t::acq_code;
```



Preamble code, owned copy. 
 


        

<hr>



### variable acq\_code\_len 

```C++
size_t dsss_burst_receiver_state_t::acq_code_len;
```



Preamble code length, chips. 
 


        

<hr>



### variable burst\_len 

```C++
size_t dsss_burst_receiver_state_t::burst_len;
```



Preamble + spread frame, in samples. 
 


        

<hr>



### variable chip\_rate 

```C++
double dsss_burst_receiver_state_t::chip_rate;
```



Chip rate, Hz. 
 


        

<hr>



### variable chunk\_max 

```C++
size_t dsss_burst_receiver_state_t::chunk_max;
```



Largest slice of one push processed at a time, so any block size is accepted without the ring overrunning its own retention. 
 


        

<hr>



### variable cn0\_dbhz\_est 

```C++
double dsss_burst_receiver_state_t::cn0_dbhz_est;
```



C/N0 lower bound, dB-Hz (saturating). 
 


        

<hr>



### variable code\_period 

```C++
size_t dsss_burst_receiver_state_t::code_period;
```



One preamble repetition, in SAMPLES. The modulus acq's code\_phase is a residue of, so every epoch ambiguity in this object is stated against it. 
 


        

<hr>



### variable corr\_buf 

```C++
float _Complex* dsss_burst_receiver_state_t::corr_buf;
```



Per-offset code-period correlations, reused across the candidate sweep so the sliding correlation is computed once and the non-coherent combine just indexes it. 
 


        

<hr>



### variable corr\_len 

```C++
size_t dsss_burst_receiver_state_t::corr_len;
```



Entries in corr\_buf. 
 


        

<hr>



### variable data\_code 

```C++
uint8_t* dsss_burst_receiver_state_t::data_code;
```



Payload spreading code, owned copy. 
 


        

<hr>



### variable data\_code\_len 

```C++
size_t dsss_burst_receiver_state_t::data_code_len;
```



Data code length, chips. 
 


        

<hr>



### variable demod 

```C++
burst_demod_state_t* dsss_burst_receiver_state_t::demod;
```



Demod stage, re-seeded per burst. 
 


        

<hr>



### variable doppler\_hz\_est 

```C++
double dsss_burst_receiver_state_t::doppler_hz_est;
```



Signed coarse Doppler, Hz. 
 


        

<hr>



### variable doppler\_res\_hz 

```C++
double dsss_burst_receiver_state_t::doppler_res_hz;
```



Width of that estimate. 
 


        

<hr>



### variable dropped 

```C++
uint64_t dsss_burst_receiver_state_t::dropped;
```



Samples the ring refused. A LOST BURST each, not a statistic  lifetime, survives reset(). 
 


        

<hr>



### variable est\_freq\_hz 

```C++
double dsss_burst_receiver_state_t::est_freq_hz;
```



Demod's own residual estimate, Hz. 
 


        

<hr>



### variable est\_rate\_hz 

```C++
double dsss_burst_receiver_state_t::est_rate_hz;
```



Demod's own chirp-rate estimate. 
 


        

<hr>



### variable est\_snr\_db 

```C++
double dsss_burst_receiver_state_t::est_snr_db;
```



Demod's own post-decode SNR estimate. 
 


        

<hr>



### variable ev 

```C++
dsss_br_event_t* dsss_burst_receiver_state_t::ev;
```



One record per burst returned. 
 


        

<hr>



### variable ev\_cap 

```C++
size_t dsss_burst_receiver_state_t::ev_cap;
```



Allocated records. 
 


        

<hr>



### variable ev\_len 

```C++
size_t dsss_burst_receiver_state_t::ev_len;
```



Records the last push() wrote. 
 


        

<hr>



### variable frame\_bits 

```C++
size_t dsss_burst_receiver_state_t::frame_bits;
```



The frame's length, from the description  the stride of a row in `llr`. 
 


        

<hr>



### variable frame\_syms 

```C++
size_t dsss_burst_receiver_state_t::frame_syms;
```



Symbols the frame occupies after the sync word, and so bits per burst out of push(). What they MEAN is a frame description's business, one layer up (doppler#1022). 
 


        

<hr>



### variable hist 

```C++
dp_f32_t* dsss_burst_receiver_state_t::hist;
```



History ring. Double-mapped, so a window that spans the wrap is ONE contiguous pointer. The receiver keeps its own rather than borrowing acq's, which consumes every frame it processes and has therefore released what is still needed. 
 


        

<hr>



### variable k\_hi 

```C++
size_t dsss_burst_receiver_state_t::k_hi;
```



...and after. 
 


        

<hr>



### variable k\_lo 

```C++
size_t dsss_burst_receiver_state_t::k_lo;
```



Whole code periods searched BEFORE the anchor. 
 


        

<hr>



### variable llr 

```C++
float* dsss_burst_receiver_state_t::llr;
```



The soft bits of every burst the last push returned, concatenated: burst i starts at i\*frame\_bits. Scratch, like `ev`  it describes one call and is never serialized. 
 


        

<hr>



### variable llr\_cap 

```C++
size_t dsss_burst_receiver_state_t::llr_cap;
```



Allocated floats. 
 


        

<hr>



### variable llr\_len 

```C++
size_t dsss_burst_receiver_state_t::llr_len;
```



Floats written by the last push. 
 


        

<hr>



### variable n\_bursts 

```C++
uint64_t dsss_burst_receiver_state_t::n_bursts;
```



Bursts demodulated, lifetime. 
 


        

<hr>



### variable pending 

```C++
size_t dsss_burst_receiver_state_t::pending;
```



Detections held because their burst window has NOT fully arrived  the caller-facing "there is not enough data yet" read-back.


push() deliberately emits nothing for these: a burst is returned when it is complete, not when it is guessed at. Feed more samples and it comes out, bit-exact, wherever the split fell. What this exists for is the OTHER end: a caller closing a file or a socket while this is non-zero is discarding a burst that would have decoded, and every other read-back looks identical to "nothing was
ever there" (`dropped` counts ring refusals, not this). 
 


        

<hr>



### variable preamble\_start 

```C++
uint64_t dsss_burst_receiver_state_t::preamble_start;
```



Stream-absolute preamble start. Never late. 


        

<hr>



### variable q 

```C++
dsss_br_pending_t* dsss_burst_receiver_state_t::q;
```



Detections, oldest first; `q_cap` long. 
 


        

<hr>



### variable q\_cap 

```C++
size_t dsss_burst_receiver_state_t::q_cap;
```



DERIVED, not a constant. Entries sit at least `refine_span` apart within `retain_span` of the head, so the count scales with burst\_len/refine\_span  about 1 at the C test geometry but 5.5x at a real link and 20x for a long payload. A fixed 8 silently dropped the hit AND the rest of the batch on anything but the test's own geometry. 
 


        

<hr>



### variable q\_head 

```C++
size_t dsss_burst_receiver_state_t::q_head;
```



Index of the oldest entry. 
 


        

<hr>



### variable ref\_sign 

```C++
float* dsss_burst_receiver_state_t::ref_sign;
```



One code period of +-1 chip signs, spc-expanded. Real, so the per-period correlation is a signed sum rather than a complex multiply. 
 


        

<hr>



### variable refine\_margin 

```C++
double dsss_burst_receiver_state_t::refine_margin;
```



Winning preamble correlation over its nearest whole-period competitor. Near 1 means the period was NOT resolved. 
 


        

<hr>



### variable refine\_span 

```C++
size_t dsss_burst_receiver_state_t::refine_span;
```



Candidate offsets searched, in samples: `(k_lo + k_hi + reps) * code_period`, which is `(4*reps + 4) * code_period` for the `k_lo` chosen in create(). This doc claimed `2*reps*code_period` until it was measured  2.4x low at reps=5  so read `refine_span` rather than the formula.


The merge test compares two resolved code epochs  burst START against burst START  so it bounds start-to-start separation, NOT the dead air between bursts. The two differ by a whole burst; the gap a caller must leave is `max(0, refine_span - burst_len)`, which is 0 for any burst longer than the reach. Reading it as dead air reserved 9% airtime for nothing (doppler#1085). 
 


        

<hr>



### variable reps 

```C++
size_t dsss_burst_receiver_state_t::reps;
```



Preamble code repetitions. 
 


        

<hr>



### variable retain\_span 

```C++
size_t dsss_burst_receiver_state_t::retain_span;
```



Samples that must stay reachable: refine span + one whole burst. Also the caller-facing minimum TRAILING context  a burst closer than this to the end of what has been pushed is not emitted until more samples arrive. 
 


        

<hr>



### variable samples\_fed 

```C++
uint64_t dsss_burst_receiver_state_t::samples_fed;
```



Stream position: total samples ever pushed. What makes an epoch stream-ABSOLUTE, and the reason preamble\_start is a quantity only this object can compute. 
 


        

<hr>



### variable spc 

```C++
size_t dsss_burst_receiver_state_t::spc;
```



Samples per chip. 
 


        

<hr>



### variable suppress\_until 

```C++
uint64_t dsss_burst_receiver_state_t::suppress_until;
```



Detections below this stream position fall inside a burst that has already DECODED, so they are the payload firing against the acquisition code rather than new bursts. Armed only on a valid frame: arming it on every detection let one spurious hit blind the search for a whole burst and discard the next real one (doppler#1004). Coalescing the several frames of ONE preamble is a separate job, done by `refine_span` proximity plus a greatest-of tie-break. 
 


        

<hr>



### variable sync 

```C++
uint8_t* dsss_burst_receiver_state_t::sync;
```



Frame sync word, owned copy. 
 


        

<hr>



### variable sync\_len 

```C++
size_t dsss_burst_receiver_state_t::sync_len;
```



Sync word length, symbols. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dsss_burst_receiver/dsss_burst_receiver_core.h`

