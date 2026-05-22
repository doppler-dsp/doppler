
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
        * **dir** [**Resampler**](dir_6dca75203c5d2d5de468e6acc97392e7.md)     
            * **file** [**Resampler\_core.h**](Resampler__core_8h.md) _Continuously-variable polyphase resampler, CF32 IQ._     
        * **dir** [**acc\_cf64**](dir_a31d3897e2036bab462df07bf5a3b557.md)     
            * **file** [**acc\_cf64\_core.h**](acc__cf64__core_8h.md) _AccCf64 component API._     
        * **dir** [**acc\_f32**](dir_0465294bf3f41af7dbdebf91d81a0c4a.md)     
            * **file** [**acc\_f32\_core.h**](acc__f32__core_8h.md) _AccF32 component API._     
        * **dir** [**accumulator**](dir_06136a2119985c3c219633f937232576.md)     
            * **file** [**accumulator\_core.h**](accumulator__core_8h.md) _Accumulator module — public C API._ 
        * **dir** [**agc**](dir_947ec4d62e9dda8dbffe026d57cfb18d.md)     
            * **file** [**agc\_core.h**](agc__core_8h.md) _Log-domain automatic gain control (AGC)._     
        * **dir** [**buffer**](dir_3a0c1aef7dcd64a21724ce24de18fb81.md)     
            * **file** [**buffer.h**](buffer_8h.md) _High-performance x86-64 Circular Buffer for RF Streaming._     
        * **dir** [**corr**](dir_17ecfb211582dadfc5fc9d22d4d97fbd.md)     
            * **file** [**corr\_core.h**](corr__core_8h.md) _1-D FFT-based cross-correlator with coherent integrate-and-dump._     
        * **dir** [**corr2d**](dir_55247951d314f4b4a6db9bf46862b830.md)     
            * **file** [**corr2d\_core.h**](corr2d__core_8h.md) _2-D FFT-based cross-correlator with coherent integrate-and-dump._     
        * **dir** [**ddc**](dir_b33dc116452ac5c7d7799725e78b6bdc.md)     
            * **file** [**ddc\_core.h**](ddc__core_8h.md) _Digital Down-Converter — composes LO + polyphase resampler._     
        * **dir** [**delay**](dir_01f4b6965a2181d172634d6670b32dc1.md)     
            * **file** [**delay\_core.h**](delay__core_8h.md) _Delay component API._     
        * **dir** [**detection**](dir_3a1e0e8c534208cc3745b2f53a028862.md)     
            * **file** [**detection\_core.h**](detection__core_8h.md) _Detection-theory utilities for the amplitude-ratio test statistic._     
        * **dir** [**detector**](dir_f93f7a52d403868792375ffc90a1c1d7.md)     
            * **file** [**det\_private.h**](det__private_8h.md) _Shared internals for detector\_core.c and detector2d\_core.c._     
            * **file** [**detector\_core.h**](detector__core_8h.md) _1-D streaming signal detector with FFT-based correlation, integrate-and-dump, and configurable noise-referenced threshold._     
        * **dir** [**detector2d**](dir_bd7354e9665bd912180ec22b3c69b55c.md)     
            * **file** [**detector2d\_core.h**](detector2d__core_8h.md) _2-D streaming signal detector with FFT2D-based correlation, integrate-and-dump, and configurable noise-referenced threshold._     
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
        * **dir** [**lo**](dir_e3bbeba8c021d4d74db794db08bafd77.md)     
            * **file** [**lo\_core.h**](lo__core_8h.md) _Local oscillator: NCO + 2^16 sin/cos LUT → CF32 phasors._     
        * **dir** [**nco**](dir_2f9ed967bc16fefd26d0244d883adb58.md)     
            * **file** [**nco\_core.h**](nco__core_8h.md) _Pure 32-bit phase-accumulator NCO._     
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
        * **dir** [**util**](dir_301ecbdb0604927cf0b3895ddfaba60f.md)     
            * **file** [**util\_core.h**](util__core_8h.md) _Util module — public C API._     

