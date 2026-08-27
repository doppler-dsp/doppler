
# File List

Here is a list of all files with brief descriptions:


* **dir** [**native**](dir_3dbb10954ed03e2c7eb007b10aa2d80b.md)     
    * **dir** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md)     
        * **file** [**clib\_common.h**](clib__common_8h.md)     
        * **file** [**doppler.h**](doppler_8h.md) 
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
        * **file** [**jm\_perf.h**](jm__perf_8h.md)     
        * **file** [**jm\_simd.h**](jm__simd_8h.md)     
        * **dir** [**HalfbandDecimator**](dir_6ac3f68ee82e011454c15c865a37e192.md)     
            * **file** [**HalfbandDecimator\_core.h**](HalfbandDecimator__core_8h.md) _Halfband 2:1 decimator for CF32 IQ (adapter over hbdecim\_core)._     
        * **dir** [**RateConverter**](dir_ab9e07a54a3e9554c466f24859c37292.md)     
            * **file** [**RateConverter\_core.h**](RateConverter__core_8h.md) _Optimal-speed rate conversion cascade._     
        * **dir** [**Resampler**](dir_6dca75203c5d2d5de468e6acc97392e7.md)     
            * **file** [**Resampler\_core.h**](Resampler__core_8h.md) _Continuously-variable polyphase resampler, CF32 IQ._     
        * **dir** [**acc\_cf64**](dir_a31d3897e2036bab462df07bf5a3b557.md)     
            * **file** [**acc\_cf64\_core.h**](acc__cf64__core_8h.md) _AccCf64 component API._     
        * **dir** [**acc\_f32**](dir_0465294bf3f41af7dbdebf91d81a0c4a.md)     
            * **file** [**acc\_f32\_core.h**](acc__f32__core_8h.md) _AccF32 component API._     
        * **dir** [**acc\_q15**](dir_df770d8a485da99b359af14931eaacf8.md)     
            * **file** [**acc\_q15\_core.h**](acc__q15__core_8h.md) _AccQ15 — a running 64-bit integer accumulator for Q15 (int16\_t) samples. Internally sums each sample into a 64-bit accumulator, which prevents overflow even for very long block lengths. Use get() to read the running total non-destructively, or dump() to read-and-reset in one call._     
        * **dir** [**acc\_q8**](dir_af45fd7415a1bcf5c13e14c3d63a83bf.md)     
            * **file** [**acc\_q8\_core.h**](acc__q8__core_8h.md) _AccQ8 — a running 32-bit integer accumulator for Q8 (int8\_t) samples. Internally sums each sample into a 32-bit accumulator, which can hold up to 2^24 maximum-magnitude Q8 samples before overflow. Use get() for a non-destructive read, or dump() to read-and-reset in one atomic call._     
        * **dir** [**acc\_trace**](dir_51e33d48c4bde6f60a2f27e75677a784.md)     
            * **file** [**acc\_trace\_core.h**](acc__trace__core_8h.md) _AccTrace — per-bin vector trace accumulator._     
        * **dir** [**accumulator**](dir_06136a2119985c3c219633f937232576.md)     
            * **file** [**accumulator\_core.h**](accumulator__core_8h.md) _Accumulator module — public C API._ 
        * **dir** [**acq**](dir_25a1e6db36731e5901b5cfb158eaa462.md)     
            * **file** [**acq\_core.h**](acq__core_8h.md) _Streaming DSSS acquisition engine — burst and continuous front doors over one shared engine._     
        * **dir** [**acquire**](dir_88b93ea666fc84a6b60ee32ac90530e8.md)     
            * **file** [**acquire\_core.h**](acquire__core_8h.md) _Acquire module — public C API._ 
        * **dir** [**adc**](dir_a6be6b8cb61d5f2be55c0b2f94afbd88.md)     
            * **file** [**adc\_core.h**](adc__core_8h.md) _Signed two's-complement ADC model._     
        * **dir** [**agc**](dir_947ec4d62e9dda8dbffe026d57cfb18d.md)     
            * **file** [**agc\_core.h**](agc__core_8h.md) _Log-domain automatic gain control (AGC)._     
        * **dir** [**analyzer**](dir_1d8187026dc08a4fbbc894d9c056e51f.md)     
            * **file** [**analyzer\_core.h**](analyzer__core_8h.md) _Analyzer module — public C API._ 
        * **dir** [**arith**](dir_51d42af7a43550d997314136379d62d2.md)     
            * **file** [**arith\_core.h**](arith__core_8h.md) _Arith module — public C API for fixed-point arithmetic on Q15 (int16\_t) and Q8 (int8\_t) arrays. All elementwise operations write into a caller-supplied output buffer of the same length as the shorter input. Saturation clamps results to the representable range rather than wrapping, matching the two's-complement DSP convention._     
        * **dir** [**async\_dsss\_receiver**](dir_385ab33ef0b6337dfa5d36daa80c4b8c.md)     
            * **file** [**async\_dsss\_receiver\_core.h**](async__dsss__receiver__core_8h.md) _Composed continuous DSSS receiver: Acquisition -&gt; handoff -&gt; CarrierAcquisition refine -&gt; Costas/Dll/RateConverter/ MpskReceiver track, one object._     
        * **dir** [**awgn**](dir_b535f71dd6c18f769df9e4bf89a97331.md)     
            * **file** [**awgn\_core.h**](awgn__core_8h.md) _Additive White Gaussian Noise generator._     
        * **dir** [**ber**](dir_b6e9705448f5ec813187161d6664687c.md)     
            * **file** [**ber\_core.h**](ber__core_8h.md) _Error-rate measurement: settled windows, detected alignment, and an exact confidence interval._     
        * **dir** [**ber\_meter**](dir_01b99f726e31084c217a09fa5a432d53.md)     
            * **file** [**ber\_meter\_core.h**](ber__meter__core_8h.md) _BerMeter — the error-rate accumulator._     
        * **dir** [**boxcar**](dir_4075e3d5389fc37fde93604059f4dd85.md)     
            * **file** [**boxcar\_core.h**](boxcar__core_8h.md) _Boxcar (rectangular) moving-average filter — cf32, fixed window._     
        * **dir** [**buffer**](dir_3a0c1aef7dcd64a21724ce24de18fb81.md)     
            * **file** [**buffer.h**](buffer_8h.md) _High-performance x86-64 Circular Buffer for RF Streaming._     
        * **dir** [**burst\_acq**](dir_d3ec06985dce876581dd948705a4d1da.md)     
            * **file** [**burst\_acq\_core.h**](burst__acq__core_8h.md) _BurstAcquisition — thin forwarder onto acq\_core.c's shared engine._     
        * **dir** [**burst\_demod**](dir_96a22b0098c79a5049df57065c5b8df4.md)     
            * **file** [**burst\_demod\_core.h**](burst__demod__core_8h.md) _Feedforward BPSK DSSS frame demodulator._     
        * **dir** [**burst\_despreader**](dir_311cad0a77759dd1ff95e00f622e2f49.md)     
            * **file** [**burst\_despreader\_core.h**](burst__despreader__core_8h.md) _BurstDespreader component API._     
        * **dir** [**carrier\_acq**](dir_fda2da85aa46b94cfd09d911f4a8e3eb.md)     
            * **file** [**carrier\_acq\_core.h**](carrier__acq__core_8h.md) _CarrierAcquisition — PSDMF residual-carrier frequency refinement._     
        * **dir** [**carrier\_mpsk**](dir_aac9a6642a6538588e08cd0551821cb3.md)     
            * **file** [**carrier\_mpsk\_core.h**](carrier__mpsk__core_8h.md) _M-PSK carrier-tracking loop (integer-NCO de-rotation + decision PLL)._     
        * **dir** [**carrier\_nda**](dir_425637d1941eacd8ae8cdd8750b207f0.md)     
            * **file** [**carrier\_nda\_core.h**](carrier__nda__core_8h.md) _Non-data-aided (NDA) M-th-power carrier-tracking loop._     
        * **dir** [**ccsds\_tm**](dir_c2a51186254da91e75ac1924b4969fdd.md)     
            * **file** [**ccsds\_tm.h**](ccsds__tm_8h.md) _CCSDS TM channel coding — the transforms a transfer frame passes through on its way to symbols._     
            * **file** [**ccsds\_tm\_frame.h**](ccsds__tm__frame_8h.md) _The CCSDS frame assembler — where the ASM goes, and the one place the stages' disagreements about what they cover become visible._     
            * **file** [**ccsds\_tm\_rs.h**](ccsds__tm__rs_8h.md) _CCSDS Reed-Solomon (255,223) — the outer code as a CONFIGURATION, and the conventions that only a published value catches._     
        * **dir** [**cic**](dir_cf560077cc62991e7289ea57a3d930a1.md)     
            * **file** [**cic\_core.h**](cic__core_8h.md) _CIC decimation filter — 4-stage, M=1, UQ16 integer pipeline._     
        * **dir** [**coding**](dir_926dd95c6532485b4c2774b3c84508b0.md)     
            * **file** [**coding\_core.h**](coding__core_8h.md) _Coding module — public C API._ 
        * **dir** [**conv**](dir_779d3467bbcde033259ac71c6a9863bb.md)     
            * **file** [**conv\_core.h**](conv__core_8h.md) _Convolutional codes: the code description, the encoder, and the maximum-likelihood decoder that reads the same description._     
        * **dir** [**conv\_enc**](dir_b689baf1ac742b6ceba235289d5a286b.md)     
            * **file** [**conv\_enc\_core.h**](conv__enc__core_8h.md) _The convolutional encoder, as a stateful object over_ `conv` _._    
        * **dir** [**corr**](dir_17ecfb211582dadfc5fc9d22d4d97fbd.md)     
            * **file** [**corr\_core.h**](corr__core_8h.md) _1-D FFT-based cross-correlator with coherent integrate-and-dump._     
        * **dir** [**corr2d**](dir_55247951d314f4b4a6db9bf46862b830.md)     
            * **file** [**corr2d\_core.h**](corr2d__core_8h.md) _2-D FFT-based cross-correlator with coherent integrate-and-dump._     
        * **dir** [**costas**](dir_9b517cb2745356d7938c9e100210a101.md)     
            * **file** [**costas\_core.h**](costas__core_8h.md) _Costas carrier-tracking loop (integer-NCO de-rotation + PI loop)._     
        * **dir** [**cvt**](dir_7aebb15fbd538257eeb7884581a8ab59.md)     
            * **file** [**cvt\_core.h**](cvt__core_8h.md) _Cvt module — public C API._ 
        * **dir** [**ddc**](dir_b33dc116452ac5c7d7799725e78b6bdc.md)     
            * **file** [**ddc\_core.h**](ddc__core_8h.md) _Digital Down-Converter — composes LO + RateConverter cascade._     
        * **dir** [**ddcr**](dir_46c04c942eb84c8716610cebe515b046.md)     
            * **file** [**ddcr\_core.h**](ddcr__core_8h.md) _Real-input Digital Down-Converter — halfband R2C + LO + cascade._     
        * **dir** [**delay**](dir_01f4b6965a2181d172634d6670b32dc1.md)     
            * **file** [**delay\_core.h**](delay__core_8h.md) _Delay component API._     
        * **dir** [**despreader**](dir_9949992fff5aebed427f83f9eaa478ca.md)     
            * **file** [**despreader\_core.h**](despreader__core_8h.md) _Continuous DSSS despreader — Costas carrier loop + DLL code loop._     
        * **dir** [**detection**](dir_3a1e0e8c534208cc3745b2f53a028862.md)     
            * **file** [**detection\_core.h**](detection__core_8h.md) _Detection-theory utilities for the amplitude-ratio test statistic._     
        * **dir** [**detector**](dir_f93f7a52d403868792375ffc90a1c1d7.md)     
            * **file** [**det\_private.h**](det__private_8h.md) _Shared internals for detector\_core.c and detector2d\_core.c._     
            * **file** [**detector\_core.h**](detector__core_8h.md) _1-D streaming signal detector with FFT-based correlation, integrate-and-dump, and configurable noise-referenced threshold._     
        * **dir** [**detector2d**](dir_bd7354e9665bd912180ec22b3c69b55c.md)     
            * **file** [**detector2d\_core.h**](detector2d__core_8h.md) _2-D streaming signal detector with FFT2D-based correlation, integrate-and-dump, and configurable noise-referenced threshold._     
        * **dir** [**dll**](dir_f3da3e2048ea3a8b9e723d3c5367d8f8.md)     
            * **file** [**dll\_core.h**](dll__core_8h.md) _Delay-lock loop (DLL) — non-coherent early/prompt/late code tracking._     
        * **dir** [**doppler\_channel**](dir_597302de6cb0d177e5e89858f2abab7e.md)     
            * **file** [**doppler\_channel\_core.h**](doppler__channel__core_8h.md) _Clock Doppler as a propagation impairment: dilate the time base and shift the carrier, coherently, from one physical parameter._     
        * **dir** [**dp\_interrupt\_guard**](dir_001936014fd0d8bf32545bf8d71a57c6.md)     
            * **file** [**dp\_interrupt\_guard\_core.h**](dp__interrupt__guard__core_8h.md)     
            * **file** [**dp\_interrupt\_guard\_procglobal.h**](dp__interrupt__guard__procglobal_8h.md)     
        * **dir** [**dp\_tlm**](dir_76b7d6d4427bc094138fa987d2f2ac6b.md)     
            * **file** [**dp\_tlm\_core.h**](dp__tlm__core_8h.md) _Lightweight scalar telemetry taps for running DSP objects._     
        * **dir** [**dp\_tlm\_capture**](dir_c53721efa35f9e05ec164f1aacd6bf30.md)     
            * **file** [**dp\_tlm\_capture\_core.h**](dp__tlm__capture__core_8h.md) _Lossless telemetry capture: sized by arithmetic, not by guesswork._     
        * **dir** [**dsss**](dir_8b18bfb9a64167292d2c60acbfcb2ae1.md)     
            * **file** [**dsss\_core.h**](dsss__core_8h.md) _Dsss module — public C API._     
        * **dir** [**dsss\_burst\_receiver**](dir_32a143d35207eb7d99f4a541895f77eb.md)     
            * **file** [**dsss\_burst\_receiver\_core.h**](dsss__burst__receiver__core_8h.md) _DsssBurstReceiver — the burst chain composed in C._     
        * **dir** [**dsss\_receiver**](dir_39e39d42b234cb6483b3a80e996300fe.md)     
            * **file** [**dsss\_receiver\_core.h**](dsss__receiver__core_8h.md) _Composed continuous DSSS receiver: Acquisition -&gt; Costas(bn\_fll) pre-despread carrier wipeoff -&gt; Dll(segments) -&gt; RateConverter -&gt; MpskReceiver, one object._     
        * **dir** [**f32\_to\_i16**](dir_e25c96329f88166d8f87eefdc2ba64fa.md)     
            * **file** [**f32\_to\_i16\_core.h**](f32__to__i16__core_8h.md) _Scale-and-saturate float-to-int16 converter._     
        * **dir** [**f32\_to\_i16u32**](dir_5361bfc3c658147f85e2e18e4bfef9b4.md)     
            * **file** [**f32\_to\_i16u32\_core.h**](f32__to__i16u32__core_8h.md) _Scale-and-saturate float to Q15-in-uint32 converter._     
        * **dir** [**f32\_to\_i16u64**](dir_212e21299d76aa740bbad8810e4bf50a.md)     
            * **file** [**f32\_to\_i16u64\_core.h**](f32__to__i16u64__core_8h.md) _Scale-and-saturate float to Q15-in-uint64 converter._     
        * **dir** [**f32\_to\_uq15**](dir_4e8c99e54919bb49218552fb8f2fb678.md)     
            * **file** [**f32\_to\_uq15\_core.h**](f32__to__uq15__core_8h.md) _Scale-and-saturate float-to-UQ15 (offset-binary uint16) converter._     
        * **dir** [**farrow**](dir_3474bb67440308cdab2155867b5160e7.md)     
            * **file** [**farrow\_core.h**](farrow__core_8h.md) _Farrow fractional-delay interpolator — linear / parabolic / cubic._     
        * **dir** [**fft**](dir_5dc24668fb1cbe963321608da9e9d4ca.md)     
            * **file** [**fft\_core.h**](fft__core_8h.md) _Per-instance 1-D FFT using pocketfft directly._     
        * **dir** [**fft2d**](dir_9009a3f6624dc57956402cd0407c056b.md)     
            * **file** [**fft2d\_core.h**](fft2d__core_8h.md) _Per-instance 2-D FFT using pocketfft directly._     
        * **dir** [**filter**](dir_8178efb5c7670e7552eaa4222282ba05.md)     
            * **file** [**filter\_core.h**](filter__core_8h.md) _Filter module — public C API._     
        * **dir** [**fir**](dir_37fd0118bf34c485dd22fe4d261d6eac.md)     
            * **file** [**fir\_core.h**](fir__core_8h.md) _Direct-form FIR filter — real-tap and complex-tap variants._     
        * **dir** [**frame**](dir_00858a83d5a24a6fcf61a222bafb8b7f.md)     
            * **file** [**frame\_core.h**](frame__core_8h.md) _A frame's bit layout, held as an object so Python can describe one._     
        * **dir** [**frame\_meter**](dir_7d049e2511dda4d27f50479ac6f6567b.md)     
            * **file** [**frame\_meter\_core.h**](frame__meter__core_8h.md) _Frame outcomes accumulated across a record: FER, and sync detection._     
        * **dir** [**gold**](dir_eaad5c90f79e5666c89030cb43ebb96d.md)     
            * **file** [**gold\_core.h**](gold__core_8h.md) _Gold code component API._     
        * **dir** [**hbdecim**](dir_3828151286b0ff520a0d701b39db5af1.md)     
            * **file** [**hbdecim\_core.h**](hbdecim__core_8h.md) _Halfband 2:1 decimator for CF32 IQ samples._     
            * **file** [**hbdecim\_r2c\_core.h**](hbdecim__r2c__core_8h.md) _Real-to-complex halfband 2:1 decimator (Architecture D2)._     
        * **dir** [**hbdecim\_q15**](dir_93499f550a23db63d09661ee916a0767.md)     
            * **file** [**hbdecim\_q15\_core.h**](hbdecim__q15__core_8h.md) _Fixed-point halfband 2:1 decimator for interleaved IQ int16 samples._     
        * **dir** [**i16\_to\_f32**](dir_5ec56354373793af7b5bc8e9296f5472.md)     
            * **file** [**i16\_to\_f32\_core.h**](i16__to__f32__core_8h.md) _int16-to-float converter with configurable inverse scale._     
        * **dir** [**i16u32\_to\_f32**](dir_a216b988e44f4b34f41ebc1122731aa5.md)     
            * **file** [**i16u32\_to\_f32\_core.h**](i16u32__to__f32__core_8h.md) _Q15-in-uint32 to float converter._     
        * **dir** [**i16u64\_to\_f32**](dir_8835689c72c9893bedb52cd5868912e0.md)     
            * **file** [**i16u64\_to\_f32\_core.h**](i16u64__to__f32__core_8h.md) _Q15-in-uint64 to float converter._     
        * **dir** [**i32\_to\_f32**](dir_3ce16833ebcc9c0a9fe9c8f4deb663cc.md)     
            * **file** [**i32\_to\_f32\_core.h**](i32__to__f32__core_8h.md) _int32-to-float converter with configurable inverse scale._     
        * **dir** [**i8\_to\_f32**](dir_fd8e995fbd9a7d674714f99e992f90b2.md)     
            * **file** [**i8\_to\_f32\_core.h**](i8__to__f32__core_8h.md) _int8-to-float converter with configurable inverse scale._     
        * **dir** [**imdmeas**](dir_2f7e0f9e46c443ab8712f0318288e016.md)     
            * **file** [**imdmeas\_core.h**](imdmeas__core_8h.md) _IMDMeasure — two-tone intermodulation (IMD2/IMD3) and intercept._     
        * **dir** [**impairment**](dir_e387feada4efddcbd9f1bef4a6cef9f7.md)     
            * **file** [**impairment\_core.h**](impairment__core_8h.md) _Impairment module — public C API._ 
        * **dir** [**interp**](dir_af5ecbc8fae4d9dc167c6ab2381d74df.md)     
            * **file** [**interp\_core.h**](interp__core_8h.md) _Interp module — public C API._ 
        * **dir** [**interp\_table**](dir_532d1478dbb04668a5390572613675ee.md)     
            * **file** [**interp\_table\_core.h**](interp__table__core_8h.md) _Periodically-extended interpolated lookup table._     
        * **dir** [**interrupt**](dir_c129c9820da25930d499ea809db42198.md)     
            * **file** [**interrupt\_core.h**](interrupt__core_8h.md) _Interrupt module — public C API._ 
        * **dir** [**lo**](dir_e3bbeba8c021d4d74db794db08bafd77.md)     
            * **file** [**lo\_core.h**](lo__core_8h.md) _Local oscillator: NCO + 2^16 sin/cos LUT → CF32 phasors._     
        * **dir** [**lockdet**](dir_87531a87e500e672b7d093c5682794b4.md)     
            * **file** [**lockdet\_core.h**](lockdet__core_8h.md) _Portable lock detector — level + time hysteresis over any scalar lock metric, embeddable in every loop that makes a lock decision._     
        * **dir** [**loop\_filter**](dir_6fa6397534e50a536c96f665c3cf0441.md)     
            * **file** [**loop\_filter\_core.h**](loop__filter__core_8h.md) _Second-order proportional-integral loop filter — the shared engine of every tracking loop (Costas/PLL, DLL, symbol timing)._     
        * **dir** [**measure**](dir_4f61a452d1df39cf8c2e8be27f29f1f2.md)     
            * **file** [**measure\_core.h**](measure__core_8h.md) _Measure module — shared result structs and module-level helpers._     
        * **dir** [**mpsk**](dir_ca9d413705226c109a44c5982d79aa0f.md)     
            * **file** [**mpsk\_core.h**](mpsk__core_8h.md) _M-PSK constellation: Gray-coded map / demap for BPSK, QPSK, 8PSK._     
        * **dir** [**mpsk\_receiver**](dir_a1dc26622ebd32726f4fc723db7ccb3b.md)     
            * **file** [**mpsk\_receiver\_core.h**](mpsk__receiver__core_8h.md) _Pulse-shaped M-PSK receiver: a tuned matched front end and two loops._     
            * **file** [**mpsk\_rx\_loops.h**](mpsk__rx__loops_8h.md) _The two loops an M-PSK receiver closes, independent of its front end._     
        * **dir** [**nco**](dir_2f9ed967bc16fefd26d0244d883adb58.md)     
            * **file** [**nco\_core.h**](nco__core_8h.md) _Phase-accumulator NCO, and the one float-&gt;integer boundary everything that steers one has to pass through._     
        * **dir** [**nprmeas**](dir_2ffe7a00bca5d7665b823d0b8c1040c3.md)     
            * **file** [**nprmeas\_core.h**](nprmeas__core_8h.md) _NPRMeasure — notched-noise Noise Power Ratio._     
        * **dir** [**pn**](dir_70aeca018f85f00e17d8853ee6bd0cbb.md)     
            * **file** [**pn\_core.h**](pn__core_8h.md) _PN component API._     
        * **dir** [**ppe**](dir_d640b2c624b0e530b2e913b3aa05ce26.md)     
            * **file** [**ppe\_core.h**](ppe__core_8h.md) _Feedforward polynomial-phase estimator (frequency + chirp rate)._     
        * **dir** [**psd**](dir_1f3d46873d925f2e533983763479900d.md)     
            * **file** [**psd\_core.h**](psd__core_8h.md) _PSD — averaging power-spectral-density estimator (Welch's method) and spectral measurement suite._     
        * **dir** [**ratesync**](dir_bd24358a1650cccc3777ef85b64503d5.md)     
            * **file** [**ratesync\_core.h**](ratesync__core_8h.md) _RateSync — symbol-timing recovery on a matched-filter rate cascade._     
        * **dir** [**resamp**](dir_289a9297ce406b952fab973539197d1c.md)     
            * **file** [**resamp\_core.h**](resamp__core_8h.md) _Continuously-variable polyphase resampler for CF32 IQ._     
            * **file** [**resamp\_impl.h**](resamp__impl_8h.md) _Resamp implementation header._ 
        * **dir** [**resample**](dir_430486ea22038fad478027f2dc6550c6.md)     
            * **file** [**resample\_core.h**](resample__core_8h.md) _Resample module — public C API._     
        * **dir** [**rs**](dir_a447329db54f84e06767f7e282ab2567.md)     
            * **file** [**rs\_core.h**](rs__core_8h.md) _Reed-Solomon codes: the code description, the encoder, the syndromes and the decoder that corrects — all reading the same description._     
        * **dir** [**rs\_codec**](dir_3e7cbca72be4a95038c1797bf5803786.md)     
            * **file** [**rs\_codec\_core.h**](rs__codec__core_8h.md) _The Reed-Solomon codec, as an object over_ `rs` _._    
        * **dir** [**snr**](dir_a0dc77cb6789ae5cf19b2d0651b00ce2.md)     
            * **file** [**snr\_core.h**](snr__core_8h.md) _Stateless SNR / Es-N0 estimators, data-aided and non-data-aided._     
        * **dir** [**source**](dir_ce1f95460e483b5f4e7af3e87d9b090c.md)     
            * **file** [**source\_core.h**](source__core_8h.md) _Source module — public C API._ 
        * **dir** [**specan**](dir_6d702d949620e4073485867cfd9038e4.md)     
            * **file** [**specan\_core.h**](specan__core_8h.md) _Specan — natural-parameter spectrum analyzer (DDC + averaging PSD)._     
        * **dir** [**spectral**](dir_2aadf81c4f49e887d76ad198d657298d.md)     
            * **file** [**spectral\_core.h**](spectral__core_8h.md) _Spectral module — public C API._     
        * **dir** [**stream**](dir_21b896cdbc030a0ded493211142b7733.md)     
            * **file** [**stream.h**](stream_8h.md) _Streaming API for doppler — PUB/SUB, PUSH/PULL, REQ/REP._     
            * **file** [**tlm\_sink.h**](tlm__sink_8h.md) _NATS PUB sink for telemetry records._     
        * **dir** [**symsync**](dir_bee143323fe2e99a30a6d3a881f82f29.md)     
            * **file** [**symsync\_core.h**](symsync__core_8h.md) _SymbolSync component API._     
        * **dir** [**syncword**](dir_8170b734982c9e3c4a0c2955e2cfa64d.md)     
            * **file** [**syncword\_core.h**](syncword__core_8h.md) _Frame synchronisation: find a known marker in a bit stream, and choose the threshold that decides what counts as finding it._     
        * **dir** [**telemetry**](dir_d4543964ddc0423cd91d16ab74a4089e.md)     
            * **file** [**telemetry\_core.h**](telemetry__core_8h.md) _Telemetry module — public C API._ 
        * **dir** [**timing**](dir_0a8cc616bc028a416e339204953e39da.md)     
            * **file** [**timing\_core.h**](timing__core_8h.md)     
        * **dir** [**tonemeas**](dir_78c9bf326243d2be956f1c1b5de2ee56.md)     
            * **file** [**tonemeas\_core.h**](tonemeas__core_8h.md) _ToneMeasure — single-tone ADC/converter spectral measurement._     
        * **dir** [**track**](dir_c2a9225c85c9c9a3475fb96606254dcb.md)     
            * **file** [**track\_core.h**](track__core_8h.md) _Track module — public C API._ 
        * **dir** [**uq15\_to\_f32**](dir_b44b8aae78dd39801a4344596faf709f.md)     
            * **file** [**uq15\_to\_f32\_core.h**](uq15__to__f32__core_8h.md) _UQ15 (offset-binary uint16) to float converter._     
        * **dir** [**util**](dir_301ecbdb0604927cf0b3895ddfaba60f.md)     
            * **file** [**util\_core.h**](util__core_8h.md) _Util module — public C API._     
        * **dir** [**viterbi**](dir_abfb52fd33d2d22e092a3b80738d1015.md)     
            * **file** [**viterbi\_core.h**](viterbi__core_8h.md) _Soft-decision Viterbi decoding of convolutional codes._     
        * **dir** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md)     
            * **file** [**wfm\_compose.h**](wfm__compose_8h.md) _Multi-segment waveform composer (Phase B)._     
            * **file** [**wfm\_core.h**](wfm__core_8h.md) _Wfmgen module — public C API._     
            * **file** [**wfm\_dsp.h**](wfm__dsp_8h.md) _DSSS spreading + root-raised-cosine pulse shaping (Phase B)._     
            * **file** [**wfm\_frame.h**](wfm__frame_8h.md) _A frame's BIT layout, described once and read from both ends._     
            * **file** [**wfm\_keywords.h**](wfm__keywords_8h.md) _BLUE extended-header keywords — the X-Midas binary tag/value codec._     
            * **file** [**wfm\_names.h**](wfm__names_8h.md)     
            * **file** [**wfm\_path.h**](wfm__path_8h.md) _Sibling-path construction shared by the wfm reader and writer._     
            * **file** [**wfm\_plan.h**](wfm__plan_8h.md)     
            * **file** [**wfm\_sink.h**](wfm__sink_8h.md) _NATS PUB sink for generated IQ (Phase B)._     
            * **file** [**wfm\_time.h**](wfm__time_8h.md)     
            * **file** [**wfmgen.h**](wfmgen_8h.md)     
        * **dir** [**wfm\_compose**](dir_cd921e547fe04d2978fad26e616ea160.md)     
            * **file** [**wfm\_compose\_bridge.h**](wfm__compose__bridge_8h.md)     
        * **dir** [**wfm\_reader**](dir_01018a3d11538c9aca2db4daa45a442f.md)     
            * **file** [**wfm\_reader\_core.h**](wfm__reader__core_8h.md) _Input file types for generated IQ — the dual of wfm\_writer._     
        * **dir** [**wfm\_synth**](dir_0493917d169dff974fa9eaf690c8d4c9.md)     
            * **file** [**wfm\_synth\_core.h**](wfm__synth__core_8h.md) _Synth component API._     
        * **dir** [**wfm\_writer**](dir_a59bfdc441aa05aed9607457147ad53f.md)     
            * **file** [**wfm\_writer\_core.h**](wfm__writer__core_8h.md) _Output file types for generated IQ: raw / csv / BLUE-1000 + SigMF meta._     
        * **file** [**q15\_mac.h**](q15__mac_8h.md) _Static inline Q15 dot-product primitives: scalar fallback and AVX2._     

