

# File burst\_demod\_core.h



[**FileList**](files.md) **>** [**burst\_demod**](dir_ab0fdf036101e4998629894503e143b8.md) **>** [**burst\_demod\_core.h**](burst__demod__core_8h.md)

[Go to the source code of this file](burst__demod__core_8h_source.md)

_Feedforward BPSK DSSS frame demodulator._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/ppe/ppe_core.h"`
* `#include "doppler/fft/fft_core.h"`
* `#include "doppler/spectral/spectral_core.h"`
* `#include "doppler/dp_complex.h"`
* `#include "doppler/conv/conv_core.h"`
* `#include "doppler/rs/rs_core.h"`
* `#include "doppler/pn/pn_core.h"`
* `#include "doppler/gold/gold_core.h"`
* `#include "doppler/wfm/wfm_frame.h"`
* `#include "doppler/mpsk/mpsk_core.h"`
* `#include "doppler/cvt/cvt_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_burst\_demod\_state\_t**](structdp__burst__demod__state__t.md) <br>_BurstDemod state. Allocate with_ [_**dp\_burst\_demod\_create\_desc()**_](burst__demod__core_8h.md#function-dp_burst_demod_create_desc) _._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_burst\_demod\_state\_t**](structdp__burst__demod__state__t.md) \* | [**dp\_burst\_demod\_create\_desc**](#function-dp_burst_demod_create_desc) (const uint8\_t \* data\_code, size\_t data\_code\_len, const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* frame, size\_t spc, double chip\_rate, double carrier\_hz, double max\_rate, size\_t est\_segments, const char \*\* why) <br>_Create a demodulator from the frame DESCRIPTION the transmitter spread, in place of a sync word and a hand-counted_ `frame_syms` _._ |
|  [**dp\_burst\_demod\_state\_t**](structdp__burst__demod__state__t.md) \* | [**dp\_burst\_demod\_create\_frame**](#function-dp_burst_demod_create_frame) (const uint8\_t \* data\_code, size\_t data\_code\_len, const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* frame, size\_t spc, double chip\_rate, double carrier\_hz, double max\_rate, size\_t est\_segments) <br>_The Python binding's constructor:_ [_**dp\_burst\_demod\_create\_desc**_](burst__demod__core_8h.md#function-dp_burst_demod_create_desc) _without the_`why` _out-parameter._ |
|  size\_t | [**dp\_burst\_demod\_demod**](#function-dp_burst_demod_demod) ([**dp\_burst\_demod\_state\_t**](structdp__burst__demod__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, uint8\_t \* out, size\_t max\_out) <br>_Demodulate one burst end to end and write the frame's bits._  |
|  size\_t | [**dp\_burst\_demod\_demod\_max\_out**](#function-dp_burst_demod_demod_max_out) ([**dp\_burst\_demod\_state\_t**](structdp__burst__demod__state__t.md) \* state) <br>_Max output bits = frame\_syms (caller sizes the buffer)._  |
|  void | [**dp\_burst\_demod\_destroy**](#function-dp_burst_demod_destroy) ([**dp\_burst\_demod\_state\_t**](structdp__burst__demod__state__t.md) \* state) <br>_Destroy a demodulator._  |
|  size\_t | [**dp\_burst\_demod\_llrs**](#function-dp_burst_demod_llrs) ([**dp\_burst\_demod\_state\_t**](structdp__burst__demod__state__t.md) \* state, size\_t n, float \* out, size\_t max\_out) <br>_LLRs the last demod() wrote — the frame's soft bits._  |
|  size\_t | [**dp\_burst\_demod\_llrs\_max\_out**](#function-dp_burst_demod_llrs_max_out) ([**dp\_burst\_demod\_state\_t**](structdp__burst__demod__state__t.md) \* state, size\_t n) <br>_Max LLRs_ [_**dp\_burst\_demod\_llrs()**_](burst__demod__core_8h.md#function-dp_burst_demod_llrs) _writes: the frame's length in bits._ |
|  void | [**dp\_burst\_demod\_reset**](#function-dp_burst_demod_reset) ([**dp\_burst\_demod\_state\_t**](structdp__burst__demod__state__t.md) \* state) <br>_Clear the per-burst read-backs, leaving the configuration intact._  |
|  void | [**dp\_burst\_demod\_set\_preamble**](#function-dp_burst_demod_set_preamble) ([**dp\_burst\_demod\_state\_t**](structdp__burst__demod__state__t.md) \* state, const uint8\_t \* acq\_code, size\_t acq\_code\_len, size\_t reps) <br>_Register the unmodulated acquisition preamble code and its repetition count used for the feedforward (f0, rate) estimate._  |
|  void | [**dp\_burst\_demod\_set\_prior**](#function-dp_burst_demod_set_prior) ([**dp\_burst\_demod\_state\_t**](structdp__burst__demod__state__t.md) \* state, double f0\_coarse, size\_t start) <br>_Seed the demodulator from acquisition with the coarse Doppler and the preamble start sample._  |
|  size\_t | [**dp\_burst\_demod\_symbols**](#function-dp_burst_demod_symbols) ([**dp\_burst\_demod\_state\_t**](structdp__burst__demod__state__t.md) \* state, size\_t n, float \_Complex \* out, size\_t max\_out) <br>_The last demod()'s DEROTATED complex symbols — the constellation the LLRs are the real part of._  |
|  size\_t | [**dp\_burst\_demod\_symbols\_max\_out**](#function-dp_burst_demod_symbols_max_out) ([**dp\_burst\_demod\_state\_t**](structdp__burst__demod__state__t.md) \* state, size\_t n) <br>_Max symbols_ [_**dp\_burst\_demod\_symbols()**_](burst__demod__core_8h.md#function-dp_burst_demod_symbols) _writes: the frame's length._ |




























## Detailed Description


The whole post-acquisition payload chain, in C, with no tracking loops:
* preamble estimate — segment-despread the unmodulated, repeated acq preamble into partial correlations and feed them to ppe, giving a coarse (frequency, chirp-rate);
* sample-rate dechirp by (f0, rate) — removes Doppler AND Doppler rate;
* despread the data section with the (short) data code -&gt; soft BPSK symbols;
* frame sync — correlate the symbols against the known sync word; the complex peak gives the frame offset and the residual phase (derotated);
* slice `frame_syms` symbols to bits, hard and soft, and STOP.




### Where this object's job ends



At a decision. It hands back one bit per symbol (demod()) and one LLR per symbol ([**dp\_burst\_demod\_llrs()**](burst__demod__core_8h.md#function-dp_burst_demod_llrs)), and it does not know what any of them mean: which are payload, which are a check, what an outer code would repair are all questions about a FRAME, and answering them needs a description this object deliberately does not hold (doppler#1022). It used to hold half of one — a hard-coded `sync | payload | CRC-16` — which is how a burst sent without a trailer came to be reported invalid.


What it does need is the sync word, to find the frame and resolve the BPSK sign, and `frame_syms`, to know how many symbols to slice. Both are physical-layer facts, and both come from ONE place: the frame description the transmitter spread. Its field 0 is the sync word and its layout is the frame's length, so the two cannot disagree with each other or with the transmitter.


Build it from that description with [**dp\_burst\_demod\_create\_desc()**](burst__demod__core_8h.md#function-dp_burst_demod_create_desc), seed it with set\_preamble(acq code, reps) and set\_prior(coarse Doppler, preamble
start), then demod(burst). One `max_rate` knob spans near-static Doppler (0) to severe LEO chirp. One-shot per burst. Composes ppe (which composes fft + spectral).



```C++
const uint8_t    sb[3] = { 1, 0, 1 }, dcode[4] = { 1, 0, 1, 1 };
const uint8_t    acode[8] = { 1, 1, 1, 0, 1, 0, 0, 1 };
const wfm_seq_t  sync  = { .kind = WFM_SEQ_LITERAL, .bits = sb, .len = 3 };
const wfm_seq_t  data  = { .kind = WFM_SEQ_DATA, .len = 8 };
wfm_frame_desc_t f;
dp_wfm_frame_fixed (&f, NULL, 0, &sync, &data, 1); // sync|data:8|crc16
dp_burst_demod_state_t *d = dp_burst_demod_create_desc (
    dcode, 4, &f, 4, 1e6, 0.0, 0.0, 10, NULL);
