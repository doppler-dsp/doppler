/**
 * ccsds_link_demo.c — a whole CCSDS TM link, transmitter to recovered text.
 *
 * The four coding stages of 131.0-B-3 in both directions, over a real noisy
 * channel, with every piece coming from the library rather than from this
 * file:
 *
 *   transfer frame --> [R-S (255,223) I=5] --> [randomiser] --> [+ASM]
 *                  --> [conv K=7 r=1/2] --> BPSK --> AWGN
 *                  --> soft demap --> [Viterbi] --> [ASM search]
 *                  --> [derandomise] --> [de-interleave + R-S decode]
 *                  --> transfer frame
 *
 * §1 sweeps Es/N0 and prints what each stage saw: the channel's own symbol
 *    error rate, what survived the inner code, and what the outer code made
 *    of the result -- including the symbols it REPAIRED, which is the margin
 *    the concatenation is spending before it starts losing frames.
 * §2 runs one link at 3 dB and prints the recovered text.
 *
 * ## Three things this demonstrates that a round trip would not
 *
 * **The inner code is continuous.** 3.3.2 fixes the output as one
 * uninterrupted symbol sequence, so one `conv_enc_t` is carried across all
 * the frames rather than restarted per frame — restarting costs the K-1 bits
 * of register memory at the head of every frame, landing on the ASM, and a
 * matched decoder absorbs it invisibly.
 *
 * **The decoder is streaming, so the tail of the stream has no bits yet.** A
 * Viterbi emits decision `i` only after seeing `depth - 1` further bits, so
 * its
 * output is ALIGNED with its input and simply stops `depth - 1` bits short.
 * The final CADU is still inside the traceback and needs its successor before
 * it resolves — which is the reason `ccsds_tm_frame_decode` takes CADU bits
 * rather than channel symbols.
 *
 * **The receiver does not start at a frame boundary.** This deliberately
 * throws away the first @ref SKIP_SYM channel symbols before decoding, the
 * way a real capture begins whenever the recorder happened to start. Nothing
 * in the stream says where a frame is; `ccsds_tm_asm_find` correlates for
 * the marker, and the offset it reports is what everything downstream is
 * measured from. The count is EVEN on purpose: an odd one would break the
 * rate-1/2 symbol pairing, and choosing between the two hypotheses is node
 * synchronisation, which this library does not have yet
 * (docs/design/fec-receive.md section 3).
 *
 * Build:
 *   cmake --build build
 *   ./build/examples/c/ccsds_link_demo
 */

#include "viterbi/viterbi_core.h"
#include <awgn/awgn_core.h>
#include <ccsds_tm/ccsds_tm.h>
#include <ccsds_tm/ccsds_tm_frame.h>
#include <conv/conv_core.h>
#include <mpsk/mpsk_core.h>

#include <complex.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Interleaving depth 5 is 4.3.5.1's mid-range choice and the one that makes
   the de-interleave observable: at depth 1 a wrong rotation is the identity.
 */
#define RS_DEPTH 5
#define FRAME_LEN (CCSDS_TM_RS_K * RS_DEPTH)
#define NFRAMES 4

#define CADU_BITS (CCSDS_TM_ASM_BITS + CCSDS_TM_RS_N * RS_DEPTH * 8)
#define SYM_PER_FRAME (2 * CADU_BITS)
#define TOTAL_SYM (NFRAMES * SYM_PER_FRAME)
#define TOTAL_CADU_BITS (NFRAMES * CADU_BITS)

/* Where the "recorder" started: an arbitrary EVEN number of channel symbols
   into the stream, so the receiver has to find the frame rather than be told.
   Even, because an odd offset swaps the two symbols of every rate-1/2 branch
   and only node synchronisation can undo that. */
#define SKIP_SYM 1554
#define SKIP_BITS (SKIP_SYM / 2)

/* 60, not 5*K = 35: the textbook depth sits 33 % above the achievable BER for
   this code (docs/design/viterbi.md section 4). */
#define TRACEBACK 60

/* What the receiver actually gets to work with: the symbols after the skip,
   less the depth-1 bits still inside the traceback when the stream ends. */
#define RX_SYM (TOTAL_SYM - SKIP_SYM)
#define RX_BITS (RX_SYM / 2 - (TRACEBACK - 1))

static const ccsds_tm_frame_cfg_t CODED = {
  .rs_depth = RS_DEPTH, .randomise = 1, .attach_asm = 1, .convolutional = 1
};

/* The same coding minus the inner code, which makes ccsds_tm_frame_encode's
   output the CADU itself — the reference the receiver is scored against. The
   CADU does not depend on the inner code, so this is the transmitted truth and
   not a second encoding of it. */
static const ccsds_tm_frame_cfg_t CADU_ONLY = {
  .rs_depth = RS_DEPTH, .randomise = 1, .attach_asm = 1, .convolutional = 0
};

