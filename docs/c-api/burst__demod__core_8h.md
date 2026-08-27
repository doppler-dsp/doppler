

# File burst\_demod\_core.h



[**FileList**](files.md) **>** [**burst\_demod**](dir_96a22b0098c79a5049df57065c5b8df4.md) **>** [**burst\_demod\_core.h**](burst__demod__core_8h.md)

[Go to the source code of this file](burst__demod__core_8h_source.md)

_Feedforward BPSK DSSS frame demodulator._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "ccsds_tm/ccsds_tm_frame.h"`
* `#include "wfm/wfm_frame.h"`
* `#include "ppe/ppe_core.h"`
* `#include "fft/fft_core.h"`
* `#include "spectral/spectral_core.h"`
* `#include <complex.h>`
* `#include "conv/conv_core.h"`
* `#include "rs/rs_core.h"`
* `#include "pn/pn_core.h"`
* `#include "gold/gold_core.h"`
* `#include "mpsk/mpsk_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**burst\_demod\_state\_t**](structburst__demod__state__t.md) <br>_BurstDemod state. Allocate with_ [_**burst\_demod\_create()**_](burst__demod__core_8h.md#function-burst_demod_create) _._ |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* | [**burst\_demod\_create**](#function-burst_demod_create) (const uint8\_t \* data\_code, size\_t data\_code\_len, size\_t spc, double chip\_rate, double carrier\_hz, double max\_rate, size\_t payload\_len, size\_t est\_segments) <br>_Create a feedforward BPSK DSSS burst demodulator._  |
|  size\_t | [**burst\_demod\_demod**](#function-burst_demod_demod) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, const float complex \* x, size\_t x\_len, uint8\_t \* out, size\_t max\_out) <br>_Demodulate one burst end to end and write the payload bits._  |
|  size\_t | [**burst\_demod\_demod\_max\_out**](#function-burst_demod_demod_max_out) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state) <br>_Max output bits = payload\_len (caller sizes the buffer)._  |
|  void | [**burst\_demod\_destroy**](#function-burst_demod_destroy) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state) <br>_Destroy a demodulator._  |
|  size\_t | [**burst\_demod\_llrs**](#function-burst_demod_llrs) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, size\_t n, float \* out, size\_t max\_out) <br>_LLRs the last demod() wrote — the frame's soft bits._  |
|  size\_t | [**burst\_demod\_llrs\_max\_out**](#function-burst_demod_llrs_max_out) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, size\_t n) <br>_Max LLRs_ [_**burst\_demod\_llrs()**_](burst__demod__core_8h.md#function-burst_demod_llrs) _writes: the frame's length in bits._ |
|  void | [**burst\_demod\_reset**](#function-burst_demod_reset) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state) <br>_Clear the per-burst read-backs, leaving the configuration intact._  |
|  int | [**burst\_demod\_set\_frame**](#function-burst_demod_set_frame) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, const uint8\_t \* sync, size\_t sync\_len, int crc, unsigned rs\_depth, int randomise, int attach\_asm) <br>_Describe the frame this burst carries — fields, stages and all._  |
|  void | [**burst\_demod\_set\_preamble**](#function-burst_demod_set_preamble) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, const uint8\_t \* acq\_code, size\_t acq\_code\_len, size\_t reps) <br>_Register the unmodulated acquisition preamble code and its repetition count used for the feedforward (f0, rate) estimate._  |
|  void | [**burst\_demod\_set\_prior**](#function-burst_demod_set_prior) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, double f0\_coarse, size\_t start) <br>_Seed the demodulator from acquisition with the coarse Doppler and the preamble start sample._  |




























## Detailed Description


The whole post-acquisition payload chain, in C, with no tracking loops:
* preamble estimate — segment-despread the unmodulated, repeated acq preamble into partial correlations and feed them to ppe, giving a coarse (frequency, chirp-rate);
* sample-rate dechirp by (f0, rate) — removes Doppler AND Doppler rate;
* despread the data section with the (short) data code -&gt; soft BPSK symbols;
* frame sync — correlate the symbols against the known sync word; the complex peak gives the frame offset and the residual phase (derotated);
* slice the frame to bits and undo its stages -&gt; `frame_valid`; the payload is read from the span the description gives it.




Seed from acquisition with set\_prior(coarse Doppler, preamble start), set\_preamble(acq code, reps) and set\_frame(the frame's shape), then demod(burst). One `max_rate` knob spans near-static Doppler (0) to severe LEO chirp. One-shot per burst. Composes ppe (which composes fft + spectral).



```C++
burst_demod_state_t *d = burst_demod_create(dcode, 50, 4, 1e6, 0, 0, 256, 10);
burst_demod_set_preamble(d, acode, 500, 5);
burst_demod_set_frame(d, sync, 31, 1, 0, 0, 0);
burst_demod_set_prior(d, f0_coarse, preamble_start);
size_t nbits = burst_demod_demod(d, x, n, bits, 256);   // d->frame_valid ...
```
 


    
## Public Functions Documentation




### function burst\_demod\_create 

_Create a feedforward BPSK DSSS burst demodulator._ 
```C++
burst_demod_state_t * burst_demod_create (
    const uint8_t * data_code,
    size_t data_code_len,
    size_t spc,
    double chip_rate,
    double carrier_hz,
    double max_rate,
    size_t payload_len,
    size_t est_segments
) 
```



Recovers the payload of a single spread burst end to end, with no tracking loops: it estimates the burst's Doppler (and Doppler rate) from the unmodulated acquisition preamble, dechirps by that estimate, despreads the data section into soft symbols, aligns on the known sync word, slices to bits, and checks the CRC-16 trailer. One `max_rate` knob spans the whole range from near-static Doppler (0) to a severe LEO chirp.


After construction, register the templates and the acquisition seed — set\_preamble(), set\_frame(), set\_prior() — then call demod() once per burst.




**Parameters:**


* `data_code` Data spreading code, one 0/1 chip per element; copied into the object (its length is the data spreading factor, chips/symbol). 
* `data_code_len` Data spreading factor (chips/symbol); the length of `data_code`. 
* `spc` Samples per chip (front-end oversample). 
* `chip_rate` Chip rate (Hz); sets the sample rate as spc\*chip\_rate. 
* `carrier_hz` RF carrier (Hz) for code-Doppler scaling; 0 = ignore. 
* `max_rate` Chirp-rate search half-span (cycles/sample^2 at the input rate); 0 = Doppler only (no rate search). 
* `payload_len` Number of payload data symbols (bits) in a frame. 
* `est_segments` Partial correlations per acq period (segmentation for the feedforward estimate; larger tolerates more rate). 
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
>>> d = BurstDemod(dcode, spc=spc, chip_rate=1e6, payload_len=64)
>>> d.set_preamble(acode, reps)   # unmodulated (f0, rate) preamble
>>> d.set_frame(sync)             # Barker-13 sync, CRC-16 trailer
0
>>> d.set_prior(f0, 0)           # coarse Doppler + preamble start
>>> bits = d.demod(x)      # estimate -> dechirp -> despread -> slice
>>> int(d.frame_valid), bool(np.array_equal(bits, payload))
(1, True)
```
 




        

