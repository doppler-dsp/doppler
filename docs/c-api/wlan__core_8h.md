

# File wlan\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wlan**](dir_9cf874ec4d1fafc8af25ee841f0634a9.md) **>** [**wlan\_core.h**](wlan__core_8h.md)

[Go to the source code of this file](wlan__core_8h_source.md)

_IEEE 802.11 non-HT OFDM (Clause 17) builders: a PSDU in, the constellation points and the layout the OFDM modulator takes out._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/conv/conv_core.h"`
* `#include "doppler/dp_crc.h"`
* `#include "doppler/ofdm/ofdm_core.h"`
* `#include <stddef.h>`
* `#include <stdint.h>`
* `#include "doppler/mpsk/mpsk_core.h"`
* `#include "doppler/fft/fft_core.h"`
* `#include "doppler/pn/pn_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_wlan\_rate\_t**](structdp__wlan__rate__t.md) <br>_A non-HT rate, by the two columns of Table 17-4 that do not change with the clocking: a constellation and a code rate._  |
| struct | [**dp\_wlan\_signal\_rate\_t**](structdp__wlan__signal__rate__t.md) <br>_A row of_ [_**dp\_wlan\_tables\_t**_](structdp__wlan__tables__t.md) _: Table 17-6 as printed, the data rate and the SIGNAL RATE bits of one pair._ |
| struct | [**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) <br>_A non-HT rate, held with what follows from it._  |
| struct | [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md) <br>_What the standard supplies: the Clause 17 literals._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**dp\_wlan\_code\_t**](#enum-dp_wlan_code_t)  <br>_The code rate of the BCC, with puncturing (Figure 17-9)._  |
| enum  | [**dp\_wlan\_mac\_t**](#enum-dp_wlan_mac_t)  <br>_What the caller supplies, and what the PSDU is (section 3)._  |
| typedef size\_t(\* | [**dp\_wlan\_perm2\_fn**](#typedef-dp_wlan_perm2_fn)  <br>_The second interleaver permutation: input index_ `i` _to output._ |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_wlan\_bcc**](#function-dp_wlan_bcc) ([**dp\_wlan\_code\_t**](wlan__core_8h.md#enum-dp_wlan_code_t) code, [**conv\_code\_t**](structconv__code__t.md) \* c) <br>_The BCC at a code rate: K = 7, generators 133 and 171 octal, with the puncture pattern of the rate._  |
|  [**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* | [**dp\_wlan\_create**](#function-dp_wlan_create) (int constellation, int code\_rate, uint32\_t window, uint32\_t oversample) <br>_Create a rate holder, by the index of the constellation and of the code rate, with the window and the oversampling its PPDUs use._  |
|  [**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* | [**dp\_wlan\_create\_why**](#function-dp_wlan_create_why) (int constellation, int code\_rate, uint32\_t window, uint32\_t oversample, const char \*\* why) <br>[_**dp\_wlan\_create()**_](wlan__core_8h.md#function-dp_wlan_create) _, and the reason when it refuses. The binding calls this one._ |
|  size\_t | [**dp\_wlan\_data\_bits**](#function-dp_wlan_data_bits) (const [**dp\_wlan\_rate\_t**](structdp__wlan__rate__t.md) \* r, const uint8\_t \* psdu, size\_t len, uint8\_t state, uint8\_t \* out, size\_t cap) <br>_The Data field's bits: SERVICE (16 zeros), the PSDU (each octet least significant bit first), six tail bits and pad bits to a whole number of symbols, all scrambled except the tail, which is reset to zero after scrambling._  |
|  size\_t | [**dp\_wlan\_data\_bits\_max\_out**](#function-dp_wlan_data_bits_max_out) (const [**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state, size\_t n\_in) <br>_The capacity_ [_**dp\_wlan\_data\_bits\_why()**_](wlan__core_8h.md#function-dp_wlan_data_bits_why) _needs:_`n_sym * n_dbps` _._ |
|  int64\_t | [**dp\_wlan\_data\_bits\_why**](#function-dp_wlan_data_bits_why) ([**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state, const uint8\_t \* in, size\_t n\_in, uint32\_t scrambler\_state, uint8\_t \* out, size\_t max\_out, const char \*\* why) <br>_The Data field's bits for a PSDU at this object's rate:_ [_**dp\_wlan\_data\_bits()**_](wlan__core_8h.md#function-dp_wlan_data_bits) _with the refusal's reason._ |
|  void | [**dp\_wlan\_destroy**](#function-dp_wlan_destroy) ([**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state) <br>_Destroy a rate holder._  |
|  size\_t | [**dp\_wlan\_encode**](#function-dp_wlan_encode) ([**dp\_wlan\_code\_t**](wlan__core_8h.md#enum-dp_wlan_code_t) code, const uint8\_t \* in, size\_t n, uint8\_t \* out, size\_t cap) <br>_The BCC over a field's bits, punctured to the rate: the encoder starts at zero and runs the whole field._  |
|  size\_t | [**dp\_wlan\_interleave**](#function-dp_wlan_interleave) (const [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md) \* t, unsigned n\_bpsc, const uint8\_t \* in, size\_t n, uint8\_t \* out) <br>_Interleave a coded field, one OFDM symbol of_ `n_cbps` _at a time: the first permutation (_`N_CBPS/16` _rows by 16 columns), then the tables' second permutation._ |
|  int | [**dp\_wlan\_layout**](#function-dp_wlan_layout) ([**dp\_ofdm\_state\_t**](structdp__ofdm__state__t.md) \* o, const [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md) \* t, size\_t n\_sym, unsigned window, unsigned oversample) <br>_Build a PPDU's layout into an OFDM modulator: four classes (L-STF, L-LTF, SIGNAL, Data), the pilots as fixed entries with the pilot polarity, and four runs._  |
|  size\_t | [**dp\_wlan\_map**](#function-dp_wlan_map) (const [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md) \* t, unsigned n\_bpsc, const uint8\_t \* bits, size\_t n, float \_Complex \* out) <br>_Map interleaved bits to constellation points: groups of_ `n_bpsc` _bits, through the library's maps (or the tables' relabeling)._ |
|  size\_t | [**dp\_wlan\_map\_max\_out**](#function-dp_wlan_map_max_out) (const [**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state, size\_t n\_in) <br>_The capacity_ [_**dp\_wlan\_map\_why()**_](wlan__core_8h.md#function-dp_wlan_map_why) _needs:_`n_in / n_bpsc` _points._ |
|  int64\_t | [**dp\_wlan\_map\_why**](#function-dp_wlan_map_why) ([**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state, const uint8\_t \* in, size\_t n\_in, float \_Complex \* out, size\_t max\_out, const char \*\* why) <br>_Map interleaved bits to this rate's constellation points:_ [_**dp\_wlan\_map()**_](wlan__core_8h.md#function-dp_wlan_map) _over the standard tables, with the refusal's reason._ |
|  int | [**dp\_wlan\_mbps**](#function-dp_wlan_mbps) (const [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md) \* t, const [**dp\_wlan\_rate\_t**](structdp__wlan__rate__t.md) \* r, unsigned \* mbps) <br>_The data rate of a pair at 20 MHz, in Mb/s: the table's printed column._  |
|  size\_t | [**dp\_wlan\_mpdu**](#function-dp_wlan_mpdu) ([**dp\_wlan\_mac\_t**](wlan__core_8h.md#enum-dp_wlan_mac_t) mode, const uint8\_t \* header, size\_t header\_len, const uint8\_t \* body, size\_t body\_len, uint8\_t \* out, size\_t out\_cap) <br>_Wrap the caller's octets as one MPDU._  |
|  size\_t | [**dp\_wlan\_mpdu\_len**](#function-dp_wlan_mpdu_len) ([**dp\_wlan\_mac\_t**](wlan__core_8h.md#enum-dp_wlan_mac_t) mode, size\_t header\_len, size\_t body\_len) <br>_The PSDU length a MAC wrap produces._  |
|  size\_t | [**dp\_wlan\_mpdu\_max\_out**](#function-dp_wlan_mpdu_max_out) (const [**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state, size\_t n\_in) <br>_The capacity_ [_**dp\_wlan\_mpdu\_why()**_](wlan__core_8h.md#function-dp_wlan_mpdu_why) _needs: WLAN\_PSDU\_MAX, the longest PSDU._ |
|  int64\_t | [**dp\_wlan\_mpdu\_why**](#function-dp_wlan_mpdu_why) ([**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state, const uint8\_t \* in, size\_t n\_in, const uint8\_t \* header, size\_t header\_len, uint8\_t \* out, size\_t max\_out, const char \*\* why) <br>_A data frame's PSDU from its body and, optionally, its MAC header:_ [_**dp\_wlan\_mpdu()**_](wlan__core_8h.md#function-dp_wlan_mpdu) _in WLAN\_MAC\_DATA mode, or in WLAN\_MAC\_FCS mode when_`header_len` _is 0, with the refusal's reason._ |
|  unsigned | [**dp\_wlan\_n\_bpsc**](#function-dp_wlan_n_bpsc) (int constellation) <br>_Bits per subcarrier of a constellation index: the one map from the index_ [_**dp\_wlan\_create()**_](wlan__core_8h.md#function-dp_wlan_create) _takes to the_`n_bpsc` _of a rate pair._ |
|  unsigned | [**dp\_wlan\_n\_cbps**](#function-dp_wlan_n_cbps) (const [**dp\_wlan\_rate\_t**](structdp__wlan__rate__t.md) \* r) <br>_Coded bits per OFDM symbol,_ `48 * n_bpsc` _._ |
|  unsigned | [**dp\_wlan\_n\_dbps**](#function-dp_wlan_n_dbps) (const [**dp\_wlan\_rate\_t**](structdp__wlan__rate__t.md) \* r) <br>_Data bits per OFDM symbol,_ `N_CBPS * code rate` _._ |
|  size\_t | [**dp\_wlan\_n\_sym**](#function-dp_wlan_n_sym) (const [**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state, size\_t psdu\_len) <br>_The Data OFDM symbols a PSDU needs at the rate this object holds:_ `ceil((16 + 8 * psdu_len + 6) / n_dbps)` _._ |
|  const char \* | [**dp\_wlan\_params\_why**](#function-dp_wlan_params_why) (int constellation, int code\_rate, uint32\_t window, uint32\_t oversample) <br>_Why_ [_**dp\_wlan\_create()**_](wlan__core_8h.md#function-dp_wlan_create) _would refuse these arguments, or NULL if it would not: the first argument that fails, by name._ |
|  size\_t | [**dp\_wlan\_points**](#function-dp_wlan_points) (const [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md) \* t, const [**dp\_wlan\_rate\_t**](structdp__wlan__rate__t.md) \* r, const uint8\_t \* psdu, size\_t len, uint8\_t state, float \_Complex \* out, size\_t cap) <br>_The constellation points a PPDU's layout takes, in layout order: the SIGNAL field's 48, then the Data field's_ `48 * n_sym` _._ |
|  size\_t | [**dp\_wlan\_points\_max\_out**](#function-dp_wlan_points_max_out) (const [**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state, size\_t n\_in) <br>_The capacity_ [_**dp\_wlan\_points\_why()**_](wlan__core_8h.md#function-dp_wlan_points_why) _needs:_`48 + 48 * n_sym` _._ |
|  int64\_t | [**dp\_wlan\_points\_why**](#function-dp_wlan_points_why) ([**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state, const uint8\_t \* in, size\_t n\_in, uint32\_t scrambler\_state, float \_Complex \* out, size\_t max\_out, const char \*\* why) <br>_The constellation points a PSDU's layout takes, at this object's rate:_ [_**dp\_wlan\_points()**_](wlan__core_8h.md#function-dp_wlan_points) _with the refusal's reason._ |
|  size\_t | [**dp\_wlan\_ppdu**](#function-dp_wlan_ppdu) (const [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md) \* t, const [**dp\_wlan\_rate\_t**](structdp__wlan__rate__t.md) \* r, const uint8\_t \* psdu, size\_t len, uint8\_t state, unsigned window, unsigned oversample, float \_Complex \* out, size\_t cap) <br>_Render a PPDU to samples._  |
|  size\_t | [**dp\_wlan\_ppdu\_len**](#function-dp_wlan_ppdu_len) (size\_t n\_sym, unsigned window, unsigned oversample) <br>_The samples a PPDU of_ `n_sym` _Data symbols occupies:_`M * (160 + 160 + 80 + 80 * n_sym)` _, plus the_`window` _samples of the last symbol's window tail, with_`M` _the oversampling factor._ |
|  size\_t | [**dp\_wlan\_ppdu\_max\_out**](#function-dp_wlan_ppdu_max_out) (const [**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state, size\_t n\_in) <br>_The samples a PPDU of_ `n_in` _PSDU octets occupies at this object's rate, window and oversampling: the capacity_[_**dp\_wlan\_ppdu\_why()**_](wlan__core_8h.md#function-dp_wlan_ppdu_why) _needs, exactly._ |
|  int64\_t | [**dp\_wlan\_ppdu\_why**](#function-dp_wlan_ppdu_why) ([**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state, const uint8\_t \* in, size\_t n\_in, uint32\_t scrambler\_state, float \_Complex \* out, size\_t max\_out, const char \*\* why) <br>_Render a PSDU to a whole PPDU: L-STF, L-LTF, SIGNAL and Data, at the rate, window and oversampling this object holds._  |
|  int | [**dp\_wlan\_rate\_from\_mbps**](#function-dp_wlan_rate_from_mbps) (const [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md) \* t, unsigned mbps, [**dp\_wlan\_rate\_t**](structdp__wlan__rate__t.md) \* r) <br>_The pair of a data rate at 20 MHz, from Table 17-6: the inverse of_ [_**dp\_wlan\_mbps()**_](wlan__core_8h.md#function-dp_wlan_mbps) _._ |
|  size\_t | [**dp\_wlan\_rate\_n\_sym**](#function-dp_wlan_rate_n_sym) (const [**dp\_wlan\_rate\_t**](structdp__wlan__rate__t.md) \* r, size\_t psdu\_len) <br>_The Data OFDM symbols a PSDU needs:_ `ceil((16 + 8 * psdu_len + 6) / N_DBPS)` _._ |
|  int | [**dp\_wlan\_rate\_ok**](#function-dp_wlan_rate_ok) (const [**dp\_wlan\_rate\_t**](structdp__wlan__rate__t.md) \* r) <br>_Whether_ `r` _is a rate this component builds:_`n_bpsc` _in {1, 2, 4, 6} and a code rate of the four. The data bits per OFDM symbol are whole for every such pair (the test shows it)._ |
|  void | [**dp\_wlan\_reset**](#function-dp_wlan_reset) ([**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state) <br>_Reset a rate holder: nothing to reset, the rate is configuration._  |
|  size\_t | [**dp\_wlan\_scrambler\_sequence**](#function-dp_wlan_scrambler_sequence) (uint8\_t state, size\_t n, uint8\_t \* out) <br>_The scrambling sequence from a register state._  |
|  uint8\_t | [**dp\_wlan\_scrambler\_state\_from\_sequence**](#function-dp_wlan_scrambler_state_from_sequence) (uint8\_t seq7) <br>_The register state that produces a scrambling sequence whose first seven bits, read as an integer with the first bit most significant, are_ `seq7` _(Annex I writes the example's start this way: 54)._ |
|  size\_t | [**dp\_wlan\_signal\_bits**](#function-dp_wlan_signal_bits) (const [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md) \* t, const [**dp\_wlan\_rate\_t**](structdp__wlan__rate__t.md) \* r, size\_t length, uint8\_t \* out) <br>_The SIGNAL field's 24 bits: RATE, a reserved 0, LENGTH least significant bit first, an even-parity bit over the first 17, and six zero tail bits (Figure 17-5, 17.3.4.1)._  |
|  size\_t | [**dp\_wlan\_signal\_bits\_max\_out**](#function-dp_wlan_signal_bits_max_out) (const [**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state) <br>_The capacity_ [_**dp\_wlan\_signal\_bits\_why()**_](wlan__core_8h.md#function-dp_wlan_signal_bits_why) _needs: WLAN\_SIG\_BITS._ |
|  int64\_t | [**dp\_wlan\_signal\_bits\_why**](#function-dp_wlan_signal_bits_why) ([**dp\_wlan\_state\_t**](structdp__wlan__state__t.md) \* state, size\_t psdu\_len, uint8\_t \* out, size\_t max\_out, const char \*\* why) <br>_The SIGNAL field's 24 bits for a PSDU of_ `psdu_len` _octets at this object's rate (Figure 17-5):_[_**dp\_wlan\_signal\_bits()**_](wlan__core_8h.md#function-dp_wlan_signal_bits) _over the standard tables, with the refusal's reason._ |
|  const [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md) \* | [**dp\_wlan\_tables\_empty**](#function-dp_wlan_tables_empty) (void) <br>_The tables with nothing in them._  |
|  const [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md) \* | [**dp\_wlan\_tables\_standard**](#function-dp_wlan_tables_standard) (void) <br>_The tables of IEEE Std 802.11-2024 Clause 17, transcribed from the text: the RATE bits (Table 17-6), the L-STF and L-LTF sequences (Eq. 17-6, 17-8), one scale for every field, the constellation bit encodings (Figure 17-10, Tables 17-14 to 17-18, as relabels of the library's maps) and the second interleaver permutation (Eq. 17-16, 17-17)._  |
|  uint32\_t | [**dp\_wlan\_window\_resolve**](#function-dp_wlan_window_resolve) (uint32\_t window, uint32\_t oversample) <br>_The window, in samples at the output rate, that a caller's_ `window` _argument means at this oversampling._ |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**dp\_wlan\_window\_standard**](#function-dp_wlan_window_standard) (uint32\_t oversample) <br>_The standard's 100 ns transition at an oversampling:_ `2 * M` _samples (17.3.2.5)._ |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**WLAN\_GI**](wlan__core_8h.md#define-wlan_gi)  `16u`<br>_Guard interval, in samples (0.8 us at 20 MHz)._  |
| define  | [**WLAN\_LTF\_LEN**](wlan__core_8h.md#define-wlan_ltf_len)  `160u`<br>_The L-LTF, in samples (8 us at 20 MHz)._  |
| define  | [**WLAN\_N\_FFT**](wlan__core_8h.md#define-wlan_n_fft)  `64u`<br>_Transform length at 20 MHz (Table 17-5)._  |
| define  | [**WLAN\_N\_SD**](wlan__core_8h.md#define-wlan_n_sd)  `48u`<br>_Data subcarriers per OFDM symbol (Table 17-5)._  |
| define  | [**WLAN\_N\_SP**](wlan__core_8h.md#define-wlan_n_sp)  `4u`<br>_Pilot subcarriers per OFDM symbol (Table 17-5)._  |
| define  | [**WLAN\_N\_SYM\_MAX**](wlan__core_8h.md#define-wlan_n_sym_max)  `1366u`<br>_The most Data OFDM symbols a PPDU can have: the largest PSDU at the slowest rate (BPSK 1/2, 24 data bits per symbol),_ `ceil ((16 + 8 * 4095 + 6) / 24)` _._ |
| define  | [**WLAN\_OVERSAMPLE\_MAX**](wlan__core_8h.md#define-wlan_oversample_max)  `128u`<br>_The largest oversampling factor: a 20 MHz PPDU in a generator running at 2.56 GS/s, so a transform of at most 8192 points._  |
| define  | [**WLAN\_PILOT\_K**](wlan__core_8h.md#define-wlan_pilot_k)  `{ -21, -7, 7, 21 }`<br>_The pilot subcarrier indices (17.3.5.9)._  |
| define  | [**WLAN\_PILOT\_V**](wlan__core_8h.md#define-wlan_pilot_v)  `{ 1.0, 1.0, 1.0, -1.0 }`<br>_The pilot values before the polarity element (17.3.5.10)._  |
| define  | [**WLAN\_POLARITY\_LEN**](wlan__core_8h.md#define-wlan_polarity_len)  `127u`<br>_Length of the pilot polarity sequence._  |
| define  | [**WLAN\_PSDU\_MAX**](wlan__core_8h.md#define-wlan_psdu_max)  `4095u`<br>_The largest PSDU, in octets (LENGTH is 12 bits; 17.3.4.3)._  |
| define  | [**WLAN\_SERVICE\_BITS**](wlan__core_8h.md#define-wlan_service_bits)  `16u`<br>_Bits in the SERVICE field._  |
| define  | [**WLAN\_SIG\_BITS**](wlan__core_8h.md#define-wlan_sig_bits)  `24u`<br>_Bits in the SIGNAL field (24: RATE, reserved, LENGTH, parity, tail)._  |
| define  | [**WLAN\_STF\_LEN**](wlan__core_8h.md#define-wlan_stf_len)  `160u`<br>_The L-STF, in samples (8 us at 20 MHz)._  |
| define  | [**WLAN\_SYM\_LEN**](wlan__core_8h.md#define-wlan_sym_len)  `80u`<br>_An OFDM symbol with its guard interval, in samples._  |
| define  | [**WLAN\_TAIL\_BITS**](wlan__core_8h.md#define-wlan_tail_bits)  `6u`<br>_Tail bits that return the BCC encoder to zero._  |
| define  | [**WLAN\_WINDOW\_STANDARD**](wlan__core_8h.md#define-wlan_window_standard)  `UINT32\_MAX`<br>_The window argument that means "the standard's 100 ns transition":_ [_**dp\_wlan\_window\_resolve()**_](wlan__core_8h.md#function-dp_wlan_window_resolve) _turns it into_`2 * M` _samples. A spelling of_[_**dp\_wlan\_create()**_](wlan__core_8h.md#function-dp_wlan_create) _only; every function below it takes a count of samples at the output rate, and refuses this value as out of range (it is above_`80 * M` _)._`0` _is the explicit rectangular choice._ |

## Detailed Description


`wlan` is a table of PPDU formats and a builder from a format and a PSDU to a layout and the constellation points the layout takes (docs/design/wlan.md section 3). It calls the library's own blocks: the parametric CRC for the FCS, `conv` with puncturing for the BCC, the block interleaver's first permutation, the M-PSK and QAM maps, and the OFDM modulator ([**ofdm\_core.h**](ofdm__core_8h.md)) for the layout. What it adds is the 802.11 rules about field contents, written once.


#### What this header claims, and what it does not



Every number the design page marks checked against the standard's text is a constant here, or a member of [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md), which [**dp\_wlan\_tables\_standard()**](wlan__core_8h.md#function-dp_wlan_tables_standard) returns filled with the Clause 17 literals transcribed from the text. What the test holds against a printed form is the RATE bits, both training sequences, the constellation encodings and the 2/3 and 3/4 puncture masks (Figure 17-9's sent bits). The second interleaver permutation is held by its worked 16-QAM example and by a re-derivation of Eq. (17-16), (17-17) written in the test for every index of all four Clause 17 pairs: that is the equation as posted on #3109, not a printed table. The pilot polarity is not held against a printed form, nor is the 5/6 mask (Clause 19). [**dp\_wlan\_tables\_empty()**](wlan__core_8h.md#function-dp_wlan_tables_empty) is the same struct with nothing in it: a builder that needs an empty entry refuses (returns 0 or -1) and changes nothing, and the machinery tests run on SYNTHETIC tables that claim no conformance. The tables are:



* the SIGNAL field's RATE bits for each (constellation, code rate);
* the L-STF and L-LTF subcarrier values (17.3.3) and each field's normalization;
* the constellation bit-to-level labels, where they differ from the library's maps (Tables 17-15 to 17-18);
* the second permutation of the interleaver (17.3.5.7).





#### A PPDU



L-STF (160 samples at 20 MHz), L-LTF (160), SIGNAL (one OFDM symbol of 80), then `n_sym` Data OFDM symbols of 80. Durations scale with the clocking (20, 10, 5 MHz) and the sample counts do not: this component counts samples at one per `1/bandwidth`.



```C++
dp_wlan_rate_t r = { 4u, WLAN_CODE_3_4 };      // 16-QAM, rate 3/4
size_t n_sym = dp_wlan_rate_n_sym (&r, 100u);  // 100-octet PSDU
```
 



    
## Public Types Documentation




### enum dp\_wlan\_code\_t 

_The code rate of the BCC, with puncturing (Figure 17-9)._ 
```C++
enum dp_wlan_code_t {
    WLAN_CODE_1_2 = 0,
    WLAN_CODE_2_3 = 1,
    WLAN_CODE_3_4 = 2,
    WLAN_CODE_5_6 = 3
};
```



`WLAN_CODE_5_6` is Clause 19's and is unchecked against the text; Clause 17 has no RATE bits for it. 


        

<hr>



### enum dp\_wlan\_mac\_t 

_What the caller supplies, and what the PSDU is (section 3)._ 
```C++
enum dp_wlan_mac_t {
    WLAN_MAC_NONE = 0,
    WLAN_MAC_FCS = 1,
    WLAN_MAC_DATA = 2
};
```




<hr>



### typedef dp\_wlan\_perm2\_fn 

_The second interleaver permutation: input index_ `i` _to output._
```C++
typedef size_t(* dp_wlan_perm2_fn) (size_t i, size_t n_cbps, size_t n_bpsc);
```




<hr>
## Public Functions Documentation




### function dp\_wlan\_bcc 

_The BCC at a code rate: K = 7, generators 133 and 171 octal, with the puncture pattern of the rate._ 
```C++
int dp_wlan_bcc (
    dp_wlan_code_t code,
    conv_code_t * c
) 
```





**Parameters:**


* `code` The rate. 
* `c` The code. Must be non-NULL. 



**Returns:**

1 on success, 0 for an unknown rate. 





        

<hr>



### function dp\_wlan\_create 

_Create a rate holder, by the index of the constellation and of the code rate, with the window and the oversampling its PPDUs use._ 
```C++
dp_wlan_state_t * dp_wlan_create (
    int constellation,
    int code_rate,
    uint32_t window,
    uint32_t oversample
) 
```





**Parameters:**


* `constellation` 0 BPSK, 1 QPSK, 2 16-QAM, 3 64-QAM. 
* `code_rate` 0 for 1/2, 1 for 2/3, 2 for 3/4, 3 for 5/6. 
* `window` Window length in samples at the output rate, or WLAN\_WINDOW\_STANDARD for the standard's 100 ns transition (`2 * M` samples, see [**dp\_wlan\_window\_resolve()**](wlan__core_8h.md#function-dp_wlan_window_resolve)); 0 is no window. A count is 0 to 80 \* `oversample:` the modulator takes no run whose window is longer than an OFDM symbol of its class, and the shortest is the SIGNAL and Data symbol of 80 \* M samples. Above the 16 \* M guard interval the cross-fade reaches into the symbol body; the standard's own transition is 2 \* M. 
* `oversample` `M`, 1 to WLAN\_OVERSAMPLE\_MAX: the output rate over the 20 MHz clock (see [**dp\_wlan\_layout()**](wlan__core_8h.md#function-dp_wlan_layout)). 



**Returns:**

Heap-allocated state, or NULL if an index, `window` or `oversample` is outside its range (every pair of the four constellations and four code rates is built). 




**Note:**

Caller must call [**dp\_wlan\_destroy()**](wlan__core_8h.md#function-dp_wlan_destroy) when done. 





        

<hr>



### function dp\_wlan\_create\_why 

[_**dp\_wlan\_create()**_](wlan__core_8h.md#function-dp_wlan_create) _, and the reason when it refuses. The binding calls this one._
```C++
dp_wlan_state_t * dp_wlan_create_why (
    int constellation,
    int code_rate,
    uint32_t window,
    uint32_t oversample,
    const char ** why
) 
```





**Parameters:**


* `constellation` 0 BPSK, 1 QPSK, 2 16-QAM, 3 64-QAM. 
* `code_rate` 0 for 1/2, 1 for 2/3, 2 for 3/4, 3 for 5/6. 
* `window` Window length in samples at the output rate. 
* `oversample` `M`. 
* `why` On a refusal only, [**dp\_wlan\_params\_why()**](wlan__core_8h.md#function-dp_wlan_params_why)'s sentence; left as the caller set it on success. May be NULL. 



**Returns:**

The state, or NULL with `why` set. 




**Note:**

Caller must call [**dp\_wlan\_destroy()**](wlan__core_8h.md#function-dp_wlan_destroy) when done.



```C++
>>> from doppler.wlan import Wlan
>>> w = Wlan(constellation="qam16", code_rate="r34")  # 36 Mb/s at 20 MHz
>>> w.n_bpsc, w.n_cbps, w.n_dbps
(4, 192, 144)
>>> Wlan(constellation="qam256")
Traceback (most recent call last):
    ...
