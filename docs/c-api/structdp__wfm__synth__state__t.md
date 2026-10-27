

# Struct dp\_wfm\_synth\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md)



_Synth state: one streaming waveform source._ [More...](#detailed-description)

* `#include <wfm_synth_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_awgn\_state\_t**](structdp__awgn__state__t.md) \* | [**awgn**](#variable-awgn)  <br> |
|  size\_t | [**bit\_idx**](#variable-bit_idx)  <br> |
|  int | [**bit\_mod**](#variable-bit_mod)  <br> |
|  uint8\_t \* | [**bits**](#variable-bits)  <br> |
|  uint64\_t | [**chip\_n**](#variable-chip_n)  <br> |
|  double | [**chips\_per\_symbol**](#variable-chips_per_symbol)  <br> |
|  double | [**chirp\_f0**](#variable-chirp_f0)  <br> |
|  double | [**chirp\_fend**](#variable-chirp_fend)  <br> |
|  double | [**chirp\_k**](#variable-chirp_k)  <br> |
|  size\_t | [**chirp\_n**](#variable-chirp_n)  <br> |
|  double | [**chirp\_ph**](#variable-chirp_ph)  <br> |
|  size\_t | [**chirp\_span**](#variable-chirp_span)  <br> |
|  uint8\_t \* | [**code**](#variable-code)  <br> |
|  size\_t | [**code\_only\_symbols**](#variable-code_only_symbols)  <br> |
|  uint8\_t | [**cur\_data**](#variable-cur_data)  <br> |
|  float | [**cur\_im**](#variable-cur_im)  <br> |
|  float | [**cur\_re**](#variable-cur_re)  <br> |
|  uint8\_t | [**data\_ended**](#variable-data_ended)  <br> |
|  int | [**data\_mode**](#variable-data_mode)  <br> |
|  [**dp\_fir\_state\_t**](structdp__fir__state__t.md) \* | [**fir**](#variable-fir)  <br> |
|  size\_t | [**frame\_symbols**](#variable-frame_symbols)  <br> |
|  [**dp\_lo\_state\_t**](structdp__lo__state__t.md) \* | [**lo**](#variable-lo)  <br> |
|  size\_t | [**n\_bits**](#variable-n_bits)  <br> |
|  size\_t | [**n\_code**](#variable-n_code)  <br> |
|  size\_t | [**n\_symbols**](#variable-n_symbols)  <br> |
|  uint64\_t | [**next\_edge**](#variable-next_edge)  <br> |
|  int | [**nsps**](#variable-nsps)  <br> |
|  [**dp\_pn\_state\_t**](structdp__pn__state__t.md) \* | [**pn**](#variable-pn)  <br> |
|  uint8\_t | [**primed**](#variable-primed)  <br> |
|  [**wfm\_synth\_refill\_fn**](wfm__synth__core_8h.md#typedef-wfm_synth_refill_fn) | [**refill**](#variable-refill)  <br> |
|  void(\* | [**refill\_free**](#variable-refill_free)  <br> |
|  const [**wfm\_synth\_refill\_state\_t**](structwfm__synth__refill__state__t.md) \* | [**refill\_state**](#variable-refill_state)  <br> |
|  void \* | [**refill\_user**](#variable-refill_user)  <br> |
|  [**resamp\_state\_t**](structresamp__state__t.md) \* | [**shaper**](#variable-shaper)  <br> |
|  uint64\_t | [**sym\_idx**](#variable-sym_idx)  <br> |
|  int | [**sym\_pos**](#variable-sym_pos)  <br> |
|  size\_t | [**sym\_read\_idx**](#variable-sym_read_idx)  <br> |
|  float \_Complex \* | [**symbols**](#variable-symbols)  <br> |
|  int | [**wtype**](#variable-wtype)  <br> |












































## Detailed Description


Allocate with [**dp\_wfm\_synth\_create()**](wfm__synth__core_8h.md#function-dp_wfm_synth_create), configure with the set\_\* calls, and release with [**dp\_wfm\_synth\_destroy()**](wfm__synth__core_8h.md#function-dp_wfm_synth_destroy). Fields are public for the inline kernels in this header; treat them as read-only and change them through the API. "config" fields are set by create() or a setter and survive [**dp\_wfm\_synth\_reset()**](wfm__synth__core_8h.md#function-dp_wfm_synth_reset); "running" fields are the stream position. 


    
## Public Attributes Documentation




### variable awgn 

```C++
dp_awgn_state_t* dp_wfm_synth_state_t::awgn;
```



noise generator, owned; NULL for a clean synth (snr &gt;= WFM\_SYNTH\_SNR\_CLEAN) 


        

<hr>



### variable bit\_idx 

```C++
size_t dp_wfm_synth_state_t::bit_idx;
```



running: read cursor into `bits`; stops at n\_bits (never wraps) 


        

<hr>



### variable bit\_mod 

```C++
int dp_wfm_synth_state_t::bit_mod;
```



config: bits per symbol for the pattern: 0 = 0/1 amplitude line, 1 = BPSK, 2 = QPSK, 3 = 8PSK 


        

<hr>



### variable bits 

```C++
uint8_t* dp_wfm_synth_state_t::bits;
```



pattern buffer (0/1 per byte), owned: the bits pattern, a dsss burst's chips, or a continuous dsss payload; a refill rewrites it with each frame 


        

<hr>



### variable chip\_n 

```C++
uint64_t dp_wfm_synth_state_t::chip_n;
```



running: chips emitted so far 


        

<hr>



### variable chips\_per\_symbol 

```C++
double dp_wfm_synth_state_t::chips_per_symbol;
```



config: chip\_rate / symbol\_rate (non-integer); 0 = burst mode 


        

<hr>



### variable chirp\_f0 

```C++
double dp_wfm_synth_state_t::chirp_f0;
```



config: chirp start frequency, cycles/sample (`freq / fs`) 


        

<hr>



### variable chirp\_fend 

```C++
double dp_wfm_synth_state_t::chirp_fend;
```



config: chirp end frequency, cycles/sample (`f_end / fs`) 


        

<hr>



### variable chirp\_k 

```C++
double dp_wfm_synth_state_t::chirp_k;
```



config: chirp slope, cycles/sample per sample; 0 until [**dp\_wfm\_synth\_set\_chirp\_span()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_chirp_span) pins it 


        

<hr>



### variable chirp\_n 

```C++
size_t dp_wfm_synth_state_t::chirp_n;
```



running: samples generated into the sweep; the frequency holds at the end once it reaches chirp\_span 


        

<hr>



### variable chirp\_ph 

```C++
double dp_wfm_synth_state_t::chirp_ph;
```



running: chirp carrier phase, cycles, in [0, 1) 


        

<hr>



### variable chirp\_span 

```C++
size_t dp_wfm_synth_state_t::chirp_span;
```



config: sweep length in samples; 0 = not pinned (the chirp holds its start frequency) 


        

<hr>



### variable code 

```C++
uint8_t* dp_wfm_synth_state_t::code;
```



config: spreading code (0/1), owned 


        

<hr>



### variable code\_only\_symbols 

```C++
size_t dp_wfm_synth_state_t::code_only_symbols;
```



config: pure-code symbols opening each frame 


        

<hr>



### variable cur\_data 

```C++
uint8_t dp_wfm_synth_state_t::cur_data;
```



running: data bit latched for this symbol (0/1), or WFM\_DSSS\_ENDED 


        

<hr>



### variable cur\_im 

```C++
float dp_wfm_synth_state_t::cur_im;
```



running: imaginary (Q) part of the held symbol 


        

<hr>



### variable cur\_re 

```C++
float dp_wfm_synth_state_t::cur_re;
```



running: real (I) part of the held symbol; 1.0 for tone/chirp, else 0.0 until the first latch 


        

<hr>



### variable data\_ended 

```C++
uint8_t dp_wfm_synth_state_t::data_ended;
```



running: 1 once the data has ended (pattern spent with no refill, or the refill reported its end); output is silence from then on 


        

<hr>



### variable data\_mode 

```C++
int dp_wfm_synth_state_t::data_mode;
```



config: WFM\_DSSS\_DATA\_{NONE,BITS,PRBS} 


        

<hr>



### variable fir 

```C++
dp_fir_state_t* dp_wfm_synth_state_t::fir;
```



dense RRC FIR (non-power-of-two sps fallback), owned; NULL = no shaping 


        

<hr>



### variable frame\_symbols 

```C++
size_t dp_wfm_synth_state_t::frame_symbols;
```



config: frame length in symbols; 0 = no window 


        

<hr>



### variable lo 

```C++
dp_lo_state_t* dp_wfm_synth_state_t::lo;
```



carrier LO, owned; NULL at freq 0 and for noise/chirp 


        

<hr>



### variable n\_bits 

```C++
size_t dp_wfm_synth_state_t::n_bits;
```



config: length of `bits` (one frame), in bits 


        

<hr>



### variable n\_code 

```C++
size_t dp_wfm_synth_state_t::n_code;
```



config: spreading code length in chips 


        

<hr>



### variable n\_symbols 

```C++
size_t dp_wfm_synth_state_t::n_symbols;
```



config: length of `symbols` 


        

<hr>



### variable next\_edge 

```C++
uint64_t dp_wfm_synth_state_t::next_edge;
```



derived: first chip of symbol sym\_idx + 1 (dp\_wfm\_dsss\_cont\_edge); not serialized  set\_state recomputes it from sym\_idx 


        

<hr>



### variable nsps 

```C++
int dp_wfm_synth_state_t::nsps;
```



config: samples per symbol (per chip for dsss), &gt;= 1 after create() (a smaller `sps` is clamped to 1) 


        

<hr>



### variable pn 

```C++
dp_pn_state_t* dp_wfm_synth_state_t::pn;
```



PN LFSR, owned: chips/data for pn, bpsk, qpsk, and the dsss PRBS data; else NULL 


        

<hr>



### variable primed 

```C++
uint8_t dp_wfm_synth_state_t::primed;
```



running: shaper's sps-sample latency primed 


        

<hr>



### variable refill 

```C++
wfm_synth_refill_fn dp_wfm_synth_state_t::refill;
```



frame source, or NULL 


        

<hr>



### variable refill\_free 

```C++
void(* dp_wfm_synth_state_t::refill_free) (void *);
```



frees `refill_user` on detach or destroy, or NULL 


        

<hr>



### variable refill\_state 

```C++
const wfm_synth_refill_state_t* dp_wfm_synth_state_t::refill_state;
```



the refill's state triplet, or NULL 


        

<hr>



### variable refill\_user 

```C++
void* dp_wfm_synth_state_t::refill_user;
```



argument passed to `refill` 


        

<hr>



### variable shaper 

```C++
resamp_state_t* dp_wfm_synth_state_t::shaper;
```



polyphase RRC shaper (power-of-two sps), owned; NULL when unused 


        

<hr>



### variable sym\_idx 

```C++
uint64_t dp_wfm_synth_state_t::sym_idx;
```



running: current data-symbol index 


        

<hr>



### variable sym\_pos 

```C++
int dp_wfm_synth_state_t::sym_pos;
```



running: sample index within the held symbol, 0..nsps-1; 0 means the next sample latches a new symbol (unused by the polyphase shaper path) 


        

<hr>



### variable sym\_read\_idx 

```C++
size_t dp_wfm_synth_state_t::sym_read_idx;
```



running: read cursor into `symbols`; wraps (the stream cycles) 


        

<hr>



### variable symbols 

```C++
float _Complex* dp_wfm_synth_state_t::symbols;
```



config: type=symbols stream, owned 


        

<hr>



### variable wtype 

```C++
int dp_wfm_synth_state_t::wtype;
```



config: waveform type, a WFM\_SYNTH\_\* value 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm_synth/wfm_synth_core.h`

