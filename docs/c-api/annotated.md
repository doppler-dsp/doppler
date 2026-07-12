
# Class List


Here are the classes, structs, unions and interfaces with brief descriptions:

* **struct** [**RateConverter\_state\_t**](structRateConverter__state__t.md) _Cascade state_  _owns all sub-stage C objects._    
* **struct** [**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) _AccCf64 state._     
* **struct** [**acc\_f32\_state\_t**](structacc__f32__state__t.md) _AccF32 state._     
* **struct** [**acc\_q15\_state\_t**](structacc__q15__state__t.md) _AccQ15 state._     
* **struct** [**acc\_q8\_state\_t**](structacc__q8__state__t.md) _AccQ8 state._     
* **struct** [**acc\_trace\_state\_t**](structacc__trace__state__t.md) _AccTrace state. Allocate with_ [_**acc\_trace\_create()**_](acc__trace__core_8h.md#function-acc_trace_create) _._    
* **struct** [**acq\_extra\_t**](structacq__extra__t.md) _Per-object extra header for an engine's cross-call state._     
* **struct** [**acq\_result\_t**](structacq__result__t.md) _One acquisition detection event._     
* **struct** [**acq\_state\_t**](structacq__state__t.md) _Streaming acquisition-engine state._     
* **struct** [**adc\_state\_t**](structadc__state__t.md) _ADC state._     
* **struct** [**agc\_state\_t**](structagc__state__t.md) _AGC state._     
* **struct** [**agc\_tlm\_t**](structagc__tlm__t.md) _Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then a single predicted-not-taken branch. Zeroed in state blobs and preserved across set\_state (DP\_DEFINE\_POD\_STATE\_TLM); telemetry is observation, not DSP state that migrates._     
* **struct** [**awgn\_state\_t**](structawgn__state__t.md)     
* **struct** [**boxcar\_state\_t**](structboxcar__state__t.md) _Boxcar moving-average state (cf32)._     
* **struct** [**burst\_demod\_state\_t**](structburst__demod__state__t.md) _BurstDemod state. Allocate with_ [_**burst\_demod\_create()**_](burst__demod__core_8h.md#function-burst_demod_create) _._    
* **struct** [**burst\_despreader\_state\_t**](structburst__despreader__state__t.md) _BurstDespreader state._     
* **struct** [**carrier\_mpsk\_state\_t**](structcarrier__mpsk__state__t.md) _M-PSK carrier loop state._     
* **struct** [**carrier\_nda\_state\_t**](structcarrier__nda__state__t.md) _NDA M-th-power carrier loop state._     
* **struct** [**carrier\_nda\_tlm\_t**](structcarrier__nda__tlm__t.md) _Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — the probe site is then a single predicted-not-taken branch per block loop. Zeroed in state blobs and preserved across set\_state (DP\_DEFINE\_POD\_STATE\_TLM)._     
* **struct** [**cic\_state\_t**](structcic__state__t.md) _CIC filter state._     
* **struct** [**corr2d\_state\_t**](structcorr2d__state__t.md) _2-D FFT correlator state._     
* **struct** [**corr\_state\_t**](structcorr__state__t.md) _1-D FFT correlator state._     
* **struct** [**costas\_state\_t**](structcostas__state__t.md) _Costas loop state._     
* **struct** [**costas\_tlm\_t**](structcostas__tlm__t.md) _Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then a single predicted-not-taken branch per symbol. Zeroed in state blobs and preserved across set\_state (DP\_DEFINE\_POD\_STATE\_TLM)._     
* **struct** [**ddc\_extra\_t**](structddc__extra__t.md)     
* **struct** [**ddcr\_extra\_t**](structddcr__extra__t.md)     
* **struct** [**delay\_state\_t**](structdelay__state__t.md) _Delay state._     
* **struct** [**despreader\_state\_t**](structdespreader__state__t.md) _Despreader state._     
* **struct** [**det\_result2d\_t**](structdet__result2d__t.md) _Detection event returned by_ [_**detector2d\_push()**_](detector2d__core_8h.md#function-detector2d_push) _._    
* **struct** [**det\_result\_t**](structdet__result__t.md) _Detection event returned by_ [_**detector\_push()**_](detector__core_8h.md#function-detector_push) _._    
* **struct** [**detector2d\_state\_t**](structdetector2d__state__t.md) _2-D signal detector state._     
* **struct** [**detector\_state\_t**](structdetector__state__t.md) _1-D signal detector state._     
* **struct** [**dll\_state\_t**](structdll__state__t.md) _DLL state._     
* **struct** [**dll\_tlm\_t**](structdll__tlm__t.md) _Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then a single predicted-not-taken branch per code epoch. Zeroed in state blobs and preserved across set\_state (the hand-written triplet treats it like the borrowed_ `code` _)._    
* **struct** [**dp\_header\_t**](structdp__header__t.md) _Frame metadata header carried in every stream message._     
* **struct** [**dp\_peak\_t**](structdp__peak__t.md) _One spectral peak returned by_ [_**find\_peaks\_f32()**_](spectral__core_8h.md#function-find_peaks_f32) _._    
* **struct** [**dp\_reader\_t**](structdp__reader__t.md)     
* **struct** [**dp\_sample\_clock\_t**](structdp__sample__clock__t.md)     
* **struct** [**dp\_state\_hdr\_t**](structdp__state__hdr__t.md) _Common 16-byte envelope at the head of every state blob._     
* **struct** [**dp\_tlm**](structdp__tlm.md) _Telemetry context: probe registry + SPSC record ring._     
* **struct** [**dp\_tlm\_probe\_t**](structdp__tlm__probe__t.md) _Per-probe registry entry: name, decimation and accounting._     
* **struct** [**dp\_tlm\_rec\_t**](structdp__tlm__rec__t.md) _One telemetry sample: a probe's scalar value at sample index_ `n` _._    
* **struct** [**dp\_writer\_t**](structdp__writer__t.md)     
* **struct** [**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) _F32ToI16 state._     
* **struct** [**f32\_to\_i16u32\_state\_t**](structf32__to__i16u32__state__t.md) _F32ToI16U32 state._     
* **struct** [**f32\_to\_i16u64\_state\_t**](structf32__to__i16u64__state__t.md) _F32ToI16U64 state._     
* **struct** [**f32\_to\_uq15\_state\_t**](structf32__to__uq15__state__t.md) _F32ToUQ15 state._     
* **struct** [**farrow\_state\_t**](structfarrow__state__t.md) _Farrow interpolator state (4-tap delay line + order)._     
* **struct** [**fft2d\_state\_t**](structfft2d__state__t.md)     
* **struct** [**fft\_state\_t**](structfft__state__t.md)     
* **struct** [**fir\_state\_t**](structfir__state__t.md)     
* **struct** [**hbdecim\_q15\_state\_t**](structhbdecim__q15__state__t.md)     
* **struct** [**hbdecim\_state\_t**](structhbdecim__state__t.md)     
* **struct** [**i16\_to\_f32\_state\_t**](structi16__to__f32__state__t.md) _I16ToF32 state._     
* **struct** [**i16u32\_to\_f32\_state\_t**](structi16u32__to__f32__state__t.md) _I16U32ToF32 state._     
* **struct** [**i16u64\_to\_f32\_state\_t**](structi16u64__to__f32__state__t.md) _I16U64ToF32 state._     
* **struct** [**i32\_to\_f32\_state\_t**](structi32__to__f32__state__t.md) _I32ToF32 state._     
* **struct** [**i8\_to\_f32\_state\_t**](structi8__to__f32__state__t.md) _I8ToF32 state._     
* **struct** [**imd\_meas\_t**](structimd__meas__t.md) _Two-tone intermodulation result (IMD2/IMD3/TOI)._     
* **struct** [**imdmeas\_state\_t**](structimdmeas__state__t.md) _IMDMeasure state: owned window, FFT plan and one-sided power scratch._     
* **struct** [**lo\_state\_t**](structlo__state__t.md) _LO state._     
* **struct** [**lockdet\_state\_t**](structlockdet__state__t.md) _Lock-detector state (embeddable by value; pointer-free POD)._     
* **struct** [**loop\_filter\_state\_t**](structloop__filter__state__t.md) _Second-order PI loop filter state (embeddable by value)._     
* **struct** [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) _M-PSK receiver state._     
* **struct** [**nco\_state\_t**](structnco__state__t.md) _NCO state._     
* **struct** [**npr\_meas\_t**](structnpr__meas__t.md) _Noise Power Ratio (notched-noise loading) result._     
* **struct** [**nprmeas\_state\_t**](structnprmeas__state__t.md) _NPRMeasure state: owned window, FFT plan and one-sided power scratch._     
* **struct** [**pn\_state\_t**](structpn__state__t.md)     
* **struct** [**ppe\_result\_t**](structppe__result__t.md) _Polynomial-phase estimate (one search)._     
* **struct** [**ppe\_state\_t**](structppe__state__t.md) _PolynomialPhaseEstimator state (FFT plan + rate grid + scratch)._     
* **struct** [**psd\_state\_t**](structpsd__state__t.md) _PSD state. Allocate with_ [_**psd\_create()**_](psd__core_8h.md#function-psd_create) _._    
* **struct** [**resamp\_state\_t**](structresamp__state__t.md)     
* **struct** [**specan\_state\_t**](structspecan__state__t.md) _Specan state. Allocate with_ [_**specan\_create()**_](specan__core_8h.md#function-specan_create) _._    
* **struct** [**symsync\_state\_t**](structsymsync__state__t.md) _SymbolSync state._     
* **struct** [**symsync\_tlm\_t**](structsymsync__tlm__t.md) _Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then a single predicted-not-taken branch per recovered symbol. Zeroed in state blobs and preserved across set\_state (DP\_DEFINE\_POD\_STATE\_TLM)._     
* **struct** [**time\_stats\_t**](structtime__stats__t.md) _Time-domain capture statistics (AC-coupled crest/PAPR)._     
* **struct** [**tone\_meas\_t**](structtone__meas__t.md) _Single-tone dynamic-measurement bag._     
* **struct** [**tonemeas\_state\_t**](structtonemeas__state__t.md) _ToneMeasure state: owned window, FFT plan and analysis scratch._     
* **struct** [**uq15\_to\_f32\_state\_t**](structuq15__to__f32__state__t.md) _UQ15ToF32 state._     
* **struct** [**wfm\_reader\_info\_t**](structwfm__reader__info__t.md)     
* **struct** [**wfm\_segment\_t**](structwfm__segment__t.md) _One composer segment: one or more sources summed over the same span, then a trailing off-time gap._     
* **struct** [**wfm\_source\_t**](structwfm__source__t.md) _One additive source within a segment: a_ `synth` _config + its level._    
* **struct** [**wfm\_span\_t**](structwfm__span__t.md) _One rendered segment instance's exact timing: where it lands in the composed stream and how its_ `delay | on | off` _spans divide it._    
* **struct** [**wfm\_synth\_state\_t**](structwfm__synth__state__t.md) _Synth state._     