ValueError: constellation must be one of "bpsk", "qpsk", "qam16", "qam64", got 'qam256'
```
 


        

<hr>



### function dp\_wlan\_data\_bits 

_The Data field's bits: SERVICE (16 zeros), the PSDU (each octet least significant bit first), six tail bits and pad bits to a whole number of symbols, all scrambled except the tail, which is reset to zero after scrambling._ 
```C++
size_t dp_wlan_data_bits (
    const dp_wlan_rate_t * r,
    const uint8_t * psdu,
    size_t len,
    uint8_t state,
    uint8_t * out,
    size_t cap
) 
```





**Parameters:**


* `r` The rate. 
* `psdu` PSDU octets. 
* `len` Their count, 1 to WLAN\_PSDU\_MAX. 
* `state` The scrambler register state, 1..127. 
* `out` `n_sym * N_DBPS` unpacked bits. 
* `cap` Capacity of `out`. 



**Returns:**

Bits written, or 0 for an invalid rate, length or state, or a capacity that is too small. 





        

<hr>



### function dp\_wlan\_data\_bits\_max\_out 

_The capacity_ [_**dp\_wlan\_data\_bits\_why()**_](wlan__core_8h.md#function-dp_wlan_data_bits_why) _needs:_`n_sym * n_dbps` _._
```C++
size_t dp_wlan_data_bits_max_out (
    const dp_wlan_state_t * state,
    size_t n_in
) 
```





**Parameters:**


* `state` A rate holder from [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create). 
* `n_in` PSDU octets. 



**Returns:**

Bits, or 0 for a length outside 1 to WLAN\_PSDU\_MAX. 





        

<hr>



### function dp\_wlan\_data\_bits\_why 

_The Data field's bits for a PSDU at this object's rate:_ [_**dp\_wlan\_data\_bits()**_](wlan__core_8h.md#function-dp_wlan_data_bits) _with the refusal's reason._
```C++
int64_t dp_wlan_data_bits_why (
    dp_wlan_state_t * state,
    const uint8_t * in,
    size_t n_in,
    uint32_t scrambler_state,
    uint8_t * out,
    size_t max_out,
    const char ** why
) 
```





**Parameters:**


* `state` A rate holder from [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create). 
* `in` The PSDU octets. 
* `n_in` Their count, 1 to WLAN\_PSDU\_MAX. 
* `scrambler_state` The scrambler register's initial state, 1 to 127. 
* `out` Receives [**dp\_wlan\_data\_bits\_max\_out()**](wlan__core_8h.md#function-dp_wlan_data_bits_max_out) bits. 
* `max_out` Capacity of `out`. 
* `why` On a refusal only, a static sentence saying why. May be NULL. 



**Returns:**

The bits written, or -1 if the length or the state is out of range or `max_out` is short.



```C++
>>> import numpy as np
>>> from doppler.wlan import Wlan
>>> w = Wlan(constellation="qam16", code_rate="r34")
>>> psdu = np.arange(100, dtype=np.uint8)
>>> w.data_bits(psdu, 93).shape  # n_sym 6 times n_dbps 144
(864,)
>>> w.data_bits(psdu, 128)
Traceback (most recent call last):
    ...
