

# Struct dll\_state\_t



[**ClassList**](annotated.md) **>** [**dll\_state\_t**](structdll__state__t.md)



_DLL state._ [More...](#detailed-description)

* `#include <dll_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float complex | [**acc\_e**](#variable-acc_e)  <br> |
|  float complex | [**acc\_l**](#variable-acc_l)  <br> |
|  float complex | [**acc\_o**](#variable-acc_o)  <br> |
|  float complex | [**acc\_p**](#variable-acc_p)  <br> |
|  double | [**aid\_alpha**](#variable-aid_alpha)  <br> |
|  size\_t | [**aid\_best**](#variable-aid_best)  <br> |
|  uint64\_t | [**aid\_count**](#variable-aid_count)  <br> |
|  size\_t | [**aid\_len**](#variable-aid_len)  <br> |
|  size\_t | [**aid\_nhyp**](#variable-aid_nhyp)  <br> |
|  double \* | [**aid\_power**](#variable-aid_power)  <br> |
|  size\_t | [**aid\_ring**](#variable-aid_ring)  <br> |
|  float complex \* | [**aid\_ring\_e**](#variable-aid_ring_e)  <br> |
|  float complex \* | [**aid\_ring\_l**](#variable-aid_ring_l)  <br> |
|  float complex \* | [**aid\_ring\_o**](#variable-aid_ring_o)  <br> |
|  float complex \* | [**aid\_ring\_p**](#variable-aid_ring_p)  <br> |
|  double | [**bn**](#variable-bn)  <br> |
|  double | [**chip\_pos**](#variable-chip_pos)  <br> |
|  float \_Complex \* | [**chunk\_e**](#variable-chunk_e)  <br> |
|  float \_Complex \* | [**chunk\_l**](#variable-chunk_l)  <br> |
|  float \_Complex \* | [**chunk\_p**](#variable-chunk_p)  <br> |
|  const uint8\_t \* | [**code**](#variable-code)  <br> |
|  [**nco\_state\_t**](structnco__state__t.md) | [**code\_nco**](#variable-code_nco)  <br> |
|  double | [**code\_rate**](#variable-code_rate)  <br> |
|  int | [**have\_prev\_epoch**](#variable-have_prev_epoch)  <br> |
|  double | [**inv\_sps**](#variable-inv_sps)  <br> |
|  double | [**inv\_tsamps**](#variable-inv_tsamps)  <br> |
|  double | [**inv\_tsamps2**](#variable-inv_tsamps2)  <br> |
|  double | [**inv\_tsamps\_sf**](#variable-inv_tsamps_sf)  <br> |
|  double | [**inv\_upd**](#variable-inv_upd)  <br> |
|  float complex \* | [**last\_backward\_p**](#variable-last_backward_p)  <br> |
|  float complex \* | [**last\_e**](#variable-last_e)  <br> |
|  double | [**last\_error**](#variable-last_error)  <br> |
|  float \_Complex \* | [**last\_l**](#variable-last_l)  <br> |
|  [**loop\_filter\_state\_t**](structloop__filter__state__t.md) | [**lf**](#variable-lf)  <br> |
|  [**lockdet\_state\_t**](structlockdet__state__t.md) | [**lock**](#variable-lock)  <br> |
|  double | [**lock\_alpha**](#variable-lock_alpha)  <br> |
|  size\_t | [**lock\_count**](#variable-lock_count)  <br> |
|  size\_t | [**lock\_nz**](#variable-lock_nz)  <br> |
|  double | [**lock\_stat**](#variable-lock_stat)  <br> |
|  double | [**lock\_sum**](#variable-lock_sum)  <br> |
|  size\_t | [**n\_looks**](#variable-n_looks)  <br> |
|  double | [**noise\_ema**](#variable-noise_ema)  <br> |
|  double | [**noise\_guard**](#variable-noise_guard)  <br> |
|  double | [**off\_chips**](#variable-off_chips)  <br> |
|  int | [**owns\_code**](#variable-owns_code)  <br> |
|  double | [**rate\_aid**](#variable-rate_aid)  <br> |
|  uint32\_t | [**rng**](#variable-rng)  <br> |
|  double | [**seed\_chip**](#variable-seed_chip)  <br> |
|  double | [**seg\_chips**](#variable-seg_chips)  <br> |
|  size\_t | [**seg\_idx**](#variable-seg_idx)  <br> |
|  double | [**seg\_norm**](#variable-seg_norm)  <br> |
|  size\_t | [**segments**](#variable-segments)  <br> |
|  size\_t | [**sf**](#variable-sf)  <br> |
|  double | [**spacing**](#variable-spacing)  <br> |
|  size\_t | [**sps**](#variable-sps)  <br> |
|  float complex \* | [**sums**](#variable-sums)  <br> |
|  double | [**sym\_period**](#variable-sym_period)  <br> |
|  [**dll\_tlm\_t**](structdll__tlm__t.md) | [**tlm**](#variable-tlm)  <br> |
|  double | [**zeta**](#variable-zeta)  <br> |












































## Detailed Description


Allocate with [**dll\_create()**](dll__core_8h.md#function-dll_create) (copies the code), or embed by value and [**dll\_init()**](dll__core_8h.md#function-dll_init) (borrows the caller's code). The loop filter `lf` is a public sub-component so the inline composition helpers can drive it; treat the correlator accumulators and code-phase fields as internal. 


    
## Public Attributes Documentation




### variable acc\_e 

```C++
float _Complex dll_state_t::acc_e;
```



early correlator accumulator. 
 


        

<hr>



### variable acc\_l 

```C++
float _Complex dll_state_t::acc_l;
```



late correlator accumulator. 
 


        

<hr>



### variable acc\_o 

```C++
float _Complex dll_state_t::acc_o;
```



offset (noise) correlator accumulator. 
 


        

<hr>



### variable acc\_p 

```C++
float _Complex dll_state_t::acc_p;
```



prompt correlator accumulator. 
 


        

<hr>



### variable aid\_alpha 

```C++
double dll_state_t::aid_alpha;
```



EMA over symbols of each window's power. 
 


        

<hr>



### variable aid\_best 

```C++
size_t dll_state_t::aid_best;
```



current best hypothesis (the timing). 
 


        

<hr>



### variable aid\_count 

```C++
uint64_t dll_state_t::aid_count;
```



partials seen since enable (ring index). 


        

<hr>



### variable aid\_len 

```C++
size_t dll_state_t::aid_len;
```



coherent window length L, partials. 
 


        

<hr>



### variable aid\_nhyp 

```C++
size_t dll_state_t::aid_nhyp;
```



boundary-phase hypotheses Q = ceil(P). 
 


        

<hr>



### variable aid\_power 

```C++
double* dll_state_t::aid_power;
```



per-hypothesis window-power EMA (Q). 
 


        

<hr>



### variable aid\_ring 

```C++
size_t dll_state_t::aid_ring;
```



ring capacity, partials (power of two). 
 


        

<hr>



### variable aid\_ring\_e 

```C++
float complex* dll_state_t::aid_ring_e;
```



last `aid_ring` early partials. 
 


        

<hr>



### variable aid\_ring\_l 

```C++
float complex* dll_state_t::aid_ring_l;
```



last `aid_ring` late partials. 
 


        

<hr>



### variable aid\_ring\_o 

```C++
float complex* dll_state_t::aid_ring_o;
```



last `aid_ring` offset (noise) partials. 


        

<hr>



### variable aid\_ring\_p 

```C++
float complex* dll_state_t::aid_ring_p;
```



last `aid_ring` partial prompts. 
 


        

<hr>



### variable bn 

```C++
double dll_state_t::bn;
```



loop noise bandwidth (retained). 
 


        

<hr>



### variable chip\_pos 

```C++
double dll_state_t::chip_pos;
```



current prompt code phase, chips; DERIVED from code\_nco.phase on every dll\_accumulate, never independently accumulated. 
 


        

<hr>



### variable chunk\_e 

```C++
float _Complex* dll_state_t::chunk_e;
```



this epoch's per-chunk early sums. 
 


        

<hr>



### variable chunk\_l 

```C++
float _Complex* dll_state_t::chunk_l;
```



this epoch's per-chunk late sums. 
 


        

<hr>



### variable chunk\_p 

```C++
float _Complex* dll_state_t::chunk_p;
```



this epoch's per-chunk prompt sums; Python's `partial_sums`. 
 


        

<hr>



### variable code 

```C++
const uint8_t* dll_state_t::code;
```



spreading code, one period (0/1 chips). 
 


        

<hr>



### variable code\_nco 

```C++
nco_state_t dll_state_t::code_nco;
```



fixed-point code-phase NCO (phase/phase\_inc). 


        

<hr>



### variable code\_rate 

```C++
double dll_state_t::code_rate;
```



chips advanced per nominal chip (~1.0). 
 


        

<hr>



### variable have\_prev\_epoch 

```C++
int dll_state_t::have_prev_epoch;
```



0 until one full epoch has completed. 
 


        

<hr>



### variable inv\_sps 

```C++
double dll_state_t::inv_sps;
```



1 / sps (per-sample chip advance scale). 
 


        

<hr>



### variable inv\_tsamps 

```C++
double dll_state_t::inv_tsamps;
```



1 / (sf\*sps)  the nominal phase\_inc, cycles/sample; precomputed once (sf/sps are create-time invariants) so the tracking loop never divides by tsamps. 
 


        

<hr>



### variable inv\_tsamps2 

```C++
double dll_state_t::inv_tsamps2;
```



1 / (sf\*sps)^2  segments&gt;1's ctrl scale. 


        

<hr>



### variable inv\_tsamps\_sf 

```C++
double dll_state_t::inv_tsamps_sf;
```



1 / (sf\*sps\*sf)  segments&lt;=1's kp\*e/(sf) ctrl term's scale. 
 


        

<hr>



### variable inv\_upd 

```C++
double dll_state_t::inv_upd;
```



1 / lf.t: one over the loop's update interval in epochs (1 per epoch; the symbol period in epochs once the aid is on). The filter's output is a correction per update; this turns it into the rate held over the interval. Precomputed with the gains, never in the loop. 
 


        

<hr>



### variable last\_backward\_p 

```C++
float _Complex* dll_state_t::last_backward_p;
```



prev epoch's reversed-cumsum prompt; Python's `backward_sums`, saved from the PREVIOUS epoch's call. 
 


        

<hr>



### variable last\_e 

```C++
float _Complex* dll_state_t::last_e;
```



prev epoch's per-chunk early sums. 
 


        

<hr>



### variable last\_error 

```C++
double dll_state_t::last_error;
```



last discriminator output (loop stress). 


        

<hr>



### variable last\_l 

```C++
float _Complex* dll_state_t::last_l;
```



prev epoch's per-chunk late sums. 
 


        

<hr>



### variable lf 

```C++
loop_filter_state_t dll_state_t::lf;
```



2nd-order code PI loop. 
 


        

<hr>



### variable lock 

```C++
lockdet_state_t dll_state_t::lock;
```



decision rule: thresholds + verify counters stepped on R at each N-look decision. 
 


        

<hr>



### variable lock\_alpha 

```C++
double dll_state_t::lock_alpha;
```



EMA coefficient 1/L\_eff (L\_eff &gt;&gt; n\_looks). 


        

<hr>



### variable lock\_count 

```C++
size_t dll_state_t::lock_count;
```



looks accumulated in the current window. 
 


        

<hr>



### variable lock\_nz 

```C++
size_t dll_state_t::lock_nz;
```



noise looks folded in (cumulative-mean boot). 


        

<hr>



### variable lock\_stat 

```C++
double dll_state_t::lock_stat;
```



last statistic R = sqrt(2 sum\|P\|^2/E\|O\|^2). 


        

<hr>



### variable lock\_sum 

```C++
double dll_state_t::lock_sum;
```



running sum\|P\_k\|^2 over the current window. 


        

<hr>



### variable n\_looks 

```C++
size_t dll_state_t::n_looks;
```



non-coherent integration depth N. 
 


        

<hr>



### variable noise\_ema 

```C++
double dll_state_t::noise_ema;
```



EMA of offset power; estimates E\|O\|^2. 
 


        

<hr>



### variable noise\_guard 

```C++
double dll_state_t::noise_guard;
```



chips around P/E/L the offset must avoid. 
 


        

<hr>



### variable off\_chips 

```C++
double dll_state_t::off_chips;
```



this look's offset code phase, whole chips. 


        

<hr>



### variable owns\_code 

```C++
int dll_state_t::owns_code;
```



1 if [**dll\_destroy()**](dll__core_8h.md#function-dll_destroy) frees `code`. 
 


        

<hr>



### variable rate\_aid 

```C++
double dll_state_t::rate_aid;
```



carrier-aiding code-rate deviation (ratio, 0 = off): a fixed fractional bias summed into the sample-and-hold phase\_inc every epoch, on top of the loop's own ctrl. For physically-coupled Doppler, the caller sets this to carrier\_offset/carrier\_freq so the code NCO rides the code-rate dilation the code discriminator alone can't pull in at low SNR. See [**dll\_set\_rate\_aid()**](dll__core_8h.md#function-dll_set_rate_aid). 
 


        

<hr>



### variable rng 

```C++
uint32_t dll_state_t::rng;
```



xorshift32 state for the random offset. 
 


        

<hr>



### variable seed\_chip 

```C++
double dll_state_t::seed_chip;
```



create-time code phase, for reset. 
 


        

<hr>



### variable seg\_chips 

```C++
double dll_state_t::seg_chips;
```



code phase per partial segment = sf/segments. 


        

<hr>



### variable seg\_idx 

```C++
size_t dll_state_t::seg_idx;
```



samples integrated into the current chunk. 


        

<hr>



### variable seg\_norm 

```C++
double dll_state_t::seg_norm;
```



nominal samples per segment (prompt scale). 


        

<hr>



### variable segments 

```C++
size_t dll_state_t::segments;
```



partial correlations per epoch (1 = full). 


        

<hr>



### variable sf 

```C++
size_t dll_state_t::sf;
```



code length (chips per period). 
 


        

<hr>



### variable spacing 

```C++
double dll_state_t::spacing;
```



early/late tap offset, chips (e.g. 0.5). 
 


        

<hr>



### variable sps 

```C++
size_t dll_state_t::sps;
```



samples per chip. 
 


        

<hr>



### variable sums 

```C++
float _Complex* dll_state_t::sums;
```



this epoch's running cumulative sum of chunk\_p; Python's `partial_sums .cumsum()`. Pure scratch (rebuilt every epoch boundary, never read across calls)  not part of the serialized state. 
 


        

<hr>



### variable sym\_period 

```C++
double dll_state_t::sym_period;
```



data-symbol period, partials (0 = off). 
 


        

<hr>



### variable tlm 

```C++
dll_tlm_t dll_state_t::tlm;
```



live telemetry attachment; zeroed in blobs 


        

<hr>



### variable zeta 

```C++
double dll_state_t::zeta;
```



damping factor (retained). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dll/dll_core.h`