if (!d)
  return 1;
dp_burst_demod_set_preamble (d, acode, 8, 5);
dp_burst_demod_set_prior (d, 0.0, 0);  // coarse Doppler, preamble start
float _Complex x[16] = { 0 };          // a real burst goes here
uint8_t        bits[3 + 8 + 16];
size_t nbits = dp_burst_demod_demod (d, x, 16, bits, sizeof bits);
(void)nbits;                           // 0: too short to be a burst
dp_burst_demod_destroy (d);
```
 



    
## Public Functions Documentation




### function dp\_burst\_demod\_create\_desc 

_Create a demodulator from the frame DESCRIPTION the transmitter spread, in place of a sync word and a hand-counted_ `frame_syms` _._
```C++
dp_burst_demod_state_t * dp_burst_demod_create_desc (
    const uint8_t * data_code,
    size_t data_code_len,
    const wfm_frame_desc_t * frame,
    size_t spc,
    double chip_rate,
    double carrier_hz,
    double max_rate,
    size_t est_segments,
    const char ** why
) 
```



The sync word is the description's field 0 and `frame_syms` is its layout's length, both read by [**dp\_wfm\_frame\_desc\_rx**](wfm__frame_8h.md#function-dp_wfm_frame_desc_rx), so the receiver and the transmitter are told the same thing by the same code. The description is read here and not kept: the sync bits are copied into the demodulator, so the caller may free `frame` at once. It is the one constructor: the demodulator is then seeded with set\_preamble() and set\_prior(), and demod() is called once per burst.




**Parameters:**


* `data_code` the data spreading code, 0/1 chips. 
* `data_code_len` its length (the spreading factor). 
* `frame` the description (`const  wfm_frame_desc_t *`). 
* `spc` samples per chip. 
* `chip_rate` chips per second. 
* `carrier_hz` the carrier the baseband is offset by, Hz. 
* `max_rate` the Doppler rate searched, cycles/sample^2; 0 selects the single-FFT estimate. 
* `est_segments` partials per acquisition period for the estimate. 
* `why` on a NULL return, receives a static sentence naming the fix (a description refused by [**dp\_wfm\_frame\_desc\_rx**](wfm__frame_8h.md#function-dp_wfm_frame_desc_rx), or a bad parameter); may be `NULL`. 



**Returns:**

the demodulator, or NULL with `why` set.



```C++
const uint8_t    sb[3] = { 1, 0, 1 }, dcode[4] = { 1, 0, 1, 1 };
const wfm_seq_t  sync  = { .kind = WFM_SEQ_LITERAL, .bits = sb,
                           .len = 3 };