ValueError: the scrambler state is 1 to 127
```
 


        

<hr>



### function dp\_wlan\_destroy 

_Destroy a rate holder._ 
```C++
void dp_wlan_destroy (
    dp_wlan_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_wlan\_encode 

_The BCC over a field's bits, punctured to the rate: the encoder starts at zero and runs the whole field._ 
```C++
size_t dp_wlan_encode (
    dp_wlan_code_t code,
    const uint8_t * in,
    size_t n,
    uint8_t * out,
    size_t cap
) 
```





**Parameters:**


* `code` The rate. 
* `in` Unpacked bits. 
* `n` Their count. 
* `out` The coded, punctured bits. 
* `cap` Capacity of `out`. 



**Returns:**

Bits written, or 0 if `code` is unknown or `cap` is too small. 





        

<hr>



### function dp\_wlan\_interleave 

_Interleave a coded field, one OFDM symbol of_ `n_cbps` _at a time: the first permutation (_`N_CBPS/16` _rows by 16 columns), then the tables' second permutation._
```C++
size_t dp_wlan_interleave (
    const dp_wlan_tables_t * t,
    unsigned n_bpsc,
    const uint8_t * in,
    size_t n,
    uint8_t * out
) 
```





**Parameters:**


* `t` The tables (second permutation). 
* `n_bpsc` Bits per subcarrier. 
* `in` Coded bits, a whole number of symbols. 
* `n` Their count. 
* `out` `n` interleaved bits. 



**Returns:**

`n`, or 0 if the tables hold no second permutation, `n` is not a whole number of symbols, or `n_bpsc` is invalid. 





        

<hr>



### function dp\_wlan\_layout 

_Build a PPDU's layout into an OFDM modulator: four classes (L-STF, L-LTF, SIGNAL, Data), the pilots as fixed entries with the pilot polarity, and four runs._ 
```C++
int dp_wlan_layout (
    dp_ofdm_state_t * o,
    const dp_wlan_tables_t * t,
    size_t n_sym,
    unsigned window,
    unsigned oversample
) 
```





**Parameters:**


* `o` A fresh modulator from [**dp\_ofdm\_create()**](ofdm__core_8h.md#function-dp_ofdm_create). 
* `t` The tables (L-STF and L-LTF values and the scales). 
* `n_sym` Data OFDM symbols, 1 to WLAN\_N\_SYM\_MAX. 
* `window` Window length in samples at each symbol boundary, at the output rate, 0 for none. 
* `oversample` `M`, 1 to WLAN\_OVERSAMPLE\_MAX. The transform is `64 M` long, the subcarriers are the same, and every length (the L-STF, the L-LTF, the guard interval, the body) is `M` times its 20 MHz value: zero-padding the grid before the inverse transform, not a resampler. The transform is unnormalized and the scale is not changed, so `x_M[M n] = x_1[n]` and the unit-power convention holds at every `M`. `M = 1` is the 20 MHz layout. 



**Returns:**

0 and a checked layout, or -1 if a table is empty, `n_sym` is 0 or above WLAN\_N\_SYM\_MAX, `oversample` is outside 1..WLAN\_OVERSAMPLE\_MAX, `window` is above 80 \* `oversample`, or the modulator refuses a call. 




**Note:**

A refusal by the arguments (`t` NULL or a table empty, `n_sym`, `oversample` or `window` out of range) leaves `o` untouched. A refusal by the modulator may leave classes in it, and then it is not to be used again, only destroyed. [**dp\_wlan\_ppdu()**](wlan__core_8h.md#function-dp_wlan_ppdu) builds into its own modulator and destroys it, so it is unaffected. 





        

<hr>



### function dp\_wlan\_map 

_Map interleaved bits to constellation points: groups of_ `n_bpsc` _bits, through the library's maps (or the tables' relabeling)._
```C++
size_t dp_wlan_map (
    const dp_wlan_tables_t * t,
    unsigned n_bpsc,
    const uint8_t * bits,
    size_t n,
    float _Complex * out
) 
```





**Parameters:**


* `t` The tables (relabeling). 
* `n_bpsc` 1, 2, 4 or 6. 
* `bits` Unpacked bits, a whole number of groups. 
* `n` Their count. 
* `out` `n / n_bpsc` unit-average-power points. 



**Returns:**

Points written, or 0 for an invalid `n_bpsc` or a partial group. 





        

<hr>



### function dp\_wlan\_map\_max\_out 

_The capacity_ [_**dp\_wlan\_map\_why()**_](wlan__core_8h.md#function-dp_wlan_map_why) _needs:_`n_in / n_bpsc` _points._
```C++
size_t dp_wlan_map_max_out (
    const dp_wlan_state_t * state,
    size_t n_in
) 
```





**Parameters:**


* `state` A rate holder from [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create). 
* `n_in` Bits. 



**Returns:**

Points. 





        

<hr>



### function dp\_wlan\_map\_why 

_Map interleaved bits to this rate's constellation points:_ [_**dp\_wlan\_map()**_](wlan__core_8h.md#function-dp_wlan_map) _over the standard tables, with the refusal's reason._
```C++
int64_t dp_wlan_map_why (
    dp_wlan_state_t * state,
    const uint8_t * in,
    size_t n_in,
    float _Complex * out,
    size_t max_out,
    const char ** why
) 
```





**Parameters:**


* `state` A rate holder from [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create). 
* `in` Unpacked bits. 
* `n_in` Their count: a whole number of groups of `n_bpsc`, not 0. 
* `out` Receives [**dp\_wlan\_map\_max\_out()**](wlan__core_8h.md#function-dp_wlan_map_max_out) points. 
* `max_out` Capacity of `out`, in points. 
* `why` On a refusal only, a static sentence saying why. May be NULL. 



**Returns:**

The points written, or -1 for a partial group, no bits or a short `max_out`.



```C++
>>> import numpy as np
>>> from doppler.wlan import Wlan
>>> w = Wlan(constellation="qam16", code_rate="r34")
>>> pts = w.map(np.array([0, 0, 0, 0, 1, 1, 1, 1], dtype=np.uint8))
>>> bool(np.allclose(pts, [-0.9487 - 0.9487j, 0.3162 + 0.3162j], atol=1e-3))
True
>>> w.map(np.zeros(7, dtype=np.uint8))
Traceback (most recent call last):
    ...
ValueError: the bits are a whole number of groups of n_bpsc
```
 


        

<hr>



### function dp\_wlan\_mbps 

_The data rate of a pair at 20 MHz, in Mb/s: the table's printed column._ 
```C++
int dp_wlan_mbps (
    const dp_wlan_tables_t * t,
    const dp_wlan_rate_t * r,
    unsigned * mbps
) 
```



Table 17-6 lists eight of the sixteen pairs; the rate is read from its first column, and the unit test holds that column against `N_DBPS / 4` for every row. 

**Parameters:**


* `t` The tables. 
* `r` The pair. 
* `mbps` The rate; written only on success. 



**Returns:**

1 on success, 0 if the table does not list `r` (or gives no rates, as [**dp\_wlan\_tables\_empty()**](wlan__core_8h.md#function-dp_wlan_tables_empty) does). `mbps` is then unchanged. 





        

<hr>



### function dp\_wlan\_mpdu 

_Wrap the caller's octets as one MPDU._ 
```C++
size_t dp_wlan_mpdu (
    dp_wlan_mac_t mode,
    const uint8_t * header,
    size_t header_len,
    const uint8_t * body,
    size_t body_len,
    uint8_t * out,
    size_t out_cap
) 
```



The FCS is CRC-32/ISO-HDLC over every octet before it, appended least significant octet first. The check value of that CRC (`CBF43926` over the nine ASCII digits) is pinned by the test, independent of 802.11.




**Parameters:**


* `mode` The wrap. 
* `header` Header octets (DATA mode; may be NULL otherwise). 
* `header_len` Their count. 
* `body` The caller's octets. 
* `body_len` Their count. 
* `out` Receives [**dp\_wlan\_mpdu\_len()**](wlan__core_8h.md#function-dp_wlan_mpdu_len) octets. 
* `out_cap` Capacity of `out`. 



**Returns:**

Octets written, or 0 (and `out` untouched) if the length is 0 or too large or `out_cap` is too small. 





        

<hr>



### function dp\_wlan\_mpdu\_len 

_The PSDU length a MAC wrap produces._ 
```C++
size_t dp_wlan_mpdu_len (
    dp_wlan_mac_t mode,
    size_t header_len,
    size_t body_len
) 
```





**Parameters:**


* `mode` How the caller's octets are wrapped. 
* `header_len` Header octets (WLAN\_MAC\_DATA only). 
* `body_len` The caller's octets: the body in DATA mode, the whole PSDU or MPDU otherwise. 



**Returns:**

Octets, or 0 if the result is 0 or above WLAN\_PSDU\_MAX. 





        

<hr>



### function dp\_wlan\_mpdu\_max\_out 

_The capacity_ [_**dp\_wlan\_mpdu\_why()**_](wlan__core_8h.md#function-dp_wlan_mpdu_why) _needs: WLAN\_PSDU\_MAX, the longest PSDU._
```C++
size_t dp_wlan_mpdu_max_out (
    const dp_wlan_state_t * state,
    size_t n_in
) 
```





**Parameters:**


* `state` A rate holder from [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create). 
* `n_in` Unused. 



**Returns:**

WLAN\_PSDU\_MAX. 





        

<hr>



### function dp\_wlan\_mpdu\_why 

_A data frame's PSDU from its body and, optionally, its MAC header:_ [_**dp\_wlan\_mpdu()**_](wlan__core_8h.md#function-dp_wlan_mpdu) _in WLAN\_MAC\_DATA mode, or in WLAN\_MAC\_FCS mode when_`header_len` _is 0, with the refusal's reason._
```C++
int64_t dp_wlan_mpdu_why (
    dp_wlan_state_t * state,
    const uint8_t * in,
    size_t n_in,
    const uint8_t * header,
    size_t header_len,
    uint8_t * out,
    size_t max_out,
    const char ** why
) 
```



With no header the body is taken as an MPDU that lacks its FCS and the result is the body and its FCS, which is also what an empty header gives in data mode, so "no header" and "an empty header" are one result. There is no way to ask for the body unchanged: WLAN\_MAC\_NONE is the body itself. The MPDU does not depend on the rate; it is on the object so a caller builds a frame and renders it with one handle.




**Parameters:**


* `state` A rate holder from [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create). 
* `in` The body octets (the whole MPDU without its FCS when there is no header). 
* `n_in` Their count. 
* `header` Header octets, or NULL. 
* `header_len` Their count; 0 for none. 
* `out` Receives the header, the body and the FCS. 
* `max_out` Capacity of `out`. 
* `why` On a refusal only, a static sentence saying why. May be NULL. 



**Returns:**

The octets written, or -1 if the MPDU is 0 or above WLAN\_PSDU\_MAX octets or `max_out` is short.



```C++
>>> import numpy as np
>>> from doppler.wlan import Wlan
>>> w = Wlan()
>>> header = np.arange(24, dtype=np.uint8)
>>> body = np.arange(10, dtype=np.uint8)
>>> psdu = w.mpdu(body, header)  # 24 + 10 + the 4-octet FCS
>>> psdu.shape
(38,)
>>> bytes(psdu[-4:]).hex()  # CRC-32/ISO-HDLC, least significant first
'c0958e66'
>>> w.mpdu(body).shape  # no header: the body and its FCS
(14,)
```
 


        

<hr>



### function dp\_wlan\_n\_bpsc 

_Bits per subcarrier of a constellation index: the one map from the index_ [_**dp\_wlan\_create()**_](wlan__core_8h.md#function-dp_wlan_create) _takes to the_`n_bpsc` _of a rate pair._
```C++
unsigned dp_wlan_n_bpsc (
    int constellation
) 
```





**Parameters:**


* `constellation` 0 BPSK, 1 QPSK, 2 16-QAM, 3 64-QAM. 



**Returns:**

1, 2, 4 or 6, or 0 for an index outside 0 to 3.



```C++
unsigned n = dp_wlan_n_bpsc (2);   // 16-QAM: 4 bits per subcarrier
```
 


        

<hr>



### function dp\_wlan\_n\_cbps 

_Coded bits per OFDM symbol,_ `48 * n_bpsc` _._
```C++
unsigned dp_wlan_n_cbps (
    const dp_wlan_rate_t * r
) 
```





**Parameters:**


* `r` A valid rate. 



**Returns:**

`N_CBPS`, or 0 for an invalid rate. 





        

<hr>



### function dp\_wlan\_n\_dbps 

_Data bits per OFDM symbol,_ `N_CBPS * code rate` _._
```C++
unsigned dp_wlan_n_dbps (
    const dp_wlan_rate_t * r
) 
```





**Parameters:**


* `r` A valid rate. 



**Returns:**

`N_DBPS`, or 0 for an invalid rate. 





        

<hr>



### function dp\_wlan\_n\_sym 

_The Data OFDM symbols a PSDU needs at the rate this object holds:_ `ceil((16 + 8 * psdu_len + 6) / n_dbps)` _._
```C++
size_t dp_wlan_n_sym (
    const dp_wlan_state_t * state,
    size_t psdu_len
) 
```





**Parameters:**


* `state` A rate holder from [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create). 
* `psdu_len` Octets, 1 to WLAN\_PSDU\_MAX. 



**Returns:**

The symbol count, or 0 for a length of 0 or above WLAN\_PSDU\_MAX.



```C++
>>> from doppler.wlan import Wlan
>>> w = Wlan(constellation="qam16", code_rate="r34")
>>> w.n_sym(100)
6
>>> w.n_sym(0)
0
```
 


        

<hr>



### function dp\_wlan\_params\_why 

_Why_ [_**dp\_wlan\_create()**_](wlan__core_8h.md#function-dp_wlan_create) _would refuse these arguments, or NULL if it would not: the first argument that fails, by name._
```C++
const char * dp_wlan_params_why (
    int constellation,
    int code_rate,
    uint32_t window,
    uint32_t oversample
) 
```



The oversampling is checked before the window, because the window's limit depends on it. 

**Parameters:**


* `constellation` 0 BPSK, 1 QPSK, 2 16-QAM, 3 64-QAM. 
* `code_rate` 0 for 1/2, 1 for 2/3, 2 for 3/4, 3 for 5/6. 
* `window` Window length in samples at the output rate. 
* `oversample` `M`. 



**Returns:**

A static sentence naming the argument and its range, or NULL. 





        

<hr>



### function dp\_wlan\_points 

_The constellation points a PPDU's layout takes, in layout order: the SIGNAL field's 48, then the Data field's_ `48 * n_sym` _._
```C++
size_t dp_wlan_points (
    const dp_wlan_tables_t * t,
    const dp_wlan_rate_t * r,
    const uint8_t * psdu,
    size_t len,
    uint8_t state,
    float _Complex * out,
    size_t cap
) 
```



The SIGNAL field is BPSK at rate 1/2, not scrambled; the Data field runs [**dp\_wlan\_data\_bits()**](wlan__core_8h.md#function-dp_wlan_data_bits), [**dp\_wlan\_encode()**](wlan__core_8h.md#function-dp_wlan_encode), [**dp\_wlan\_interleave()**](wlan__core_8h.md#function-dp_wlan_interleave) and [**dp\_wlan\_map()**](wlan__core_8h.md#function-dp_wlan_map).




**Parameters:**


* `t` The tables. 
* `r` The rate. 
* `psdu` PSDU octets. 
* `len` Their count. 
* `state` The scrambler register state, 1..127. 
* `out` Receives `48 + 48 * n_sym` points. 
* `cap` Capacity of `out`, in points. 



**Returns:**

Points written, or 0 if any stage refuses. 





        

<hr>



### function dp\_wlan\_points\_max\_out 

_The capacity_ [_**dp\_wlan\_points\_why()**_](wlan__core_8h.md#function-dp_wlan_points_why) _needs:_`48 + 48 * n_sym` _._
```C++
size_t dp_wlan_points_max_out (
    const dp_wlan_state_t * state,
    size_t n_in
) 
```





**Parameters:**


* `state` A rate holder from [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create). 
* `n_in` PSDU octets. 



**Returns:**

Points, or 0 for a length outside 1 to WLAN\_PSDU\_MAX. 





        

<hr>



### function dp\_wlan\_points\_why 

_The constellation points a PSDU's layout takes, at this object's rate:_ [_**dp\_wlan\_points()**_](wlan__core_8h.md#function-dp_wlan_points) _with the refusal's reason._
```C++
int64_t dp_wlan_points_why (
    dp_wlan_state_t * state,
    const uint8_t * in,
    size_t n_in,
    uint32_t scrambler_state,
    float _Complex * out,
    size_t max_out,
    const char ** why
) 
```





**Parameters:**


* `state` A rate holder from [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create). 
* `in` The PSDU octets. 
* `n_in` Their count, 1 to WLAN\_PSDU\_MAX. 
* `scrambler_state` The scrambler register's initial state, 1 to 127. 
* `out` Receives [**dp\_wlan\_points\_max\_out()**](wlan__core_8h.md#function-dp_wlan_points_max_out) points. 
* `max_out` Capacity of `out`, in points. 
* `why` On a refusal only, a static sentence saying why. May be NULL. 



**Returns:**

The points written, or -1 if the rate has no SIGNAL RATE bits, the length or the state is out of range or `max_out` is short.



```C++
>>> import numpy as np
>>> from doppler.wlan import Wlan
>>> w = Wlan(constellation="qam16", code_rate="r34")
>>> psdu = np.arange(100, dtype=np.uint8)
>>> w.points(psdu, 93).shape  # 48 SIGNAL + 48 * 6 Data
(336,)
>>> w.points_max_out(100)
336
>>> w.points(psdu, 0)
Traceback (most recent call last):
    ...
ValueError: the scrambler state is 1 to 127
```
 


        

<hr>



### function dp\_wlan\_ppdu 

_Render a PPDU to samples._ 
```C++
size_t dp_wlan_ppdu (
    const dp_wlan_tables_t * t,
    const dp_wlan_rate_t * r,
    const uint8_t * psdu,
    size_t len,
    uint8_t state,
    unsigned window,
    unsigned oversample,
    float _Complex * out,
    size_t cap
) 
```





**Parameters:**


* `t` The tables. 
* `r` The rate. 
* `psdu` PSDU octets. 
* `len` Their count. 
* `state` The scrambler register state, 1..127. 
* `window` Window length in samples at the output rate, 0 for none. 
* `oversample` `M`, 1 to WLAN\_OVERSAMPLE\_MAX (see [**dp\_wlan\_layout()**](wlan__core_8h.md#function-dp_wlan_layout)). 
* `out` Receives [**dp\_wlan\_ppdu\_len()**](wlan__core_8h.md#function-dp_wlan_ppdu_len) samples. 
* `cap` Capacity of `out`, in samples. 



**Returns:**

Samples written, or 0 if any stage refuses (changing nothing). 





        

<hr>



### function dp\_wlan\_ppdu\_len 

_The samples a PPDU of_ `n_sym` _Data symbols occupies:_`M * (160 + 160 + 80 + 80 * n_sym)` _, plus the_`window` _samples of the last symbol's window tail, with_`M` _the oversampling factor._
```C++
size_t dp_wlan_ppdu_len (
    size_t n_sym,
    unsigned window,
    unsigned oversample
) 
```





**Parameters:**


* `n_sym` Data OFDM symbols, 1 to WLAN\_N\_SYM\_MAX. 
* `window` Window length in samples at the output rate, 0 for none. A count: WLAN\_WINDOW\_STANDARD is resolved by the caller ([**dp\_wlan\_window\_resolve()**](wlan__core_8h.md#function-dp_wlan_window_resolve)), not here. 
* `oversample` `M`: the output rate over the 20 MHz clock, 1 to WLAN\_OVERSAMPLE\_MAX (128). 



**Returns:**

Samples, or 0 if `n_sym` or `oversample` is outside its range (the ranges [**dp\_wlan\_layout()**](wlan__core_8h.md#function-dp_wlan_layout) accepts). 





        

<hr>



### function dp\_wlan\_ppdu\_max\_out 

_The samples a PPDU of_ `n_in` _PSDU octets occupies at this object's rate, window and oversampling: the capacity_[_**dp\_wlan\_ppdu\_why()**_](wlan__core_8h.md#function-dp_wlan_ppdu_why) _needs, exactly._
```C++
size_t dp_wlan_ppdu_max_out (
    const dp_wlan_state_t * state,
    size_t n_in
) 
```





**Parameters:**


* `state` A rate holder from [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create). 
* `n_in` PSDU octets, 1 to WLAN\_PSDU\_MAX. 



**Returns:**

Samples, or 0 for a length outside that range or for a rate with no SIGNAL RATE bits (Table 17-6 lists eight of the sixteen pairs; the others cannot be sent, and `rate_mbps` is 0 for them). 





        

<hr>



### function dp\_wlan\_ppdu\_why 

_Render a PSDU to a whole PPDU: L-STF, L-LTF, SIGNAL and Data, at the rate, window and oversampling this object holds._ 
```C++
int64_t dp_wlan_ppdu_why (
    dp_wlan_state_t * state,
    const uint8_t * in,
    size_t n_in,
    uint32_t scrambler_state,
    float _Complex * out,
    size_t max_out,
    const char ** why
) 
```



The standard's tables are the library's ([**dp\_wlan\_tables\_standard()**](wlan__core_8h.md#function-dp_wlan_tables_standard)); the PSDU is the caller's octets as given, FCS included. The result is the [**dp\_wlan\_ppdu()**](wlan__core_8h.md#function-dp_wlan_ppdu) output for the same arguments, with the refusal's reason.




**Parameters:**


* `state` A rate holder from [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create). 
* `in` The PSDU octets. 
* `n_in` Their count, 1 to WLAN\_PSDU\_MAX. 
* `scrambler_state` The scrambler register's initial state, 1 to 127. 
* `out` Receives [**dp\_wlan\_ppdu\_max\_out()**](wlan__core_8h.md#function-dp_wlan_ppdu_max_out) samples. 
* `max_out` Capacity of `out`, in samples. 
* `why` On a refusal only, a static sentence saying why; left as the caller set it on success. May be NULL. 



**Returns:**

The samples written, or -1 if the rate has no SIGNAL RATE bits, the PSDU length or the scrambler state is out of range, or `max_out` is short; `out` is then untouched. On the Python face a short `out=` is refused before the other arguments are, by the binding.



```C++
>>> import numpy as np
>>> from doppler.wlan import Wlan
>>> w = Wlan(constellation="qam16", code_rate="r34")  # 36 Mb/s
>>> psdu = np.arange(100, dtype=np.uint8)
>>> x = w.ppdu(psdu, 93)
>>> x.dtype, x.shape  # 160 + 160 + 80 + 80 * 6, plus the 2-sample window
(dtype('complex64'), (882,))
>>> w2 = Wlan(constellation="qam16", code_rate="r34", oversample=2)
>>> w2.ppdu(psdu, 93).shape  # twice as fine, and a 4-sample window
(1764,)
>>> w.ppdu(psdu, 0)
Traceback (most recent call last):
    ...
ValueError: the scrambler state is 1 to 127
```
 


        

<hr>



### function dp\_wlan\_rate\_from\_mbps 

_The pair of a data rate at 20 MHz, from Table 17-6: the inverse of_ [_**dp\_wlan\_mbps()**_](wlan__core_8h.md#function-dp_wlan_mbps) _._
```C++
int dp_wlan_rate_from_mbps (
    const dp_wlan_tables_t * t,
    unsigned mbps,
    dp_wlan_rate_t * r
) 
```





**Parameters:**


* `t` The tables. 
* `mbps` The rate: 6, 9, 12, 18, 24, 36, 48 or 54 in the standard's table. 
* `r` The pair; written only on success. 



**Returns:**

1 on success, 0 if the table lists no such rate. `r` is then unchanged. 





        

<hr>



### function dp\_wlan\_rate\_n\_sym 

_The Data OFDM symbols a PSDU needs:_ `ceil((16 + 8 * psdu_len + 6) / N_DBPS)` _._
```C++
size_t dp_wlan_rate_n_sym (
    const dp_wlan_rate_t * r,
    size_t psdu_len
) 
```





**Parameters:**


* `r` A valid rate. 
* `psdu_len` Octets, 1 to WLAN\_PSDU\_MAX. 



**Returns:**

The symbol count, or 0 for an invalid rate or length. 





        

<hr>



### function dp\_wlan\_rate\_ok 

_Whether_ `r` _is a rate this component builds:_`n_bpsc` _in {1, 2, 4, 6} and a code rate of the four. The data bits per OFDM symbol are whole for every such pair (the test shows it)._
```C++
int dp_wlan_rate_ok (
    const dp_wlan_rate_t * r
) 
```





**Parameters:**


* `r` The rate. 



**Returns:**

1 if it is, 0 if not. 





        

<hr>



### function dp\_wlan\_reset 

_Reset a rate holder: nothing to reset, the rate is configuration._ 
```C++
void dp_wlan_reset (
    dp_wlan_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.wlan import Wlan
>>> w = Wlan(constellation="qpsk", code_rate="r34")
>>> w.reset()                      # nothing runs, so nothing changes
>>> w.n_dbps
72
```
 


        

<hr>



### function dp\_wlan\_scrambler\_sequence 

_The scrambling sequence from a register state._ 
```C++
size_t dp_wlan_scrambler_sequence (
    uint8_t state,
    size_t n,
    uint8_t * out
) 
```





**Parameters:**


* `state` The register state, 1..127 (`x1` the most significant bit). 
* `n` Bits wanted. 
* `out` `n` unpacked bits. 



**Returns:**

`n`, or 0 for a state of 0 or above 127. 





        

<hr>



### function dp\_wlan\_scrambler\_state\_from\_sequence 

_The register state that produces a scrambling sequence whose first seven bits, read as an integer with the first bit most significant, are_ `seq7` _(Annex I writes the example's start this way: 54)._
```C++
uint8_t dp_wlan_scrambler_state_from_sequence (
    uint8_t seq7
) 
```



The sequence is the library's `pn` (a Galois register, polynomial `0x48`, seven bits) and obeys `s[n] = s[n-7] xor s[n-4]`, so it is its own history: the register `x1..x7` is `s[0], s[-1], ..., s[-6]` (an integer with `x1` most significant) and the first output is `x7 xor x4`. The state that produces given first seven bits is the recurrence run backward seven steps, and the state 1011101 (93) produces the sequence 0110110 (54).




**Parameters:**


* `seq7` The first seven sequence bits, 1..127. 



**Returns:**

The register state, 1..127; 0 for 0 or a value above 127. 





        

<hr>



### function dp\_wlan\_signal\_bits 

_The SIGNAL field's 24 bits: RATE, a reserved 0, LENGTH least significant bit first, an even-parity bit over the first 17, and six zero tail bits (Figure 17-5, 17.3.4.1)._ 
```C++
size_t dp_wlan_signal_bits (
    const dp_wlan_tables_t * t,
    const dp_wlan_rate_t * r,
    size_t length,
    uint8_t * out
) 
```





**Parameters:**


* `t` The tables (RATE bits). 
* `r` The rate. 
* `length` PSDU octets, 1 to WLAN\_PSDU\_MAX. 
* `out` WLAN\_SIG\_BITS unpacked bits. 



**Returns:**

WLAN\_SIG\_BITS, or 0 if the rate has no RATE bits in the tables or the length is outside 1 to WLAN\_PSDU\_MAX. 





        

<hr>



### function dp\_wlan\_signal\_bits\_max\_out 

_The capacity_ [_**dp\_wlan\_signal\_bits\_why()**_](wlan__core_8h.md#function-dp_wlan_signal_bits_why) _needs: WLAN\_SIG\_BITS._
```C++
size_t dp_wlan_signal_bits_max_out (
    const dp_wlan_state_t * state
) 
```





**Parameters:**


* `state` A rate holder from [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create). 



**Returns:**

WLAN\_SIG\_BITS. 





        

<hr>



### function dp\_wlan\_signal\_bits\_why 

_The SIGNAL field's 24 bits for a PSDU of_ `psdu_len` _octets at this object's rate (Figure 17-5):_[_**dp\_wlan\_signal\_bits()**_](wlan__core_8h.md#function-dp_wlan_signal_bits) _over the standard tables, with the refusal's reason._
```C++
int64_t dp_wlan_signal_bits_why (
    dp_wlan_state_t * state,
    size_t psdu_len,
    uint8_t * out,
    size_t max_out,
    const char ** why
) 
```





**Parameters:**


* `state` A rate holder from [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create). 
* `psdu_len` PSDU octets, 1 to WLAN\_PSDU\_MAX. 
* `out` Receives WLAN\_SIG\_BITS unpacked bits. 
* `max_out` Capacity of `out`. 
* `why` On a refusal only, a static sentence saying why. May be NULL. 



**Returns:**

The bits written, or -1 if the rate has no SIGNAL RATE bits, the length is out of range or `max_out` is short.



```C++
>>> from doppler.wlan import Wlan
>>> w = Wlan(constellation="qam16", code_rate="r34")  # 36 Mb/s
>>> w.signal_bits(100).tolist()  # RATE, 0, LENGTH lsb first, parity, tail
[1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
>>> w.signal_bits(0)
Traceback (most recent call last):
    ...
ValueError: the PSDU is 1 to 4095 octets
```
 


        

<hr>



### function dp\_wlan\_tables\_empty 

_The tables with nothing in them._ 
```C++
const dp_wlan_tables_t * dp_wlan_tables_empty (
    void
) 
```





**Returns:**

A pointer to a static, all-empty [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md). 





        

<hr>



### function dp\_wlan\_tables\_standard 

_The tables of IEEE Std 802.11-2024 Clause 17, transcribed from the text: the RATE bits (Table 17-6), the L-STF and L-LTF sequences (Eq. 17-6, 17-8), one scale for every field, the constellation bit encodings (Figure 17-10, Tables 17-14 to 17-18, as relabels of the library's maps) and the second interleaver permutation (Eq. 17-16, 17-17)._ 
```C++
const dp_wlan_tables_t * dp_wlan_tables_standard (
    void
) 
```



The 5/6 code rate has no entry: it is Clause 19's, not Clause 17's, and [**dp\_wlan\_signal\_bits()**](wlan__core_8h.md#function-dp_wlan_signal_bits) refuses it. 

**Returns:**

A pointer to a static, filled [**dp\_wlan\_tables\_t**](structdp__wlan__tables__t.md). 





        

<hr>



### function dp\_wlan\_window\_resolve 

_The window, in samples at the output rate, that a caller's_ `window` _argument means at this oversampling._
```C++
uint32_t dp_wlan_window_resolve (
    uint32_t window,
    uint32_t oversample
) 
```



WLAN\_WINDOW\_STANDARD gives `2 * oversample`, the standard's 100 ns transition (17.3.2.5); any other value is returned unchanged, so 0 stays the explicit rectangular choice. The one place the default is defined: [**dp\_wlan\_create()**](wlan__core_8h.md#function-dp_wlan_create) and the wfmgen source call it, and a caller who wants the default for a call that takes a count ([**dp\_wlan\_layout()**](wlan__core_8h.md#function-dp_wlan_layout), [**dp\_wlan\_ppdu()**](wlan__core_8h.md#function-dp_wlan_ppdu)) asks it too. It does not range-check: [**dp\_wlan\_params\_why()**](wlan__core_8h.md#function-dp_wlan_params_why) does, on the resolved count. 

**Parameters:**


* `window` A count, or WLAN\_WINDOW\_STANDARD. 
* `oversample` `M`. 



**Returns:**

The count.



```C++
>>> from doppler.wlan import Wlan
>>> Wlan(oversample=4).window   # the default: 100 ns at 80 MHz
8
>>> Wlan(window=0, oversample=4).window   # rectangular, by asking
0
```
 


        

<hr>
## Public Static Functions Documentation




### function dp\_wlan\_window\_standard 

_The standard's 100 ns transition at an oversampling:_ `2 * M` _samples (17.3.2.5)._
```C++
static inline uint32_t dp_wlan_window_standard (
    uint32_t oversample
) 
```



The one definition of the rule. It is `static inline` so that wfmgen's draw library, which links no DSP, calls it instead of keeping a copy; [**dp\_wlan\_window\_resolve()**](wlan__core_8h.md#function-dp_wlan_window_resolve) returns it for WLAN\_WINDOW\_STANDARD.




**Parameters:**


* `oversample` `M`. 



**Returns:**

The count. 





        

<hr>
## Macro Definition Documentation





### define WLAN\_GI 

_Guard interval, in samples (0.8 us at 20 MHz)._ 
```C++
#define WLAN_GI `16u`
```




<hr>



### define WLAN\_LTF\_LEN 

_The L-LTF, in samples (8 us at 20 MHz)._ 
```C++
#define WLAN_LTF_LEN `160u`
```




<hr>



### define WLAN\_N\_FFT 

_Transform length at 20 MHz (Table 17-5)._ 
```C++
#define WLAN_N_FFT `64u`
```




<hr>



### define WLAN\_N\_SD 

_Data subcarriers per OFDM symbol (Table 17-5)._ 
```C++
#define WLAN_N_SD `48u`
```




<hr>



### define WLAN\_N\_SP 

_Pilot subcarriers per OFDM symbol (Table 17-5)._ 
```C++
#define WLAN_N_SP `4u`
```




<hr>



### define WLAN\_N\_SYM\_MAX 

_The most Data OFDM symbols a PPDU can have: the largest PSDU at the slowest rate (BPSK 1/2, 24 data bits per symbol),_ `ceil ((16 + 8 * 4095 + 6) / 24)` _._
```C++
#define WLAN_N_SYM_MAX `1366u`
```




<hr>



### define WLAN\_OVERSAMPLE\_MAX 

_The largest oversampling factor: a 20 MHz PPDU in a generator running at 2.56 GS/s, so a transform of at most 8192 points._ 
```C++
#define WLAN_OVERSAMPLE_MAX `128u`
```




<hr>



### define WLAN\_PILOT\_K 

_The pilot subcarrier indices (17.3.5.9)._ 
```C++
#define WLAN_PILOT_K `{ -21, -7, 7, 21 }`
```




<hr>



### define WLAN\_PILOT\_V 

_The pilot values before the polarity element (17.3.5.10)._ 
```C++
#define WLAN_PILOT_V `{ 1.0, 1.0, 1.0, -1.0 }`
```




<hr>



### define WLAN\_POLARITY\_LEN 

_Length of the pilot polarity sequence._ 
```C++
#define WLAN_POLARITY_LEN `127u`
```




<hr>



### define WLAN\_PSDU\_MAX 

_The largest PSDU, in octets (LENGTH is 12 bits; 17.3.4.3)._ 
```C++
#define WLAN_PSDU_MAX `4095u`
```




<hr>



### define WLAN\_SERVICE\_BITS 

_Bits in the SERVICE field._ 
```C++
#define WLAN_SERVICE_BITS `16u`
```




<hr>



### define WLAN\_SIG\_BITS 

_Bits in the SIGNAL field (24: RATE, reserved, LENGTH, parity, tail)._ 
```C++
#define WLAN_SIG_BITS `24u`
```




<hr>



### define WLAN\_STF\_LEN 

_The L-STF, in samples (8 us at 20 MHz)._ 
```C++
#define WLAN_STF_LEN `160u`
```




<hr>



### define WLAN\_SYM\_LEN 

_An OFDM symbol with its guard interval, in samples._ 
```C++
#define WLAN_SYM_LEN `80u`
```




<hr>



### define WLAN\_TAIL\_BITS 

_Tail bits that return the BCC encoder to zero._ 
```C++
#define WLAN_TAIL_BITS `6u`
```




<hr>



### define WLAN\_WINDOW\_STANDARD 

_The window argument that means "the standard's 100 ns transition":_ [_**dp\_wlan\_window\_resolve()**_](wlan__core_8h.md#function-dp_wlan_window_resolve) _turns it into_`2 * M` _samples. A spelling of_[_**dp\_wlan\_create()**_](wlan__core_8h.md#function-dp_wlan_create) _only; every function below it takes a count of samples at the output rate, and refuses this value as out of range (it is above_`80 * M` _)._`0` _is the explicit rectangular choice._
```C++
#define WLAN_WINDOW_STANDARD `UINT32_MAX`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wlan/wlan_core.h`

