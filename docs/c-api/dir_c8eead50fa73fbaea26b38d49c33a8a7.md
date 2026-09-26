

# Dir native/inc/doppler



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md)












## Files

| Type | Name |
| ---: | :--- |
| file | [**clib\_common.h**](clib__common_8h.md) <br> |
| file | [**doppler.h**](doppler_8h.md) <br> |
| file | [**dp\_complex.h**](dp__complex_8h.md) <br>_The complex-math surface, routed so it survives Windows._  |
| file | [**dp\_crc16.h**](dp__crc16_8h.md) <br>_CRC-16-CCITT over a bit stream — the one CRC shared by every doppler frame producer and consumer._  |
| file | [**dp\_format.h**](dp__format_8h.md) <br>_Complex sample formats, named by their BLUE/Platinum codes._  |
| file | [**dp\_interleave.h**](dp__interleave_8h.md) <br>_Block interleaving — the permutation, and nothing else._  |
| file | [**dp\_interrupt.h**](dp__interrupt_8h.md) <br>_Asking a blocking wait to stop, whatever it is waiting on._  |
| file | [**dp\_interrupt\_pyadopt.h**](dp__interrupt__pyadopt_8h.md) <br> |
| file | [**dp\_isotime.h**](dp__isotime_8h.md) <br>_ISO 8601 UTC timestamps in both spellings — filename-safe_ **basic** _for names doppler writes,_**extended** _for the wire formats that mandate it._ |
| file | [**dp\_parallel.h**](dp__parallel_8h.md) <br> |
| file | [**dp\_simd.h**](dp__simd_8h.md) <br>_doppler's own composite SIMD reductions, layered over_ `jm_simd.h` _._ |
| file | [**dp\_state.h**](dp__state_8h.md) <br> |
| file | [**dp\_state\_pyhelp.h**](dp__state__pyhelp_8h.md) <br> |
| file | [**dp\_syncword.h**](dp__syncword_8h.md) <br>_Finding a known bit pattern in an unpacked bit stream — the sync word search, and the arithmetic for choosing its threshold._  |
| file | [**dp\_thread.h**](dp__thread_8h.md) <br>_The threading primitives doppler uses, with one platform split._  |
| file | [**jm\_perf.h**](jm__perf_8h.md) <br> |
| file | [**jm\_simd.h**](jm__simd_8h.md) <br> |
| file | [**q15\_mac.h**](q15__mac_8h.md) <br>_Static inline Q15 dot-product primitives: scalar fallback and AVX2._  |


## Directories

