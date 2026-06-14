
# File List

Here is a list of all files with brief descriptions:


* **dir** [**native**](dir_3dbb10954ed03e2c7eb007b10aa2d80b.md)     
    * **dir** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md)     
        * **file** [**clib\_common.h**](clib__common_8h.md)     
        * **file** [**doppler.h**](doppler_8h.md) 
        * **file** [**jm\_perf.h**](jm__perf_8h.md) _just-makeit performance annotation macros._     
        * **file** [**jm\_simd.h**](jm__simd_8h.md) _Width-portable SIMD operation macros._     
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
        * **dir** [**adc**](dir_a6be6b8cb61d5f2be55c0b2f94afbd88.md)     
            * **file** [**adc\_core.h**](adc__core_8h.md) _Signed two's-complement ADC model._     
        * **dir** [**agc**](dir_947ec4d62e9dda8dbffe026d57cfb18d.md)     
            * **file** [**agc\_core.h**](agc__core_8h.md) _Log-domain automatic gain control (AGC)._     
        * **dir** [**arith**](dir_51d42af7a43550d997314136379d62d2.md)     
            * **file** [**arith\_core.h**](arith__core_8h.md) _Arith module — public C API for fixed-point arithmetic on Q15 (int16\_t) and Q8 (int8\_t) arrays. All elementwise operations write into a caller-supplied output buffer of the same length as the shorter input. Saturation clamps results to the representable range rather than wrapping, matching the two's-complement DSP convention._     
        * **dir** [**awgn**](dir_b535f71dd6c18f769df9e4bf89a97331.md)     
            * **file** [**awgn\_core.h**](awgn__core_8h.md) _Additive White Gaussian Noise generator._     
        * **dir** [**buffer**](dir_3a0c1aef7dcd64a21724ce24de18fb81.md)     
            * **file** [**buffer.h**](buffer_8h.md) _High-performance x86-64 Circular Buffer for RF Streaming._     
        * **dir** [**cic**](dir_cf560077cc62991e7289ea57a3d930a1.md)     
            * **file** [**cic\_core.h**](cic__core_8h.md) _CIC decimation filter — 4-stage, M=1, UQ16 integer pipeline._     
        * **dir** [**corr**](dir_17ecfb211582dadfc5fc9d22d4d97fbd.md)     
            * **file** [**corr\_core.h**](corr__core_8h.md) _1-D FFT-based cross-correlator with coherent integrate-and-dump._     
        * **dir** [**corr2d**](dir_55247951d314f4b4a6db9bf46862b830.md)     
            * **file** [**corr2d\_core.h**](corr2d__core_8h.md) _2-D FFT-based cross-correlator with coherent integrate-and-dump._     
        * **dir** [**cvt**](dir_7aebb15fbd538257eeb7884581a8ab59.md)     
            * **file** [**cvt\_core.h**](cvt__core_8h.md) _Cvt module — public C API._ 
        * **dir** [**ddc**](dir_b33dc116452ac5c7d7799725e78b6bdc.md)     
            * **file** [**ddc\_core.h**](ddc__core_8h.md) _Digital Down-Converter — composes LO + RateConverter cascade._     
        * **dir** [**ddcr**](dir_46c04c942eb84c8716610cebe515b046.md)     
            * **file** [**ddcr\_core.h**](ddcr__core_8h.md) _DDCR (real-input DDC) — re-exports from_ [_**ddc\_core.h**_](ddc__core_8h.md) _._    
        * **dir** [**delay**](dir_01f4b6965a2181d172634d6670b32dc1.md)     
            * **file** [**delay\_core.h**](delay__core_8h.md) _Delay component API._     
        * **dir** [**detection**](dir_3a1e0e8c534208cc3745b2f53a028862.md)     
            * **file** [**detection\_core.h**](detection__core_8h.md) _Detection-theory utilities for the amplitude-ratio test statistic._     
        * **dir** [**detector**](dir_f93f7a52d403868792375ffc90a1c1d7.md)     
            * **file** [**det\_private.h**](det__private_8h.md) _Shared internals for detector\_core.c and detector2d\_core.c._     
            * **file** [**detector\_core.h**](detector__core_8h.md) _1-D streaming signal detector with FFT-based correlation, integrate-and-dump, and configurable noise-referenced threshold._     
        * **dir** [**detector2d**](dir_bd7354e9665bd912180ec22b3c69b55c.md)     
            * **file** [**detector2d\_core.h**](detector2d__core_8h.md) _2-D streaming signal detector with FFT2D-based correlation, integrate-and-dump, and configurable noise-referenced threshold._     
        * **dir** [**f32\_to\_i16**](dir_e25c96329f88166d8f87eefdc2ba64fa.md)     
            * **file** [**f32\_to\_i16\_core.h**](f32__to__i16__core_8h.md) _Scale-and-saturate float-to-int16 converter._     
        * **dir** [**f32\_to\_i16u32**](dir_5361bfc3c658147f85e2e18e4bfef9b4.md)     
            * **file** [**f32\_to\_i16u32\_core.h**](f32__to__i16u32__core_8h.md) _Scale-and-saturate float to Q15-in-uint32 converter._     
        * **dir** [**f32\_to\_i16u64**](dir_212e21299d76aa740bbad8810e4bf50a.md)     
            * **file** [**f32\_to\_i16u64\_core.h**](f32__to__i16u64__core_8h.md) _Scale-and-saturate float to Q15-in-uint64 converter._     
        * **dir** [**f32\_to\_uq15**](dir_4e8c99e54919bb49218552fb8f2fb678.md)     
            * **file** [**f32\_to\_uq15\_core.h**](f32__to__uq15__core_8h.md) _Scale-and-saturate float-to-UQ15 (offset-binary uint16) converter._     
        * **dir** [**fft**](dir_5dc24668fb1cbe963321608da9e9d4ca.md)     
            * **file** [**fft\_core.h**](fft__core_8h.md) _Per-instance 1-D FFT using pocketfft directly._     
        * **dir** [**fft2d**](dir_9009a3f6624dc57956402cd0407c056b.md)     
            * **file** [**fft2d\_core.h**](fft2d__core_8h.md) _Per-instance 2-D FFT using pocketfft directly._     
        * **dir** [**filter**](dir_8178efb5c7670e7552eaa4222282ba05.md)     
            * **file** [**filter\_core.h**](filter__core_8h.md) _Filter module — public C API._ 
        * **dir** [**fir**](dir_37fd0118bf34c485dd22fe4d261d6eac.md)     
            * **file** [**fir\_core.h**](fir__core_8h.md) _Direct-form FIR filter — real-tap and complex-tap variants._     
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
        * **dir** [**lo**](dir_e3bbeba8c021d4d74db794db08bafd77.md)     
            * **file** [**lo\_core.h**](lo__core_8h.md) _Local oscillator: NCO + 2^16 sin/cos LUT → CF32 phasors._     
        * **dir** [**measure**](dir_4f61a452d1df39cf8c2e8be27f29f1f2.md)     
            * **file** [**measure\_core.h**](measure__core_8h.md) _Measure module — shared result structs and module-level helpers._     
        * **dir** [**nco**](dir_2f9ed967bc16fefd26d0244d883adb58.md)     
            * **file** [**nco\_core.h**](nco__core_8h.md) _Pure 32-bit phase-accumulator NCO._     
        * **dir** [**pffft**](dir_2e0e79537247ed1eb65cd0be05071701.md)     
            * **file** [**pffft.h**](pffft_8h.md)     
        * **dir** [**pn**](dir_70aeca018f85f00e17d8853ee6bd0cbb.md)     
            * **file** [**pn\_core.h**](pn__core_8h.md) _PN component API._     
        * **dir** [**resamp**](dir_289a9297ce406b952fab973539197d1c.md)     
            * **file** [**resamp\_core.h**](resamp__core_8h.md) _Continuously-variable polyphase resampler for CF32 IQ._     
            * **file** [**resamp\_impl.h**](resamp__impl_8h.md) _Resamp implementation header._ 
        * **dir** [**resample**](dir_430486ea22038fad478027f2dc6550c6.md)     
            * **file** [**resample\_core.h**](resample__core_8h.md) _Resample module — public C API._     
        * **dir** [**source**](dir_ce1f95460e483b5f4e7af3e87d9b090c.md)     
            * **file** [**source\_core.h**](source__core_8h.md) _Source module — public C API._ 
        * **dir** [**spectral**](dir_2aadf81c4f49e887d76ad198d657298d.md)     
            * **file** [**spectral\_core.h**](spectral__core_8h.md) _Spectral module — public C API._     
        * **dir** [**stream**](dir_21b896cdbc030a0ded493211142b7733.md)     
            * **file** [**stream.h**](stream_8h.md) _Streaming API for doppler — PUB/SUB, PUSH/PULL, REQ/REP._     
            * **file** [**stream\_core.h**](stream__core_8h.md) _Stream module — public C API._ 
        * **dir** [**timing**](dir_0a8cc616bc028a416e339204953e39da.md)     
            * **file** [**timing\_core.h**](timing__core_8h.md)     
        * **dir** [**tonemeas**](dir_78c9bf326243d2be956f1c1b5de2ee56.md)     
            * **file** [**tonemeas\_core.h**](tonemeas__core_8h.md) _ToneMeasure — single-tone ADC/converter spectral measurement._     
        * **dir** [**uq15\_to\_f32**](dir_b44b8aae78dd39801a4344596faf709f.md)     
            * **file** [**uq15\_to\_f32\_core.h**](uq15__to__f32__core_8h.md) _UQ15 (offset-binary uint16) to float converter._     
        * **dir** [**util**](dir_301ecbdb0604927cf0b3895ddfaba60f.md)     
            * **file** [**util\_core.h**](util__core_8h.md) _Util module — public C API._     
        * **dir** [**welch**](dir_aeb9e26b0edb1fd5fc61c8cd35fcdcfb.md)     
            * **file** [**welch\_core.h**](welch__core_8h.md) _Welch — averaging PSD estimator and spectral measurement suite._     
        * **dir** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md)     
            * **file** [**wfm\_compose.h**](wfm__compose_8h.md) _Multi-segment waveform composer (Phase B)._     
            * **file** [**wfm\_core.h**](wfm__core_8h.md) _Wfmgen module — public C API._     
            * **file** [**wfm\_dsp.h**](wfm__dsp_8h.md) _DSSS spreading + root-raised-cosine pulse shaping (Phase B)._     
            * **file** [**wfm\_reader.h**](wfm__reader_8h.md) _Input containers for generated IQ — the dual of wfm\_writer._     
            * **file** [**wfm\_sink.h**](wfm__sink_8h.md) _ZMQ PUB sink for generated IQ (Phase B)._     
            * **file** [**wfm\_writer.h**](wfm__writer_8h.md) _Output containers for generated IQ: raw / csv / BLUE-1000 + SigMF meta._     
            * **file** [**wfmgen.h**](wfmgen_8h.md)     
        * **dir** [**wfm\_synth**](dir_0493917d169dff974fa9eaf690c8d4c9.md)     
            * **file** [**wfm\_synth\_core.h**](wfm__synth__core_8h.md) _Synth component API._     
        * **file** [**q15\_mac.h**](q15__mac_8h.md) _Static inline Q15 dot-product primitives: scalar fallback and AVX2._     

