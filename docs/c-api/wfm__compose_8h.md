

# File wfm\_compose.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_compose.h**](wfm__compose_8h.md)

[Go to the source code of this file](wfm__compose_8h_source.md)

_Multi-segment waveform composer (Phase B)._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/wfm_synth/wfm_synth_core.h"`
* `#include "doppler/wfm/wfm_frame.h"`
* `#include "doppler/wfm/wfm_data.h"`
* `#include "doppler/doppler_channel/doppler_channel_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**wfm\_draw\_t**](structwfm__draw__t.md) <br>_One rendered source instance: its timing AND the values it was actually rendered with._  |
| struct | [**wfm\_segment\_t**](structwfm__segment__t.md) <br>_One composer segment: one or more sources summed over the same span, then a trailing off-time gap._  |
| struct | [**wfm\_source\_t**](structwfm__source__t.md) <br>_One additive source within a segment: a_ `synth` _config + its level._ |
| struct | [**wfm\_span\_t**](structwfm__span__t.md) <br>_One rendered segment instance's exact timing: where it lands in the composed stream and how its_ `delay | on | off` _spans divide it._ |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct wfm\_compose\_state | [**dp\_wfm\_compose\_state\_t**](#typedef-dp_wfm_compose_state_t)  <br> |
| enum  | [**wfm\_\_compose\_8h\_1ab04a0655cd1e3bcac5e8f48c18df1a57**](#enum-wfm__compose_8h_1ab04a0655cd1e3bcac5e8f48c18df1a57)  <br>_Per-field "draw uniformly each repeat" flags (_ `ranged` _bitmask)._ |
| enum  | [**wfm\_bitmod\_t**](#enum-wfm_bitmod_t)  <br>_How a_ `WFM_SYNTH_BITS` _source maps its payload to symbols._ |
| enum  | [**wfm\_doppler\_lifetime\_t**](#enum-wfm_doppler_lifetime_t)  <br>_When a source's Doppler channel restarts._  |
| typedef struct wfm\_render | [**wfm\_render\_t**](#typedef-wfm_render_t)  <br>_One source's renderer: its synth, plus its Doppler channel._  |
| enum  | [**wfm\_seed\_advance\_t**](#enum-wfm_seed_advance_t)  <br>_Per-repeat seed policy for a looped/continuous stream._  |
| enum  | [**wfm\_snr\_mode\_t**](#enum-wfm_snr_mode_t)  <br>_What a source's_ `snr` _is measured against._ |




## Public Attributes

| Type | Name |
| ---: | :--- |
|  const char | [**dp\_wfm\_why\_dsss\_cont\_no\_data\_code**](#variable-dp_wfm_why_dsss_cont_no_data_code)  <br>_The reasons_ [_**dp\_wfm\_source\_error()**_](wfm__compose_8h.md#function-dp_wfm_source_error) _gives a dsss source missing a code (doppler#1696): a burst whose frame has no data\_code to spread it, a burst with neither a preamble nor a frame, and a continuous stream with no data\_code. Exported so a test or a face can hold a refusal to its reason by identity._ |
|  const char | [**dp\_wfm\_why\_dsss\_cont\_rate**](#variable-dp_wfm_why_dsss_cont_rate)  <br>_The reason_ [_**dp\_wfm\_scene\_error()**_](wfm__compose_8h.md#function-dp_wfm_scene_error) _gives a continuous dsss source whose chip rate is below its symbol rate_ _exported so the wfmgen CLI can name the values beside it, by identity._ |
|  const char | [**dp\_wfm\_why\_dsss\_empty**](#variable-dp_wfm_why_dsss_empty)  <br>_The reasons_ [_**dp\_wfm\_source\_error()**_](wfm__compose_8h.md#function-dp_wfm_source_error) _gives a dsss source missing a code (doppler#1696): a burst whose frame has no data\_code to spread it, a burst with neither a preamble nor a frame, and a continuous stream with no data\_code. Exported so a test or a face can hold a refusal to its reason by identity._ |
|  const char | [**dp\_wfm\_why\_dsss\_frame\_no\_data\_code**](#variable-dp_wfm_why_dsss_frame_no_data_code)  <br>_The reasons_ [_**dp\_wfm\_source\_error()**_](wfm__compose_8h.md#function-dp_wfm_source_error) _gives a dsss source missing a code (doppler#1696): a burst whose frame has no data\_code to spread it, a burst with neither a preamble nor a frame, and a continuous stream with no data\_code. Exported so a test or a face can hold a refusal to its reason by identity._ |
|  const char | [**dp\_wfm\_why\_pn\_poly**](#variable-dp_wfm_why_pn_poly)  <br>_The reason_ [_**dp\_wfm\_source\_error()**_](wfm__compose_8h.md#function-dp_wfm_source_error) _gives for a_`pn_poly` _wider than its register_ _exported so a face that knows the values (the wfmgen CLI) can name them beside it, by identity rather than by matching text._ |
|  const char | [**dp\_wfm\_why\_retired\_bits**](#variable-dp_wfm_why_retired_bits)  <br>_The reason_ [_**dp\_wfm\_source\_error()**_](wfm__compose_8h.md#function-dp_wfm_source_error) _gives for_`retired_bits` _set: Python's retired_`bits=` _(and_`payload=` _,_`pattern=` _), named once. The CLI's and a scene's RETIRED tables say the same in their own spelling (doppler#1718)._ |
















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**wfm\_render\_t**](wfm__compose_8h.md#typedef-wfm_render_t) \* | [**dp\_wfm\_compose\_build\_render**](#function-dp_wfm_compose_build_render) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src, double fs, size\_t on\_len, double freq, double snr, double f\_end, double doppler, double doppler\_rate, unsigned epoch, int seed\_advance, size\_t instance, [**dp\_doppler\_channel\_state\_t**](structdp__doppler__channel__state__t.md) \* borrow) <br>_Build a source's renderer —_ `dp_wfm_compose_build_synth` _plus the clock-Doppler channel the source declares, if it declares one._ |
|  [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* | [**dp\_wfm\_compose\_build\_synth**](#function-dp_wfm_compose_build_synth) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src, double fs, size\_t on\_len, double freq, double snr, double f\_end, unsigned epoch, int seed\_advance, size\_t instance) <br>_Construct + configure the synth for one resolved source._  |
|  [**dp\_wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-dp_wfm_compose_state_t) \* | [**dp\_wfm\_compose\_create**](#function-dp_wfm_compose_create) (const [**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n\_segs, int repeat, int continuous) <br>_Build a composer over a copy of_ `segs` _._ |
|  [**dp\_wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-dp_wfm_compose_state_t) \* | [**dp\_wfm\_compose\_create\_why**](#function-dp_wfm_compose_create_why) (const [**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n\_segs, int repeat, int continuous, const char \*\* why) <br>[_**dp\_wfm\_compose\_create()**_](wfm__compose_8h.md#function-dp_wfm_compose_create) _, able to say why the scene was refused._ |
|  void | [**dp\_wfm\_compose\_destroy**](#function-dp_wfm_compose_destroy) ([**dp\_wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-dp_wfm_compose_state_t) \* state) <br>_Destroy a composer and its active synth._  |
|  size\_t | [**dp\_wfm\_compose\_draws**](#function-dp_wfm_compose_draws) (const [**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n\_segs, [**wfm\_draw\_t**](structwfm__draw__t.md) \* out, size\_t cap) <br>_Replay the (epoch 0) instance timeline AND its drawn source values._  |
|  size\_t | [**dp\_wfm\_compose\_execute**](#function-dp_wfm_compose_execute) ([**dp\_wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-dp_wfm_compose_state_t) \* state, float \_Complex \* out, size\_t max) <br>_Emit up to_ `max` _samples of the composed stream._ |
|  size\_t | [**dp\_wfm\_compose\_execute\_rate**](#function-dp_wfm_compose_execute_rate) ([**dp\_wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-dp_wfm_compose_state_t) \* state, float \_Complex \* out, size\_t max, double \* fs) <br>_Emit up to_ `max` _samples, all at ONE sample rate, and say which._ |
|  [**dp\_wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-dp_wfm_compose_state_t) \* | [**dp\_wfm\_compose\_from\_file**](#function-dp_wfm_compose_from_file) (const char \* path) <br> |
|  [**dp\_wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-dp_wfm_compose_state_t) \* | [**dp\_wfm\_compose\_from\_file\_why**](#function-dp_wfm_compose_from_file_why) (const char \* path, const char \*\* why) <br>[_**dp\_wfm\_compose\_from\_file**_](wfm__compose_8h.md#function-dp_wfm_compose_from_file) _, able to say why a scene was refused._ |
|  [**dp\_wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-dp_wfm_compose_state_t) \* | [**dp\_wfm\_compose\_from\_json**](#function-dp_wfm_compose_from_json) (const char \* json) <br>_Build a composer from a JSON spec string (for_  _from-file)._ |
|  [**dp\_wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-dp_wfm_compose_state_t) \* | [**dp\_wfm\_compose\_from\_json\_at**](#function-dp_wfm_compose_from_json_at) (const char \* json, const char \* base, const char \*\* why) <br>_Build a composer from a JSON spec file._  |
|  [**dp\_wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-dp_wfm_compose_state_t) \* | [**dp\_wfm\_compose\_from\_json\_why**](#function-dp_wfm_compose_from_json_why) (const char \* json, const char \*\* why) <br>_The same, but able to say why a FRAME was refused._  |
|  int | [**dp\_wfm\_compose\_seed\_advance**](#function-dp_wfm_compose_seed_advance) (const [**dp\_wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-dp_wfm_compose_state_t) \* state) <br>_The composer's current seed-advance mode (a_ `wfm_seed_advance_t` _)._ |
|  const [**wfm\_segment\_t**](structwfm__segment__t.md) \* | [**dp\_wfm\_compose\_segments**](#function-dp_wfm_compose_segments) (const [**dp\_wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-dp_wfm_compose_state_t) \* state, size\_t \* n\_out, int \* repeat, int \* continuous) <br>_Borrow the composer's stored segment list (for_  _record / SigMF)._ |
|  void | [**dp\_wfm\_compose\_set\_data\_pacing**](#function-dp_wfm_compose_set_data_pacing) ([**dp\_wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-dp_wfm_compose_state_t) \* state, [**wfm\_data\_pacing\_t**](wfm__data_8h.md#enum-wfm_data_pacing_t) pacing) <br>_Pace a composer's data sources: WFM\_DATA\_PACED under_ `--realtime` _._ |
|  void | [**dp\_wfm\_compose\_set\_seed\_advance**](#function-dp_wfm_compose_set_seed_advance) ([**dp\_wfm\_compose\_state\_t**](wfm__compose_8h.md#typedef-dp_wfm_compose_state_t) \* state, int mode) <br>_Choose how the seed advances on each repeat of a looped/continuous stream (a_ `wfm_seed_advance_t` _):_ |
|  size\_t | [**dp\_wfm\_compose\_spans**](#function-dp_wfm_compose_spans) (const [**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n\_segs, [**wfm\_span\_t**](structwfm__span__t.md) \* out, size\_t cap) <br>_Replay the (epoch 0) instance timeline of a resolved segment list._  |
|  char \* | [**dp\_wfm\_draws\_json**](#function-dp_wfm_draws_json) (const [**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n\_segs) <br>_The same rows_ [_**dp\_wfm\_compose\_draws()**_](wfm__compose_8h.md#function-dp_wfm_compose_draws) _reports, as a JSON array._ |
|  [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* | [**dp\_wfm\_frame\_copy**](#function-dp_wfm_frame_copy) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d) <br>_Deep-copy a description: the struct and each literal field's bits._  |
|  void | [**dp\_wfm\_frame\_free**](#function-dp_wfm_frame_free) ([**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d) <br>_Free a description returned by_ [_**dp\_wfm\_frame\_from\_json()**_](wfm__compose_8h.md#function-dp_wfm_frame_from_json) _, bits and all. NULL is a no-op._ |
|  [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* | [**dp\_wfm\_frame\_from\_json**](#function-dp_wfm_frame_from_json) (const char \* json, const char \*\* why) <br>_Read a frame description from its JSON form._  |
|  [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* | [**dp\_wfm\_frame\_refuse\_text**](#function-dp_wfm_frame_refuse_text) (const char \* text, const char \*\* why) <br>_Refuse text for a source's_ `frame=` _: it takes a description._ |
|  char \* | [**dp\_wfm\_frame\_to\_json**](#function-dp_wfm_frame_to_json) (const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d) <br>_Write a description as its JSON frame object, the text_ [_**dp\_wfm\_frame\_from\_json()**_](wfm__compose_8h.md#function-dp_wfm_frame_from_json) _reads back._ |
|  void | [**dp\_wfm\_render\_destroy**](#function-dp_wfm_render_destroy) ([**wfm\_render\_t**](wfm__compose_8h.md#typedef-wfm_render_t) \* r) <br>_Free a renderer and everything it owns. NULL-safe._  |
|  void | [**dp\_wfm\_render\_noise\_steps**](#function-dp_wfm_render_noise_steps) ([**wfm\_render\_t**](wfm__compose_8h.md#typedef-wfm_render_t) \* r, float \_Complex \* dst, size\_t n) <br>_Pull_ `n` _samples of the source's NOISE FLOOR only, through the same channel._ |
|  void | [**dp\_wfm\_render\_steps**](#function-dp_wfm_render_steps) ([**wfm\_render\_t**](wfm__compose_8h.md#typedef-wfm_render_t) \* r, float \_Complex \* dst, size\_t n) <br>_Pull exactly_ `n` _samples from_`r` _, through its channel if any._ |
|  int | [**dp\_wfm\_resolve\_noise**](#function-dp_wfm_resolve_noise) ([**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n) <br>_Resolve a segment list's noise model in place (Phase 4b)._  |
|  const char \* | [**dp\_wfm\_scene\_error**](#function-dp_wfm_scene_error) (const [**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n\_segs, int repeat, int continuous) <br>_Why a scene cannot be composed, or NULL: the one validator._  |
|  double | [**dp\_wfm\_snr\_over\_fs**](#function-dp_wfm_snr_over_fs) (int snr\_mode, int type, int sps, size\_t sf, double sym\_span, double snr) <br>_SNR (dB) referred to fs, from a source's snr/snr\_mode/sps/type._  |
|  int | [**dp\_wfm\_source\_attach\_dsss**](#function-dp_wfm_source_attach_dsss) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* syn, const [**wfm\_source\_t**](structwfm__source__t.md) \* src, double fs) <br>_Attach a dsss source's data to a freshly-created synth._  |
|  int | [**dp\_wfm\_source\_attach\_frame**](#function-dp_wfm_source_attach_frame) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* syn, const [**wfm\_source\_t**](structwfm__source__t.md) \* src) <br>_Attach an unspread source's bit pattern, framed or not._  |
|  size\_t | [**dp\_wfm\_source\_bits\_refuse\_text**](#function-dp_wfm_source_bits_refuse_text) (const char \* text, uint8\_t \* out, size\_t max\_out, const char \*\* why) <br>_Refuse text for a source's bit field: an object takes bits._  |
|  double | [**dp\_wfm\_source\_create\_snr**](#function-dp_wfm_source_create_snr) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src, double fs, double snr, int \* snr\_mode) <br>_Resolve a source's (snr, snr\_mode) into the pair to hand to_ `dp_wfm_synth_create()` _._ |
|  uint64\_t | [**dp\_wfm\_source\_data\_frames**](#function-dp_wfm_source_data_frames) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src) <br>_Frames a FINITE data source makes,_ `ceil(bits / LEN)` _; 0 for a stream or none. On continuous dsss, which has no frame, its bits._ |
|  int | [**dp\_wfm\_source\_data\_is\_stream**](#function-dp_wfm_source_data_is_stream) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src) <br>_Whether a source's data is a stream:_ `data_from_file` _is_`-` _._ |
|  uint64\_t | [**dp\_wfm\_source\_data\_samples**](#function-dp_wfm_source_data_samples) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src, double fs, uint64\_t frames) <br>_Samples the first_ `frames` _frames of a source's data occupy; 0 with no data._ |
|  double | [**dp\_wfm\_source\_dsss\_cps**](#function-dp_wfm_source_dsss_cps) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src, double fs) <br>_Chips per data symbol of a CONTINUOUS dsss source at_ `fs` _._ |
|  size\_t | [**dp\_wfm\_source\_dsss\_nchips**](#function-dp_wfm_source_dsss_nchips) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src) <br>_Chips one DSSS BURST from this source occupies, description and all._  |
|  const char \* | [**dp\_wfm\_source\_error**](#function-dp_wfm_source_error) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src) <br>_NULL when this source can be built; else why not, as a sentence._  |
|  const char \* | [**dp\_wfm\_source\_frame\_error**](#function-dp_wfm_source_frame_error) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src) <br>_NULL when this source's frame fields can be honoured; else why not._  |
|  int | [**dp\_wfm\_source\_has\_frame**](#function-dp_wfm_source_has_frame) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src) <br>_Non-zero when this source describes a FRAME._  |
|  int | [**dp\_wfm\_source\_synth\_type**](#function-dp_wfm_source_synth_type) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src) <br>_The synth type to create this source with._  |
|  const char \* | [**dp\_wfm\_source\_to\_synth\_error**](#function-dp_wfm_source_to_synth_error) (const [**wfm\_source\_t**](structwfm__source__t.md) \* src, double fs) <br>_Why_ [_**dp\_wfm\_source\_to\_synth()**_](wfm__compose__bridge_8h.md#function-dp_wfm_source_to_synth) _refused this source, or NULL._ |
|  double | [**dp\_wfm\_spec\_headroom**](#function-dp_wfm_spec_headroom) (const char \* json) <br>_The top-level_ `headroom` _(dB) from a spec JSON, or 0 if absent._ |
|  char \* | [**dp\_wfm\_spec\_template\_json**](#function-dp_wfm_spec_template_json) (void) <br>_A ready-to-edit example spec in the canonical_  _from-file schema._ |
|  char \* | [**dp\_wfm\_spec\_to\_json**](#function-dp_wfm_spec_to_json) (const [**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n\_segs, int repeat, int continuous, int seed\_advance, double headroom) <br>_Serialise a spec to a JSON string (for_  _record)._ |
|  int | [**dp\_wfm\_synth\_attach\_data**](#function-dp_wfm_synth_attach_data) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* syn, const [**wfm\_frame\_desc\_t**](structwfm__frame__desc__t.md) \* d, const [**wfm\_frame\_ops\_t**](structwfm__frame__ops__t.md) \* ops, [**wfm\_data\_src\_t**](wfm__data_8h.md#typedef-wfm_data_src_t) \* src, [**wfm\_data\_pacing\_t**](wfm__data_8h.md#enum-wfm_data_pacing_t) pacing, int modulation) <br>_Drive a type=bits synth from a frame whose payload is a data source._  |
|  const [**wfm\_data\_src\_t**](wfm__data_8h.md#typedef-wfm_data_src_t) \* | [**dp\_wfm\_synth\_data\_source**](#function-dp_wfm_synth_data_source) (const [**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* syn) <br>_The data source a synth pulls from (dp\_wfm\_synth\_attach\_data), or NULL: its stats are the run's truth for scoring._  |
|  void | [**dp\_wfm\_synth\_set\_data\_pacing**](#function-dp_wfm_synth_set_data_pacing) ([**dp\_wfm\_synth\_state\_t**](structdp__wfm__synth__state__t.md) \* syn, [**wfm\_data\_pacing\_t**](wfm__data_8h.md#enum-wfm_data_pacing_t) pacing) <br>_Pace a synth's data source: WFM\_DATA\_PACED for_ `--realtime` _._ |


## Public Static Functions

| Type | Name |
| ---: | :--- |
|  double | [**dp\_wfm\_scene\_fs**](#function-dp_wfm_scene_fs) (const [**wfm\_segment\_t**](structwfm__segment__t.md) \* segs, size\_t n\_segs) <br>_The ONE answer to "what is this stream's sample rate": the_ `fs` _every segment shares, or 0.0 when they differ._ |


























## Detailed Description


Sequences a list of segments — each one a `synth` configuration plus an on-time and a trailing off-time gap — into a single IQ stream, optionally repeating the whole sequence or running forever. The composer owns one `synth` at a time (the active segment) and reuses the Phase-A engine verbatim, so every waveform type / SNR mode / MLS behaviour is identical to the single-waveform path; a one-segment spec is byte-identical to calling `synth` directly.


Lifecycle: dp\_wfm\_compose\_create -&gt; dp\_wfm\_compose\_execute\* -&gt; dp\_wfm\_compose\_destroy



```C++
wfm_source_t tone = {.type = 0, .freq = 1e5, .snr = 100.0};
wfm_source_t qpsk = {.type = 4, .sps = 8, .snr = 9.0};
wfm_segment_t segs[2] = {
    {.sources = &tone, .n_sources = 1, .fs = 1e6,
     .num_samples = 1000, .off_samples = 500},          // tone, then a gap
    {.sources = &qpsk, .n_sources = 1, .fs = 1e6,
     .num_samples = 4096, .off_samples = 0},            // qpsk
};
dp_wfm_compose_state_t *c = dp_wfm_compose_create(segs, 2, 0, 0);
float _Complex buf[4096];
size_t n;
while ((n = dp_wfm_compose_execute(c, buf, 4096)) > 0) { ... }
dp_wfm_compose_destroy(c);
```
 


    
## Public Types Documentation




### typedef dp\_wfm\_compose\_state\_t 

```C++
typedef struct wfm_compose_state dp_wfm_compose_state_t;
```



Opaque composer state. 


        

<hr>



### enum wfm\_\_compose\_8h\_1ab04a0655cd1e3bcac5e8f48c18df1a57 

_Per-field "draw uniformly each repeat" flags (_ `ranged` _bitmask)._
```C++
enum wfm__compose_8h_1ab04a0655cd1e3bcac5e8f48c18df1a57 {
    WFM_RANGE_FREQ = 1u << 0,
    WFM_RANGE_SNR = 1u << 1,
    WFM_RANGE_LEVEL = 1u << 2,
    WFM_RANGE_FEND = 1u << 3,
    WFM_RANGE_NUM_SAMPLES = 1u << 4,
    WFM_RANGE_OFF_SAMPLES = 1u << 5,
    WFM_RANGE_DELAY_SAMPLES = 1u << 6,
    WFM_RANGE_DOPPLER = 1u << 7,
    WFM_RANGE_DOPPLER_RATE = 1u << 8
};
```



A scalar field is a constant; a _ranged_ field carries a `[lo, hi]` span (the scalar holds `lo`, a companion `*_hi` holds `hi`) and is redrawn uniformly in `[lo, hi]` at the start of every repeat (composer epoch) — so a looped / continuous stream can vary Doppler (`freq`), arrival jitter (`off_samples`), etc. burst-to-burst while staying _reproducible_: the draw is a deterministic hash of the source seed, the epoch, the segment/source index, and the field, so `--record` stores the span (not a drawn value) and `--from-file` replays the same sequence byte-for-byte. Bits 0–3 and 7–8 live on `wfm_source_t.ranged`; bits 4–6 on `wfm_segment_t.ranged`. 


        

<hr>



### enum wfm\_bitmod\_t 

_How a_ `WFM_SYNTH_BITS` _source maps its payload to symbols._
```C++
enum wfm_bitmod_t {
    WFM_BITMOD_NONE = 0,
    WFM_BITMOD_BPSK = 1,
    WFM_BITMOD_QPSK = 2
};
```



Order IS the wire value; `BITMOD_NAMES[]` and the `[[enum]] bitmod` manifest are held to this by `make lint-wfm-enum-tables`. 


        

<hr>



### enum wfm\_doppler\_lifetime\_t 

_When a source's Doppler channel restarts._ 
```C++
enum wfm_doppler_lifetime_t {
    WFM_DOPPLER_PER_INSTANCE = 0,
    WFM_DOPPLER_PERSIST = 1
};
```



Neither is a superset of the other, so it is declared rather than defaulted into an argument:



* `PER_INSTANCE` (default) restarts the geometry for every burst instance, which is the repeated-trial shape — every burst sees the same pass, and it composes with the per-instance re-draw of a ranged `doppler`.
* `PERSIST` carries one emitter's motion across every REPEAT INSTANCE of its segment, and across the gaps between them, so burst _k_ sees where the pass has got to. It is the only lifetime under which `doppler_rate` means anything over a multi-burst scene.




The channel is keyed by (segment, source), because that is the only source identity the composer has — a position. So a PERSIST source persists over its own segment's instances; two DIFFERENT segments each get their own pass, even where a reader might call them the same emitter. Sharing one across segments needs a declared source id, which nothing in the scene format carries yet; gh-942 says as much ("no per-source identity that
survives it ... the repeats/epoch machinery is where one would hang"). 


        

<hr>



### typedef wfm\_render\_t 

_One source's renderer: its synth, plus its Doppler channel._ 
```C++
typedef struct wfm_render wfm_render_t;
```




<hr>



### enum wfm\_seed\_advance\_t 

_Per-repeat seed policy for a looped/continuous stream._ 
```C++
enum wfm_seed_advance_t {
    WFM_SEED_ADVANCE_NONE = 0,
    WFM_SEED_ADVANCE_NOISE = 1,
    WFM_SEED_ADVANCE_ALL = 2
};
```



A source's single `seed` feeds two RNGs: the PN LFSR (spreading code _and_ data bits — one register) and the AWGN generator. The clean cut is therefore signal (code+data) vs. noise, exposed as an ordered, cumulative level. 


        

<hr>



### enum wfm\_snr\_mode\_t 

_What a source's_ `snr` _is measured against._
```C++
enum wfm_snr_mode_t {
    WFM_SNR_AUTO = 0,
    WFM_SNR_FS = 1,
    WFM_SNR_EBNO = 2,
    WFM_SNR_ESNO = 3
};
```



The scale a number in dB is quoted on is not a detail a caller can infer, and it changes the noise by 10log10(sps) between `fs` and `esno`. Naming the modes is what lets a downstream write the mode it means instead of a literal whose meaning lives in a comment. Order IS the wire value — the `[[enum]] snr_mode` manifest and `MODE_NAMES[]` in [**wfm\_names.h**](wfm__names_8h.md) are held to this by `make lint-wfm-enum-tables`. 


        

<hr>
## Public Attributes Documentation




### variable dp\_wfm\_why\_dsss\_cont\_no\_data\_code 

_The reasons_ [_**dp\_wfm\_source\_error()**_](wfm__compose_8h.md#function-dp_wfm_source_error) _gives a dsss source missing a code (doppler#1696): a burst whose frame has no data\_code to spread it, a burst with neither a preamble nor a frame, and a continuous stream with no data\_code. Exported so a test or a face can hold a refusal to its reason by identity._
```C++
const char dp_wfm_why_dsss_cont_no_data_code[];
```




<hr>



### variable dp\_wfm\_why\_dsss\_cont\_rate 

_The reason_ [_**dp\_wfm\_scene\_error()**_](wfm__compose_8h.md#function-dp_wfm_scene_error) _gives a continuous dsss source whose chip rate is below its symbol rate_ _exported so the wfmgen CLI can name the values beside it, by identity._
```C++
const char dp_wfm_why_dsss_cont_rate[];
```




<hr>



### variable dp\_wfm\_why\_dsss\_empty 

_The reasons_ [_**dp\_wfm\_source\_error()**_](wfm__compose_8h.md#function-dp_wfm_source_error) _gives a dsss source missing a code (doppler#1696): a burst whose frame has no data\_code to spread it, a burst with neither a preamble nor a frame, and a continuous stream with no data\_code. Exported so a test or a face can hold a refusal to its reason by identity._
```C++
const char dp_wfm_why_dsss_empty[];
```




<hr>



### variable dp\_wfm\_why\_dsss\_frame\_no\_data\_code 

_The reasons_ [_**dp\_wfm\_source\_error()**_](wfm__compose_8h.md#function-dp_wfm_source_error) _gives a dsss source missing a code (doppler#1696): a burst whose frame has no data\_code to spread it, a burst with neither a preamble nor a frame, and a continuous stream with no data\_code. Exported so a test or a face can hold a refusal to its reason by identity._
```C++
const char dp_wfm_why_dsss_frame_no_data_code[];
```




<hr>



### variable dp\_wfm\_why\_pn\_poly 

_The reason_ [_**dp\_wfm\_source\_error()**_](wfm__compose_8h.md#function-dp_wfm_source_error) _gives for a_`pn_poly` _wider than its register_ _exported so a face that knows the values (the wfmgen CLI) can name them beside it, by identity rather than by matching text._
```C++
const char dp_wfm_why_pn_poly[];
```




<hr>



### variable dp\_wfm\_why\_retired\_bits 

_The reason_ [_**dp\_wfm\_source\_error()**_](wfm__compose_8h.md#function-dp_wfm_source_error) _gives for_`retired_bits` _set: Python's retired_`bits=` _(and_`payload=` _,_`pattern=` _), named once. The CLI's and a scene's RETIRED tables say the same in their own spelling (doppler#1718)._
```C++
const char dp_wfm_why_retired_bits[];
```




<hr>
## Public Functions Documentation




### function dp\_wfm\_compose\_build\_render 

_Build a source's renderer —_ `dp_wfm_compose_build_synth` _plus the clock-Doppler channel the source declares, if it declares one._
```C++
wfm_render_t * dp_wfm_compose_build_render (
    const wfm_source_t * src,
    double fs,
    size_t on_len,
    double freq,
    double snr,
    double f_end,
    double doppler,
    double doppler_rate,
    unsigned epoch,
    int seed_advance,
    size_t instance,
    dp_doppler_channel_state_t * borrow
) 
```



THE pull path. Both faces go through `dp_wfm_render_steps()` rather than calling `dp_wfm_synth_steps()` themselves, because a Doppler channel is a RESAMPLER: it consumes about `n*(1+d)` inputs per `n` outputs, so "pull
`k`, get `k`" only holds if something keeps the remainder. Two implementations that agreed today would drift the moment either grew a holdover the other did not.


A source with `doppler == 0 && doppler_rate == 0` gets no channel and `dp_wfm_render_steps()` is then literally `dp_wfm_synth_steps()`, so every scene that does not ask for Doppler renders through exactly the path it always did — byte-identical, not merely equivalent.


`doppler`/`doppler_rate` arrive ranged-resolved, like `freq`/`snr`/`f_end`.


`borrow` is the channel a `WFM_DOPPLER_PERSIST` source keeps ACROSS segments: the composer owns it for the life of the scene and passes it in here, so the renderer uses it without adopting it and the geometry does not restart when the synth is torn down at a segment boundary. NULL means the ordinary case — the renderer creates and owns a channel if the source declares Doppler, and destroys it with itself.




**Returns:**

A heap renderer (caller [**dp\_wfm\_render\_destroy()**](wfm__compose_8h.md#function-dp_wfm_render_destroy)s it), or NULL. 





        

<hr>



### function dp\_wfm\_compose\_build\_synth 

_Construct + configure the synth for one resolved source._ 
```C++
dp_wfm_synth_state_t * dp_wfm_compose_build_synth (
    const wfm_source_t * src,
    double fs,
    size_t on_len,
    double freq,
    double snr,
    double f_end,
    unsigned epoch,
    int seed_advance,
    size_t instance
) 
```



THE single synth-construction path (create + chirp-span pin + bits/symbols/RRC attach + per-repeat NOISE reseed) shared by the streaming composer and the Plan stimulus cache, so a cached per-source render is byte-identical to the composed one. `freq/snr/f_end` are passed already ranged-resolved by the caller; `on_len` pins a chirp's sweep to the on-time; `epoch`/`seed_advance` (a [**wfm\_seed\_advance\_t**](wfm__compose_8h.md#enum-wfm_seed_advance_t)) drive the per-repeat seed policy — `epoch == 0` yields the unmodified seed. `instance` is the segment's `repeats` counter (0-based): a non-zero instance always reseeds the AWGN (fresh noise per burst instance, signal fixed, regardless of `seed_advance`); instance 0 is byte-identical to the pre-`repeats` behaviour.




**Returns:**

A heap synth (caller [**dp\_wfm\_synth\_destroy()**](wfm__synth__core_8h.md#function-dp_wfm_synth_destroy)s it), or NULL on failure. 





        

<hr>



### function dp\_wfm\_compose\_create 

_Build a composer over a copy of_ `segs` _._
```C++
dp_wfm_compose_state_t * dp_wfm_compose_create (
    const wfm_segment_t * segs,
    size_t n_segs,
    int repeat,
    int continuous
) 
```





**Parameters:**


* `segs` Segment list (copied; caller keeps ownership). 
* `n_segs` Number of segments (&gt;= 1). 
* `repeat` Non-zero: loop the whole sequence after the last segment. 
* `continuous` Non-zero: never finish (implies repeat); execute always returns `max`. 



**Returns:**

Heap state, or NULL on bad args / allocation / synth failure. 




**Note:**

Caller must [**dp\_wfm\_compose\_destroy()**](wfm__compose_8h.md#function-dp_wfm_compose_destroy) when done. 





        

<hr>



### function dp\_wfm\_compose\_create\_why 

[_**dp\_wfm\_compose\_create()**_](wfm__compose_8h.md#function-dp_wfm_compose_create) _, able to say why the scene was refused._
```C++
dp_wfm_compose_state_t * dp_wfm_compose_create_why (
    const wfm_segment_t * segs,
    size_t n_segs,
    int repeat,
    int continuous,
    const char ** why
) 
```



The scene is asked [**dp\_wfm\_scene\_error()**](wfm__compose_8h.md#function-dp_wfm_scene_error) before anything is built, and its sentence is what `why` receives  the same sentence the wfmgen CLI, a scene read by [**dp\_wfm\_compose\_from\_json\_why()**](wfm__compose_8h.md#function-dp_wfm_compose_from_json_why) and the standalone `Synth` report, because it is the same validator. It is the create the generated `Composer([...])` calls (just-makeit's `create_why`), so a refused composer raises `ValueError(<the reason>)`.




**Parameters:**


* `segs` as for [**dp\_wfm\_compose\_create()**](wfm__compose_8h.md#function-dp_wfm_compose_create). 
* `n_segs` as for [**dp\_wfm\_compose\_create()**](wfm__compose_8h.md#function-dp_wfm_compose_create). 
* `repeat` as for [**dp\_wfm\_compose\_create()**](wfm__compose_8h.md#function-dp_wfm_compose_create). 
* `continuous` as for [**dp\_wfm\_compose\_create()**](wfm__compose_8h.md#function-dp_wfm_compose_create). 
* `why` optional; receives a STATIC reason when the scene is refused, and is left as it was in every other case (success, bad arguments, an allocation or synth failure). 



**Returns:**

Heap state, or NULL as for [**dp\_wfm\_compose\_create()**](wfm__compose_8h.md#function-dp_wfm_compose_create).



```C++
wfm_source_t  src = { .type = WFM_SYNTH_DSSS, .sps = 2 };   // no codes
wfm_segment_t seg = { .sources = &src, .n_sources = 1, .fs = 1e6,
                      .num_samples = 64 };