static uint8_t       g_frame[NFRAMES][FRAME_LEN];
static uint8_t       g_tx_cadu[TOTAL_CADU_BITS];
static uint8_t       g_tx_sym[TOTAL_SYM];
static float complex g_mod[TOTAL_SYM];
static float complex g_noise[TOTAL_SYM];
static float         g_llr[TOTAL_SYM];
static uint8_t       g_rx_bits[TOTAL_SYM];
static uint8_t       g_back[FRAME_LEN];

static const char *const MESSAGE
    = "DOPPLER CCSDS TM LINK -- 131.0-B-3 concatenated coding: "
      "Reed-Solomon (255,223) I=5, pseudo-randomiser, attached sync marker, "
      "convolutional K=7 rate 1/2. If you can read this, all four stages "
      "agreed with their inverses about which bits they covered.";

/* Fill the frames: the message, then a recognisable pattern behind it so the
   R-S check is scoring real data rather than a run of zeros. */
static void
build_frames (void)
{
  for (unsigned f = 0; f < NFRAMES; f++)
    {
      memset (g_frame[f], 0, FRAME_LEN);
      for (size_t i = 0; i < FRAME_LEN; i++)
        g_frame[f][i] = (uint8_t)(i * 31u + f * 17u);
      /* In EVERY frame: which one the receiver recovers first depends on
         where the capture started, and a message in only one of them would
         make this demo's output depend on SKIP_SYM. */
      memcpy (g_frame[f], MESSAGE, strlen (MESSAGE));
    }
}

/* Encode all frames once, carrying ONE inner encoder across them (3.3.2), and
   record the CADU bits the receiver will be scored against. */
static void
transmit (void)
{
  conv_enc_t conv;
  conv_enc_init (&conv);
  for (unsigned f = 0; f < NFRAMES; f++)
    {
      ccsds_tm_frame_encode (&CADU_ONLY, NULL, g_frame[f], FRAME_LEN,
                             g_tx_cadu + (size_t)f * CADU_BITS, CADU_BITS);
      ccsds_tm_frame_encode (&CODED, &conv, g_frame[f], FRAME_LEN,
                             g_tx_sym + (size_t)f * SYM_PER_FRAME,
                             SYM_PER_FRAME);
    }
  mpsk_map (g_tx_sym, TOTAL_SYM, g_mod, 2);
}

/* One pass of the receiver. Returns the number of CADUs fully recovered, and
   reports what each stage saw. */
static unsigned
receive (float esn0_db, uint64_t seed, size_t *chan_errs, size_t *bit_errs,
         unsigned *rs_words, unsigned *rs_ok, unsigned *rs_syms, int verbose)
{
  /* One place answers "per rail or total power?", so a 3 dB error cannot be
     introduced here by deriving sigma by hand. */
  const float   sigma = awgn_amplitude_for_snr (esn0_db, 1.0f);
  const float   n0    = 2.0f * sigma * sigma;
  awgn_state_t *ch    = awgn_create (seed, sigma);
  awgn_generate (ch, TOTAL_SYM, g_noise, TOTAL_SYM);
  awgn_destroy (ch);

  for (size_t i = 0; i < TOTAL_SYM; i++)
    g_mod[i] += g_noise[i];

  mpsk_soft_demap (g_mod, TOTAL_SYM, g_llr, TOTAL_SYM, 2, n0);

  /* Undo the noise so the caller's modulated copy can be reused. */
  for (size_t i = 0; i < TOTAL_SYM; i++)
    g_mod[i] -= g_noise[i];

  /* What the channel did, scored before any decoding: positive LLR is
     symbol 0, the same rule mpsk_demap applies. */
  *chan_errs = 0;
  for (size_t i = SKIP_SYM; i < TOTAL_SYM; i++)
    if ((g_llr[i] < 0.0f ? 1u : 0u) != g_tx_sym[i])
      (*chan_errs)++;

  /* The capture starts SKIP_SYM symbols in, so the decoder never sees the
     head of the stream -- and starts from viterbi_reset's all-zero prior,
     which is simply wrong here. It is a wrong PRIOR rather than a wrong
     answer: the survivor paths are determined by the data within a few
     constraint lengths, long before the first marker this finds. */
  viterbi_state_t *v    = viterbi_create_code (&CCSDS_TM_CONV, TRACEBACK);
  const size_t     n_rx = viterbi_decode (
      v, g_llr + SKIP_SYM, TOTAL_SYM - SKIP_SYM, g_rx_bits, TOTAL_SYM);
  viterbi_destroy (v);

  /* Decision i is emitted after depth-1 further bits, so the output is ALIGNED
     with the decoder's input and merely stops short -- g_rx_bits[i] is the
     transmitted CADU bit i + SKIP_BITS, with no further shift. */
  *bit_errs = 0;
  for (size_t i = 0; i < n_rx; i++)
    if (g_rx_bits[i] != g_tx_cadu[i + SKIP_BITS])
      (*bit_errs)++;

  /* Bounded to a window that must contain exactly one marker, because the
     false-alarm rate is a property of the SEARCH LENGTH: at a tolerance of 2
     a random 32-bit window matches with probability 1058/2^32, which is 0.25%
     over this window and would be several percent over the whole stream.
     test_ccsds_tm_asm.c carries the arithmetic. */
  const size_t       win = CADU_BITS + CCSDS_TM_ASM_BITS;
  ccsds_tm_asm_hit_t hit;
  if (!ccsds_tm_asm_find (g_rx_bits, n_rx < win ? n_rx : win, 2u, &hit))
    {
      *rs_words = 0;
      *rs_ok    = 0;
      *rs_syms  = 0;
      return 0;
    }
  if (verbose)
    printf ("  sync      ASM at bit %zu, %u bit error(s), polarity %s\n",
            hit.offset, hit.errors, hit.inverted ? "inverted" : "normal");

  *rs_words       = 0;
  *rs_ok          = 0;
  *rs_syms        = 0;
  unsigned frames = 0;
  for (size_t at = hit.offset; at + CADU_BITS <= n_rx; at += CADU_BITS)
    {
      ccsds_tm_frame_rx_t rx;
      if (ccsds_tm_frame_decode (&CODED, g_rx_bits + at, CADU_BITS, g_back,
                                 sizeof g_back, &rx)
          == 0)
        break;
      *rs_words += rx.rs_codewords;
      *rs_ok += rx.rs_ok;
      *rs_syms += rx.rs_symbols;
      frames++;
      /* Which transmitted frame this CADU is, from where it sits in the
         stream -- the capture did not start at frame 0. */
      const unsigned which = (unsigned)((at + SKIP_BITS) / CADU_BITS);
      if (verbose)
        {
          char text[73];
          memcpy (text, g_back, sizeof text - 1u);
          text[sizeof text - 1u] = '\0';
          printf ("  frame %u   %zu octets, R-S %u/%u OK (%u symbol(s) "
                  "repaired), %s\n",
                  which, rx.frame_len, rx.rs_ok, rx.rs_codewords,
                  rx.rs_symbols,
                  which < NFRAMES
                          && memcmp (g_back, g_frame[which], FRAME_LEN) == 0
                      ? "byte-exact"
                      : "MISMATCH");
          printf ("            \"%s...\"\n", text);
        }
    }
  return frames;
}