<hr>



### function burst\_demod\_demod 

_Demodulate one burst end to end and write the payload bits._ 
```C++
size_t burst_demod_demod (
    burst_demod_state_t * state,
    const float complex * x,
    size_t x_len,
    uint8_t * out,
    size_t max_out
) 
```



Runs the whole feedforward chain on the supplied samples: estimate the (frequency, chirp-rate) from the preamble, dechirp, despread the data section to soft symbols, sync-align and derotate, slice to bits, and check the CRC-16 trailer. On return the read-back fields report the outcome — `frame_valid` (CRC match), `frame_offset`, `n_symbols`, and the `est_freq_hz` / `est_rate_hz` / `est_snr_db` estimates. The templates and prior must already be set via set\_preamble(), set\_frame(), set\_prior().


The C function returns the number of bits written; the Python binding returns those bits as an array (a view into a reused buffer unless an `out` buffer is supplied).




**Parameters:**


* `state` Demodulator handle. 
* `x` Burst samples (complex baseband at spc\*chip\_rate). 
* `x_len` Number of input samples. 
* `out` Caller-provided output buffer for the payload bits. 
* `max_out` Capacity of `out`, in bits. 



**Returns:**

Number of payload bits written (0 on failure / too-short burst). 
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
>>> d = BurstDemod(dcode, spc=spc, chip_rate=1e6, payload_len=64)
>>> d.set_preamble(acode, reps)
>>> d.set_frame(sync)
0
>>> d.set_prior(f0, 0)
>>> bits = d.demod(x)
>>> int(d.frame_valid), bool(np.array_equal(bits, payload))
(1, True)
```
 





        

<hr>



### function burst\_demod\_demod\_max\_out 

_Max output bits = payload\_len (caller sizes the buffer)._ 
```C++
size_t burst_demod_demod_max_out (
    burst_demod_state_t * state
) 
```




<hr>



### function burst\_demod\_destroy 

_Destroy a demodulator._ 
```C++
void burst_demod_destroy (
    burst_demod_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function burst\_demod\_llrs 

_LLRs the last demod() wrote — the frame's soft bits._ 
```C++
size_t burst_demod_llrs (
    burst_demod_state_t * state,
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
* `max_out` Capacity of `out`; see [**burst\_demod\_llrs\_max\_out()**](burst__demod__core_8h.md#function-burst_demod_llrs_max_out). 



**Returns:**

LLRs written — `min(frame bits, max_out)`, or 0 if the last demod() produced no frame. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDemod
>>> dcode = (np.arange(50) & 1).astype(np.uint8)
>>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, payload_len=64)
>>> d.set_frame(np.zeros(13, dtype=np.uint8))
0
>>> d.llrs_max_out(1)          # sync + payload + the CRC trailer
93
```
 





        

<hr>



### function burst\_demod\_llrs\_max\_out 

_Max LLRs_ [_**burst\_demod\_llrs()**_](burst__demod__core_8h.md#function-burst_demod_llrs) _writes: the frame's length in bits._
```C++
size_t burst_demod_llrs_max_out (
    burst_demod_state_t * state,
    size_t n
) 
```





**Parameters:**


* `state` Demodulator handle. 




        

<hr>



### function burst\_demod\_reset 

_Clear the per-burst read-backs, leaving the configuration intact._ 
```C++
void burst_demod_reset (
    burst_demod_state_t * state
) 
```



Zeros the after-demod fields (`frame_valid`, `frame_offset`, `n_symbols`, and the `est_*` estimates) so a stale result cannot be mistaken for a fresh one. The spreading codes, sync word, and prior set up before the first burst are preserved, so the object is immediately ready to demodulate the next burst.




**Parameters:**


* `state` Demodulator handle. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDemod
>>> dcode = (np.arange(50) & 1).astype(np.uint8)
>>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, payload_len=64)
>>> d.reset()          # clears est_ + frame_valid, keeps config
>>> d.frame_valid
0
```
 




        

<hr>



### function burst\_demod\_set\_frame 

_Describe the frame this burst carries — fields, stages and all._ 
```C++
int burst_demod_set_frame (
    burst_demod_state_t * state,
    const uint8_t * sync,
    size_t sync_len,
    int crc,
    unsigned rs_depth,
    int randomise,
    int attach_asm
) 
```



Replaces `set_sync()`, and the difference is the point: a sync word is one FIELD of a frame, and this demodulator used to hard-code the rest of it as `sync | payload | CRC-16`. A burst generated without a CRC decoded bit-exactly and was reported INVALID; one carrying an outer code could not be described at all. The description built here is the same `wfm_frame_desc_t` the generator assembles from ([**wfm\_frame\_desc\_of**](wfm__frame_8h.md#function-wfm_frame_desc_of)), so the two ends cannot disagree about the frame's length, its field order, or which stage covers what.


What changes for a caller:



* **The frame's length is the layout's**, so a frame with no CRC is 16 bits shorter here rather than 16 bits of noise the receiver insisted on;
* \*\*`frame_valid` means "every check that RAN came out good"\*\*, and `frame_checked` says how many did. A frame carrying no check reports 0 and 0 — different from a failed one;
* **an outer code REPAIRS before the payload is read**, because `wfm_frame_check()` corrects in place over the span its own description gave it.




The correlation template becomes the frame's leading literal group: the marker (when `attach_asm`) then the sync word. Those are the fields a receiver FINDS rather than decodes, and correlating over both is free gain when a marker is present.


**The inner code is not accepted here.** A convolutional stage covers everything including the sync word, so its bits are coded ON THE WIRE and a hard-decision correlator cannot find the frame at all; undoing it needs the soft symbols this object currently discards (doppler#1018). There is no flag for it rather than a flag that silently does nothing.




**Parameters:**


* `state` Demodulator handle. 
* `sync` Frame-sync word, one 0/1 symbol per element; copied. May be NULL when the frame carries a marker instead. 
* `sync_len` Sync word length (symbols). 
* `crc` Non-zero: a CRC-16 trailer follows the payload. 
* `rs_depth` Outer-code interleaving depth; 0 = no outer code. 
* `randomise` Randomiser generator (0 = off), as the generator's own `randomise` field spells it. 
* `attach_asm` Non-zero: the frame opens with the CCSDS ASM. 



**Returns:**

0, or -1 if the geometry is refused or an allocation failed. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDemod
>>> dcode = (np.arange(50) & 1).astype(np.uint8)
>>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, payload_len=64)
>>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
>>> d.set_frame(sync)              # Barker-13, CRC-16 trailer
0
>>> d.set_frame(sync, crc=0)       # ...or no trailer at all
0
```
 





        

<hr>



### function burst\_demod\_set\_preamble 

_Register the unmodulated acquisition preamble code and its repetition count used for the feedforward (f0, rate) estimate._ 
```C++
void burst_demod_set_preamble (
    burst_demod_state_t * state,
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
>>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, payload_len=64)
>>> acode = (np.arange(500) & 1).astype(np.uint8)  # unmodulated
>>> d.set_preamble(acode, reps=5)  # 5 reps drive the (f0, rate) fit
```
 




        

<hr>



### function burst\_demod\_set\_prior 

_Seed the demodulator from acquisition with the coarse Doppler and the preamble start sample._ 
```C++
void burst_demod_set_prior (
    burst_demod_state_t * state,
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
>>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, payload_len=64)
>>> d.set_prior(0.012, start=0)   # coarse Doppler + start, from acq
```
 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/burst_demod/burst_demod_core.h`