const wfm_seq_t  data  = { .kind = WFM_SEQ_DATA, .len = 8 };
wfm_frame_desc_t f;
dp_wfm_frame_fixed (&f, NULL, 0, &sync, &data, 1); // sync|data:8|crc16
const char *why = NULL;
dp_burst_demod_state_t *d = dp_burst_demod_create_desc (
    dcode, 4, &f, 4, 1e6, 0.0, 0.0, 10, &why);
if (!d || dp_burst_demod_llrs_max_out (d, 1) != 3 + 8 + 16)
  return 1;
dp_burst_demod_destroy (d);
```
 


        

<hr>



### function dp\_burst\_demod\_create\_frame 

_The Python binding's constructor:_ [_**dp\_burst\_demod\_create\_desc**_](burst__demod__core_8h.md#function-dp_burst_demod_create_desc) _without the_`why` _out-parameter._
```C++
dp_burst_demod_state_t * dp_burst_demod_create_frame (
    const uint8_t * data_code,
    size_t data_code_len,
    const wfm_frame_desc_t * frame,
    size_t spc,
    double chip_rate,
    double carrier_hz,
    double max_rate,
    size_t est_segments
) 
```



An object's generated constructor has no channel for a reason, so a refused description surfaces as the manifest's `create_error_message`, which names the rules. C callers that want the reason call the `_desc` form.




**Parameters:**


* `data_code` the data spreading code, 0/1 chips. 
* `data_code_len` its length (the spreading factor). 
* `frame` the description (`const  wfm_frame_desc_t *`). 
* `spc` samples per chip. 
* `chip_rate` chips per second. 
* `carrier_hz` the carrier the baseband is offset by, Hz. 
* `max_rate` the Doppler rate searched, cycles/sample^2. 
* `est_segments` partials per acquisition period for the estimate. 



**Returns:**

the demodulator, or NULL.



```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDemod
>>> from doppler.wfm import Frame
>>> spc, acq_sf, reps, data_sf = 4, 500, 5, 50
>>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
>>> acode = ((np.arange(acq_sf) * 2654435761 >> 13) & 1).astype(
...     np.uint8)
>>> dcode = ((np.arange(data_sf) * 40503 >> 7) & 1).astype(np.uint8)
>>> payload = ((np.arange(64) * 7 + 3) & 1).astype(np.uint8)
>>> desc = Frame(sync=sync, payload=payload, crc="crc16")
>>> frame = desc.bits()    # sync | payload | CRC-16: ONE description
>>> csign = lambda b: np.where(np.asarray(b) & 1, -1.0, 1.0)
>>> chips = ([np.tile(csign(acode), reps)]
...          + [csign(b) * csign(dcode) for b in frame])
>>> bb = np.repeat(np.concatenate(chips), spc).astype(np.complex64)
>>> n = np.arange(len(bb))
>>> f0 = 0.012
>>> x = (bb * np.exp(2j * np.pi * f0 * n)).astype(np.complex64)
>>> d = BurstDemod(dcode, desc, spc=spc, chip_rate=1e6)
>>> d.set_preamble(acode, reps)   # unmodulated (f0, rate) preamble
>>> d.set_prior(f0, 0)            # coarse Doppler + preamble start
>>> bits = d.demod(x)      # estimate -> dechirp -> despread -> slice
>>> bool(np.array_equal(bits, frame))   # the FRAME, not the payload
True
```
 


        

<hr>



### function dp\_burst\_demod\_demod 

_Demodulate one burst end to end and write the frame's bits._ 
```C++
size_t dp_burst_demod_demod (
    dp_burst_demod_state_t * state,
    const float _Complex * x,
    size_t x_len,
    uint8_t * out,
    size_t max_out
) 
```



Runs the whole feedforward chain on the supplied samples: estimate the (frequency, chirp-rate) from the preamble, dechirp, despread the data section to soft symbols, sync-align and derotate, and slice `frame_syms` symbols to bits. It writes the frame as received — sync word first — and makes no claim about what those bits are for: undoing the frame needs a description, and that is a caller's, not this object's. The soft twin of the same decisions is [**dp\_burst\_demod\_llrs()**](burst__demod__core_8h.md#function-dp_burst_demod_llrs).


On return the read-back fields report the outcome — `frame_offset`, `n_symbols`, and the `est_freq_hz` / `est_rate_hz` / `est_cn0_dbhz` / `est_timing_chips` estimates. The templates and prior must already be set via set\_preamble() and set\_prior().


The C function returns the number of bits written; the Python binding returns those bits as an array (a view into a reused buffer unless an `out` buffer is supplied).




**Parameters:**


* `state` Demodulator handle. 
* `x` Burst samples (complex baseband at spc\*chip\_rate). 
* `x_len` Number of input samples. 
* `out` Caller-provided output buffer for the frame's bits. 
* `max_out` Capacity of `out`, in bits. 



**Returns:**

Number of frame bits written (0 on failure / too-short burst). 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDemod
>>> spc, acq_sf, reps, data_sf = 4, 500, 5, 50
>>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
>>> acode = ((np.arange(acq_sf) * 2654435761 >> 13) & 1).astype(
...     np.uint8)
>>> dcode = ((np.arange(data_sf) * 40503 >> 7) & 1).astype(np.uint8)
>>> payload = ((np.arange(64) * 7 + 3) & 1).astype(np.uint8)
>>> def crc16(bits):
...     c = 0xFFFF
...     for b in bits:
...         c ^= (int(b) & 1) << 15
...         c = (((c << 1) ^ 0x1021) & 0xFFFF
...              if c & 0x8000 else (c << 1) & 0xFFFF)
...     return c
>>> crc = crc16(payload)
>>> crc_bits = np.array(
...     [(crc >> (15 - j)) & 1 for j in range(16)], np.uint8)
>>> frame = np.concatenate([sync, payload, crc_bits])
>>> csign = lambda b: np.where(np.asarray(b) & 1, -1.0, 1.0)
>>> chips = ([np.tile(csign(acode), reps)]
...          + [csign(b) * csign(dcode) for b in frame])
>>> bb = np.repeat(np.concatenate(chips), spc).astype(np.complex64)
>>> n = np.arange(len(bb))
>>> f0 = 0.012
>>> x = (bb * np.exp(2j * np.pi * f0 * n)).astype(np.complex64)
>>> from doppler.wfm import Frame
>>> desc = Frame(sync=sync, payload=np.zeros(64, np.uint8), crc="crc16")
>>> d = BurstDemod(dcode, desc, spc=spc, chip_rate=1e6)
>>> d.set_preamble(acode, reps)
>>> d.set_prior(f0, 0)
>>> bits = d.demod(x)
>>> bool(np.array_equal(bits, frame))     # sync | payload | CRC, as sent
True
>>> from doppler.wfm import crc16
>>> int(crc16(bits[13:77])) == crc        # the CHECK is the caller's
True
```
 





        