int
main (void)
{
  printf ("=== doppler CCSDS TM link demo ===\n\n");
  build_frames ();
  transmit ();

  ccsds_tm_frame_layout_t lay;
  ccsds_tm_frame_layout (&CODED, FRAME_LEN, &lay);
  printf ("transfer frame   %d octets\n", FRAME_LEN);
  printf ("codeblock        %zu bits (R-S 255/223, I=%d)\n", lay.block_bits,
          RS_DEPTH);
  printf ("CADU             %zu bits (ASM covers bits %zu..%zu)\n",
          lay.cadu_bits, lay.marker.first, lay.marker.n - 1u);
  printf ("channel symbols  %zu per frame, %d frames sent\n", lay.out_bits,
          NFRAMES);
  printf ("randomiser spans bits %zu..%zu -- NOT the marker\n",
          lay.randomised.first, lay.randomised.first + lay.randomised.n - 1u);
  printf ("inner code spans bits %zu..%zu -- marker INCLUDED\n\n",
          lay.inner.first, lay.inner.first + lay.inner.n - 1u);

  /* ── 1. what each stage sees, against Es/N0 ─────────────────────────── */
  printf ("§1  Es/N0 sweep (BPSK, rate 1/2, so Eb/N0 = Es/N0 + 3 dB)\n\n");
  printf ("   Es/N0   channel SER   post-Viterbi BER   R-S OK   repaired   "
          "frames\n");
  printf ("   -----   -----------   ----------------   ------   --------   "
          "------\n");
  for (int i = 0; i <= 4; i++)
    {
      const float    esn0 = (float)i;
      size_t         ce = 0, be = 0;
      unsigned       words = 0, ok = 0, syms = 0;
      const unsigned frames = receive (esn0, 20260817u + (uint64_t)i, &ce, &be,
                                       &words, &ok, &syms, 0);
      printf ("   %4.1f     %8.4f%%   %14.2e   %3u/%-3u   %8u   %4u\n",
              (double)esn0, 100.0 * (double)ce / (double)RX_SYM,
              (double)be / (double)RX_BITS, ok, words, syms, frames);
    }

  /* ── 2. the contents, at an operating point that works ──────────────── */
  printf ("\n§2  One link at 3.0 dB, end to end\n\n");
  size_t         ce = 0, be = 0;
  unsigned       words = 0, ok = 0, syms = 0;
  const unsigned frames
      = receive (3.0f, 20260817u + 3u, &ce, &be, &words, &ok, &syms, 1);
  printf ("  channel   %zu of %d symbols wrong (%.4f%%)\n", ce, RX_SYM,
          100.0 * (double)ce / (double)RX_SYM);
  printf ("  Viterbi   %zu bit error(s) in %d decoded bits\n", be, RX_BITS);
  printf ("  R-S       %u of %u codewords good, %u symbol(s) repaired\n", ok,
          words, syms);
  printf ("  frames    %u whole CADUs in a capture starting %d symbols late\n",
          frames, SKIP_SYM);

  printf ("\nDemo complete.\n");
  return 0;
}
