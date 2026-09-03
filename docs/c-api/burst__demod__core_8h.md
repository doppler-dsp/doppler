

# File burst\_demod\_core.h



[**FileList**](files.md) **>** [**burst\_demod**](dir_96a22b0098c79a5049df57065c5b8df4.md) **>** [**burst\_demod\_core.h**](burst__demod__core_8h.md)

[Go to the source code of this file](burst__demod__core_8h_source.md)

_Feedforward BPSK DSSS frame demodulator._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
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
|  [**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* | [**burst\_demod\_create**](#function-burst_demod_create) (const uint8\_t \* data\_code, size\_t data\_code\_len, size\_t spc, double chip\_rate, double carrier\_hz, double max\_rate, size\_t frame\_syms, size\_t est\_segments) <br>_Create a feedforward BPSK DSSS burst demodulator._  |
|  size\_t | [**burst\_demod\_demod**](#function-burst_demod_demod) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, const float \_Complex \* x, size\_t x\_len, uint8\_t \* out, size\_t max\_out) <br>_Demodulate one burst end to end and write the frame's bits._  |
|  size\_t | [**burst\_demod\_demod\_max\_out**](#function-burst_demod_demod_max_out) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state) <br>_Max output bits = frame\_syms (caller sizes the buffer)._  |
|  void | [**burst\_demod\_destroy**](#function-burst_demod_destroy) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state) <br>_Destroy a demodulator._  |
|  size\_t | [**burst\_demod\_llrs**](#function-burst_demod_llrs) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, size\_t n, float \* out, size\_t max\_out) <br>_LLRs the last demod() wrote — the frame's soft bits._  |
|  size\_t | [**burst\_demod\_llrs\_max\_out**](#function-burst_demod_llrs_max_out) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, size\_t n) <br>_Max LLRs_ [_**burst\_demod\_llrs()**_](burst__demod__core_8h.md#function-burst_demod_llrs) _writes: the frame's length in bits._ |
|  void | [**burst\_demod\_reset**](#function-burst_demod_reset) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state) <br>_Clear the per-burst read-backs, leaving the configuration intact._  |
|  void | [**burst\_demod\_set\_preamble**](#function-burst_demod_set_preamble) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, const uint8\_t \* acq\_code, size\_t acq\_code\_len, size\_t reps) <br>_Register the unmodulated acquisition preamble code and its repetition count used for the feedforward (f0, rate) estimate._  |
|  void | [**burst\_demod\_set\_prior**](#function-burst_demod_set_prior) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, double f0\_coarse, size\_t start) <br>_Seed the demodulator from acquisition with the coarse Doppler and the preamble start sample._  |
|  void | [**burst\_demod\_set\_sync**](#function-burst_demod_set_sync) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, const uint8\_t \* sync, size\_t sync\_len) <br>_Register the known frame-sync word used for frame alignment and phase/sign resolution._  |
|  size\_t | [**burst\_demod\_symbols**](#function-burst_demod_symbols) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, size\_t n, float \_Complex \* out, size\_t max\_out) <br>_The last demod()'s DEROTATED complex symbols — the constellation the LLRs are the real part of._  |
|  size\_t | [**burst\_demod\_symbols\_max\_out**](#function-burst_demod_symbols_max_out) ([**burst\_demod\_state\_t**](structburst__demod__state__t.md) \* state, size\_t n) <br>_Max symbols_ [_**burst\_demod\_symbols()**_](burst__demod__core_8h.md#function-burst_demod_symbols) _writes: the frame's length._ |




























## Detailed Description


The whole post-acquisition payload chain, in C, with no tracking loops:
* preamble estimate — segment-despread the unmodulated, repeated acq preamble into partial correlations and feed them to ppe, giving a coarse (frequency, chirp-rate);
* sample-rate dechirp by (f0, rate) — removes Doppler AND Doppler rate;
* despread the data section with the (short) data code -&gt; soft BPSK symbols;
* frame sync — correlate the symbols against the known sync word; the complex peak gives the frame offset and the residual phase (derotated);
* slice `frame_syms` symbols to bits, hard and soft, and STOP.




### Where this object's job ends



At a decision. It hands back one bit per symbol (demod()) and one LLR per symbol ([**burst\_demod\_llrs()**](burst__demod__core_8h.md#function-burst_demod_llrs)), and it does not know what any of them mean: which are payload, which are a check, what an outer code would repair are all questions about a FRAME, and answering them needs a description this object deliberately does not hold (doppler#1022). It used to hold half of one — a hard-coded `sync | payload | CRC-16` — which is how a burst sent without a trailer came to be reported invalid.


What it does need is the sync word, to find the frame and resolve the BPSK sign, and `frame_syms`, to know how many symbols to slice. Both are physical-layer facts.


Seed from acquisition with set\_prior(coarse Doppler, preamble start), set\_preamble(acq code, reps) and set\_sync(sync word), then demod(burst). One `max_rate` knob spans near-static Doppler (0) to severe LEO chirp. One-shot per burst. Composes ppe (which composes fft + spectral).



```C++
burst_demod_state_t *d = burst_demod_create(dcode, 50, 4, 1e6, 0, 0, 256, 10);
burst_demod_set_preamble(d, acode, 500, 5);
burst_demod_set_sync(d, sync, 31);
burst_demod_set_prior(d, f0_coarse, preamble_start);
size_t nbits = burst_demod_demod(d, x, n, bits, 256);   // frame bits out
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
    size_t frame_syms,
    size_t est_segments
) 
```



Recovers the payload of a single spread burst end to end, with no tracking loops: it estimates the burst's Doppler (and Doppler rate) from the unmodulated acquisition preamble, dechirps by that estimate, despreads the data section into soft symbols, aligns on the known sync word, slices to bits, and checks the CRC-16 trailer. One `max_rate` knob spans the whole range from near-static Doppler (0) to a severe LEO chirp.


After construction, register the templates and the acquisition seed — set\_preamble(), set\_sync(), set\_prior() — then call demod() once per burst.




**Parameters:**


* `data_code` Data spreading code, one 0/1 chip per element; copied into the object (its length is the data spreading factor, chips/symbol). 
* `data_code_len` Data spreading factor (chips/symbol); the length of `data_code`. 
* `spc` Samples per chip (front-end oversample). 
* `chip_rate` Chip rate (Hz); sets the sample rate as spc\*chip\_rate. 
* `carrier_hz` RF carrier (Hz) for code-Doppler scaling; 0 = ignore. 
* `max_rate` Chirp-rate search half-span (cycles/sample^2 at the input rate); 0 = Doppler only (no rate search). 
* `frame_syms` Symbols the frame occupies after the sync word — how many bits demod() hands back per burst. What they mean is a frame description's business. 
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
>>> d = BurstDemod(dcode, spc=spc, chip_rate=1e6, frame_syms=len(frame))
>>> d.set_preamble(acode, reps)   # unmodulated (f0, rate) preamble
>>> d.set_sync(sync)              # Barker-13: frame align + sign fix
>>> d.set_prior(f0, 0)            # coarse Doppler + preamble start
>>> bits = d.demod(x)      # estimate -> dechirp -> despread -> slice
>>> bool(np.array_equal(bits, frame))   # the FRAME, not the payload
True
```
 




        

<hr>



### function burst\_demod\_demod 

_Demodulate one burst end to end and write the frame's bits._ 
```C++
size_t burst_demod_demod (
    burst_demod_state_t * state,
    const float _Complex * x,
    size_t x_len,
    uint8_t * out,
    size_t max_out
) 
```



Runs the whole feedforward chain on the supplied samples: estimate the (frequency, chirp-rate) from the preamble, dechirp, despread the data section to soft symbols, sync-align and derotate, and slice `frame_syms` symbols to bits. It writes the frame as received — sync word first — and makes no claim about what those bits are for: undoing the frame needs a description, and that is a caller's, not this object's. The soft twin of the same decisions is [**burst\_demod\_llrs()**](burst__demod__core_8h.md#function-burst_demod_llrs).


On return the read-back fields report the outcome — `frame_offset`, `n_symbols`, and the `est_freq_hz` / `est_rate_hz` / `est_snr_db` estimates. The templates and prior must already be set via set\_preamble(), set\_sync(), set\_prior().


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
>>> d = BurstDemod(dcode, spc=spc, chip_rate=1e6, frame_syms=93)
>>> d.set_preamble(acode, reps)
>>> d.set_sync(sync)
>>> d.set_prior(f0, 0)
>>> bits = d.demod(x)
>>> bool(np.array_equal(bits, frame))     # sync | payload | CRC, as sent
True
>>> from doppler.wfm import crc16
>>> int(crc16(bits[13:77])) == crc        # the CHECK is the caller's
True
```
 





        

<hr>



### function burst\_demod\_demod\_max\_out 

_Max output bits = frame\_syms (caller sizes the buffer)._ 
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
>>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)
>>> d.set_sync(np.zeros(13, dtype=np.uint8))
>>> d.llrs_max_out(1)          # one per frame symbol
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
* `n` Ignored — the count is the last demod()'s frame. 




        

<hr>



### function burst\_demod\_reset 

_Clear the per-burst read-backs, leaving the configuration intact._ 
```C++
void burst_demod_reset (
    burst_demod_state_t * state
) 
```



Zeros the after-demod fields (`frame_offset`, `n_symbols`, and the `est_*` estimates) so a stale result cannot be mistaken for a fresh one. The spreading codes, sync word, and prior set up before the first burst are preserved, so the object is immediately ready to demodulate the next burst.




**Parameters:**


* `state` Demodulator handle. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDemod
>>> dcode = (np.arange(50) & 1).astype(np.uint8)
>>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)
>>> d.reset()          # clears the estimates, keeps the config
>>> d.frame_offset
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
>>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)
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
>>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)
>>> d.set_prior(0.012, start=0)   # coarse Doppler + start, from acq
```
 




        

<hr>



### function burst\_demod\_set\_sync 

_Register the known frame-sync word used for frame alignment and phase/sign resolution._ 
```C++
void burst_demod_set_sync (
    burst_demod_state_t * state,
    const uint8_t * sync,
    size_t sync_len
) 
```



After the data section is despread to soft BPSK symbols, demod() correlates them against this word; the complex correlation peak locates the frame (its `frame_offset`) and its phase resolves the residual carrier rotation and the BPSK sign ambiguity before slicing. Pass the word as 0/1 symbols; it is copied and stored internally as +/-1.


This is the ONLY thing this object is told about the frame's content, and it is told it for a physical-layer reason: without the sign the slicer would be a coin toss. Everything else — where the payload sits, which stages cover what, whether a check passed — needs the frame's description and belongs one layer up (doppler#1022).




**Parameters:**


* `state` Demodulator handle. 
* `sync` Frame-sync word, one 0/1 symbol per element; copied. 
* `sync_len` Sync word length (symbols); the length of `sync`. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDemod
>>> dcode = (np.arange(50) & 1).astype(np.uint8)
>>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)
>>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
>>> d.set_sync(sync)   # Barker-13: frame align + phase/sign fix
```
 




        

<hr>



### function burst\_demod\_symbols 

_The last demod()'s DEROTATED complex symbols — the constellation the LLRs are the real part of._ 
```C++
size_t burst_demod_symbols (
    burst_demod_state_t * state,
    size_t n,
    float _Complex * out,
    size_t max_out
) 
```



Same span and same normalisation as [**burst\_demod\_llrs()**](burst__demod__core_8h.md#function-burst_demod_llrs): the whole frame, scaled to unit mean-\|Re\| by the burst's own estimate, so `crealf(symbols[k])` is that bit's LLR up to `est_n0`.


The quadrature is why this exists. After derotation the real axis carries the signal and the imaginary axis carries noise alone, so Q is diagnostic: a residual phase error scales Re by `cos(phi)` WITHOUT adding noise, which makes it indistinguishable from a genuine amplitude or SNR loss in mean \|LLR\|, in LLR spread and in BER alike. Measured over 20000 BPSK symbols, a 30 degree phase error and an amplitude loss of `cos(30 deg)` agreed to three decimals in all three, and differed only in Q/I energy — 0.386 against 0.077 (doppler#1087). That is the difference between a pointing problem and a link-budget one, on a burst this object already characterised well enough to know.




**Parameters:**


* `state` Demodulator handle. 
* `n` Ignored — the count is the last demod()'s frame. 
* `out` Receives the symbols, one per frame bit. 
* `max_out` Capacity of `out`; see [**burst\_demod\_symbols\_max\_out()**](burst__demod__core_8h.md#function-burst_demod_symbols_max_out). 



**Returns:**

Symbols written — `min(frame bits, max_out)`, or 0 if the last demod() produced no frame. 
```C++
>>> import numpy as np
>>> from doppler.dsss import BurstDemod
>>> dcode = (np.arange(50) & 1).astype(np.uint8)
>>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, frame_syms=93)
>>> d.set_sync(np.zeros(13, dtype=np.uint8))
>>> d.symbols_max_out(1)       # one per frame symbol, as llrs()
93
```
 





        

<hr>



### function burst\_demod\_symbols\_max\_out 

_Max symbols_ [_**burst\_demod\_symbols()**_](burst__demod__core_8h.md#function-burst_demod_symbols) _writes: the frame's length._
```C++
size_t burst_demod_symbols_max_out (
    burst_demod_state_t * state,
    size_t n
) 
```





**Parameters:**


* `state` Demodulator handle. 
* `n` Ignored — the count is the last demod()'s frame. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/burst_demod/burst_demod_core.h`