| Type | Name |
| ---: | :--- |
| dir | [**HalfbandDecimator**](dir_7d0e752fb42c448bafa7d80e7fc4aaf0.md) <br> |
| dir | [**RateConverter**](dir_f243cfbf2f82d96e6953dd99cc2498dd.md) <br> |
| dir | [**Resampler**](dir_9b0990bb8296ade48d8f038050fb64f1.md) <br> |
| dir | [**acc\_cf64**](dir_d950eea1844c6f23a7b4df3cf640e9a2.md) <br> |
| dir | [**acc\_f32**](dir_c19b9e056cbdf00f39bf52805b44beb0.md) <br> |
| dir | [**acc\_q15**](dir_2344cd9a4aadb833503124e5257faf5b.md) <br> |
| dir | [**acc\_q8**](dir_ea0f50795da7b2248dba00cbaac7e694.md) <br> |
| dir | [**acc\_trace**](dir_ea4259ba3dd1c044f0efb519286a18a5.md) <br> |
| dir | [**accumulator**](dir_36cdc8980ffec967038c3fbc81b64143.md) <br> |
| dir | [**acq**](dir_34926d0c3adbcf3f65d9e19727d3354d.md) <br> |
| dir | [**acquire**](dir_684079cd19bcefcd0ee51c221517a041.md) <br> |
| dir | [**adc**](dir_eddcd1edcea89729b407f545f79f2d08.md) <br> |
| dir | [**agc**](dir_da2fce83534b434d126c978bac57abe5.md) <br> |
| dir | [**analyzer**](dir_bc9ffe1c503aeccbb822ea34694fc346.md) <br> |
| dir | [**arith**](dir_d0f844c85d44525a1700464c9da275c4.md) <br> |
| dir | [**async\_dsss\_pool**](dir_8a6668f3097fb7847a23b1f65b3df26f.md) <br> |
| dir | [**async\_dsss\_receiver**](dir_565c73e0c8995e904663dd9fb6485ceb.md) <br> |
| dir | [**awgn**](dir_6240b6c8e1c7fd073a984e370d89f937.md) <br> |
| dir | [**ber**](dir_742028dd4040117c60c3f886fa044d64.md) <br> |
| dir | [**ber\_meter**](dir_88fe8aa1742881a2471cfb33f972e762.md) <br> |
| dir | [**boxcar**](dir_5b2ea30dc12e54f23750507f860119fd.md) <br> |
| dir | [**buffer**](dir_bada8e9c2056a5c5c150b079933e5759.md) <br> |
| dir | [**burst\_acq**](dir_55efb80a743a0f920c38ee6730a791eb.md) <br> |
| dir | [**burst\_capture**](dir_5fb975a28c31ccaec359941e78dbfe20.md) <br> |
| dir | [**burst\_demod**](dir_ab0fdf036101e4998629894503e143b8.md) <br> |
| dir | [**burst\_despreader**](dir_28ffcd911995597d422ebd972d69802b.md) <br> |
| dir | [**carrier\_acq**](dir_92c8a71a0b308d7c304bd53eb19bc933.md) <br> |
| dir | [**carrier\_mpsk**](dir_bb3a0f9e61a286c66b69840ec2385900.md) <br> |
| dir | [**carrier\_nda**](dir_6eb92e8a380cf8a945db661ecf671c65.md) <br> |
| dir | [**ccsds**](dir_c033bcb1c91e23f54b625bfe2cf0448c.md) <br> |
| dir | [**ccsds\_tm**](dir_755172a25247ef56b5f4144aad408377.md) <br> |
| dir | [**cic**](dir_b18ea702eaae2e8004fd6583c8b1e641.md) <br> |
| dir | [**coding**](dir_69d8a89cba5242a3dfe9624bc2e7c6f6.md) <br> |
| dir | [**conv**](dir_dadcb1d47e07452fac6fef665f736671.md) <br> |
| dir | [**conv\_enc**](dir_c22965c7b72380eff65e84661867f314.md) <br> |
| dir | [**corr**](dir_28e39a8c94dc713f6e89cb1d02b66afa.md) <br> |
| dir | [**corr2d**](dir_ac96ca94cfbb355eb7a82b081cfe387c.md) <br> |
| dir | [**costas**](dir_8ebd78c7800b34d5dee6ef27ff63e7b3.md) <br> |
| dir | [**cvt**](dir_409c6e92c9ef1b7281540388592da57d.md) <br> |
| dir | [**ddc**](dir_4a67ebc391a3fd8e8259ec0993c7169b.md) <br> |
| dir | [**ddcr**](dir_18bb1adfae8df578c1da0090bbf10ccf.md) <br> |
| dir | [**delay**](dir_e9520af345bba2408e131802acc7e37b.md) <br> |
| dir | [**despreader**](dir_0568e7ebbbbb935946ff07943e2ec07c.md) <br> |
| dir | [**detection**](dir_c7528e0bd68524c48f260a564c045102.md) <br> |
| dir | [**detector**](dir_4cdf6fdfdd426ef1a31e056182554d6b.md) <br> |
| dir | [**detector2d**](dir_8496bb9d19545edf0ab852f69ada11c8.md) <br> |
| dir | [**dll**](dir_c5ff741ba8e1e68126223ee4df379547.md) <br> |
| dir | [**doppler\_channel**](dir_8380ecb58e2e244790f54835382515ec.md) <br> |
| dir | [**dp\_event\_log**](dir_3dd394e4a68853f038b67a354736a79d.md) <br> |
| dir | [**dp\_interrupt\_guard**](dir_069c076bf5cb5bab676dd02f8ef44735.md) <br> |
| dir | [**dp\_tlm**](dir_6b0129a28aff69bb09c5c8857f722994.md) <br> |
| dir | [**dp\_tlm\_capture**](dir_d9291c2a213cf080735d669eb6078971.md) <br> |
| dir | [**dsss**](dir_c33ba84b8db3f8ead33f9dfeadf377c6.md) <br> |
| dir | [**dsss\_burst\_receiver**](dir_630068a67b306c85c8348e5ba842eaef.md) <br> |
| dir | [**dsss\_receiver**](dir_6f60e61f873068f58aa42ec05bfb1a20.md) <br> |
| dir | [**f32\_buffer**](dir_81ff08c9d14656f475312d0561398a88.md) <br> |
| dir | [**f32\_to\_i16**](dir_70d41be9b98525952faca7fac2a162b6.md) <br> |
| dir | [**f32\_to\_i16u32**](dir_8ed3b501fdbf556e99afaad1b0fb95c8.md) <br> |
| dir | [**f32\_to\_i16u64**](dir_9062d1e43f9eb10312d10225fea9c765.md) <br> |
| dir | [**f32\_to\_i32**](dir_b3babbbd6f70adf16ee9e6e7966b639c.md) <br> |
| dir | [**f32\_to\_i8**](dir_ed78fc1c59eed5a28e12ccfcd3b5d37e.md) <br> |
| dir | [**f32\_to\_uq15**](dir_8d86eabfdce5eb918e7f78782ceed683.md) <br> |
| dir | [**f64\_buffer**](dir_d7320aa8439c659edb0fbbfd27af0c55.md) <br> |
| dir | [**farrow**](dir_f5d5da611c5546f094b053d8a6116219.md) <br> |
| dir | [**fft**](dir_1dec96a47631eec21a10469aab3e9a96.md) <br> |
| dir | [**fft2d**](dir_384db21119e775e355fd287e1a7652d5.md) <br> |
| dir | [**filter**](dir_820039e5f1fe5ce84fe724a5385272f3.md) <br> |
| dir | [**fir**](dir_057b0102e9f7b23965a3d08d49a38aec.md) <br> |
| dir | [**frame**](dir_f1fb4d4532bf7057e52e2ed064f78108.md) <br> |
| dir | [**frame\_meter**](dir_ea37d4cf1a6c64bbd4d366fb73a4d95d.md) <br> |
| dir | [**gold**](dir_3809e94b548bea726f665dae5e933c35.md) <br> |
| dir | [**hbdecim**](dir_29792a392a590bbead7fbef545faea73.md) <br> |
| dir | [**hbdecim\_q15**](dir_0dbd0f8cb615d6b620ef5cfddeb71b4a.md) <br> |
| dir | [**i16\_buffer**](dir_5e38c689ce67e24fea3517ffd89658ee.md) <br> |
| dir | [**i16\_to\_f32**](dir_01f347c62069c5233a090c3b4d985bee.md) <br> |
| dir | [**i16u32\_to\_f32**](dir_c1ecd6bb977db755472db2284b0a6806.md) <br> |
| dir | [**i16u64\_to\_f32**](dir_4ae5ec00179a07705a9cd9969d88421c.md) <br> |
| dir | [**i32\_to\_f32**](dir_04852aba3b033ffe951e6f7f0437e86c.md) <br> |
| dir | [**i8\_to\_f32**](dir_fc9b7ccc74f65689d9e6b216ebcebff3.md) <br> |
| dir | [**imdmeas**](dir_2a8d4e9dde298cc63e616d81cd7ff06c.md) <br> |
| dir | [**impairment**](dir_110bd2cfc83959b687efb664e4fc8d2d.md) <br> |
| dir | [**interleaver**](dir_bb303c16bb49af033112c76b5cc87569.md) <br> |
| dir | [**interp**](dir_50f22f4a69ea3c5d52671b7845e67b28.md) <br> |
| dir | [**interp\_table**](dir_ff54084ce651244803741a1f6d284d09.md) <br> |
| dir | [**interrupt**](dir_bb0d81ed85f0a381609b116697c05606.md) <br> |
| dir | [**lo**](dir_939b5fbb8d3e3ebd3276389efab5bbba.md) <br> |
| dir | [**lockdet**](dir_0a3dcc380b80b2c01366f0cb5ed7ee07.md) <br> |
| dir | [**loop\_filter**](dir_5a5f36bef1d931095e74791b275c008f.md) <br> |
| dir | [**measure**](dir_0caaff9683efc5ec33140a5572570268.md) <br> |
| dir | [**mpsk**](dir_c69176d330fb67baeda366bef5232651.md) <br> |
| dir | [**mpsk\_receiver**](dir_7dd9db0064eee02f2344cc7ae60c611e.md) <br> |
| dir | [**nco**](dir_64d3c3497a73d925321234bd516bd8bf.md) <br> |
| dir | [**nprmeas**](dir_df189228e030028408da81bd0afea7e3.md) <br> |
| dir | [**pn**](dir_95c43a97468bba5f35ca9859d48b4113.md) <br> |
| dir | [**ppe**](dir_2835f10376dc04900139beed5b4d2457.md) <br> |
| dir | [**psd**](dir_80b4a8b440284bffd6bf0fbeb7bfe41f.md) <br> |
| dir | [**ratesync**](dir_6debd7d9ad90888bb5b8a40a122d95e8.md) <br> |
| dir | [**resamp**](dir_9d8524a806701f72e323943267d27190.md) <br> |
| dir | [**resample**](dir_4efb1181c8663bd7a6953f73a4eb8dc9.md) <br> |
| dir | [**rs**](dir_947c1c4c0fb4d4a4d0adf0ec99900cc5.md) <br> |
| dir | [**rs\_codec**](dir_73572debd583c0e71af980e0bf0d5ccd.md) <br> |
| dir | [**snr**](dir_02c47206b4cbe462773a89d72d8a4c36.md) <br> |
| dir | [**source**](dir_aa288ec3ae47721f4b7c9a32d3b8f472.md) <br> |
| dir | [**specan**](dir_6ce576ad24803d600633e2545d7ab991.md) <br> |
| dir | [**spectral**](dir_bb80cb4693a043f62f72e53e1f90a405.md) <br> |
| dir | [**stream**](dir_2fbcc177cba4f14addc502f26acbb8f7.md) <br> |
| dir | [**symsync**](dir_f3bf1d4e9482041dff7e7824fe611ddf.md) <br> |
| dir | [**syncword**](dir_e299403a0e03806f7aa46ec15fc4c91a.md) <br> |
| dir | [**telemetry**](dir_d30f8ea704930bef6ef378fb8c932c43.md) <br> |
| dir | [**timing**](dir_f312c7d1315f2596c2b71ef05d5d7b2b.md) <br> |
| dir | [**tonemeas**](dir_e0665d68cb2c2ff7feb28506d37a8bd1.md) <br> |
| dir | [**track**](dir_f98cb3fb09d38460dfcf46a77aa842c0.md) <br> |
| dir | [**u8\_to\_f32**](dir_b468cb3e760d860bad02e34079834f5d.md) <br> |
| dir | [**uq15\_to\_f32**](dir_289e6f8543a5d0b92e78da373782efe4.md) <br> |
| dir | [**util**](dir_7dd94ac9e5a2e34ed236c6361f93c476.md) <br> |
| dir | [**viterbi**](dir_63cd492a1a551091390943fc51915433.md) <br> |
| dir | [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) <br> |
| dir | [**wfm\_compose**](dir_6d794a7fa9423fe9f7b14be42b83e035.md) <br> |
| dir | [**wfm\_reader**](dir_f352698a51aeb04a6ff33c180c5d8d41.md) <br> |
| dir | [**wfm\_synth**](dir_3fd4fbfbc7cedac951bbaaf096533da9.md) <br> |
| dir | [**wfm\_writer**](dir_b07d3bab2c69a70a146b9de213e56c65.md) <br> |

























































------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/`