const char   *why = NULL;
dp_wfm_compose_state_t *c = dp_wfm_compose_create_why (&seg, 1, 0, 0, &why);
if (c != NULL || why != dp_wfm_why_dsss_empty)   // refused, and says why
  return 1;
```
 


        

<hr>



### function dp\_wfm\_compose\_destroy 

_Destroy a composer and its active synth._ 
```C++
void dp_wfm_compose_destroy (
    dp_wfm_compose_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_wfm\_compose\_draws 

_Replay the (epoch 0) instance timeline AND its drawn source values._ 
```C++
size_t dp_wfm_compose_draws (
    const wfm_segment_t * segs,
    size_t n_segs,
    wfm_draw_t * out,
    size_t cap
) 
```



Same size-then-fill protocol as [**dp\_wfm\_compose\_spans()**](wfm__compose_8h.md#function-dp_wfm_compose_spans): call once with `cap` 0 to size, then again with a buffer. Emits one row per SOURCE per instance, in stream order, because that is the granularity a per-source annotation or a scoring pipeline needs. Pass the RESOLVED segments ([**dp\_wfm\_compose\_segments()**](wfm__compose_8h.md#function-dp_wfm_compose_segments) on a live composer) so intrinsic on-times are already folded in.


The rows are produced by the same dp\_wfm\_draw\_segment()/dp\_wfm\_draw\_source() calls the renderer resolves through, so a field added to the draw reaches both by construction rather than by a reviewer noticing.




**Parameters:**


* `segs` Resolved segment array. 
* `n_segs` Segment count. 
* `out` Row buffer (may be NULL when cap is 0). 
* `cap` Capacity of out in rows. 



**Returns:**

Total rows in one pass of the spec (sum of n\_sources over instances), regardless of `cap`. 





        

<hr>



### function dp\_wfm\_compose\_execute 

_Emit up to_ `max` _samples of the composed stream._
```C++
size_t dp_wfm_compose_execute (
    dp_wfm_compose_state_t * state,
    float _Complex * out,
    size_t max
) 
```





**Returns:**

Number of samples written: &lt; `max` (or 0) signals the sequence finished (never, when `continuous`). 





        

<hr>



### function dp\_wfm\_compose\_execute\_rate 

_Emit up to_ `max` _samples, all at ONE sample rate, and say which._
```C++
size_t dp_wfm_compose_execute_rate (
    dp_wfm_compose_state_t * state,
    float _Complex * out,
    size_t max,
    double * fs
) 
```



[**dp\_wfm\_compose\_execute()**](wfm__compose_8h.md#function-dp_wfm_compose_execute) for an output that states a rate per block: it stops early where the next segment's `fs` differs from the samples already written, so every block it returns has one rate. A scene whose segments share an `fs` never stops early, and its samples are the same, byte for byte, as [**dp\_wfm\_compose\_execute()**](wfm__compose_8h.md#function-dp_wfm_compose_execute)'s. A short return therefore does NOT mean the scene finished; 0 does.



```C++
wfm_source_t  src     = { .type = WFM_SYNTH_TONE, .snr = 100.0 };
wfm_segment_t segs[2] = {
  { .sources = &src, .n_sources = 1, .fs = 6e6, .num_samples = 8 },
  { .sources = &src, .n_sources = 1, .fs = 2e6, .num_samples = 8 },
};
dp_wfm_compose_state_t *c = dp_wfm_compose_create (segs, 2, 0, 0);
float _Complex buf[64];
double         fs = 0.0;
size_t a = dp_wfm_compose_execute_rate (c, buf, 64, &fs); // 8 at 6e6
int    ok = a == 8 && fs == 6e6;
size_t b = dp_wfm_compose_execute_rate (c, buf, 64, &fs); // 8 at 2e6
ok = ok && b == 8 && fs == 2e6;
ok = ok && dp_wfm_compose_execute_rate (c, buf, 64, &fs) == 0;
dp_wfm_compose_destroy (c);
return ok ? 0 : 1;
```





**Parameters:**


* `state` the composer. 
* `out` destination, `max` samples. 
* `max` capacity of `out`. 
* `fs` receives the rate of the samples written (left untouched when none are). 



**Returns:**

samples written; 0 when the scene has finished. 





        

<hr>



### function dp\_wfm\_compose\_from\_file 

```C++
dp_wfm_compose_state_t * dp_wfm_compose_from_file (
    const char * path
) 
```




<hr>



### function dp\_wfm\_compose\_from\_file\_why 

[_**dp\_wfm\_compose\_from\_file**_](wfm__compose_8h.md#function-dp_wfm_compose_from_file) _, able to say why a scene was refused._
```C++
dp_wfm_compose_state_t * dp_wfm_compose_from_file_why (
    const char * path,
    const char ** why
) 
```



Reads `path` and hands its text to [**dp\_wfm\_compose\_from\_json\_why**](wfm__compose_8h.md#function-dp_wfm_compose_from_json_why), so the reason is that function's: a retired key or a refused frame, named. A file that cannot be read gives NULL and leaves `why` NULL.




**Parameters:**


* `path` the spec file. 
* `why` optional; as for [**dp\_wfm\_compose\_from\_json\_why**](wfm__compose_8h.md#function-dp_wfm_compose_from_json_why). 



**Returns:**

Composer state, or NULL on read/parse error / a refused scene. 





        

<hr>



### function dp\_wfm\_compose\_from\_json 

_Build a composer from a JSON spec string (for_  _from-file)._
```C++
dp_wfm_compose_state_t * dp_wfm_compose_from_json (
    const char * json
) 
```





**Returns:**

Composer state, or NULL on parse error / bad type / no segments. 





        

<hr>



### function dp\_wfm\_compose\_from\_json\_at 

_Build a composer from a JSON spec file._ 
```C++
dp_wfm_compose_state_t * dp_wfm_compose_from_json_at (
    const char * json,
    const char * base,
    const char ** why
) 
```





**Returns:**

Composer state, or NULL on read/parse error.


[**dp\_wfm\_compose\_from\_json\_why()**](wfm__compose_8h.md#function-dp_wfm_compose_from_json_why), reading a scene that lives in `base`.


A source's relative `"data_from_file"` is the scene's: it resolves against `base`, the directory the scene was read from, so a scene and its data move together and replay from anywhere. NULL (as from\_json\_why passes) leaves a relative path relative to the caller's working directory.




**Parameters:**


* `json` the scene's text. 
* `base` the scene's directory, or NULL. 
* `why` optional; receives a static reason for a refusal. 



**Returns:**

the composer, or NULL.



```C++
const char *why = NULL;
dp_wfm_compose_state_t *c = dp_wfm_compose_from_json_at (
    "{\"segments\":[{\"type\":\"tone\",\"num_samples\":8}]}", ".",
    &why);
if (!c)
  return 1;
dp_wfm_compose_destroy (c);
```
 


        

<hr>



### function dp\_wfm\_compose\_from\_json\_why 

_The same, but able to say why a FRAME was refused._ 
```C++
dp_wfm_compose_state_t * dp_wfm_compose_from_json_why (
    const char * json,
    const char ** why
) 
```



A spec is the interface most likely to be hand-written, and a NULL return is the one answer that cannot teach anything. This runs [**dp\_wfm\_source\_frame\_error**](wfm__compose_8h.md#function-dp_wfm_source_frame_error) over every parsed source before handing them to the composer — which asks the same question and would refuse either way — so the reason survives the boundary as a sentence instead of a pointer.


Only the frame rule reports this way. A parse error or a bad type is still a bare NULL, because those are cJSON's to describe and duplicating its diagnostics here would be a second opinion about the same text.




**Parameters:**


* `json` the spec. 
* `why` optional; receives a STATIC message when a source's frame is refused, or NULL in every other case (including success). Passing NULL makes this exactly [**dp\_wfm\_compose\_from\_json**](wfm__compose_8h.md#function-dp_wfm_compose_from_json). 



**Returns:**

Composer state, or NULL on parse error / bad type / no segments / a refused frame. 





        

<hr>



### function dp\_wfm\_compose\_seed\_advance 

_The composer's current seed-advance mode (a_ `wfm_seed_advance_t` _)._
```C++
int dp_wfm_compose_seed_advance (
    const dp_wfm_compose_state_t * state
) 
```



The composer is the SSOT for it: `--from-file` sets it from the spec and the flag path sets it from `--seed-advance`, so a serialiser must read it back from here rather than from whichever half happened to supply it. 

**Parameters:**


* `state` Compose state (may be NULL → `WFM_SEED_ADVANCE_NONE`). 




        

<hr>



### function dp\_wfm\_compose\_segments 

_Borrow the composer's stored segment list (for_  _record / SigMF)._
```C++
const wfm_segment_t * dp_wfm_compose_segments (
    const dp_wfm_compose_state_t * state,
    size_t * n_out,
    int * repeat,
    int * continuous
) 
```





**Parameters:**


* `state` the composer. 
* `n_out` receives the segment count. 
* `repeat` receives the repeat flag (may be NULL). 
* `continuous` receives the continuous flag (may be NULL). 



**Returns:**

Pointer to the internal segments (owned by the composer; valid until dp\_wfm\_compose\_destroy). 





        

<hr>



### function dp\_wfm\_compose\_set\_data\_pacing 

_Pace a composer's data sources: WFM\_DATA\_PACED under_ `--realtime` _._
```C++
void dp_wfm_compose_set_data_pacing (
    dp_wfm_compose_state_t * state,
    wfm_data_pacing_t pacing
) 
```



Applied to every synth the composer builds from here on, so a data stream with nothing yet sends an idle frame of fill rather than waiting (dp\_wfm\_data\_frame, the one rule). The default, WFM\_DATA\_UNPACED, waits. 


        

<hr>



### function dp\_wfm\_compose\_set\_seed\_advance 

_Choose how the seed advances on each repeat of a looped/continuous stream (a_ `wfm_seed_advance_t` _):_
```C++
void dp_wfm_compose_set_seed_advance (
    dp_wfm_compose_state_t * state,
    int mode
) 
```




* `WFM_SEED_ADVANCE_NONE` (default): byte-identical repeats.
* `WFM_SEED_ADVANCE_NOISE`: advance only the AWGN seed → a fresh noise realization each pass while the signal (LO / PN code / data / pulse) stays bit-identical (so a fixed preamble/code re-acquires every burst).
* `WFM_SEED_ADVANCE_ALL`: advance the whole seed → code, data, and noise all change (a fully stochastic stream).




Set before the first execute(); the first pass is always unchanged. An out-of-range mode is ignored. 

**Parameters:**


* `state` Compose state (may be NULL). 
* `mode` A wfm\_seed\_advance\_t value. 




        

<hr>



### function dp\_wfm\_compose\_spans 

_Replay the (epoch 0) instance timeline of a resolved segment list._ 
```C++
size_t dp_wfm_compose_spans (
    const wfm_segment_t * segs,
    size_t n_segs,
    wfm_span_t * out,
    size_t cap
) 
```



Walks every segment's `repeats` instances, re-deriving each instance's drawn delay/on/off exactly as the streaming composer will (identical draw hash), and fills `out` with up to `cap` spans in stream order. Returns the TOTAL instance count regardless of `cap` — call once with cap 0 to size, then again with a buffer. Pass the RESOLVED segments ([**dp\_wfm\_compose\_segments()**](wfm__compose_8h.md#function-dp_wfm_compose_segments) on a live composer) so intrinsic on-times (dsss) are already folded in.


Assumes every segment builds: a segment that fails at render time (invalid burst geometry) degrades to its gaps only, so positions after it would shift relative to this replay.




**Parameters:**


* `segs` Resolved segment array. 
* `n_segs` Segment count. 
* `out` Span buffer (may be NULL when cap is 0). 
* `cap` Capacity of out in spans. 



**Returns:**

Total number of instances in one pass of the spec. 





        

<hr>



### function dp\_wfm\_draws\_json 

_The same rows_ [_**dp\_wfm\_compose\_draws()**_](wfm__compose_8h.md#function-dp_wfm_compose_draws) _reports, as a JSON array._
```C++
char * dp_wfm_draws_json (
    const wfm_segment_t * segs,
    size_t n_segs
) 
```



One object per source per instance, in stream order, with the keys named after the `wfm_draw_t` fields. Exists so a binding can hand a caller its GROUND TRUTH without marshalling a struct array itself: a ranged field is only usable if what it drew can be read back, and scoring a receiver against a scene whose `freq` re-draws per instance means scoring against a number the caller does not otherwise have (doppler#1112).


Reads through [**dp\_wfm\_compose\_draws()**](wfm__compose_8h.md#function-dp_wfm_compose_draws), so it cannot disagree with the SigMF annotations, which read through it too.




**Parameters:**


* `segs` Resolved segment array ([**dp\_wfm\_compose\_segments()**](wfm__compose_8h.md#function-dp_wfm_compose_segments)). 
* `n_segs` Segment count. 



**Returns:**

Heap JSON string the caller free()s; never NULL — the allocations go through the abort-on-OOM helpers. A spec with no rows yields `[]`.



```C++
size_t n; int rp, ct;
const wfm_segment_t *segs = dp_wfm_compose_segments(c, &n, &rp, &ct);
char *js = dp_wfm_draws_json(segs, n);
puts(js);
free(js);
```
 


        

<hr>



### function dp\_wfm\_frame\_copy 

_Deep-copy a description: the struct and each literal field's bits._ 
```C++
wfm_frame_desc_t * dp_wfm_frame_copy (
    const wfm_frame_desc_t * d
) 
```



A source borrows its description; a holder that must outlive the caller's (the composer, a Python `Synth`) takes a copy instead, and releases it with [**dp\_wfm\_frame\_free()**](wfm__compose_8h.md#function-dp_wfm_frame_free). A later change to the original, or freeing it, does not reach the copy.




**Parameters:**


* `d` the description to copy; NULL gives NULL. 



**Returns:**

the owned copy.



```C++
wfm_frame_desc_t *a
    = dp_wfm_frame_from_json ("{\"fields\": [{\"spec\": \"1010\"}]}", NULL);
wfm_frame_desc_t *b = dp_wfm_frame_copy (a);
dp_wfm_frame_free (a);   // b is unaffected
// b->field[0].seq.len == 4
dp_wfm_frame_free (b);
```
 


        

<hr>



### function dp\_wfm\_frame\_free 

_Free a description returned by_ [_**dp\_wfm\_frame\_from\_json()**_](wfm__compose_8h.md#function-dp_wfm_frame_from_json) _, bits and all. NULL is a no-op._
```C++
void dp_wfm_frame_free (
    wfm_frame_desc_t * d
) 
```




```C++
dp_wfm_frame_free (dp_wfm_frame_from_json ("{\"fields\": []}", NULL));
dp_wfm_frame_free (NULL);   // nothing to free
```
 


        

<hr>



### function dp\_wfm\_frame\_from\_json 

_Read a frame description from its JSON form._ 
```C++
wfm_frame_desc_t * dp_wfm_frame_from_json (
    const char * json,
    const char ** why
) 
```



The form a scene's `"frame"` key holds, and what `wfmgen --frame FILE` reads — one reader for both: `{"fields": [...], "stages": [...]}`. A field with bits is its Field text, `"spec"` (`"0x1ACFFC1D"`, `"pn:31:5*4"`); a derived field is its `"bits"` and the `"derived_by"` stage (index plus one). A stage names its `"kind"` (`"crc16"`, `"rs"`, `"randomise"`, `"conv"`, `"interleave"`, or a number from `WFM_STAGE_USER` up) and its cover as `"first_field"`/`"n_fields"`, plus `"depth"`, `"unit_bits"` and `"emit_num"`/`"emit_den"` where the kind uses them.


A malformed description is REFUSED, never salvaged: a frame read wrong builds a waveform that looks fine and is not the one described. Whether it lays out is a separate question, asked by `dp_wfm_source_frame_error()` once a source carries it.




**Parameters:**


* `json` the frame object, NUL-terminated. 
* `why` receives a static reason on failure; may be NULL. 



**Returns:**

the description, owning its literal bits (free it with [**dp\_wfm\_frame\_free()**](wfm__compose_8h.md#function-dp_wfm_frame_free)), or NULL if the text is not a frame object.



```C++
const char       *why;
wfm_frame_desc_t *d = dp_wfm_frame_from_json (
    "{\"fields\": [{\"name\": \"sync\", \"spec\": \"0x1ACFFC1D\"},"
    "              {\"name\": \"data\", \"spec\": \"pn:31:5*4\"}]}",
    &why);
if (!d)
  fprintf (stderr, "error: %s\n", why);
// d->n_fields == 2; d->field[1].reps == 4
dp_wfm_frame_free (d);
```
 


        

<hr>



### function dp\_wfm\_frame\_refuse\_text 

_Refuse text for a source's_ `frame=` _: it takes a description._
```C++
wfm_frame_desc_t * dp_wfm_frame_refuse_text (
    const char * text,
    const char ** why
) 
```



The frame face of [**dp\_wfm\_source\_bits\_refuse\_text()**](wfm__compose_8h.md#function-dp_wfm_source_bits_refuse_text). A source's `frame=` takes a `FrameDesc` or a `Frame` on the Python face, and a `str` is refused rather than read as JSON; a description written as JSON is a scene's `"frame"` key, read by [**dp\_wfm\_frame\_from\_json()**](wfm__compose_8h.md#function-dp_wfm_frame_from_json). It has the shape of that reader because the binding calls it where the reader would be called (just-makeit's owned-pointer `parse_fn` with `parse_why`).




**Parameters:**


* `text` ignored. 
* `why` receives the static reason; may be NULL. 



**Returns:**

NULL, always.



```C++
const char *why;
wfm_frame_desc_t *d = dp_wfm_frame_refuse_text ("{\"fields\": []}", &why);
// d == NULL; why names FrameDesc
```
 


        

<hr>



### function dp\_wfm\_frame\_to\_json 

_Write a description as its JSON frame object, the text_ [_**dp\_wfm\_frame\_from\_json()**_](wfm__compose_8h.md#function-dp_wfm_frame_from_json) _reads back._
```C++
char * dp_wfm_frame_to_json (
    const wfm_frame_desc_t * d
) 
```



The same writer a scene's `"frame"` key uses, so the two cannot spell a description differently: a literal field is its Field text, a derived field its `bits` and `derived_by`, a stage its kind and cover.




**Parameters:**


* `d` the description; NULL gives NULL. 



**Returns:**

a NUL-terminated string the caller releases with free().



```C++
wfm_frame_desc_t *d
    = dp_wfm_frame_from_json ("{\"fields\": [{\"spec\": \"1010\"}]}", NULL);
char *text = dp_wfm_frame_to_json (d);
// text: {"fields":[{"spec":"0xa"}],"stages":[]}
free (text);
dp_wfm_frame_free (d);
```
 


        

<hr>



### function dp\_wfm\_render\_destroy 

_Free a renderer and everything it owns. NULL-safe._ 
```C++
void dp_wfm_render_destroy (
    wfm_render_t * r
) 
```




<hr>



### function dp\_wfm\_render\_noise\_steps 

_Pull_ `n` _samples of the source's NOISE FLOOR only, through the same channel._
```C++
void dp_wfm_render_noise_steps (
    wfm_render_t * r,
    float _Complex * dst,
    size_t n
) 
```



What a gap renders (gh-409). The channel runs here too, and deliberately: an emitter does not stop moving because its burst ended, so a pass is continuous and during a gap the thing propagating is the noise floor. Skip the channel over gaps and `doppler_rate` across a multi-burst scene quietly means "rate per unit of ON time" instead of per second. 


        

<hr>



### function dp\_wfm\_render\_steps 

_Pull exactly_ `n` _samples from_`r` _, through its channel if any._
```C++
void dp_wfm_render_steps (
    wfm_render_t * r,
    float _Complex * dst,
    size_t n
) 
```




<hr>



### function dp\_wfm\_resolve\_noise 

_Resolve a segment list's noise model in place (Phase 4b)._ 
```C++
int dp_wfm_resolve_noise (
    wfm_segment_t * segs,
    size_t n
) 
```



No-op for 1-source segments (keeps the bundled-synth path byte-identical). For a multi-source segment it sets one shared noise floor (from an explicit WFM\_SYNTH\_NOISE source, else the first snr-bearing source), cleans the signal sources, and appends a WFM\_SYNTH\_NOISE source at the floor — so the composer's accumulator just sums. May `realloc` each segment's `sources`. Idempotent.


`dp_wfm_compose_create()` calls this on its private copy, so every face (CLI, JSON, Python) resolves identically.




**Returns:**

0 on success; -1 if a non-anchor source over-specifies (snr + level) or on allocation failure. 





        

<hr>



### function dp\_wfm\_scene\_error 

_Why a scene cannot be composed, or NULL: the one validator._ 
```C++
const char * dp_wfm_scene_error (
    const wfm_segment_t * segs,
    size_t n_segs,
    int repeat,
    int continuous
) 
```



Every source's [**dp\_wfm\_source\_error()**](wfm__compose_8h.md#function-dp_wfm_source_error), then what only the scene can say:



* A continuous dsss source's chip rate `fs / sps`, at its segment's `fs`, is at least its `symbol_rate`  one chip per data symbol, the synth's own floor ([**dp\_wfm\_source\_dsss\_cps()**](wfm__compose_8h.md#function-dp_wfm_source_dsss_cps)). The default `fs = 1.0` with a `symbol_rate` in Hz is the case that finds it (doppler#1706); the reason is dp\_wfm\_why\_dsss\_cont\_rate.
* A data STREAM (`--data-from-file -`) has no end to repeat, so `repeat`, `continuous` and a segment's `repeats > 1` are refused, and stdin feeds at most one source.




[**dp\_wfm\_compose\_create()**](wfm__compose_8h.md#function-dp_wfm_compose_create) refuses exactly these, and [**dp\_wfm\_compose\_create\_why()**](wfm__compose_8h.md#function-dp_wfm_compose_create_why) says which; a face calls this to say why.




**Returns:**

a static sentence naming the fault and its fix, or NULL. 





        

<hr>



### function dp\_wfm\_snr\_over\_fs 

_SNR (dB) referred to fs, from a source's snr/snr\_mode/sps/type._ 
```C++
double dp_wfm_snr_over_fs (
    int snr_mode,
    int type,
    int sps,
    size_t sf,
    double sym_span,
    double snr
) 
```



The single source of truth for the Es/No, Eb/No, and over-fs conventions (`snr_mode` 0 auto / 1 fs / 2 ebno / 3 esno). `dp_wfm_resolve_noise()` uses it to place the shared noise floor at `level(anchor) − dp_wfm_snr_over_fs(anchor)`, and the Plan stimulus engine reuses it to recompute the floor at an arbitrary swept SNR — so both agree to the bit.


For `type=dsss` the symbol is the outer _data_ symbol. For a BURST that spans `sf * sps` samples (sf chips, sps samples per chip). For a CONTINUOUS async stream the data clock is independent of the code, so the span is `fs / symbol_rate` samples — passed as `sym_span` (non-integer), which OVERRIDES the `sf·sps` reconstruction when non-zero. `auto` picks esno, and esno/ebno convert as `snr − 10·log10(span)` (BPSK payload, so the two coincide). Every other type ignores `sf` and `sym_span`.




**Parameters:**


* `snr_mode` 0 auto, 1 fs, 2 ebno, 3 esno. 
* `type` A WFM\_SYNTH\_\* waveform type (selects the auto convention). 
* `sps` Samples per symbol/chip (≥1; &lt;1 treated as 1). 
* `sf` Spreading factor — chips per data symbol (burst dsss; ≥1, &lt;1 treated as 1). 
* `sym_span` Continuous-dsss symbol span in samples (`fs/symbol_rate`); 0 = burst/non-dsss, derive from `sf·sps`. 
* `snr` The declared SNR in dB. 



**Returns:**

SNR over fs in dB. 





        

<hr>



### function dp\_wfm\_source\_attach\_dsss 

_Attach a dsss source's data to a freshly-created synth._ 
```C++
int dp_wfm_source_attach_dsss (
    dp_wfm_synth_state_t * syn,
    const wfm_source_t * src,
    double fs
) 
```



The single dsss-attach path, called by BOTH synth-construction faces (`dp_wfm_compose_build_synth` and the standalone `dp_wfm_source_to_synth`), so the two cannot drift on how a dsss stream is configured. Selects on `symbol_rate`: 0 → the burst form (`dp_wfm_synth_set_dsss_chips`); &gt; 0 → the continuous form (`dp_wfm_synth_set_dsss_cont`) with `chips_per_symbol = (fs/sps)/symbol_rate`, taking the data from the payload when one is supplied (`bits`) and otherwise from the seeded PN. A no-op for a non-dsss source.




**Parameters:**


* `syn` A synth from [**dp\_wfm\_synth\_create()**](wfm__synth__core_8h.md#function-dp_wfm_synth_create) with `wtype == WFM_SYNTH_DSSS`. 
* `src` The source (codes, payload, symbol\_rate, pn config). 
* `fs` Segment sample rate (Hz) — the continuous chip rate is fs/sps. 



**Returns:**

0 on success (or non-dsss no-op); -1 on invalid geometry. 





        

<hr>



### function dp\_wfm\_source\_attach\_frame 

_Attach an unspread source's bit pattern, framed or not._ 
```C++
int dp_wfm_source_attach_frame (
    dp_wfm_synth_state_t * syn,
    const wfm_source_t * src
) 
```



The `type=bits` counterpart of [**dp\_wfm\_source\_attach\_dsss()**](wfm__compose_8h.md#function-dp_wfm_source_attach_dsss), and called from the same two places for the same reason. When the source carries a frame, the pattern handed to `dp_wfm_synth_set_bits()` is `dp_wfm_frame_assemble()` of `[preamble x reps | sync | payload | crc]` rather than the payload alone — so the layout, the CRC's position and its bit order come from the one descriptor that the DSSS path and the receiver already read.


The frame CYCLES, exactly as an unframed pattern does: one descriptor fills whatever length is asked for, which is what turns a one-frame description into a multi-frame record.




**Parameters:**


* `syn` A synth from [**dp\_wfm\_synth\_create()**](wfm__synth__core_8h.md#function-dp_wfm_synth_create) with `wtype == WFM_SYNTH_BITS`. 
* `src` The source (pattern, modulation, and any frame fields). 



**Returns:**

0 on success (or a non-bits/no-pattern no-op); -1 on failure. 





        

<hr>



### function dp\_wfm\_source\_bits\_refuse\_text 

_Refuse text for a source's bit field: an object takes bits._ 
```C++
size_t dp_wfm_source_bits_refuse_text (
    const char * text,
    uint8_t * out,
    size_t max_out,
    const char ** why
) 
```



A composer source's bit fields (`payload`, `sync`, `acq_code`, `data_code`) take BITS on the Python face  a `uint8` array, bytes or a sequence of 0/1  and module helpers make them from other forms: `field_bits(text)` for the Field grammar, `cvt.hex_to_bin`, `cvt.bytes_to_bin`. A `str` is refused rather than read, so the object has one shape and the Field grammar one door. The text faces (the CLI, a scene) keep reading a Field through [**dp\_wfm\_field\_parse()**](wfm__frame_8h.md#function-dp_wfm_field_parse).


It has the shape of [**dp\_wfm\_field\_bits()**](wfm__frame_8h.md#function-dp_wfm_field_bits) because the binding calls it where that would be called (just-makeit's `coerce_str_fn`), and it always refuses: it returns 0 and sets `why` to its one static reason.




**Parameters:**


* `text` ignored. 
* `out` never written. 
* `max_out` ignored. 
* `why` receives the static reason; may be NULL. 



**Returns:**

0, always.



```C++
const char *why;
size_t      n = dp_wfm_source_bits_refuse_text ("0101", NULL, 0, &why);
// n == 0; why names field_bits()
```
 


        

<hr>



### function dp\_wfm\_source\_create\_snr 

_Resolve a source's (snr, snr\_mode) into the pair to hand to_ `dp_wfm_synth_create()` _._
```C++
double dp_wfm_source_create_snr (
    const wfm_source_t * src,
    double fs,
    double snr,
    int * snr_mode
) 
```



`dp_wfm_synth_create()` runs before a dsss source's codes are attached, so it cannot know the spreading factor its own esno would need. This helper — the one create-time entry point shared by the composer (`dp_wfm_compose_build_synth`) and the standalone-Synth bridge (`dp_wfm_source_to_synth`), so every face agrees to the bit — converts a dsss source's SNR to the over-fs reference (via `dp_wfm_snr_over_fs`; the burst span is `sf = n_data_code`, a continuous stream uses `fs/symbol_rate`) and returns `snr_mode=fs`. A framed `bpsk`/`qpsk`/`pn` is referred the same way, because its synth is created as BITS ([**dp\_wfm\_source\_synth\_type()**](wfm__compose_8h.md#function-dp_wfm_source_synth_type)). Every other source passes through unchanged.




**Parameters:**


* `src` The source (supplies type/sps/snr\_mode/n\_data\_code/ symbol\_rate). 
* `fs` Segment sample rate (Hz) — needed for a continuous dsss source's `fs/symbol_rate` span; ignored otherwise. 
* `snr` The declared SNR in dB, already ranged-resolved. 
* `snr_mode` Receives the snr\_mode for create. 



**Returns:**

The SNR in dB for create. 





        

<hr>



### function dp\_wfm\_source\_data\_frames 

_Frames a FINITE data source makes,_ `ceil(bits / LEN)` _; 0 for a stream or none. On continuous dsss, which has no frame, its bits._
```C++
uint64_t dp_wfm_source_data_frames (
    const wfm_source_t * src
) 
```



Known before the first sample (a file's length from fstat), which is what lets a finite run's length be derived rather than given (§4.6).



```C++
static const uint8_t bits[40] = { 1 };
wfm_source_t src = { .type = WFM_SYNTH_BPSK, .sps = 4 };
src.data      = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .bits = bits,
                             .len = 40 };
src.data_len  = 16;
src.fill      = (wfm_seq_t){ .kind = WFM_SEQ_DOTTED, .len = 2 };
if (dp_wfm_source_data_frames (&src) != 3) // 40 bits in 16-bit frames
  return 1;
if (dp_wfm_source_data_samples (&src, 1e6, 3) != 3 * 16 * 4) // bpsk
  return 1;
```
 


        

<hr>



### function dp\_wfm\_source\_data\_is\_stream 

_Whether a source's data is a stream:_ `data_from_file` _is_`-` _._
```C++
int dp_wfm_source_data_is_stream (
    const wfm_source_t * src
) 
```



A stream has no length up front and no end to repeat, so a scene refuses it with `repeat`, `continuous` or `repeats > 1`, Plan refuses it, and a segment carrying one runs until it ends ([**dp\_wfm\_scene\_error()**](wfm__compose_8h.md#function-dp_wfm_scene_error)). 


        

<hr>



### function dp\_wfm\_source\_data\_samples 

_Samples the first_ `frames` _frames of a source's data occupy; 0 with no data._
```C++
uint64_t dp_wfm_source_data_samples (
    const wfm_source_t * src,
    double fs,
    uint64_t frames
) 
```



The one length a run with a data source is measured in: a finite run is this over [**dp\_wfm\_source\_data\_frames()**](wfm__compose_8h.md#function-dp_wfm_source_data_frames), and a stream ends at this over the frames it sent.



* **A frame** (bits, bpsk/qpsk/pn): its output bits at the mapping's bits per symbol (rounded up), times `sps`, per frame.
* **A dsss burst**: its chips times `sps` (samples per chip), per burst.
* **Continuous dsss** has no frame: a "frame" is one data bit, one per data symbol, and the run is every chip of the first `frames` symbols at `fs / sps / symbol_rate` chips per symbol  not a whole number, so not a product  times `sps`.






**Parameters:**


* `src` the source. 
* `fs` the segment's sample rate (only continuous dsss reads it). 
* `frames` frames (data bits, for continuous dsss) sent. 




        

<hr>



### function dp\_wfm\_source\_dsss\_cps 

_Chips per data symbol of a CONTINUOUS dsss source at_ `fs` _._
```C++
double dp_wfm_source_dsss_cps (
    const wfm_source_t * src,
    double fs
) 
```



`sps` is samples per CHIP for dsss, so the chip rate is `fs / sps`, and the data clock is `symbol_rate`: their ratio, non-integer in general, which is the asynchronicity. It is the number the builder hands [**dp\_wfm\_synth\_set\_dsss\_cont()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_dsss_cont), and the one [**dp\_wfm\_scene\_error()**](wfm__compose_8h.md#function-dp_wfm_scene_error) holds to `>= 1`  so the rule and the synth read the same value.




**Parameters:**


* `src` the source. 
* `fs` its segment's sample rate, in Hz. 



**Returns:**

chips per data symbol, or 0 for a source that is not continuous dsss (or has no `sps`).



```C++
wfm_source_t s = { .type = WFM_SYNTH_DSSS, .sps = 2,
                   .symbol_rate = 1000.0 };
if (dp_wfm_source_dsss_cps (&s, 1e6) != 500.0)   // 500 chips per symbol
  return 1;
```
 


        

<hr>



### function dp\_wfm\_source\_dsss\_nchips 

_Chips one DSSS BURST from this source occupies, description and all._ 
```C++
size_t dp_wfm_source_dsss_nchips (
    const wfm_source_t * src
) 
```



What sizes a lone dsss segment's intrinsic on-time. It reads the same description the burst is assembled from, so a stage that lengthens the frame  a rate-1/2 inner code doubles it  lengthens the segment by the same arithmetic instead of by a second copy of it.




**Parameters:**


* `src` the source. 



**Returns:**

burst chips, or 0 for a non-dsss source, a CONTINUOUS dsss source (which has no intrinsic length), or an empty/refused geometry. 





        

<hr>



### function dp\_wfm\_source\_error 

_NULL when this source can be built; else why not, as a sentence._ 
```C++
const char * dp_wfm_source_error (
    const wfm_source_t * src
) 
```



The question every face asks before it builds  the wfmgen CLI, a scene read by [**dp\_wfm\_compose\_from\_json\_why()**](wfm__compose_8h.md#function-dp_wfm_compose_from_json_why), the standalone `Synth` through [**dp\_wfm\_source\_to\_synth()**](wfm__compose__bridge_8h.md#function-dp_wfm_source_to_synth) and the composer through [**dp\_wfm\_compose\_create()**](wfm__compose_8h.md#function-dp_wfm_compose_create)  so all four refuse the same sources for the same reason. It checks the source's own parameters, then asks [**dp\_wfm\_source\_frame\_error()**](wfm__compose_8h.md#function-dp_wfm_source_frame_error) about its frame:



* `pn_poly` must fit the `pn_length`-bit register ([**pn\_fits\_register()**](pn__core_8h.md#function-pn_fits_register)). The generator masks a wider one, silently, so `pn_poly = 0x40` on a 5-bit register is a register with no feedback: the seed, then zeros, a constant waveform that still looks like a PN source (doppler#1636). 0 selects the maximal-length polynomial and always fits.
* No two members the surface table declares exclusive may both be set (`WFM_SURFACE_EXCLUSIVE`, [**wfm\_surface.h**](wfm__surface_8h.md)): a carried `frame` is the whole frame, so a `payload` beside it would be dropped (doppler#1683). The CLI and a scene refuse the same pair first, naming their own spelling.
* A dsss source has the codes it needs (doppler#1696): a burst whose frame has bits to spread needs `data_code` (dp\_wfm\_why\_dsss\_frame\_no\_data\_code), a burst needs a preamble or a frame (dp\_wfm\_why\_dsss\_empty), and a continuous stream needs `data_code` (dp\_wfm\_why\_dsss\_cont\_no\_data\_code). A preamble alone is a valid burst: an acquisition stimulus.




A rule that needs the segment's sample rate is not here  a source does not carry it  but in [**dp\_wfm\_scene\_error()**](wfm__compose_8h.md#function-dp_wfm_scene_error), which asks this first.




**Parameters:**


* `src` The source. 



**Returns:**

NULL if there is nothing wrong, else a static message.



```C++
wfm_source_t s = { .type = WFM_SYNTH_PN, .sps = 1, .pn_length = 5,
                   .pn_poly = 0x40 };
dp_wfm_source_error (&s);   // "pn_poly has a bit above ..."
s.pn_poly = 0x12;
dp_wfm_source_error (&s);   // NULL: x^5 + x^2 + 1 fits
```
 


        

<hr>



### function dp\_wfm\_source\_frame\_error 

_NULL when this source's frame fields can be honoured; else why not._ 
```C++
const char * dp_wfm_source_frame_error (
    const wfm_source_t * src
) 
```



ONE rule, asked by all three faces — the wfmgen CLI before it generates, the standalone `Synth` through `dp_wfm_source_to_synth`, and the composer through `dp_wfm_compose_create` — because the alternative is what shipped: the flags were accepted, stored and readable back on every face, and applied on none of them, so a caller who asked for a framed waveform silently got an unframed one.


A frame needs a payload. `type=bits` and the PN-sourced `bpsk`/`qpsk`/`pn` carry one when a payload is given (literal or generated, gh-762); the latter are then built as a BITS synth ([**dp\_wfm\_source\_synth\_type()**](wfm__compose_8h.md#function-dp_wfm_source_synth_type)). Types with no bit stream (tone, noise, chirp, symbols) are refused with a reason.




**Parameters:**


* `src` The source. 



**Returns:**

NULL if there is nothing wrong, else a static message. 





        

<hr>



### function dp\_wfm\_source\_has\_frame 

_Non-zero when this source describes a FRAME._ 
```C++
int dp_wfm_source_has_frame (
    const wfm_source_t * src
) 
```



A carried description, a preamble or a sync word is what says "framed". **Deliberately not `crc`**: it defaults to crc16 on every source (`[[module.wfm_compose.source.fields]]` and wfmgen alike), so reading it as intent would silently append a trailer to every unframed bit pattern anyone has ever generated. With neither a preamble nor a sync word, `crc` stays inert exactly as it always was.




**Parameters:**


* `src` The source; NULL reads as unframed. 




        

<hr>



### function dp\_wfm\_source\_synth\_type 

_The synth type to create this source with._ 
```C++
int dp_wfm_source_synth_type (
    const wfm_source_t * src
) 
```



The source's own type, except for a FRAMED `bpsk`/`qpsk`/`pn`: that one transmits its frame, and the only synth that plays a bit pattern is a `WFM_SYNTH_BITS` one (`dp_wfm_synth_set_bits()` is a no-op on any other), so it is created as BITS and [**dp\_wfm\_source\_attach\_frame()**](wfm__compose_8h.md#function-dp_wfm_source_attach_frame) hands it the frame with the mapping the type names (bpsk for `bpsk`/`pn`, Gray QPSK for `qpsk`). Created with its own type it played the LFSR stream and dropped the frame (doppler#1616).


Asked by both construction faces (`dp_wfm_compose_build_synth` and the standalone `dp_wfm_source_to_synth`) and by [**dp\_wfm\_source\_create\_snr()**](wfm__compose_8h.md#function-dp_wfm_source_create_snr), which refers such a source's SNR to fs so its noise stays in the reference its type names.



```C++
wfm_source_t s = { .type = WFM_SYNTH_BPSK };
int t = dp_wfm_source_synth_type (&s); // unframed: WFM_SYNTH_BPSK
```





**Parameters:**


* `src` The source. 



**Returns:**

A `WFM_SYNTH_*` type. 





        

<hr>



### function dp\_wfm\_source\_to\_synth\_error 

_Why_ [_**dp\_wfm\_source\_to\_synth()**_](wfm__compose__bridge_8h.md#function-dp_wfm_source_to_synth) _refused this source, or NULL._
```C++
const char * dp_wfm_source_to_synth_error (
    const wfm_source_t * src,
    double fs
) 
```



The standalone `Synth`'s reason channel (just-makeit's `bridge_error_fn`, which takes the bridge's own arguments): [**dp\_wfm\_scene\_error()**](wfm__compose_8h.md#function-dp_wfm_scene_error) of a scene of one segment at `fs`, so a refused Synth raises the same sentence as every other face, including a rule that needs the rate. NULL leaves the binding's generic error, for a refusal that is not the source's.




**Parameters:**


* `src` The source. 
* `fs` The sample rate the bridge was given. 



**Returns:**

A static sentence, or NULL.



```C++
wfm_source_t s = { .type = WFM_SYNTH_PN, .sps = 1, .pn_length = 5,
                   .pn_poly = 0x40 };
dp_wfm_source_to_synth_error (&s, 1e6);   // dp_wfm_why_pn_poly
```
 


        

<hr>



### function dp\_wfm\_spec\_headroom 

_The top-level_ `headroom` _(dB) from a spec JSON, or 0 if absent._
```C++
double dp_wfm_spec_headroom (
    const char * json
) 
```



Lets `--from-file` reproduce a recorded `--headroom`; the value is a writer gain, so it lives outside the composer state. 


        

<hr>



### function dp\_wfm\_spec\_template\_json 

_A ready-to-edit example spec in the canonical_  _from-file schema._
```C++
char * dp_wfm_spec_template_json (
    void
) 
```



Returns a representative multi-segment template — an inline tone, an RRC-shaped QPSK-from-bits burst with a trailing gap, and a two-source additive `sum` mix — serialised with [**dp\_wfm\_spec\_to\_json()**](wfm__compose_8h.md#function-dp_wfm_spec_to_json), so it is valid by construction and round-trips through [**dp\_wfm\_compose\_from\_json()**](wfm__compose_8h.md#function-dp_wfm_compose_from_json) unchanged. It therefore doubles as a working starting point for `wfmgen --from-file`, not just documentation: dump it, edit the fields, feed it back.




**Returns:**

malloc'd JSON (caller frees), or NULL on allocation failure. 





        

<hr>



### function dp\_wfm\_spec\_to\_json 

_Serialise a spec to a JSON string (for_  _record)._
```C++
char * dp_wfm_spec_to_json (
    const wfm_segment_t * segs,
    size_t n_segs,
    int repeat,
    int continuous,
    int seed_advance,
    double headroom
) 
```



`seed_advance` (a `wfm_seed_advance_t`) and `headroom` (dB of output backoff applied at the writer, not the composer) are each emitted as a top-level field only when non-default, so an unrecorded run and any older spec stay byte-identical. Read `headroom` back with [**dp\_wfm\_spec\_headroom()**](wfm__compose_8h.md#function-dp_wfm_spec_headroom); the parser reads `seed_advance` straight onto the composer.


`seed_advance` is a parameter rather than something read from `segs` because it is a property of the whole stream, like `repeat`/`continuous`. Omitting it is what made a recorded run replay a DIFFERENT waveform (doppler#978): the key was parsed and never written, so the round-trip silently fell back to NONE and every loop after the first came out identical.




**Returns:**

malloc'd JSON (caller frees), or NULL on allocation failure. 





        

<hr>



### function dp\_wfm\_synth\_attach\_data 

_Drive a type=bits synth from a frame whose payload is a data source._ 
```C++
int dp_wfm_synth_attach_data (
    dp_wfm_synth_state_t * syn,
    const wfm_frame_desc_t * d,
    const wfm_frame_ops_t * ops,
    wfm_data_src_t * src,
    wfm_data_pacing_t pacing,
    int modulation
) 
```



The pull that replaces the cycle (docs/design/payload-data-source.md §7). Each frame is `d` assembled over the next chunk of `src` (dp\_wfm\_frame\_assemble\_data): the first now, and each one after at the previous frame's last bit, so every stage over the payload covers its own frame's chunk. Under `pacing`, a source with nothing yet sends an idle frame (dp\_wfm\_data\_frame, the one rule). When `src` ends, the synth goes silent and [**dp\_wfm\_synth\_data\_ended()**](wfm__synth__core_8h.md#function-dp_wfm_synth_data_ended) says so.




**Parameters:**


* `syn` a synth created with type=bits. 
* `d` the frame; exactly one field is `data:LEN`, and LEN is the source's. 
* `ops` stage kernels, as [**dp\_wfm\_frame\_assemble()**](wfm__frame_8h.md#function-dp_wfm_frame_assemble); may be NULL. 
* `src` the data source; the synth OWNS it from here, success or not, and frees it with the synth. 
* `pacing` WFM\_DATA\_PACED for `--realtime`, else WFM\_DATA\_UNPACED. 
* `modulation` as [**dp\_wfm\_synth\_set\_bits()**](wfm__synth__core_8h.md#function-dp_wfm_synth_set_bits). 



**Returns:**

0, or -1: not a bits synth, no data field in `d`, a frame that does not assemble, or a source that failed its first read.



```C++
wfm_frame_desc_t d;
const wfm_seq_t  data = { .kind = WFM_SEQ_DATA, .len = 16 };
dp_wfm_frame_fixed (&d, NULL, 0, NULL, &data, 1);        // [data:16 | crc]
wfm_data_src_t *src = dp_wfm_data_create ("0x0123456789AB", NULL, 16, NULL,
                                          NULL);
dp_wfm_synth_state_t *s
    = dp_wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0, 100.0, 0, 1, 8, 7, 0,
                           0, 0.0);
dp_wfm_synth_attach_data (s, &d, NULL, src, WFM_DATA_UNPACED, 1); // bpsk
// three 32-bit frames, each over its own 16-bit chunk, then silence
dp_wfm_synth_destroy (s);                 // frees src too
```
 


        

<hr>



### function dp\_wfm\_synth\_data\_source 

_The data source a synth pulls from (dp\_wfm\_synth\_attach\_data), or NULL: its stats are the run's truth for scoring._ 
```C++
const wfm_data_src_t * dp_wfm_synth_data_source (
    const dp_wfm_synth_state_t * syn
) 
```




<hr>



### function dp\_wfm\_synth\_set\_data\_pacing 

_Pace a synth's data source: WFM\_DATA\_PACED for_ `--realtime` _._
```C++
void dp_wfm_synth_set_data_pacing (
    dp_wfm_synth_state_t * syn,
    wfm_data_pacing_t pacing
) 
```



A synth attached by [**dp\_wfm\_source\_attach\_frame()**](wfm__compose_8h.md#function-dp_wfm_source_attach_frame) pulls UNPACED  it waits for its data, as `cat` does. A paced caller (the composer under `--realtime`) sets this after the build, so a source with nothing yet sends an idle frame instead (dp\_wfm\_data\_frame, the one rule). A synth with no data source ignores it.



```C++
dp_wfm_synth_state_t *s = dp_wfm_synth_create (
    WFM_SYNTH_BITS, 1e6, 0.0, 100.0, 0, 1, 8, 7, 0, 0, 0.0);
dp_wfm_synth_set_data_pacing (s, WFM_DATA_PACED); // no source: no-op
dp_wfm_synth_destroy (s);
```
 


        

<hr>
## Public Static Functions Documentation




### function dp\_wfm\_scene\_fs 

_The ONE answer to "what is this stream's sample rate": the_ `fs` _every segment shares, or 0.0 when they differ._
```C++
static inline double dp_wfm_scene_fs (
    const wfm_segment_t * segs,
    size_t n_segs
) 
```



`fs` is per segment, and a scene whose segments differ is legal: no single rate is true of it. 0.0 is the library's "not stated" (a Writer opened at `fs=0.0`, a SigMF document without `core:sample_rate`), so an output asks this and either states the rate it returns or, given 0.0, says nothing  or refuses, if its format cannot say nothing (a BLUE header has one `xdelta`). Every output asks here rather than reading `segs[0].fs`, which is a rate only when they agree (doppler#1733).



```C++
wfm_segment_t s[2] = { { .fs = 6e6 }, { .fs = 6e6 } };
int ok = dp_wfm_scene_fs (s, 2) == 6e6;
s[1].fs = 2e6;
ok = ok && dp_wfm_scene_fs (s, 2) == 0.0 && dp_wfm_scene_fs (s, 0) == 0.0;
return ok ? 0 : 1;
```





**Parameters:**


* `segs` the segments; may be NULL when `n_segs` is 0. 
* `n_segs` their count. 



**Returns:**

the shared fs, or 0.0 when the segments differ or there are none. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_compose.h`