<hr>



### function dp\_burst\_demod\_demod\_max\_out 

_Max output bits = frame\_syms (caller sizes the buffer)._ 
```C++
size_t dp_burst_demod_demod_max_out (
    dp_burst_demod_state_t * state
) 
```




<hr>



### function dp\_burst\_demod\_destroy 

_Destroy a demodulator._ 
```C++
void dp_burst_demod_destroy (
    dp_burst_demod_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_burst\_demod\_llrs 

_LLRs the last demod() wrote — the frame's soft bits._ 
```C++
size_t dp_burst_demod_llrs (
    dp_burst_demod_state_t * state,
    size_t n,
    float * out,
    size_t max_out
) 
```



`crealf(sym * derot)` IS the log-likelihood ratio up to a scale, and it was computed, sliced to one bit and freed on every burst. A hard decision throws away roughly 2 dB of the coding gain a soft-input decoder exists to deliver (`mpsk_soft_demap`'s own docstring), so this is what makes a coded burst worth coding.


**The convention is not a new one**: `mpsk_soft_demap`'s, which is `mpsk_demap`'s decision rule seen a second way. Positive means bit 0, so `L < 0` reproduces exactly the bits demod() returned — asserted in the tests rather than assumed.


Spans the WHOLE frame, not just the payload, because a code covers what its description says it covers and a decoder needs the bits the code protects. The payload's own span is `field_off`/`field_bits` of the layout.


Scaled by `est_n0` rather than left raw: a Viterbi is invariant to a positive scale, but LLRs from different bursts are not comparable without one, and combining across bursts needs them to be.




**Parameters:**


* `state` Demodulator handle. 
* `n` Ignored — the count is the last demod()'s frame. 
* `out` Receives the LLRs, one per frame bit. 
* `max_out` Capacity of `out`; see [**dp\_burst\_demod\_llrs\_max\_out()**](burst__demod__core_8h.md#function-dp_burst_demod_llrs_max_out). 



**Returns:**

LLRs written — `min(frame bits, max_out)`, or 0 if the last demod() produced no frame. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDemod
>>> dcode = (np.arange(50) & 1).astype(np.uint8)
>>> from doppler.wfm import Frame
>>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
>>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
>>> d.llrs_max_out(1)          # one per frame symbol
93
```
 





        

