

# File doppler.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**doppler.h**](doppler_8h.md)

[Go to the source code of this file](doppler_8h_source.md)



* `#include "doppler/wfm_reader/wfm_reader_core.h"`
* `#include "doppler/wfm_writer/wfm_writer_core.h"`
* `#include "doppler/f32_to_i8/f32_to_i8_core.h"`
* `#include "doppler/f32_to_i16/f32_to_i16_core.h"`
* `#include "doppler/f32_to_i32/f32_to_i32_core.h"`
* `#include "doppler/i8_to_f32/i8_to_f32_core.h"`
* `#include "doppler/u8_to_f32/u8_to_f32_core.h"`
* `#include "doppler/i16_to_f32/i16_to_f32_core.h"`
* `#include "doppler/i32_to_f32/i32_to_f32_core.h"`
* `#include "doppler/f32_to_i16u32/f32_to_i16u32_core.h"`
* `#include "doppler/f32_to_i16u64/f32_to_i16u64_core.h"`
* `#include "doppler/i16u32_to_f32/i16u32_to_f32_core.h"`
* `#include "doppler/i16u64_to_f32/i16u64_to_f32_core.h"`
* `#include "doppler/f32_to_uq15/f32_to_uq15_core.h"`
* `#include "doppler/uq15_to_f32/uq15_to_f32_core.h"`
* `#include "doppler/adc/adc_core.h"`
* `#include "doppler/acc_f32/acc_f32_core.h"`
* `#include "doppler/acc_cf64/acc_cf64_core.h"`
* `#include "doppler/acc_trace/acc_trace_core.h"`
* `#include "doppler/fir/fir_core.h"`
* `#include "doppler/boxcar/boxcar_core.h"`
* `#include "doppler/nco/nco_core.h"`
* `#include "doppler/lo/lo_core.h"`
* `#include "doppler/awgn/awgn_core.h"`
* `#include "doppler/pn/pn_core.h"`
* `#include "doppler/wfm_synth/wfm_synth_core.h"`
* `#include "doppler/gold/gold_core.h"`
* `#include "doppler/frame/frame_core.h"`
* `#include "doppler/delay/delay_core.h"`
* `#include "doppler/fft/fft_core.h"`
* `#include "doppler/fft2d/fft2d_core.h"`
* `#include "doppler/corr/corr_core.h"`
* `#include "doppler/corr2d/corr2d_core.h"`
* `#include "doppler/detector/detector_core.h"`
* `#include "doppler/detector2d/detector2d_core.h"`
* `#include "doppler/psd/psd_core.h"`
* `#include "doppler/tonemeas/tonemeas_core.h"`
* `#include "doppler/nprmeas/nprmeas_core.h"`
* `#include "doppler/imdmeas/imdmeas_core.h"`
* `#include "doppler/dp_interrupt_guard/dp_interrupt_guard_core.h"`
* `#include "doppler/f32_buffer/f32_buffer_core.h"`
* `#include "doppler/f64_buffer/f64_buffer_core.h"`
* `#include "doppler/i16_buffer/i16_buffer_core.h"`
* `#include "doppler/dp_tlm/dp_tlm_core.h"`
* `#include "doppler/dp_tlm_capture/dp_tlm_capture_core.h"`
* `#include "doppler/dp_event_log/dp_event_log_core.h"`
* `#include "doppler/ddc/ddc_core.h"`
* `#include "doppler/ddcr/ddcr_core.h"`
* `#include "doppler/specan/specan_core.h"`
* `#include "doppler/Resampler/Resampler_core.h"`
* `#include "doppler/HalfbandDecimator/HalfbandDecimator_core.h"`
* `#include "doppler/cic/cic_core.h"`
* `#include "doppler/RateConverter/RateConverter_core.h"`
* `#include "doppler/farrow/farrow_core.h"`
* `#include "doppler/hbdecim_q15/hbdecim_q15_core.h"`
* `#include "doppler/lockdet/lockdet_core.h"`
* `#include "doppler/syncword/syncword_core.h"`
* `#include "doppler/agc/agc_core.h"`
* `#include "doppler/doppler_channel/doppler_channel_core.h"`
* `#include "doppler/acc_q15/acc_q15_core.h"`
* `#include "doppler/acc_q8/acc_q8_core.h"`
* `#include "doppler/interp_table/interp_table_core.h"`
* `#include "doppler/loop_filter/loop_filter_core.h"`
* `#include "doppler/costas/costas_core.h"`
* `#include "doppler/dll/dll_core.h"`
* `#include "doppler/symsync/symsync_core.h"`
* `#include "doppler/ratesync/ratesync_core.h"`
* `#include "doppler/carrier_mpsk/carrier_mpsk_core.h"`
* `#include "doppler/carrier_nda/carrier_nda_core.h"`
* `#include "doppler/mpsk_receiver/mpsk_receiver_core.h"`
* `#include "doppler/conv_enc/conv_enc_core.h"`
* `#include "doppler/viterbi/viterbi_core.h"`
* `#include "doppler/rs_codec/rs_codec_core.h"`
* `#include "doppler/interleaver/interleaver_core.h"`
* `#include "doppler/despreader/despreader_core.h"`
* `#include "doppler/burst_despreader/burst_despreader_core.h"`
* `#include "doppler/ppe/ppe_core.h"`
* `#include "doppler/burst_demod/burst_demod_core.h"`
* `#include "doppler/dsss_receiver/dsss_receiver_core.h"`
* `#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"`
* `#include "doppler/async_dsss_pool/async_dsss_pool_core.h"`
* `#include "doppler/dsss_burst_receiver/dsss_burst_receiver_core.h"`
* `#include "doppler/carrier_acq/carrier_acq_core.h"`
* `#include "doppler/acq/acq_core.h"`
* `#include "doppler/burst_acq/burst_acq_core.h"`
* `#include "doppler/burst_capture/burst_capture_core.h"`
* `#include "doppler/ber_meter/ber_meter_core.h"`
* `#include "doppler/frame_meter/frame_meter_core.h"`


































































------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/doppler.h`

