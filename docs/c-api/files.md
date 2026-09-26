
# File List

Here is a list of all files with brief descriptions:


* **dir** [**native**](dir_3dbb10954ed03e2c7eb007b10aa2d80b.md)     
    * **dir** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md)     
        * **dir** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md)     
            * **file** [**clib\_common.h**](clib__common_8h.md)     
            * **file** [**doppler.h**](doppler_8h.md) 
            * **file** [**dp\_complex.h**](dp__complex_8h.md) _The complex-math surface, routed so it survives Windows._ 
            * **file** [**dp\_crc16.h**](dp__crc16_8h.md) _CRC-16-CCITT over a bit stream — the one CRC shared by every doppler frame producer and consumer._     
            * **file** [**dp\_format.h**](dp__format_8h.md) _Complex sample formats, named by their BLUE/Platinum codes._     
            * **file** [**dp\_interleave.h**](dp__interleave_8h.md) _Block interleaving — the permutation, and nothing else._     
            * **file** [**dp\_interrupt.h**](dp__interrupt_8h.md) _Asking a blocking wait to stop, whatever it is waiting on._     
            * **file** [**dp\_interrupt\_pyadopt.h**](dp__interrupt__pyadopt_8h.md)     
            * **file** [**dp\_isotime.h**](dp__isotime_8h.md) _ISO 8601 UTC timestamps in both spellings — filename-safe_ **basic** _for names doppler writes,_**extended** _for the wire formats that mandate it._    
            * **file** [**dp\_parallel.h**](dp__parallel_8h.md)     
            * **file** [**dp\_simd.h**](dp__simd_8h.md) _doppler's own composite SIMD reductions, layered over_ `jm_simd.h` _._    
            * **file** [**dp\_state.h**](dp__state_8h.md)     
            * **file** [**dp\_state\_pyhelp.h**](dp__state__pyhelp_8h.md)     
            * **file** [**dp\_syncword.h**](dp__syncword_8h.md) _Finding a known bit pattern in an unpacked bit stream — the sync word search, and the arithmetic for choosing its threshold._     
            * **file** [**dp\_thread.h**](dp__thread_8h.md) _The threading primitives doppler uses, with one platform split._     
            * **file** [**jm\_perf.h**](jm__perf_8h.md)     
            * **file** [**jm\_simd.h**](jm__simd_8h.md)     
            * **dir** [**HalfbandDecimator**](dir_7d0e752fb42c448bafa7d80e7fc4aaf0.md)     
                * **file** [**HalfbandDecimator\_core.h**](HalfbandDecimator__core_8h.md) _Halfband 2:1 decimator for CF32 IQ (adapter over hbdecim\_core)._     
            * **dir** [**RateConverter**](dir_f243cfbf2f82d96e6953dd99cc2498dd.md)     
                * **file** [**RateConverter\_core.h**](RateConverter__core_8h.md) _Optimal-speed rate conversion cascade._     
            * **dir** [**Resampler**](dir_9b0990bb8296ade48d8f038050fb64f1.md)     
                * **file** [**Resampler\_core.h**](Resampler__core_8h.md) _Continuously-variable polyphase resampler, CF32 IQ._     
            * **dir** [**acc\_cf64**](dir_d950eea1844c6f23a7b4df3cf640e9a2.md)     
                * **file** [**acc\_cf64\_core.h**](acc__cf64__core_8h.md) _AccCf64 component API._     
            * **dir** [**acc\_f32**](dir_c19b9e056cbdf00f39bf52805b44beb0.md)     
                * **file** [**acc\_f32\_core.h**](acc__f32__core_8h.md) _AccF32 component API._     
            * **dir** [**acc\_q15**](dir_2344cd9a4aadb833503124e5257faf5b.md)     
                * **file** [**acc\_q15\_core.h**](acc__q15__core_8h.md) _AccQ15 — a running 64-bit integer accumulator for Q15 (int16\_t) samples. Internally sums each sample into a 64-bit accumulator, which prevents overflow even for very long block lengths. Use get() to read the running total non-destructively, or dump() to read-and-reset in one call._     
            * **dir** [**acc\_q8**](dir_ea0f50795da7b2248dba00cbaac7e694.md)     
                * **file** [**acc\_q8\_core.h**](acc__q8__core_8h.md) _AccQ8 — a running 32-bit integer accumulator for Q8 (int8\_t) samples. Internally sums each sample into a 32-bit accumulator, which can hold up to 2^24 maximum-magnitude Q8 samples before overflow. Use get() for a non-destructive read, or dump() to read-and-reset in one atomic call._     
            * **dir** [**acc\_trace**](dir_ea4259ba3dd1c044f0efb519286a18a5.md)     
                * **file** [**acc\_trace\_core.h**](acc__trace__core_8h.md) _AccTrace — per-bin vector trace accumulator._     
            * **dir** [**accumulator**](dir_36cdc8980ffec967038c3fbc81b64143.md)     
                * **file** [**accumulator\_core.h**](accumulator__core_8h.md) _Accumulator module — public C API._ 
            * **dir** [**acq**](dir_34926d0c3adbcf3f65d9e19727d3354d.md)     
                * **file** [**acq\_core.h**](acq__core_8h.md) _Streaming DSSS acquisition engine — burst and continuous front doors over one shared engine._     
            * **dir** [**acquire**](dir_684079cd19bcefcd0ee51c221517a041.md)     
                * **file** [**acquire\_core.h**](acquire__core_8h.md) _Acquire module — public C API._     
            * **dir** [**adc**](dir_eddcd1edcea89729b407f545f79f2d08.md)     
                * **file** [**adc\_core.h**](adc__core_8h.md) _Signed two's-complement ADC model._     
            * **dir** [**agc**](dir_da2fce83534b434d126c978bac57abe5.md)     
                * **file** [**agc\_core.h**](agc__core_8h.md) _Log-domain automatic gain control (AGC)._     
            * **dir** [**analyzer**](dir_bc9ffe1c503aeccbb822ea34694fc346.md)     
                * **file** [**analyzer\_core.h**](analyzer__core_8h.md) _Analyzer module — public C API._ 
            * **dir** [**arith**](dir_d0f844c85d44525a1700464c9da275c4.md)     
                * **file** [**arith\_core.h**](arith__core_8h.md) _Arith module — public C API for fixed-point arithmetic on Q15 (int16\_t) and Q8 (int8\_t) arrays. All elementwise operations write into a caller-supplied output buffer of the same length as the shorter input. Saturation clamps results to the representable range rather than wrapping, matching the two's-complement DSP convention._     
            * **dir** [**async\_dsss\_pool**](dir_8a6668f3097fb7847a23b1f65b3df26f.md)     
                * **file** [**async\_dsss\_pool\_core.h**](async__dsss__pool__core_8h.md) _AsyncDsssPool_  _one object holds the population: a searcher, a pool of cell receivers, the assigned table and the event log (docs/design/async-dsss-receiver.md section 8.2)._    
            * **dir** [**async\_dsss\_receiver**](dir_565c73e0c8995e904663dd9fb6485ceb.md)     
                * **file** [**async\_dsss\_receiver\_core.h**](async__dsss__receiver__core_8h.md) _Composed continuous DSSS receiver: Acquisition -&gt; handoff -&gt; CarrierAcquisition refine -&gt; Costas/Dll/RateConverter/ MpskReceiver track, one object._     
            * **dir** [**awgn**](dir_6240b6c8e1c7fd073a984e370d89f937.md)     
                * **file** [**awgn\_core.h**](awgn__core_8h.md) _Additive White Gaussian Noise generator._     
            * **dir** [**ber**](dir_742028dd4040117c60c3f886fa044d64.md)     
                * **file** [**ber\_core.h**](ber__core_8h.md) _Error-rate measurement: settled windows, detected alignment, and an exact confidence interval._     
            * **dir** [**ber\_meter**](dir_88fe8aa1742881a2471cfb33f972e762.md)     
                * **file** [**ber\_meter\_core.h**](ber__meter__core_8h.md) _BerMeter — the error-rate accumulator._     
            * **dir** [**boxcar**](dir_5b2ea30dc12e54f23750507f860119fd.md)     
                * **file** [**boxcar\_core.h**](boxcar__core_8h.md) _Boxcar (rectangular) moving-average filter — cf32, fixed window._     
            * **dir** [**buffer**](dir_bada8e9c2056a5c5c150b079933e5759.md)     
                * **file** [**buffer.h**](buffer_8h.md) _High-performance x86-64 Circular Buffer for RF Streaming._     
                * **file** [**buffer\_core.h**](buffer__core_8h.md) _Buffer module — public C API._ 
            * **dir** [**burst\_acq**](dir_55efb80a743a0f920c38ee6730a791eb.md)     
                * **file** [**burst\_acq\_core.h**](burst__acq__core_8h.md) _BurstAcquisition — thin forwarder onto acq\_core.c's shared engine._     
            * **dir** [**burst\_capture**](dir_5fb975a28c31ccaec359941e78dbfe20.md)     
                * **file** [**burst\_capture\_core.h**](burst__capture__core_8h.md) _BurstCapture — acquisition's output turned into aligned bursts._     
            * **dir** [**burst\_demod**](dir_ab0fdf036101e4998629894503e143b8.md)     
                * **file** [**burst\_demod\_core.h**](burst__demod__core_8h.md) _Feedforward BPSK DSSS frame demodulator._     
            * **dir** [**burst\_despreader**](dir_28ffcd911995597d422ebd972d69802b.md)     
                * **file** [**burst\_despreader\_core.h**](burst__despreader__core_8h.md) _BurstDespreader component API._     
            * **dir** [**carrier\_acq**](dir_92c8a71a0b308d7c304bd53eb19bc933.md)     
                * **file** [**carrier\_acq\_core.h**](carrier__acq__core_8h.md) _CarrierAcquisition — PSDMF residual-carrier frequency refinement._     
            * **dir** [**carrier\_mpsk**](dir_bb3a0f9e61a286c66b69840ec2385900.md)     
                * **file** [**carrier\_mpsk\_core.h**](carrier__mpsk__core_8h.md) _M-PSK carrier-tracking loop (integer-NCO de-rotation + decision PLL)._     
            * **dir** [**carrier\_nda**](dir_6eb92e8a380cf8a945db661ecf671c65.md)     
                * **file** [**carrier\_nda\_core.h**](carrier__nda__core_8h.md) _Non-data-aided (NDA) M-th-power carrier-tracking loop._     
            * **dir** [**ccsds**](dir_c033bcb1c91e23f54b625bfe2cf0448c.md)     
                * **file** [**ccsds\_core.h**](ccsds__core_8h.md) _CCSDS 131.0-B's published literals, as a Python-facing component._     
            * **dir** [**ccsds\_tm**](dir_755172a25247ef56b5f4144aad408377.md)     
                * **file** [**ccsds\_tm.h**](ccsds__tm_8h.md) _CCSDS TM channel coding — the transforms a transfer frame passes through on its way to symbols._     
                * **file** [**ccsds\_tm\_frame.h**](ccsds__tm__frame_8h.md) _The CCSDS frame assembler — where the ASM goes, and the one place the stages' disagreements about what they cover become visible._     
                * **file** [**ccsds\_tm\_rs.h**](ccsds__tm__rs_8h.md) _CCSDS Reed-Solomon (255,223) — the outer code as a CONFIGURATION, and the conventions that only a published value catches._     
            * **dir** [**cic**](dir_b18ea702eaae2e8004fd6583c8b1e641.md)     
                * **file** [**cic\_core.h**](cic__core_8h.md) _CIC decimation filter — 4-stage, M=1, UQ16 integer pipeline._     
            * **dir** [**coding**](dir_69d8a89cba5242a3dfe9624bc2e7c6f6.md)     
                * **file** [**coding\_core.h**](coding__core_8h.md) _Coding module — public C API._ 
            * **dir** [**conv**](dir_dadcb1d47e07452fac6fef665f736671.md)     
                * **file** [**conv\_core.h**](conv__core_8h.md) _Convolutional codes: the code description, the encoder, and the maximum-likelihood decoder that reads the same description._     
            * **dir** [**conv\_enc**](dir_c22965c7b72380eff65e84661867f314.md)     
                * **file** [**conv\_enc\_core.h**](conv__enc__core_8h.md) _The convolutional encoder, as a stateful object over_ `conv` _._    
            * **dir** [**corr**](dir_28e39a8c94dc713f6e89cb1d02b66afa.md)     
                * **file** [**corr\_core.h**](corr__core_8h.md) _1-D FFT-based cross-correlator with coherent integrate-and-dump._     
            * **dir** [**corr2d**](dir_ac96ca94cfbb355eb7a82b081cfe387c.md)     
                * **file** [**corr2d\_core.h**](corr2d__core_8h.md) _2-D FFT-based cross-correlator with coherent integrate-and-dump._     
            * **dir** [**costas**](dir_8ebd78c7800b34d5dee6ef27ff63e7b3.md)     
                * **file** [**costas\_core.h**](costas__core_8h.md) _Costas carrier-tracking loop (integer-NCO de-rotation + PI loop)._     
            * **dir** [**cvt**](dir_409c6e92c9ef1b7281540388592da57d.md)     
                * **file** [**cvt\_core.h**](cvt__core_8h.md) _Cvt module — public C API._     
            * **dir** [**ddc**](dir_4a67ebc391a3fd8e8259ec0993c7169b.md)     
                * **file** [**ddc\_core.h**](ddc__core_8h.md) _Digital Down-Converter — composes LO + RateConverter cascade._     
            * **dir** [**ddcr**](dir_18bb1adfae8df578c1da0090bbf10ccf.md)     
                * **file** [**ddcr\_core.h**](ddcr__core_8h.md) _Real-input Digital Down-Converter — halfband R2C + LO + cascade._     
            * **dir** [**delay**](dir_e9520af345bba2408e131802acc7e37b.md)     
                * **file** [**delay\_core.h**](delay__core_8h.md) _Delay component API._     
            * **dir** [**despreader**](dir_0568e7ebbbbb935946ff07943e2ec07c.md)     
                * **file** [**despreader\_core.h**](despreader__core_8h.md) _Continuous DSSS despreader — Costas carrier loop + DLL code loop._     
            * **dir** [**detection**](dir_c7528e0bd68524c48f260a564c045102.md)     
                * **file** [**detection\_core.h**](detection__core_8h.md) _Detection-theory utilities for the amplitude-ratio test statistic._     
            * **dir** [**detector**](dir_4cdf6fdfdd426ef1a31e056182554d6b.md)     
                * **file** [**det\_private.h**](det__private_8h.md) _Shared internals for detector\_core.c and detector2d\_core.c._     
                * **file** [**detector\_core.h**](detector__core_8h.md) _1-D streaming signal detector with FFT-based correlation, integrate-and-dump, and configurable noise-referenced threshold._     
            * **dir** [**detector2d**](dir_8496bb9d19545edf0ab852f69ada11c8.md)     
                * **file** [**detector2d\_core.h**](detector2d__core_8h.md) _2-D streaming signal detector with FFT2D-based correlation, integrate-and-dump, and configurable noise-referenced threshold._     
            * **dir** [**dll**](dir_c5ff741ba8e1e68126223ee4df379547.md)     
                * **file** [**dll\_core.h**](dll__core_8h.md) _Delay-lock loop (DLL) — non-coherent early/prompt/late code tracking._     
            * **dir** [**doppler\_channel**](dir_8380ecb58e2e244790f54835382515ec.md)     
                * **file** [**doppler\_channel\_core.h**](doppler__channel__core_8h.md) _Clock Doppler as a propagation impairment: dilate the time base and shift the carrier, coherently, from one physical parameter._     
            * **dir** [**dp\_event\_log**](dir_3dd394e4a68853f038b67a354736a79d.md)     
                * **file** [**dp\_event\_log\_core.h**](dp__event__log__core_8h.md) _A run's events as SigMF annotations: appended live, finalized at close._     
            * **dir** [**dp\_interrupt\_guard**](dir_069c076bf5cb5bab676dd02f8ef44735.md)     
                * **file** [**dp\_interrupt\_guard\_core.h**](dp__interrupt__guard__core_8h.md)     
                * **file** [**dp\_interrupt\_guard\_procglobal.h**](dp__interrupt__guard__procglobal_8h.md)     
            * **dir** [**dp\_tlm**](dir_6b0129a28aff69bb09c5c8857f722994.md)     
                * **file** [**dp\_tlm\_core.h**](dp__tlm__core_8h.md) _Lightweight scalar telemetry taps for running DSP objects._     
            * **dir** [**dp\_tlm\_capture**](dir_d9291c2a213cf080735d669eb6078971.md)     
                * **file** [**dp\_tlm\_capture\_core.h**](dp__tlm__capture__core_8h.md) _Lossless telemetry capture: sized by arithmetic, not by guesswork._     
            * **dir** [**dsss**](dir_c33ba84b8db3f8ead33f9dfeadf377c6.md)     
                * **file** [**dsss\_core.h**](dsss__core_8h.md) _Dsss module — public C API._ 
            * **dir** [**dsss\_burst\_receiver**](dir_630068a67b306c85c8348e5ba842eaef.md)     
                * **file** [**dsss\_burst\_receiver\_core.h**](dsss__burst__receiver__core_8h.md) _DsssBurstReceiver — the burst chain composed in C._     
            * **dir** [**dsss\_receiver**](dir_6f60e61f873068f58aa42ec05bfb1a20.md)     
                * **file** [**dsss\_receiver\_core.h**](dsss__receiver__core_8h.md) _Composed continuous DSSS receiver: Acquisition -&gt; Costas(bn\_fll) pre-despread carrier wipeoff -&gt; Dll(segments) -&gt; RateConverter -&gt; MpskReceiver, one object._     
            * **dir** [**f32\_buffer**](dir_81ff08c9d14656f475312d0561398a88.md)     
                * **file** [**f32\_buffer\_core.h**](f32__buffer__core_8h.md) _The complex64 ring as the component just-makeit binds._     
            * **dir** [**f32\_to\_i16**](dir_70d41be9b98525952faca7fac2a162b6.md)     
                * **file** [**f32\_to\_i16\_core.h**](f32__to__i16__core_8h.md) _Scale-and-saturate float-to-int16 converter._     
            * **dir** [**f32\_to\_i16u32**](dir_8ed3b501fdbf556e99afaad1b0fb95c8.md)     
                * **file** [**f32\_to\_i16u32\_core.h**](f32__to__i16u32__core_8h.md) _Scale-and-saturate float to Q15-in-uint32 converter._     
            * **dir** [**f32\_to\_i16u64**](dir_9062d1e43f9eb10312d10225fea9c765.md)     
                * **file** [**f32\_to\_i16u64\_core.h**](f32__to__i16u64__core_8h.md) _Scale-and-saturate float to Q15-in-uint64 converter._     
            * **dir** [**f32\_to\_i32**](dir_b3babbbd6f70adf16ee9e6e7966b639c.md)     
                * **file** [**f32\_to\_i32\_core.h**](f32__to__i32__core_8h.md) _Scale-and-saturate float-to-int32 converter._     
            * **dir** [**f32\_to\_i8**](dir_ed78fc1c59eed5a28e12ccfcd3b5d37e.md)     
                * **file** [**f32\_to\_i8\_core.h**](f32__to__i8__core_8h.md) _Scale-and-saturate float-to-int8 converter._     
            * **dir** [**f32\_to\_uq15**](dir_8d86eabfdce5eb918e7f78782ceed683.md)     
                * **file** [**f32\_to\_uq15\_core.h**](f32__to__uq15__core_8h.md) _Scale-and-saturate float-to-UQ15 (offset-binary uint16) converter._     
            * **dir** [**f64\_buffer**](dir_d7320aa8439c659edb0fbbfd27af0c55.md)     
                * **file** [**f64\_buffer\_core.h**](f64__buffer__core_8h.md) _The complex128 ring as the component just-makeit binds._     
            * **dir** [**farrow**](dir_f5d5da611c5546f094b053d8a6116219.md)     
                * **file** [**farrow\_core.h**](farrow__core_8h.md) _Farrow fractional-delay interpolator — linear / parabolic / cubic._     
            * **dir** [**fft**](dir_1dec96a47631eec21a10469aab3e9a96.md)     
                * **file** [**fft\_core.h**](fft__core_8h.md) _Per-instance 1-D FFT using pocketfft directly._     
            * **dir** [**fft2d**](dir_384db21119e775e355fd287e1a7652d5.md)     
                * **file** [**fft2d\_core.h**](fft2d__core_8h.md) _Per-instance 2-D FFT using pocketfft directly._     
            * **dir** [**filter**](dir_820039e5f1fe5ce84fe724a5385272f3.md)     
                * **file** [**filter\_core.h**](filter__core_8h.md) _Filter module — public C API._     
            * **dir** [**fir**](dir_057b0102e9f7b23965a3d08d49a38aec.md)     
                * **file** [**fir\_core.h**](fir__core_8h.md) _Direct-form FIR filter — real-tap and complex-tap variants._     
            * **dir** [**frame**](dir_f1fb4d4532bf7057e52e2ed064f78108.md)     
                * **file** [**frame\_core.h**](frame__core_8h.md) _A frame's bit layout, held as an object so Python can describe one._     
            * **dir** [**frame\_meter**](dir_ea37d4cf1a6c64bbd4d366fb73a4d95d.md)     
                * **file** [**frame\_meter\_core.h**](frame__meter__core_8h.md) _Frame outcomes accumulated across a record: FER, and sync detection._     
            * **dir** [**gold**](dir_3809e94b548bea726f665dae5e933c35.md)     
                * **file** [**gold\_core.h**](gold__core_8h.md) _Gold code component API._     
            * **dir** [**hbdecim**](dir_29792a392a590bbead7fbef545faea73.md)     
                * **file** [**hbdecim\_core.h**](hbdecim__core_8h.md) _Halfband 2:1 decimator for CF32 IQ samples._     
                * **file** [**hbdecim\_r2c\_core.h**](hbdecim__r2c__core_8h.md) _Real-to-complex halfband 2:1 decimator (Architecture D2)._     
            * **dir** [**hbdecim\_q15**](dir_0dbd0f8cb615d6b620ef5cfddeb71b4a.md)     
                * **file** [**hbdecim\_q15\_core.h**](hbdecim__q15__core_8h.md) _Fixed-point halfband 2:1 decimator for interleaved IQ int16 samples._     
            * **dir** [**i16\_buffer**](dir_5e38c689ce67e24fea3517ffd89658ee.md)     
                * **file** [**i16\_buffer\_core.h**](i16__buffer__core_8h.md) _The int16 I/Q pair ring as the component just-makeit binds._     
            * **dir** [**i16\_to\_f32**](dir_01f347c62069c5233a090c3b4d985bee.md)     
                * **file** [**i16\_to\_f32\_core.h**](i16__to__f32__core_8h.md) _int16-to-float converter with configurable inverse scale._     
            * **dir** [**i16u32\_to\_f32**](dir_c1ecd6bb977db755472db2284b0a6806.md)     
                * **file** [**i16u32\_to\_f32\_core.h**](i16u32__to__f32__core_8h.md) _Q15-in-uint32 to float converter._     
            * **dir** [**i16u64\_to\_f32**](dir_4ae5ec00179a07705a9cd9969d88421c.md)     
                * **file** [**i16u64\_to\_f32\_core.h**](i16u64__to__f32__core_8h.md) _Q15-in-uint64 to float converter._     
            * **dir** [**i32\_to\_f32**](dir_04852aba3b033ffe951e6f7f0437e86c.md)     
                * **file** [**i32\_to\_f32\_core.h**](i32__to__f32__core_8h.md) _int32-to-float converter with configurable inverse scale._     
            * **dir** [**i8\_to\_f32**](dir_fc9b7ccc74f65689d9e6b216ebcebff3.md)     
                * **file** [**i8\_to\_f32\_core.h**](i8__to__f32__core_8h.md) _int8-to-float converter with configurable inverse scale._     
            * **dir** [**imdmeas**](dir_2a8d4e9dde298cc63e616d81cd7ff06c.md)     
                * **file** [**imdmeas\_core.h**](imdmeas__core_8h.md) _IMDMeasure — two-tone intermodulation (IMD2/IMD3) and intercept._     
            * **dir** [**impairment**](dir_110bd2cfc83959b687efb664e4fc8d2d.md)     
                * **file** [**impairment\_core.h**](impairment__core_8h.md) _Impairment module — public C API._ 
            * **dir** [**interleaver**](dir_bb303c16bb49af033112c76b5cc87569.md)     
                * **file** [**interleaver\_core.h**](interleaver__core_8h.md) _Block interleaving as an object — the geometry, held._     
            * **dir** [**interp**](dir_50f22f4a69ea3c5d52671b7845e67b28.md)     
                * **file** [**interp\_core.h**](interp__core_8h.md) _Interp module — public C API._ 
            * **dir** [**interp\_table**](dir_ff54084ce651244803741a1f6d284d09.md)     
                * **file** [**interp\_table\_core.h**](interp__table__core_8h.md) _Periodically-extended interpolated lookup table._     
            * **dir** [**interrupt**](dir_bb0d81ed85f0a381609b116697c05606.md)     
                * **file** [**interrupt\_core.h**](interrupt__core_8h.md) _Interrupt module — public C API._ 
            * **dir** [**lo**](dir_939b5fbb8d3e3ebd3276389efab5bbba.md)     
                * **file** [**lo\_core.h**](lo__core_8h.md) _Local oscillator: NCO + 2^16 sin/cos LUT → CF32 phasors._     
            * **dir** [**lockdet**](dir_0a3dcc380b80b2c01366f0cb5ed7ee07.md)     
                * **file** [**lockdet\_core.h**](lockdet__core_8h.md) _Portable lock detector — level + time hysteresis over any scalar lock metric, embeddable in every loop that makes a lock decision._     
            * **dir** [**loop\_filter**](dir_5a5f36bef1d931095e74791b275c008f.md)     
                * **file** [**loop\_filter\_core.h**](loop__filter__core_8h.md) _Second-order proportional-integral loop filter — the shared engine of every tracking loop (Costas/PLL, DLL, symbol timing)._     
            * **dir** [**measure**](dir_0caaff9683efc5ec33140a5572570268.md)     
                * **file** [**measure\_core.h**](measure__core_8h.md) _Measure module — shared result structs and module-level helpers._     
            * **dir** [**mpsk**](dir_c69176d330fb67baeda366bef5232651.md)     
                * **file** [**mpsk\_core.h**](mpsk__core_8h.md) _M-PSK constellation: Gray-coded map / demap for BPSK, QPSK, 8PSK._     
            * **dir** [**mpsk\_receiver**](dir_7dd9db0064eee02f2344cc7ae60c611e.md)     
                * **file** [**mpsk\_receiver\_core.h**](mpsk__receiver__core_8h.md) _Pulse-shaped M-PSK receiver: a tuned matched front end and two loops._     
                * **file** [**mpsk\_rx\_loops.h**](mpsk__rx__loops_8h.md) _The two loops an M-PSK receiver closes, independent of its front end._     
            * **dir** [**nco**](dir_64d3c3497a73d925321234bd516bd8bf.md)     
                * **file** [**nco\_core.h**](nco__core_8h.md) _Phase-accumulator NCO, and the one float-&gt;integer boundary everything that steers one has to pass through._     
            * **dir** [**nprmeas**](dir_df189228e030028408da81bd0afea7e3.md)     
                * **file** [**nprmeas\_core.h**](nprmeas__core_8h.md) _NPRMeasure — notched-noise Noise Power Ratio._     
            * **dir** [**pn**](dir_95c43a97468bba5f35ca9859d48b4113.md)     
                * **file** [**pn\_core.h**](pn__core_8h.md) _PN component API._     
            * **dir** [**ppe**](dir_2835f10376dc04900139beed5b4d2457.md)     
                * **file** [**ppe\_core.h**](ppe__core_8h.md) _Feedforward polynomial-phase estimator (frequency + chirp rate)._     
            * **dir** [**psd**](dir_80b4a8b440284bffd6bf0fbeb7bfe41f.md)     
                * **file** [**psd\_core.h**](psd__core_8h.md) _PSD — averaging power-spectral-density estimator (Welch's method) and spectral measurement suite._     
            * **dir** [**ratesync**](dir_6debd7d9ad90888bb5b8a40a122d95e8.md)     
                * **file** [**ratesync\_core.h**](ratesync__core_8h.md) _RateSync — symbol-timing recovery on a matched-filter rate cascade._     
            * **dir** [**resamp**](dir_9d8524a806701f72e323943267d27190.md)     
                * **file** [**resamp\_core.h**](resamp__core_8h.md) _Continuously-variable polyphase resampler for CF32 IQ._     
                * **file** [**resamp\_impl.h**](resamp__impl_8h.md) _Resamp implementation header._ 
            * **dir** [**resample**](dir_4efb1181c8663bd7a6953f73a4eb8dc9.md)     
                * **file** [**resample\_core.h**](resample__core_8h.md) _Resample module — public C API._     
            * **dir** [**rs**](dir_947c1c4c0fb4d4a4d0adf0ec99900cc5.md)     
                * **file** [**rs\_core.h**](rs__core_8h.md) _Reed-Solomon codes: the code description, the encoder, the syndromes and the decoder that corrects — all reading the same description._     
            * **dir** [**rs\_codec**](dir_73572debd583c0e71af980e0bf0d5ccd.md)     
                * **file** [**rs\_codec\_core.h**](rs__codec__core_8h.md) _The Reed-Solomon codec, as an object over_ `rs` _._    
            * **dir** [**snr**](dir_02c47206b4cbe462773a89d72d8a4c36.md)     
                * **file** [**snr\_core.h**](snr__core_8h.md) _Stateless SNR / Es-N0 estimators, data-aided and non-data-aided._     
            * **dir** [**source**](dir_aa288ec3ae47721f4b7c9a32d3b8f472.md)     
                * **file** [**source\_core.h**](source__core_8h.md) _Source module — public C API._ 
            * **dir** [**specan**](dir_6ce576ad24803d600633e2545d7ab991.md)     
                * **file** [**specan\_core.h**](specan__core_8h.md) _Specan — natural-parameter spectrum analyzer (DDC + averaging PSD)._     
            * **dir** [**spectral**](dir_bb80cb4693a043f62f72e53e1f90a405.md)     
                * **file** [**spectral\_core.h**](spectral__core_8h.md) _Spectral module — public C API._     
            * **dir** [**stream**](dir_2fbcc177cba4f14addc502f26acbb8f7.md)     
                * **file** [**stream.h**](stream_8h.md) _Streaming API for doppler — PUB/SUB, PUSH/PULL, REQ/REP._     
                * **file** [**tlm\_sink.h**](tlm__sink_8h.md) _NATS PUB sink for telemetry records._     
            * **dir** [**symsync**](dir_f3bf1d4e9482041dff7e7824fe611ddf.md)     
                * **file** [**symsync\_core.h**](symsync__core_8h.md) _SymbolSync component API._     
            * **dir** [**syncword**](dir_e299403a0e03806f7aa46ec15fc4c91a.md)     
                * **file** [**syncword\_core.h**](syncword__core_8h.md) _Frame synchronisation: find a known marker in a bit stream, and choose the threshold that decides what counts as finding it._     
            * **dir** [**telemetry**](dir_d30f8ea704930bef6ef378fb8c932c43.md)     
                * **file** [**telemetry\_core.h**](telemetry__core_8h.md) _Telemetry module — public C API._ 
            * **dir** [**timing**](dir_f312c7d1315f2596c2b71ef05d5d7b2b.md)     
                * **file** [**timing\_core.h**](timing__core_8h.md)     
            * **dir** [**tonemeas**](dir_e0665d68cb2c2ff7feb28506d37a8bd1.md)     
                * **file** [**tonemeas\_core.h**](tonemeas__core_8h.md) _ToneMeasure — single-tone ADC/converter spectral measurement._     
            * **dir** [**track**](dir_f98cb3fb09d38460dfcf46a77aa842c0.md)     
                * **file** [**track\_core.h**](track__core_8h.md) _Track module — public C API._ 
            * **dir** [**u8\_to\_f32**](dir_b468cb3e760d860bad02e34079834f5d.md)     
                * **file** [**u8\_to\_f32\_core.h**](u8__to__f32__core_8h.md) _Offset-binary uint8 to float converter — the RTL-SDR_ `cu8` _front end._    
            * **dir** [**uq15\_to\_f32**](dir_289e6f8543a5d0b92e78da373782efe4.md)     
                * **file** [**uq15\_to\_f32\_core.h**](uq15__to__f32__core_8h.md) _UQ15 (offset-binary uint16) to float converter._     
            * **dir** [**util**](dir_7dd94ac9e5a2e34ed236c6361f93c476.md)     
                * **file** [**util\_core.h**](util__core_8h.md) _Util module — public C API._     
            * **dir** [**viterbi**](dir_63cd492a1a551091390943fc51915433.md)     
                * **file** [**viterbi\_core.h**](viterbi__core_8h.md) _Soft-decision Viterbi decoding of convolutional codes._     
            * **dir** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md)     
                * **file** [**wfm\_compose.h**](wfm__compose_8h.md) _Multi-segment waveform composer (Phase B)._     
                * **file** [**wfm\_core.h**](wfm__core_8h.md) _Wfmgen module — public C API._     
                * **file** [**wfm\_defaults.h**](wfm__defaults_8h.md)     
                * **file** [**wfm\_dsp.h**](wfm__dsp_8h.md) _DSSS spreading + root-raised-cosine pulse shaping (Phase B)._     
                * **file** [**wfm\_frame.h**](wfm__frame_8h.md) _A frame's BIT layout, described once and read from both ends._     
                * **file** [**wfm\_keywords.h**](wfm__keywords_8h.md) _BLUE extended-header keywords — the X-Midas binary tag/value codec._     
                * **file** [**wfm\_names.h**](wfm__names_8h.md)     
                * **file** [**wfm\_path.h**](wfm__path_8h.md) _Sibling-path construction shared by the wfm reader and writer._     
                * **file** [**wfm\_plan.h**](wfm__plan_8h.md)     
                * **file** [**wfm\_sink.h**](wfm__sink_8h.md) _NATS PUB sink for generated IQ (Phase B)._     
                * **file** [**wfm\_time.h**](wfm__time_8h.md)     
                * **file** [**wfmgen.h**](wfmgen_8h.md)     
            * **dir** [**wfm\_compose**](dir_6d794a7fa9423fe9f7b14be42b83e035.md)     
                * **file** [**wfm\_compose\_bridge.h**](wfm__compose__bridge_8h.md)     
            * **dir** [**wfm\_reader**](dir_f352698a51aeb04a6ff33c180c5d8d41.md)     
                * **file** [**wfm\_reader\_core.h**](wfm__reader__core_8h.md) _Input file types for generated IQ — the dual of wfm\_writer._     
            * **dir** [**wfm\_synth**](dir_3fd4fbfbc7cedac951bbaaf096533da9.md)     
                * **file** [**wfm\_synth\_core.h**](wfm__synth__core_8h.md) _Synth component API._     
            * **dir** [**wfm\_writer**](dir_b07d3bab2c69a70a146b9de213e56c65.md)     
                * **file** [**wfm\_writer\_core.h**](wfm__writer__core_8h.md) _Output file types for generated IQ: raw / csv / BLUE-1000 + SigMF meta._     
            * **file** [**q15\_mac.h**](q15__mac_8h.md) _Static inline Q15 dot-product primitives: scalar fallback and AVX2._     

