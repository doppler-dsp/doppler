

# Struct wfm\_source\_t



[**ClassList**](annotated.md) **>** [**wfm\_source\_t**](structwfm__source__t.md)



_One additive source within a segment: a_ `synth` _config + its level._[More...](#detailed-description)

* `#include <wfm_compose.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**acq\_code**](#variable-acq_code)  <br> |
|  size\_t | [**acq\_reps**](#variable-acq_reps)  <br> |
|  int | [**background**](#variable-background)  <br> |
|  double | [**carrier\_hz**](#variable-carrier_hz)  <br> |
|  size\_t | [**code\_only\_symbols**](#variable-code_only_symbols)  <br> |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**data**](#variable-data)  <br> |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**data\_code**](#variable-data_code)  <br> |
|  const char \* | [**data\_from\_file**](#variable-data_from_file)  <br> |
|  size\_t | [**data\_len**](#variable-data_len)  <br> |
|  [**wfm\_data\_stats\_t**](structwfm__data__stats__t.md) | [**data\_sent**](#variable-data_sent)  <br> |
|  double | [**doppler**](#variable-doppler)  <br> |
|  double | [**doppler\_hi**](#variable-doppler_hi)  <br> |
|  int | [**doppler\_lifetime**](#variable-doppler_lifetime)  <br> |
|  double | [**doppler\_rate**](#variable-doppler_rate)  <br> |
|  double | [**doppler\_rate\_hi**](#variable-doppler_rate_hi)  <br> |
|  int | [**dsss\_code\_only**](#variable-dsss_code_only)  <br> |
|  double | [**f\_end**](#variable-f_end)  <br> |
|  double | [**f\_end\_hi**](#variable-f_end_hi)  <br> |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**fill**](#variable-fill)  <br> |
|  const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* | [**frame**](#variable-frame)  <br> |
|  size\_t | [**frame\_symbols**](#variable-frame_symbols)  <br> |
|  double | [**freq**](#variable-freq)  <br> |
|  double | [**freq\_hi**](#variable-freq_hi)  <br> |
|  double | [**level**](#variable-level)  <br> |
|  double | [**level\_hi**](#variable-level_hi)  <br> |
|  int | [**lfsr**](#variable-lfsr)  <br> |
|  int | [**modulation**](#variable-modulation)  <br> |
|  size\_t | [**n\_symbols**](#variable-n_symbols)  <br> |
|  int | [**pn\_length**](#variable-pn_length)  <br> |
|  uint64\_t | [**pn\_poly**](#variable-pn_poly)  <br> |
|  int | [**pulse**](#variable-pulse)  <br> |
|  unsigned | [**ranged**](#variable-ranged)  <br> |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**retired\_bits**](#variable-retired_bits)  <br> |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**retired\_crc**](#variable-retired_crc)  <br> |
|  [**wfm\_seq\_t**](structwfm__seq__t.md) | [**retired\_sync**](#variable-retired_sync)  <br> |
|  double | [**rrc\_beta**](#variable-rrc_beta)  <br> |
|  int | [**rrc\_span**](#variable-rrc_span)  <br> |
|  uint32\_t | [**seed**](#variable-seed)  <br> |
|  double | [**snr**](#variable-snr)  <br> |
|  double | [**snr\_hi**](#variable-snr_hi)  <br> |
|  int | [**snr\_mode**](#variable-snr_mode)  <br> |
|  size\_t | [**span**](#variable-span)  <br> |
|  int | [**sps**](#variable-sps)  <br> |
|  double | [**symbol\_rate**](#variable-symbol_rate)  <br> |
|  float \_Complex \* | [**symbols**](#variable-symbols)  <br> |
|  int | [**type**](#variable-type)  <br> |












































## Detailed Description


The nine synth fields mirror `dp_wfm_synth_create()` (minus `fs`, which is the segment's — one receiver, one sample rate). `level` is the source's average power in dBFS (≤0); the segment sums its sources, each scaled by `10^(level/20)`.


Any of `freq`/`snr`/`level`/`f_end` may be a per-repeat uniform draw: set the matching `WFM_RANGE_*` bit in `ranged`, leave the scalar as `lo`, and put `hi` in the `*_hi` companion (see the `ranged` enum). 


    
## Public Attributes Documentation




### variable acq\_code 

```C++
wfm_seq_t wfm_source_t::acq_code;
```



The preamble code, sent acq\_reps times at the head of the frame (a Field's \*REPS on the command line and in a scene)  the coherent pull-in target BurstDespreader.set\_acq and BurstDemod.set\_preamble lock to. For type=dsss it is unmodulated chips ahead of the spread frame; for type=bits it is the head of the bit pattern. Setting it is what makes a source FRAMED. 


        

<hr>



### variable acq\_reps 

```C++
size_t wfm_source_t::acq_reps;
```



Preamble repetitions: periods of acq\_code before the sync word. On the command line and in a scene it is acq\_code's \*REPS. 


        

<hr>



### variable background 

```C++
int wfm_source_t::background;
```



Mark this source as part of the static background field (0/1). Plan.prepare() folds a contiguous leading run of background sources into ONE pre-summed cache entry instead of caching each separately, so a scene of many fixed emitters costs one buffer rather than hundreds. The composite is overridable as a unit: it takes a single slot in gains/phases/enable and counts as one in n\_sources(), so scaling it trims the whole field while its members keep their relative levels. Background sources must come first in the segment (a non-prefix ordering is rejected by prepare, since the fold would no longer reproduce compose bit-for-bit). Ignored by compose() and by standalone Synth.steps(). 


        

<hr>



### variable carrier\_hz 

```C++
double wfm_source_t::carrier_hz;
```



RF carrier in Hz that the ppm figures are referred to. It gives the coherent carrier rotation that accompanies the time-base warp; 0 warps the clock alone, with no carrier rotation  a legitimate scene, not an unset field. Independent of doppler/doppler\_rate. 


        

<hr>



### variable code\_only\_symbols 

```C++
size_t wfm_source_t::code_only_symbols;
```



Continuous dsss: pure-code symbols opening each frame of frame\_symbols symbols. The data pauses through them and runs on across frames, so a searcher can integrate coherently over the window; once a payload ends, no window is sent. At most frame\_symbols, and fewer when a data source is set; 0 with frame\_symbols 0 = no window. 


        

<hr>



### variable data 

```C++
wfm_seq_t wfm_source_t::data;
```



A frame's payload drawn from a data source: a Field on the command line and in a scene, a bit array in Python. The source is split into data\_len-bit frames, one chunk per frame, and its last chunk is padded from fill. For type=bits, bpsk/qpsk/pn framed, a dsss burst (one burst per frame), and continuous dsss (one bit per data symbol, no frame); not with data\_from\_file. 


        

<hr>



### variable data\_code 

```C++
wfm_seq_t wfm_source_t::data_code;
```



For type=dsss: the payload spreading code, a second code distinct from acq\_code. Every frame bit (sync, payload, crc) is XOR-spread across its full length, so len(data\_code) is the spreading factor. 


        

<hr>



### variable data\_from\_file 

```C++
const char* wfm_source_t::data_from_file;
```



A data source read from a file of packed octets, MSB first, or `-` for stdin, instead of data. Not on the Python face: there a file is cvt.bytes\_to\_bin of its bytes, passed as data (payload-data-source.md section 4.9). Borrowed, so it must outlive the source. 


        

<hr>



### variable data\_len 

```C++
size_t wfm_source_t::data_len;
```



Bits of the data source per frame: the data:LEN field of the common frame (preamble x reps, sync, data:LEN, crc). 0 takes a finite source whole, as one frame. A carried frame names its own data field, and this is then 0 or that field's LEN. Continuous dsss has no frame, and refuses it. 


        

<hr>



### variable data\_sent 

```C++
wfm_data_stats_t wfm_source_t::data_sent;
```



What this source's data sent in the composer's latest instance: its frames, the fill bits padding the last, its idle frames, the source bits read and, for data\_from\_file, the dp\_hash64 of the octets read. The truth record and SigMF carry (payload-data-source.md section 4.8), kept by the composer, which clears it at create; zero until an instance has sent a frame. Not an input: no face sets it. 


        

<hr>



### variable doppler 

```C++
double wfm_source_t::doppler;
```



Clock Doppler in ppm: the received time base is rescaled by 1 + doppler\*1e-6, so the symbol and chip rates move with the carrier and a timing loop sees the error a carrier-only `freq` offset hides. Accepts a (lo, hi) tuple drawn uniformly per repeat, like freq/snr. Zero doppler AND zero doppler\_rate means no channel is built at all, so a source that does not ask for Doppler renders exactly as it always did. 


        

<hr>



### variable doppler\_hi 

```C++
double wfm_source_t::doppler_hi;
```



upper bound when WFM\_RANGE\_DOPPLER is set 


        

<hr>



### variable doppler\_lifetime 

```C++
int wfm_source_t::doppler_lifetime;
```



How long this source's Doppler channel lives. per\_instance: the channel dies with each `repeats` instance, so the geometry restarts  the repeated-trial shape, which composes with a ranged doppler re-drawn per instance. persist: one continuous pass carries across the segment's gaps and repeat instances, keyed by (segment, source) position  the only lifetime under which doppler\_rate accumulates across a multi-burst scene. compose(), stream() and Plan.prepare() all honour both. 


        

<hr>



### variable doppler\_rate 

```C++
double wfm_source_t::doppler_rate;
```



Linear ramp on `doppler`, in ppm per second of elapsed stream time. The channel runs through a segment's gaps as well as its on-time  an emitter does not stop moving because its burst ended  so this is per second, not per second of on-time. Accepts a (lo, hi) tuple drawn uniformly per repeat. 


        

<hr>



### variable doppler\_rate\_hi 

```C++
double wfm_source_t::doppler_rate_hi;
```



upper bound when WFM\_RANGE\_DOPPLER\_RATE 


        

<hr>



### variable dsss\_code\_only 

```C++
int wfm_source_t::dsss_code_only;
```



Continuous dsss data source: 1 = code-only (the pure spreading code, no data modulation); 0 = data-modulated (the payload when supplied, else the seeded PN). Ignored for burst dsss and non-dsss types. 


        

<hr>



### variable f\_end 

```C++
double wfm_source_t::f_end;
```



Chirp end frequency in Hz; ignored by other types. 


        

<hr>



### variable f\_end\_hi 

```C++
double wfm_source_t::f_end_hi;
```



upper bound when WFM\_RANGE\_FEND is set 


        

<hr>



### variable fill 

```C++
wfm_seq_t wfm_source_t::fill;
```



The bits that pad a data source's last frame when it does not divide into data\_len-bit frames, tiled from their first bit; stdin on a framed source always needs them. Without them such a source is refused before the first sample. A Field on the command line and in a scene, a bit array in Python. Continuous dsss has no frame to pad, and refuses it. 


        

<hr>



### variable frame 

```C++
const wfm_frame_desc_t* wfm_source_t::frame;
```



A frame DESCRIPTION, the whole frame: fields in wire order, and stages that each name the span they cover (crc16, rs, randomise, interleave, conv, or a kind of your own). It is the only way a source says anything but the common frame: a coding stage, a field of the caller's own bits at a position of their choosing, a stage covering a span they name. `wfmgen --frame FILE`, a scene's `frame` key and Python's `frame=` (a FrameDesc or a Frame) all land here. When it is set it IS the frame, and an unspread acq\_code beside it is refused. NULL means the common frame, `[preamble x reps | data]`, which `dp_wfm_frame_fixed()` builds from acq\_code and the data source; a sync word or a CRC is a field or a stage of a description, never a flat field. A C caller's description is borrowed, exactly as `wfm_seq_t` is borrowed elsewhere here, so it must outlive the source. The composer and a Python source hold their own copy (`dp_wfm_frame_copy()`), so a later change to the FrameDesc does not reach them. On the Python face `frame=` is an input: read it back from the composer's JSON (its getter is jm's, pending removal: doppler#1694). KERNELS stay in C by design. A description names a stage's KIND; the code that runs it is a `wfm_frame_ops_t` entry, and a caller adding a genuinely new transform (convolutional interleaving, say) writes that kernel in C and hands it to `dp_wfm_frame_assemble` directly. 


        

<hr>



### variable frame\_symbols 

```C++
size_t wfm_source_t::frame_symbols;
```



Continuous dsss: the frame, in symbols, that code\_only\_symbols opens; give it with code\_only\_symbols, or both 0 for no window. The frame runs on the stream's own symbol clock, so its edges land at any chip phase. 


        

<hr>



### variable freq 

```C++
double wfm_source_t::freq;
```



Carrier or offset frequency in Hz; for chirp, the sweep start. With fs = 1 it is in normalised cycles per sample. 


        

<hr>



### variable freq\_hi 

```C++
double wfm_source_t::freq_hi;
```



upper bound when WFM\_RANGE\_FREQ is set 


        

<hr>



### variable level 

```C++
double wfm_source_t::level;
```



Source power in dBFS (&lt;= 0; 0 is unit power). Applies when summed in a Segment or Composer, as a gain of 10^(level/20); a standalone Synth.steps() ignores it. 


        

<hr>



### variable level\_hi 

```C++
double wfm_source_t::level_hi;
```



upper bound when WFM\_RANGE\_LEVEL is set 


        

<hr>



### variable lfsr 

```C++
int wfm_source_t::lfsr;
```



PN LFSR realisation. Both give the same period; fibonacci's chips are galois's in reverse order. 


        

<hr>



### variable modulation 

```C++
int wfm_source_t::modulation;
```



Symbol mapping of a bits pattern. none: the pattern shaped and output as-is (NRZ). bpsk: +/-1 symbols. qpsk: Gray-coded symbols from pairs of bits. 


        

<hr>



### variable n\_symbols 

```C++
size_t wfm_source_t::n_symbols;
```



type=symbols: stream length 


        

<hr>



### variable pn\_length 

```C++
int wfm_source_t::pn_length;
```



PN LFSR register length in bits, 2 to 64 for the PN-bearing types; the sequence period is 2^pn\_length - 1. Unused by noise, which records it as 0. 


        

<hr>



### variable pn\_poly 

```C++
uint64_t wfm_source_t::pn_poly;
```



PN generator polynomial, in the Galois bit-vector convention; 0 selects a maximal-length (MLS) polynomial for pn\_length. It must fit the pn\_length-bit register: a bit at or above it is refused, not masked. A polynomial above 2^53 does not survive a JSON number, so a scene file needs 0 (auto) for such a register. 


        

<hr>



### variable pulse 

```C++
int wfm_source_t::pulse;
```



Pulse shape per symbol or chip, for pn/bpsk/qpsk/bits/symbols/dsss. rect: rectangular, no ISI filtering. rrc: root-raised cosine; see rrc\_beta and rrc\_span. 


        

<hr>



### variable ranged 

```C++
unsigned wfm_source_t::ranged;
```



WFM\_RANGE\_{FREQ,SNR,LEVEL,FEND,DOPPLER\*} bitmask 


        

<hr>



### variable retired\_bits 

```C++
wfm_seq_t wfm_source_t::retired_bits;
```



RETIRED (doppler#1718): nothing reads it but the refusal. A payload is drawn from a data source, so bits=, and its aliases payload= and pattern=, are refused naming data=; the CLI and a scene refuse bits and "payload" the same way. 


        

<hr>



### variable retired\_crc 

```C++
wfm_seq_t wfm_source_t::retired_crc;
```



RETIRED (doppler#1617): nothing reads it but the refusal. A CRC is a stage of the frame DESCRIPTION, so crc= is refused naming frame=, whatever its value (crc="none" included); the CLI's crc builds that description for you, and a scene refuses the "crc" key. 


        

<hr>



### variable retired\_sync 

```C++
wfm_seq_t wfm_source_t::retired_sync;
```



RETIRED (doppler#1617): nothing reads it but the refusal. The frame-sync word is a field of the frame DESCRIPTION, so sync= is refused naming frame=; the CLI's sync builds that description for you, and a scene refuses the "sync" key. 


        

<hr>



### variable rrc\_beta 

```C++
double wfm_source_t::rrc_beta;
```



RRC roll-off factor, in (0, 1], when pulse=rrc. 


        

<hr>



### variable rrc\_span 

```C++
int wfm_source_t::rrc_span;
```



RRC filter support in symbols when pulse=rrc, ONE-SIDED: the filter has 2\*rrc\_span\*sps + 1 taps, unit energy (sum of h^2 = 1). 


        

<hr>



### variable seed 

```C++
uint32_t wfm_source_t::seed;
```



PRNG and LFSR seed for the noise and PN streams. Deterministic: vary it for run-to-run change. For a PN-sourced type it starts the pn\_length-bit register, masked: a seed whose low pn\_length bits are zero starts the register at 1, as seed 0 does. 


        

<hr>



### variable snr 

```C++
double wfm_source_t::snr;
```



Signal-to-noise ratio in dB, interpreted per snr\_mode. 100 or more is clean: no AWGN is added. 


        

<hr>



### variable snr\_hi 

```C++
double wfm_source_t::snr_hi;
```



upper bound when WFM\_RANGE\_SNR is set 


        

<hr>



### variable snr\_mode 

```C++
int wfm_source_t::snr_mode;
```



How snr is interpreted. auto: Es/N0 for bpsk/qpsk/dsss, and relative to full scale for tone/noise/pn/chirp/bits/symbols  bits included, because a bits frame has no symbol rate the engine can infer. fs: dB relative to full scale. ebno: Eb/N0, per bit. esno: Es/N0, per symbol; for a dsss burst the outer data symbol of len(data\_code) chips x sps samples, for a continuous dsss stream the fs/symbol\_rate samples the async symbol spans. 


        

<hr>



### variable span 

```C++
size_t wfm_source_t::span;
```



Chirp sweep length in samples: the frequency ramps from freq to f\_end over this many samples, then holds at f\_end. 0 means the enclosing Segment's num\_samples. A standalone chirp (f\_end != freq) must declare it, so step(), steps(N) and any chunking of reads produce the same waveform; generating one without it raises. Ignored by non-chirp types. 


        

<hr>



### variable sps 

```C++
int wfm_source_t::sps;
```



Samples per symbol (PSK) or per chip (PN): the oversampling factor. Unused by noise, which records it as 0. 


        

<hr>



### variable symbol\_rate 

```C++
double wfm_source_t::symbol_rate;
```



For type=dsss: &gt; 0 selects CONTINUOUS asynchronous mode. The spreading code repeats endlessly and data rides on it at this symbol rate (Hz), independent of the code-epoch rate (chips/symbol = fs/sps/symbol\_rate, non-integer). No preamble/sync/CRC frame; data comes from the payload when supplied, else a seeded PN a receiver regenerates. Absent/0 = burst. 


        

<hr>



### variable symbols 

```C++
float _Complex* wfm_source_t::symbols;
```



For type=symbols: a complex constellation stream. Each element is the output point itself, oversampled by sps, cycled, and RRC-shaped with pulse=rrc, which generalises any modulation (pi/4-QPSK, QAM, ...). 


        

<hr>



### variable type 

```C++
int wfm_source_t::type;
```



Waveform type. tone: complex sinusoid. noise: AWGN. pn: PN sequence (LFSR). bpsk/qpsk: PN-driven modulation. chirp: linear-FM sweep. bits: a caller's bit pattern, with selectable modulation. symbols: a caller's complex constellation stream. dsss: spread spectrum  a two-code burst (repeated preamble + data-code-spread frame) by default, or a continuous asynchronous stream when symbol\_rate is set. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_compose.h`

