/*
 * conv.c — the CCSDS inner code, as a configuration.
 *
 * There is no encoder here any more. 131.0-B-3 section 3.3 picks a code out
 * of a family `conv_core.h` already implements, and this is that pick. The
 * arithmetic lives once, in conv_outputs(), which both conv_encode and
 * viterbi_decode read -- so the G2 inversion cannot be present in one
 * direction and absent in the other.
 */
#include "ccsds_tm/ccsds_tm.h"
#include "viterbi/viterbi_core.h"

const conv_code_t CCSDS_TM_CONV = {
  /* .k      */ 7u,
  /* .n      */ 2u,
  /* .poly   */ { 0171u, 0133u },
  /* .invert */ 0x2u /* 3.3.1(5): G2's output path only */
};
