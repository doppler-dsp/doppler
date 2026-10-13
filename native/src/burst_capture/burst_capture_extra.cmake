# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# test_burst_capture_core builds a swept carrier with dp_swept_test.h, which is
# dp_wfm_synth_prod over a chirp synth. That is a TEST-ONLY dependency (the
# engine never calls the generator), so it is added here to the test target
# alone: a `depends_on` entry would put the generator on burst_capture_core's own link
# line. The list is wfm_synth_core's closure, as test_wfm_synth_core links it.
target_link_libraries(test_burst_capture_core PRIVATE
    wfm_synth_core lo_core awgn_core pn_core fir_core resamp_core
    wfm_dsp_core wfm_frame_core cvt_core gold_core)