<hr>



### function dp\_burst\_demod\_llrs\_max\_out 

_Max LLRs_ [_**dp\_burst\_demod\_llrs()**_](burst__demod__core_8h.md#function-dp_burst_demod_llrs) _writes: the frame's length in bits._
```C++
size_t dp_burst_demod_llrs_max_out (
    dp_burst_demod_state_t * state,
    size_t n
) 
```





**Parameters:**


* `state` Demodulator handle. 
* `n` Ignored — the count is the last demod()'s frame. 




        

<hr>



### function dp\_burst\_demod\_reset 

_Clear the per-burst read-backs, leaving the configuration intact._ 
```C++
void dp_burst_demod_reset (
    dp_burst_demod_state_t * state
) 
```



Zeros the after-demod fields (`frame_offset`, `n_symbols`, and the `est_*` estimates) so a stale result cannot be mistaken for a fresh one. The spreading codes, sync word, and prior set up before the first burst are preserved, so the object is immediately ready to demodulate the next burst.




**Parameters:**


* `state` Demodulator handle. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDemod
>>> dcode = (np.arange(50) & 1).astype(np.uint8)
>>> from doppler.wfm import Frame
>>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
>>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
>>> d.reset()          # clears the estimates, keeps the config
>>> d.frame_offset
0
```
 




        

<hr>



### function dp\_burst\_demod\_set\_preamble 

_Register the unmodulated acquisition preamble code and its repetition count used for the feedforward (f0, rate) estimate._ 
```C++
void dp_burst_demod_set_preamble (
    dp_burst_demod_state_t * state,
    const uint8_t * acq_code,
    size_t acq_code_len,
    size_t reps
) 
```



The preamble is the acq spreading code transmitted `reps` times with no data modulation; demod() segment-despreads it into partial correlations and feeds those to the polynomial-phase estimator to recover the coarse (frequency, chirp-rate). Call once after construction; the code is copied.




**Parameters:**


* `state` Demodulator handle. 
* `acq_code` Acq preamble spreading code, one 0/1 chip per element; copied into the object. 
* `acq_code_len` Acq code length (chips); the length of `acq_code`. 
* `reps` Number of preamble repetitions in the burst. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDemod
>>> dcode = (np.arange(50) & 1).astype(np.uint8)
>>> from doppler.wfm import Frame
>>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
>>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
>>> acode = (np.arange(500) & 1).astype(np.uint8)  # unmodulated
>>> d.set_preamble(acode, reps=5)  # 5 reps drive the (f0, rate) fit
```
 




        

<hr>



### function dp\_burst\_demod\_set\_prior 

_Seed the demodulator from acquisition with the coarse Doppler and the preamble start sample._ 
```C++
void dp_burst_demod_set_prior (
    dp_burst_demod_state_t * state,
    double f0_coarse,
    size_t start
) 
```



These come from the upstream acquisition stage: `f0_coarse` centres the feedforward frequency search near the true Doppler, and `start` tells demod() where the preamble begins within the burst so it despreads the right samples. Call once per burst before demod().




**Parameters:**


* `state` Demodulator handle. 
* `f0_coarse` Coarse Doppler prior (cycles/sample at the input rate). 
* `start` Preamble start sample index within the burst. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDemod
>>> dcode = (np.arange(50) & 1).astype(np.uint8)
>>> from doppler.wfm import Frame
>>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
>>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
>>> d.set_prior(0.012, start=0)   # coarse Doppler + start, from acq
```
 




        

<hr>



### function dp\_burst\_demod\_symbols 

_The last demod()'s DEROTATED complex symbols — the constellation the LLRs are the real part of._ 
```C++
size_t dp_burst_demod_symbols (
    dp_burst_demod_state_t * state,
    size_t n,
    float _Complex * out,
    size_t max_out
) 
```



Same span and same normalisation as [**dp\_burst\_demod\_llrs()**](burst__demod__core_8h.md#function-dp_burst_demod_llrs): the whole frame, scaled to unit mean-\|Re\| by the burst's own estimate, so `crealf(symbols[k])` is that bit's LLR up to `est_n0`.


The quadrature is why this exists. After derotation the real axis carries the signal and the imaginary axis carries noise alone, so Q is diagnostic: a residual phase error scales Re by `cos(phi)` WITHOUT adding noise, which makes it indistinguishable from a genuine amplitude or SNR loss in mean \|LLR\|, in LLR spread and in BER alike. Measured over 20000 BPSK symbols, a 30 degree phase error and an amplitude loss of `cos(30 deg)` agreed to three decimals in all three, and differed only in Q/I energy — 0.386 against 0.077 (doppler#1087). That is the difference between a pointing problem and a link-budget one, on a burst this object already characterised well enough to know.




**Parameters:**


* `state` Demodulator handle. 
* `n` Ignored — the count is the last demod()'s frame. 
* `out` Receives the symbols, one per frame bit. 
* `max_out` Capacity of `out`; see [**dp\_burst\_demod\_symbols\_max\_out()**](burst__demod__core_8h.md#function-dp_burst_demod_symbols_max_out). 



**Returns:**

Symbols written — `min(frame bits, max_out)`, or 0 if the last demod() produced no frame. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDemod
>>> dcode = (np.arange(50) & 1).astype(np.uint8)
>>> from doppler.wfm import Frame
>>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
>>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
>>> d.symbols_max_out(1)       # one per frame symbol, as llrs()
93
```
 





        

<hr>



### function dp\_burst\_demod\_symbols\_max\_out 

_Max symbols_ [_**dp\_burst\_demod\_symbols()**_](burst__demod__core_8h.md#function-dp_burst_demod_symbols) _writes: the frame's length._
```C++
size_t dp_burst_demod_symbols_max_out (
    dp_burst_demod_state_t * state,
    size_t n
) 
```





**Parameters:**


* `state` Demodulator handle. 
* `n` Ignored — the count is the last demod()'s frame. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/burst_demod/burst_demod_core.h`

