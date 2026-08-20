

# File dp\_syncword.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_syncword.h**](dp__syncword_8h.md)

[Go to the source code of this file](dp__syncword_8h_source.md)

_Finding a known bit pattern in an unpacked bit stream — the sync word search, and the arithmetic for choosing its threshold._ [More...](#detailed-description)

* `#include <math.h>`
* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_syncword\_hit\_t**](structdp__syncword__hit__t.md) <br>_Where a marker was found, and in which polarity._  |
























## Public Static Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_syncword\_find**](#function-dp_syncword_find) (const uint8\_t \* bits, size\_t n\_bits, const uint8\_t \* marker, size\_t n\_marker, unsigned max\_errors, [**dp\_syncword\_hit\_t**](structdp__syncword__hit__t.md) \* hit) <br>_Find the first marker in a run of unpacked bits, either polarity._  |
|  int | [**dp\_syncword\_max\_errors**](#function-dp_syncword_max_errors) (size\_t n\_marker, size\_t window\_bits, double pfa) <br>_The largest tolerance whose false-frame rate over a search window still meets_ `pfa` _._ |
|  double | [**dp\_syncword\_pfa**](#function-dp_syncword_pfa) (size\_t n\_marker, unsigned max\_errors) <br>_Probability that ONE random offset false-hits an_ `n_marker` _-bit marker at a tolerance of_`max_errors` _._ |


























## Detailed Description


A frame synchroniser correlates a marker it knows against the bits it is handed, in both polarities, and reports the first offset close enough to accept. That is one kernel, and every framing that has a sync word wants it: CCSDS calls its 32-bit marker an ASM and `ccsds_tm_asm_find` is this function configured with `0x1ACFFC1D`, exactly as `CCSDS_TM_CONV` configures `conv_code_t`. The standard picks a pattern; the search is not the standard's.


Header-only (like `dp_crc16.h`) so no component grows a link-line dependency for a kernel this size — a receiver correlating a marker should not link a Reed-Solomon encoder to do it.


Bit convention: **unpacked** bits, one per byte in the LSB, which is what `wfm_frame_bits`, `dp_crc16_ccitt`, `ccsds_tm_randomise` and the spreader already pass around.


### Choosing @p max\_errors — it is not a property of the marker



The threshold is the whole of the trade, and the number a caller needs is a function of **how much stream they search**, not of how long the marker is. A 32-bit marker invites "half of 32 is 16, so 8 sounds safe", and 8 finds the marker at its true offset only 58 % of the time on a stream with no channel errors at all — because each preceding offset is an independent chance to false-hit first, and the search reports the FIRST acceptable offset rather than the best one.


So the arithmetic ships beside the search. Per offset, a random window lands within `t` of an `n` -bit marker in one polarity or the other with


P\_fa(n, t) = 2 \* sum\_{i &lt;= t} C(n, i) / 2^n (`dp_syncword_pfa`)


and over `W` offsets tried ahead of the true marker the chance one of them wins the race is `1 - (1 - P_fa)^W`. `dp_syncword_max_errors` inverts that: give it the window and the false-frame rate you will accept and it returns the largest threshold that holds.


Measured for the 32-bit CCSDS marker (doppler#897, and `src/doppler/tests/validation/ccsds_tm/results.md` §2.2–2.3): the measured false-alarm rate tracks the closed form above to within 20 % at every threshold where the count supports a rate, `t <= 1` produced zero false markers in 2000000 random bits, and polarity was never once reported wrong in 155901 detections. 



    
## Public Static Functions Documentation




### function dp\_syncword\_find 

_Find the first marker in a run of unpacked bits, either polarity._ 
```C++
static inline int dp_syncword_find (
    const uint8_t * bits,
    size_t n_bits,
    const uint8_t * marker,
    size_t n_marker,
    unsigned max_errors,
    dp_syncword_hit_t * hit
) 
```



Correlates `marker` against every bit offset and against its complement, and reports the **first** offset whose Hamming distance is at most `max_errors`.


First rather than best, and the difference matters: a best-match search has to see the whole stream before it can answer, which a frame synchroniser reading a live capture cannot do. First-below-threshold is what is implementable in both settings, so it is what this promises — and it is why `max_errors` must be chosen against the search window, as the file comment sets out.




**Parameters:**


* `bits` Unpacked bits, one per byte. 
* `n_bits` Number of bits. 
* `marker` The pattern to find, unpacked bits, one per byte. 
* `n_marker` Length of `marker` in bits. 
* `max_errors` Largest tolerated Hamming distance, in bits. 
* `hit` Receives the location; untouched when nothing matched. 



**Returns:**

Non-zero if a marker was found. 





        

<hr>



### function dp\_syncword\_max\_errors 

_The largest tolerance whose false-frame rate over a search window still meets_ `pfa` _._
```C++
static inline int dp_syncword_max_errors (
    size_t n_marker,
    size_t window_bits,
    double pfa
) 
```



The counterpart of `det_threshold` for this detector, and the answer to the question the signature of `dp_syncword_find` cannot ask: a caller knows how much stream their synchroniser reads before the marker arrives, and that — not the marker length — is what sets the threshold.


Every offset ahead of the true marker is an independent chance to win the race, so `P = 1 - (1 - P_fa(n, t))^W`. `P` rises with `t`, so the largest `t` that holds is the most tolerant threshold that keeps the false-frame rate at or under `pfa`.




**Parameters:**


* `n_marker` Marker length in bits. 
* `window_bits` Offsets tried AHEAD of the marker — the length of stream searched, not the length of the frame. 
* `pfa` Tolerated probability that the window produces a false frame. 



**Returns:**

Tolerance in bits, or -1 when even an exact match (`t = 0`) exceeds `pfa` over that window. 





        

<hr>



### function dp\_syncword\_pfa 

_Probability that ONE random offset false-hits an_ `n_marker` _-bit marker at a tolerance of_`max_errors` _._
```C++
static inline double dp_syncword_pfa (
    size_t n_marker,
    unsigned max_errors
) 
```



`P_fa = 2 * sum_{i <= max_errors} C(n, i) / 2^n` — the factor of two because `dp_syncword_find` searches the complement too, and a random window is as likely to land near one polarity as the other. The two events are disjoint while `2 * max_errors < n_marker`; at and above that every window matches in one polarity or the other, and the result is 1.


This is the per-offset number. What a synchroniser actually cares about is the whole window it searches — see `dp_syncword_max_errors`, which is this function inverted through `1 - (1 - P_fa)^W`.


The terms are summed in log space rather than as binomial coefficients, so a long marker (`C(1030, 515)` overflows a double) is no different from a short one.




**Parameters:**


* `n_marker` Marker length in bits; 0 gives 0. 
* `max_errors` Tolerance in bits. 



**Returns:**

Probability in &#91;0, 1&#93;. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_syncword.h`

