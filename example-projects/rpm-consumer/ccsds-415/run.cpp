// run.cpp -- the run both front ends drive (run.hpp).
#include "run.hpp"

#include <doppler/ber/ber_core.h>
#include <doppler/ccsds/ccsds_415.h>
#include <doppler/ccsds/ccsds_415_frame.h>
#include <doppler/gold/gold_core.h>
#include <doppler/pn/pn_core.h>
#include <doppler/syncword/syncword_core.h>
#include <doppler/timing/timing_core.h>
#include <doppler/wfm/wfm_core.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ccsds415
{
namespace
{

// std::complex<float> is layout-compatible with float _Complex, so a
// vector's storage crosses to the C API by pointer.
float _Complex *
c_ptr (std::vector<cf32> &v)
{
  return reinterpret_cast<float _Complex *> (v.data ());
}

constexpr double LEAD_IN_S = 0.05; // noise alone before the signal arrives
constexpr unsigned PAYLOAD_SEED = 1, PAYLOAD_PN = 15;
// The data lengths' own PN, apart from the data's.
constexpr unsigned LENGTH_SEED = 2;
// What "sustained" means for the symbol lock is dp_ber_lock_symbol()'s
// own: 0 and 0.0 select its defaults, so this file does not restate them.
constexpr size_t LOCK_SUSTAIN = 0;
constexpr double LOCK_MIN_FRAC = 0.0;
// The receiver's allowed implementation loss against the BPSK bound: the
// worst settled loss the certification record measured at this point, 1.13
// dB (2.8 GHz, Es/N0 4 dB; 2.2 and 2.8 GHz from 4 to 7 dB read 0.72 to 1.13
// dB), in src/doppler/dsss/tests/validation/async_dsss_receiver/results.md
// §2.6. Measured there over unslipped bursts at 4 to 7 dB, not chosen
// here; above 7 dB it is carried beyond what the record measured. The
// owner's allowed loss is doppler#3157's to set.
constexpr double LOSS_DB = 1.13;
constexpr long LAG_SPAN = 1000; // symbols the stream may start into the data

// The mission's code, a 1,023-chip Gold code from the CCSDS command-link
// generator (415.0-G-1 §5.2.2.4, Figure 5-1). The library leaves the code
// to the caller (ccsds_415.h: "the mission's, so it is an argument"), and
// its C face has no names for the standard's registers yet (#3348), so
// each argument is named here.
std::vector<uint8_t>
gold_code ()
{
  // Register A's polynomial x^10+x^9+x^8+x^6+x^3+x^2+1: fixed.
  const uint64_t taps_a = 934;
  // Register A's initial value: user dependent, so it picks the mission's
  // member of the family. 350 is Figure 5-2's worked example (PN Code
  // Library Table 1, code number 365).
  const uint64_t seed_a = 350;
  // Register B's polynomial and its initial value 1001001000: both fixed.
  const uint64_t taps_b = 567, seed_b = 73;
  // Ten-stage registers: a period of 2^10 - 1 = 1,023 chips.
  const uint32_t length = 10;
  std::vector<uint8_t> code (DP_CCSDS_415_SF);
  dp_gold_state_t *g = dp_gold_create (taps_a, seed_a, taps_b, seed_b, length);
  check (g != nullptr, "the Gold generator opens");
  dp_gold_generate (g, code.size (), code.data (), code.size ());
  dp_gold_destroy (g);
  return code;
}

// One period of the 415.1-B-1 stimulus (§1.4) with a frame of `octets`
// data octets: the library's description of it and the description's
// layout. The description points into `f`, so a Period is never copied.
struct Period
{
  dp_ccsds_415_frame_t f;
  wfm_frame_desc_layout_t lay;

  explicit Period (unsigned octets)
  {
    const char *why = nullptr;
    check (dp_ccsds_415_frame_desc (&f, nullptr, octets, &why) == 0,
           std::string ("the frame description: ") + (why ? why : "?"));
    check (dp_wfm_frame_desc_layout (&f.desc, &lay) == 0,
           "the frame description lays out");
  }
  Period (const Period &) = delete;
  Period &operator= (const Period &) = delete;

  // A field's first bit in the period, by its name in the description.
  size_t
  off (const char *field) const
  {
    const int i = dp_wfm_frame_field_index (&f.desc, field);
    check (i >= 0, std::string ("the description has a field ") + field);
    return lay.field_off[i];
  }
};

// A data length, 1 to 1,017 octets with every value equally likely: ten
// bits of the length PN at a time, a draw outside the range rejected. A PN
// rather than <random>'s distributions, whose output differs between
// standard libraries, so every build sends the same stimulus.
unsigned
draw_octets (dp_pn_state_t *pn)
{
  for (;;)
    {
      uint8_t b[10];
      dp_pn_generate (pn, sizeof b, b, sizeof b);
      unsigned v = 0;
      for (const uint8_t bit : b)
        v = (v << 1) | bit;
      if (v >= DP_CCSDS_415_DATA_MIN && v <= DP_CCSDS_415_DATA_MAX)
        return v;
    }
}

// At least n bits of the stimulus, whole periods of it: the gap, the
// acquisition and start sequences, and one TC transfer frame, each frame
// with a data field of a length drawn per frame and filled from doppler's
// PN15. The library assembles every period from its description; this
// only draws the lengths and the data, and keeps where each period sits.
Stimulus
stimulus (size_t n)
{
  Owned<dp_pn_state_t, dp_pn_destroy> data (
      dp_pn_create_mls (0, PAYLOAD_SEED, PAYLOAD_PN, PN_GALOIS));
  Owned<dp_pn_state_t, dp_pn_destroy> lengths (
      dp_pn_create_mls (0, LENGTH_SEED, PAYLOAD_PN, PN_GALOIS));
  check (data && lengths, "the stimulus PNs open");
  Stimulus s;
  std::vector<uint8_t> chunk (8 * DP_CCSDS_415_DATA_MAX), period;
  while (s.bits.size () < n)
    {
      const unsigned octets = draw_octets (lengths.get ());
      const Period p (octets);
      dp_pn_generate (data.get (), 8 * octets, chunk.data (), chunk.size ());
      period.resize (p.lay.frame_bits);
      check (dp_wfm_frame_assemble_data (&p.f.desc, nullptr, chunk.data (),
                                         period.data (), period.size ())
                 == period.size (),
             "a period assembles");
      s.at.push_back (s.bits.size ());
      s.len.push_back (period.size ());
      s.data_at.push_back (s.bits.size () + p.off ("data"));
      s.data_len.push_back (8 * octets);
      s.bits.insert (s.bits.end (), period.begin (), period.end ());
    }
  return s;
}

// Where the BER meter's marker starts: the first data field at least
// LAG_SPAN into the locked stream that is long enough to be the whole
// marker. Data, because the gap, the acquisition sequence and the header
// repeat from period to period, and a marker inside them would match more
// than one lag.
size_t
marker_at (const Stimulus &s, size_t lo)
{
  for (size_t i = 0; i < s.at.size (); i++)
    if (s.data_at[i] >= lo + LAG_SPAN && s.data_len[i] >= BER_SYNC_SYMS)
      return s.data_at[i];
  fail ("no data field long enough for the BER marker in the stimulus");
}

} // namespace

// Reads the stimulus's frames from the receiver's decided symbols, a chunk
// at a time, with no truth: the receiver's side of the link. Each symbol
// is decided by its sign, a bit 1 for a negative real part (bit 0 is sent
// as +1); the stream's polarity is whatever the carrier loop settled on,
// and the start sequence says which.
//
// It finds a period by what §1.4 puts there for the purpose: the 128-bit
// acquisition sequence and the 16-bit start sequence after it, one 144-bit
// marker, searched in either polarity. The tolerance is the most errors
// whose false-frame probability over the bits searched is at most 1e-6
// (dp_syncword_max_errors_for()), so a noisy marker is found and data that
// happens to resemble one is not. It then reads the header's length through
// the library's description and checks the CRC; every frame found counts,
// whatever its CRC says. Once a frame's CRC holds, its length is trusted
// and the reader tracks: the next marker is one period on, where one place
// is tried, and a marker not there is a miss. After a frame whose CRC
// failed, whose length field may itself be wrong, the search resumes.
class FrameReader
{
public:
  // Where a frame was read: its period's first bit in the decided stream,
  // its bits, the polarity, and the CRC's verdict.
  struct Seen
  {
    size_t at, len;
    bool inverted, crc_ok;
  };

  FrameReader () : hdr_ (DP_CCSDS_415_DATA_MIN)
  {
    // Every period's fields up to the data sit at the same offsets, so the
    // shortest description reads any frame's header.
    start_ = hdr_.off ("start");
    acq_ = hdr_.off ("acq");
    header_end_ = hdr_.off ("data");
    // The marker's bits, as the library assembles them: the acquisition
    // and start sequences of a period (its data is irrelevant here).
    std::vector<uint8_t> data (8 * DP_CCSDS_415_DATA_MIN, 0),
        period (hdr_.lay.frame_bits);
    check (dp_wfm_frame_assemble_data (&hdr_.f.desc, nullptr, data.data (),
                                       period.data (), period.size ())
               == period.size (),
           "the marker's period assembles");
    marker_len_ = start_ + DP_CCSDS_415_START_BITS - acq_;
    sw_.reset (dp_syncword_create (&period[acq_], marker_len_));
    check (sw_ != nullptr, "the marker search opens");
    // One offset is tried while tracking.
    track_errors_ = tolerance (1);
  }

  void
  feed (const cf32 *syms, size_t n)
  {
    for (size_t i = 0; i < n; i++)
      bits_.push_back (syms[i].real () < 0.0f ? 1 : 0);
    if (bits_.size () >= need_)
      process ();
  }

  const FrameStats &
  stats () const
  {
    return st_;
  }
  const std::vector<Seen> &
  seen () const
  {
    return seen_;
  }
  const std::vector<uint8_t> &
  bits () const
  {
    return bits_;
  }

private:
  enum Outcome
  {
    NEED,   // not enough bits yet
    MISS,   // tracking: no start sequence where it should be
    REJECT, // searching: not a frame
    READ    // a frame was read and counted
  };

  // The most marker errors whose false-frame probability over `window`
  // offsets is at most 1e-6.
  uint32_t
  tolerance (size_t window) const
  {
    const int t = dp_syncword_max_errors_for (sw_.get (), window, 1e-6);
    return t > 0 ? static_cast<uint32_t> (t) : 0u;
  }

  // The frame whose start sequence begins at bit h, its marker found
  // with `errors` errors in polarity `inverted`. While tracking, the
  // marker is checked here first.
  Outcome
  read_at (size_t h, bool tracking, uint32_t errors = 0, bool inverted = false)
  {
    if (h < start_)
      return tracking ? MISS : REJECT;
    const size_t at = h - start_;
    if (bits_.size () < at + header_end_)
      {
        need_ = at + header_end_;
        return NEED;
      }
    syncword_hit_t hit{ 1, 0, inverted ? 1 : 0, errors };
    if (tracking)
      {
        hit = dp_syncword_find (sw_.get (), &bits_[at + acq_], marker_len_,
                                track_errors_);
        if (!hit.found)
          return MISS;
      }
    const uint8_t flip = hit.inverted ? 1 : 0;

    // The header, through the shortest description: the frame-length field
    // holds the frame's octets less one (232.0-B-4 §4.1.2.7.2).
    buf_.assign (hdr_.lay.frame_bits, 0);
    for (size_t i = 0; i < header_end_; i++)
      buf_[i] = bits_[at + i] ^ flip;
    uint64_t length = 0;
    const char *why = nullptr;
    check (dp_wfm_frame_desc_field (&hdr_.f.desc, buf_.data (), buf_.size (),
                                    "length", &length, &why)
               == 0,
           std::string ("the length field reads: ") + (why ? why : "?"));
    const uint64_t octets = length + 1 - DP_CCSDS_415_HEADER_BITS / 8 - 2;
    if (length + 1 < DP_CCSDS_415_HEADER_BITS / 8 + 2 + DP_CCSDS_415_DATA_MIN
        || octets > DP_CCSDS_415_DATA_MAX)
      {
        if (!tracking)
          return REJECT;
        // Tracked, but no frame has this length: a frame whose header was
        // received wrong, so its end is unknown and the search resumes.
        count (h, at, 0, flip, false, hit.errors);
        st_.tracking = false;
        scan_ = h + 1;
        return READ;
      }

    const Period p (static_cast<unsigned> (octets));
    if (bits_.size () < at + p.lay.frame_bits)
      {
        need_ = at + p.lay.frame_bits;
        return NEED;
      }
    buf_.resize (p.lay.frame_bits);
    for (size_t i = 0; i < p.lay.frame_bits; i++)
      buf_[i] = bits_[at + i] ^ flip;
    const bool ok = dp_wfm_frame_desc_crc_ok (&p.f.desc, buf_.data ()) == 1;
    count (h, at, p.lay.frame_bits, flip, ok, hit.errors);
    // The header's fields as received, read back by name.
    uint64_t v = 0;
    if (dp_wfm_frame_desc_field (&p.f.desc, buf_.data (), buf_.size (),
                                 "scid", &v, nullptr)
        == 0)
      st_.scid = static_cast<unsigned> (v);
    if (dp_wfm_frame_desc_field (&p.f.desc, buf_.data (), buf_.size (),
                                 "vcid", &v, nullptr)
        == 0)
      st_.vcid = static_cast<unsigned> (v);
    if (dp_wfm_frame_desc_field (&p.f.desc, buf_.data (), buf_.size (),
                                 "seq", &v, nullptr)
        == 0)
      st_.seq = static_cast<unsigned> (v);
    st_.octets = static_cast<unsigned> (octets);
    // The next period's start sequence is one period on, if this frame's
    // length can be trusted: only when its CRC held. A failed frame's
    // length field may be wrong itself, so the search resumes after its
    // start instead.
    if (ok)
      {
        next_ = h + p.lay.frame_bits;
        st_.tracking = true;
      }
    else
      {
        st_.tracking = false;
        scan_ = h + 1;
      }
    return READ;
  }

  void
  count (size_t h, size_t at, size_t len, uint8_t flip, bool ok,
         uint32_t errors)
  {
    st_.found++;
    (ok ? st_.crc_ok : st_.crc_failed)++;
    st_.any = true;
    st_.inverted = flip != 0;
    st_.start_errors = errors;
    seen_.push_back ({ at, len, flip != 0, ok });
    last_h_ = h;
  }

  void
  process ()
  {
    need_ = 0;
    for (;;)
      {
        if (st_.tracking)
          {
            const Outcome o = read_at (next_, true);
            if (o == NEED)
              return;
            if (o == MISS)
              {
                st_.missed++;
                st_.tracking = false;
                scan_ = last_h_ + 1;
              }
            continue;
          }
        if (bits_.size () < scan_ + marker_len_)
          return;
        const size_t window = bits_.size () - scan_ - marker_len_ + 1;
        const syncword_hit_t hit
            = dp_syncword_find (sw_.get (), &bits_[scan_],
                                bits_.size () - scan_, tolerance (window));
        if (!hit.found)
          {
            // The last bits may be a marker's first: keep them.
            scan_ = bits_.size () - (marker_len_ - 1);
            return;
          }
        // The search answers with the FIRST offset within the tolerance,
        // and the acquisition sequence alternates, so a marker shifted by
        // two bits still matches all of it and the first answer can come a
        // few bits early. The marker sits where its errors are fewest, so
        // the offsets the marker's length on are compared and the fewest
        // taken (the earliest of a tie).
        size_t m = scan_ + hit.offset;
        syncword_hit_t best = hit;
        if (bits_.size () < m + 2 * marker_len_)
          {
            scan_ = m;
            need_ = m + 2 * marker_len_;
            return;
          }
        for (size_t k = 1; k < marker_len_; k++)
          {
            const syncword_hit_t c
                = dp_syncword_find (sw_.get (), &bits_[scan_ + hit.offset + k],
                                    marker_len_, best.errors);
            if (c.found && c.offset == 0 && c.errors < best.errors)
              {
                best = c;
                m = scan_ + hit.offset + k;
              }
          }
        // The marker starts at the acquisition sequence; h is the start
        // sequence's first bit.
        const size_t h = m + (start_ - acq_);
        const Outcome o
            = read_at (h, false, best.errors, best.inverted != 0);
        if (o == NEED)
          {
            // Search again from this marker once the frame's bits are in.
            scan_ = h - (start_ - acq_);
            return;
          }
        if (o == REJECT)
          scan_ = h + 1;
      }
  }

  const Period hdr_;
  size_t start_ = 0, acq_ = 0, header_end_ = 0, marker_len_ = 0;
  Owned<dp_syncword_state_t, dp_syncword_destroy> sw_;
  uint32_t track_errors_ = 0;
  std::vector<uint8_t> bits_, buf_;
  size_t scan_ = 0, next_ = 0, last_h_ = 0, need_ = 0;
  FrameStats st_;
  std::vector<Seen> seen_;
};

double
sample_period_ns ()
{
  return 1e9 / DP_CCSDS_415_FS;
}

[[noreturn]] void
fail (const std::string &why)
{
  std::fprintf (stderr, "FAIL: %s\n", why.c_str ());
  std::exit (1);
}

void
check (bool ok, const std::string &why)
{
  if (!ok)
    fail (why);
}

// A whole option value as a number, or a failed check naming the option:
// strtod and strtoul read "abc" as 0 and "6x" as 6 without complaint.
static double
number (const std::string &opt, const std::string &v)
{
  char *end = nullptr;
  errno = 0;
  const double x = std::strtod (v.c_str (), &end);
  check (!v.empty () && end == v.c_str () + v.size () && errno == 0
             && std::isfinite (x),
         opt + " takes a number, not '" + v + "'");
  return x;
}

static size_t
count (const std::string &opt, const std::string &v)
{
  char *end = nullptr;
  errno = 0;
  const unsigned long long x = std::strtoull (v.c_str (), &end, 10);
  check (!v.empty () && v[0] != '-' && end == v.c_str () + v.size ()
             && errno == 0,
         opt + " takes a whole number, not '" + v + "'");
  return static_cast<size_t> (x);
}

void
parse (int argc, char **argv, Options &o, bool tui)
{
  o.carriers = { DP_CCSDS_415_FC_LO, DP_CCSDS_415_FC_HI };
  for (int i = 1; i < argc; i++)
    {
      const std::string a = argv[i];
      if (a == "-h" || a == "--help")
        {
          std::printf (
              "usage: %s [--carrier lo|hi|both] [--esn0 DB] [--seconds S]\n"
              "       [--chunk N] [--out DIR]%s\n"
              "  --carrier   2.2 GHz (lo), 2.8 GHz (hi) or both (default)\n"
              "  --esn0      Es/N0 at the receiver input, dB (default %g)\n"
              "  --seconds   signal duration after the lead-in (default %g)\n"
              "  --chunk     samples a push, at least %zu (default %zu)\n"
              "  --out       directory for the captures (default .)\n"
              "  --errors    print where the bit errors fall in the scored "
              "stream\n",
              tui ? "ccsds415-tui" : "ccsds415",
              tui ? " [--unpaced] [--snapshot]" : "", o.esn0_db, o.seconds,
              MIN_CHUNK, o.chunk);
          if (tui)
            std::printf (
                "  --unpaced  run as fast as it goes, not at real time\n"
                "  --snapshot no terminal: run unpaced, print the last "
                "frame\n"
                "  q          quit early, through the same checks\n");
          std::exit (0);
        }
      if (a == "--errors")
        {
          o.errors = true;
          continue;
        }
      if (tui && a == "--unpaced")
        {
          o.paced = false;
          continue;
        }
      if (tui && a == "--snapshot")
        {
          o.snapshot = true;
          o.paced = false;
          continue;
        }
      check (i + 1 < argc, a + " needs a value");
      const std::string v = argv[++i];
      if (a == "--carrier")
        {
          if (v == "lo")
            o.carriers = { DP_CCSDS_415_FC_LO };
          else if (v == "hi")
            o.carriers = { DP_CCSDS_415_FC_HI };
          else
            check (v == "both", "--carrier is lo, hi or both");
        }
      else if (a == "--esn0")
        o.esn0_db = number (a, v);
      else if (a == "--seconds")
        o.seconds = number (a, v);
      else if (a == "--chunk")
        o.chunk = count (a, v);
      else if (a == "--out")
        o.out = v;
      else
        fail ("unknown option " + a);
    }
  check (o.chunk >= MIN_CHUNK,
         "--chunk is at least " + std::to_string (MIN_CHUNK)
             + ": below it one call's symbols can outnumber its samples");
  check (o.seconds > 0.0 && std::isfinite (o.seconds),
         "--seconds is positive");
}

// The capture, streamed: a clean emitter at two samples a chip, the rate
// converter to the capture rate, the Doppler channel, then noise. The
// converter's and the channel's output lengths vary by a sample from call
// to call, so a stage holds what the next chunk has not taken yet.
class Scene
{
public:
  Scene (const uint8_t *code, const std::vector<uint8_t> &payload, double fc,
         double esn0_db, double seconds, size_t chunk)
      : chunk_ (chunk)
  {
    // The point's derived rates, computed once by the library: the chip
    // rate, the converter's ratio from two samples a chip, and the Doppler
    // ramp in ppm of the carrier. The ramp crosses zero at mid-run.
    const double start_hz = -DP_CCSDS_415_R_ENV_HZ_S * seconds / 2.0;
    const dp_ccsds_415_scene_t s = dp_ccsds_415_scene (fc, 2.0, start_hz);
    const double fs2 = 2.0 * s.chip_rate;
    // The emitter: a continuous DSSS source at two samples a chip, clean
    // (the noise is added after the channel). Its own PN arguments are
    // unused, since its data is the payload set below.
    const int type = WFM_SYNTH_DSSS;
    const double freq_hz = 0.0;             // no carrier offset of its own
    const double clean_db = WFM_SYNTH_SNR_CLEAN; // 100 dB and up: no AWGN
    const int snr_mode = 0;                 // auto; moot with no AWGN
    const uint32_t seed = 0;                // its PRNG: no AWGN, no PN data
    const int sps = 2;                      // samples a chip
    const int pn_length = 15;               // its PN register: unused
    const uint64_t pn_poly = 0;             // 0: the table's MLS polynomial
    const int lfsr = PN_GALOIS;
    const double f_end_hz = 0.0;            // chirps only
    syn_.reset (dp_wfm_synth_create (type, fs2, freq_hz, clean_db, snr_mode,
                                     seed, sps, pn_length, pn_poly, lfsr,
                                     f_end_hz));
    check (syn_ != nullptr, "the emitter opens");
    check (dp_wfm_synth_set_dsss_cont (
               syn_.get (), code, DP_CCSDS_415_SF,
               s.chip_rate / DP_CCSDS_415_SYM, WFM_DSSS_DATA_BITS,
               payload.data (), payload.size ())
               == 0,
           "the emitter takes the payload");
    rc_.reset (dp_RateConverter_create (s.ratio, 0));
    ch_.reset (dp_doppler_channel_create (DP_CCSDS_415_FS, fc, s.start_ppm,
                                          s.rate_ppm_s));
    // Es/N0 over one symbol's FS / SYM samples.
    const double snr_db
        = esn0_db - 10.0 * std::log10 (DP_CCSDS_415_FS / DP_CCSDS_415_SYM);
    awg_.reset (dp_awgn_create (
        1, dp_wfm_awgn_amplitude (static_cast<float> (snr_db), 1.0f)));
    check (rc_ && ch_ && awg_, "the converter, channel and noise open");
    lead_left_ = lead_in ();
    clean_left_ = static_cast<size_t> (seconds * fs2);
    clean_.resize (chunk);
    conv_.resize (2 * chunk + 64);
    stage_.resize (2 * chunk + 64);
    noise_.resize (chunk);
  }

  // Fill x with the next chunk; false once the signal ends inside it.
  bool
  next (std::vector<cf32> &x)
  {
    size_t k = 0;
    while (k < chunk_)
      {
        if (lead_left_)
          {
            const size_t take = std::min (lead_left_, chunk_ - k);
            std::fill (x.begin () + k, x.begin () + k + take, cf32{});
            lead_left_ -= take;
            k += take;
            continue;
          }
        if (pos_ == len_)
          {
            if (!clean_left_)
              return false;
            const size_t n = std::min (clean_left_, chunk_);
            dp_wfm_synth_steps (syn_.get (), c_ptr (clean_), n);
            clean_left_ -= n;
            const size_t m = dp_RateConverter_execute (
                rc_.get (), c_ptr (clean_), n, c_ptr (conv_), conv_.size ());
            len_ = dp_doppler_channel_execute (ch_.get (), c_ptr (conv_), m,
                                               c_ptr (stage_), stage_.size ());
            pos_ = 0;
            continue;
          }
        const size_t take = std::min (len_ - pos_, chunk_ - k);
        std::copy (stage_.begin () + pos_, stage_.begin () + pos_ + take,
                   x.begin () + k);
        pos_ += take;
        k += take;
      }
    dp_awgn_generate (awg_.get (), chunk_, c_ptr (noise_), chunk_);
    for (size_t i = 0; i < chunk_; i++)
      x[i] += noise_[i];
    return true;
  }

  static size_t
  lead_in ()
  {
    return static_cast<size_t> (LEAD_IN_S * DP_CCSDS_415_FS);
  }

private:
  size_t chunk_;
  Owned<dp_wfm_synth_state_t, dp_wfm_synth_destroy> syn_;
  Owned<dp_RateConverter_state_t, dp_RateConverter_destroy> rc_;
  Owned<dp_doppler_channel_state_t, dp_doppler_channel_destroy> ch_;
  Owned<dp_awgn_state_t, dp_awgn_destroy> awg_;
  size_t lead_left_ = 0, clean_left_ = 0, pos_ = 0, len_ = 0;
  std::vector<cf32> clean_, conv_, stage_, noise_;
};

Run::Run (const Options &o, double fc) : o_ (o), fc_ (fc)
{
  char name[32];
  std::snprintf (name, sizeof name, "ccsds415-%.1fGHz", fc / 1e9);
  name_ = name;
  code_ = gold_code ();
  stim_ = stimulus (
      static_cast<size_t> ((o.seconds + LEAD_IN_S) * DP_CCSDS_415_SYM) + 64);
  reader_ = std::make_unique<FrameReader> ();

  // The receiver the library builds at the point, for Es/N0 4 dB.
  const char *why = nullptr;
  rx_.reset (dp_async_dsss_receiver_create_ccsds415 (
      code_.data (), code_.size (), fc, 0.0, true, &why));
  check (rx_ != nullptr, std::string ("the receiver: ") + (why ? why : "?"));

  // Telemetry: every probe the receiver has, under "rx.", and one of this
  // program's own, the chunk's processing time per sample. Probes first:
  // the capture sizes its ring from them.
  tlm_.reset (dp_tlm_create (1 << 12));
  check (tlm_ != nullptr, "the telemetry context opens");
  check (dp_async_dsss_receiver_set_telemetry (rx_.get (), tlm_.get (), "rx",
                                               1)
             == DP_OK,
         "the receiver attaches its probes");
  proc_id_ = dp_tlm_probe (tlm_.get (), "app.proc_ns", 1);
  check (proc_id_ >= 0, "the timing probe registers");

  // The capture: lossless by construction when every boundary is at most
  // one chunk apart, with the sample clock as its time base.
  dp_sample_clock_init (&clock_, DP_CCSDS_415_FS, 0);
  const std::string path = o.out + "/" + name_ + ".tlm";
  cap_.reset (
      dp_tlm_capture_open (tlm_.get (), o.chunk, path.c_str (), &clock_));
  check (cap_ != nullptr, "the capture opens " + path);

  scene_ = std::make_unique<Scene> (code_.data (), stim_.bits, fc, o.esn0_db,
                                    o.seconds, o.chunk);
  x_.resize (o.chunk);
  out_.resize (o.chunk);
}

Run::~Run () = default;

double
Run::elapsed_s () const
{
  return static_cast<double> (n_) / DP_CCSDS_415_FS;
}

double
Run::length_s () const
{
  return o_.seconds + LEAD_IN_S;
}

bool
Run::step ()
{
  if (!scene_->next (x_))
    return false;
  // The boundary: the capture drains the chunk just finished.
  dp_tlm_set_now (tlm_.get (), n_);
  // The timed span is the receiver call and nothing else.
  const uint64_t t0 = dp_mono_ns ();
  const size_t got = dp_async_dsss_receiver_steps (
      rx_.get (), c_ptr (x_), o_.chunk, c_ptr (out_), out_.size ());
  const uint64_t dt = dp_mono_ns () - t0;
  // capacity = chunk; a call that fills it may have had more to give.
  // Recorded, not exited on: the TUI must restore the terminal before a
  // check reports, so the finish reports it and the run stops here.
  if (got >= out_.size ())
    {
      filled_ = true;
      return false;
    }
  last_ns_ = static_cast<double> (dt) / static_cast<double> (o_.chunk);
  dp_tlm_emit_checked (tlm_.get (), proc_id_, last_ns_);
  total_ns_ += dt;
  worst_ns_ = std::max (worst_ns_, last_ns_);
  st_ = dp_async_dsss_receiver_status (rx_.get ());
  for (size_t i = 0; i < got; i++)
    {
      syms_.push_back (out_[i]);
      flags_.push_back (static_cast<uint8_t> (st_.locked));
      code_flags_.push_back (static_cast<uint8_t> (st_.code_locked));
    }
  emitted_.push_back (syms_.size ());
  // The frames, read from what this chunk decided: outside the timed span.
  reader_->feed (out_.data (), got);
  n_ += o_.chunk;
  return true;
}

// Where the scored window opens: the later of the symbol lock's and the
// code lock's sustained symbols, or -1 if either never sustained. The
// design's rule (async-dsss-receiver.md §4.12, "The error-rate window") is
// the latest of five flags; status() publishes these two, and the other
// three are private (#3350), so this is that rule over the public ones.
int64_t
Run::window_start () const
{
  const int64_t sym = dp_ber_lock_symbol (flags_.data (), flags_.size (),
                                          LOCK_SUSTAIN, LOCK_MIN_FRAC);
  const int64_t code
      = dp_ber_lock_symbol (code_flags_.data (), code_flags_.size (),
                            LOCK_SUSTAIN, LOCK_MIN_FRAC);
  return sym < 0 || code < 0 ? -1 : std::max (sym, code);
}

Result
Run::finish ()
{
  dp_tlm_set_now (tlm_.get (), n_);
  check (!filled_, "[" + name_
                      + "] a call filled its capacity: symbols may have "
                        "been dropped");
  Result r;
  r.chunks = emitted_.size ();
  r.proc_ns = mean_proc_ns ();
  r.worst_ns = worst_ns_;

  // Complete, or the capture has a hole: close says which.
  check (dp_tlm_capture_close (cap_.get ()) == DP_OK,
         "the capture lost records");
  // Every probe the receiver has, and ours, reached the file: a run that
  // searched, handed over, refined and tracked exercises all of them.
  const size_t probes = dp_tlm_probe_count (tlm_.get ());
  for (size_t i = 0; i < probes; i++)
    {
      const int id = dp_tlm_probe_id_at (tlm_.get (), i);
      check (dp_tlm_emitted (tlm_.get (), id) > 0,
             "[" + name_ + "] probe " + dp_tlm_probe_name (tlm_.get (), id)
                 + " emitted nothing");
    }

  // Score: the window opens where the lock is sustained (window_start()),
  // and the meter finds where the stream sits in the payload from a marker.
  const int64_t lo = window_start ();
  check (lo >= 0, "[" + name_ + "] no sustained symbol and code lock");
  size_t at = 0;
  while (emitted_[at] <= static_cast<size_t> (lo))
    at++;
  r.lock_s = (static_cast<double> (at + 1) * o_.chunk
              - static_cast<double> (Scene::lead_in ()))
             / DP_CCSDS_415_FS;
  Owned<dp_ber_meter_state_t, dp_ber_meter_destroy> meter (
      dp_ber_meter_create (2, 0, 0.0));
  check (meter != nullptr, "the BER meter opens");
  check (dp_ber_meter_set_truth (meter.get (), stim_.bits.data (),
                                 stim_.bits.size ())
             == 0,
         "the meter takes the stimulus");
  // Scoring needs the marker, BER_SYNC_SYMS long, starting LAG_SPAN into
  // the locked stream, and LAG_SPAN more for the alignment's search.
  const size_t need
      = marker_at (stim_, static_cast<size_t> (lo)) + BER_SYNC_SYMS + LAG_SPAN;
  if (need > syms_.size ())
    {
      char b[200];
      std::snprintf (b, sizeof b,
                     "[%s] the scene is too short to score: scoring needs "
                     "%zu symbols, %.2f s, and the scene gave %zu; a longer "
                     "--seconds has them",
                     name_.c_str (), need,
                     static_cast<double> (need) / DP_CCSDS_415_SYM,
                     syms_.size ());
      fail (b);
    }
  check (dp_ber_meter_align (meter.get (), c_ptr (syms_), syms_.size (),
                             marker_at (stim_, static_cast<size_t> (lo)), 0, 0,
                             LAG_SPAN, 0.0),
         "[" + name_ + "] the payload marker is in the locked stream");
  dp_ber_meter_score (meter.get (), c_ptr (syms_), syms_.size (),
                      static_cast<size_t> (lo), syms_.size ());
  const ber_interval_t b = dp_ber_meter_ber (meter.get ());
  r.ber = b.p_hat;
  r.ber_lo = b.lo;
  r.ber_hi = b.hi;
  r.conf = b.conf;
  r.errors = b.errors;
  r.bits = b.symbols;
  r.esn0_db = o_.esn0_db;
  if (o_.errors)
    print_errors (meter.get (), static_cast<size_t> (lo));

  // The frames, against the truth, at the lag the meter detected
  // (symbol i carries stimulus bit i + lag): every period sent whole
  // inside the locked stream must have been read, and each CRC verdict must
  // be what comparing the frame with what was sent says. A frame read
  // where no period begins is a wrong verdict too.
  const long lag = dp_ber_meter_get_lag (meter.get ());
  const auto in_window = [&] (long at, size_t len) {
    return at >= lo && at + static_cast<long> (len) <= static_cast<long> (syms_.size ());
  };
  for (size_t i = 0; i < stim_.at.size (); i++)
    if (in_window (static_cast<long> (stim_.at[i]) - lag, stim_.len[i]))
      r.frames_sent++;
  const std::vector<uint8_t> &rx = reader_->bits ();
  for (const FrameReader::Seen &f : reader_->seen ())
    {
      if (!in_window (static_cast<long> (f.at), f.len))
        continue;
      r.frames_read++;
      (f.crc_ok ? r.frames_ok : r.frames_failed)++;
      const long t = static_cast<long> (f.at) + lag;
      const auto p = std::lower_bound (stim_.at.begin (), stim_.at.end (),
                                       static_cast<size_t> (std::max (t, 0L)));
      const size_t i = static_cast<size_t> (p - stim_.at.begin ());
      // A frame read where no period begins is wrong whatever its CRC
      // says: nothing was sent there.
      if (p == stim_.at.end () || static_cast<long> (*p) != t)
        {
          r.frames_wrong++;
          continue;
        }
      // Read at a period's start with another length: its length field
      // arrived wrong, and only a CRC that passed it is wrong.
      if (stim_.len[i] != f.len)
        {
          if (f.crc_ok)
            r.frames_wrong++;
          continue;
        }
      bool same = true;
      for (size_t k = 0; same && k < f.len; k++)
        same = (rx[f.at + k] ^ (f.inverted ? 1 : 0))
               == stim_.bits[static_cast<size_t> (t) + k];
      if (same != f.crc_ok)
        r.frames_wrong++;
    }
  return r;
}

const FrameStats &
Run::frames () const
{
  return reader_->stats ();
}

// Where the scored stream's bit errors fall, at the meter's own detected lag
// and phase (symbol i carries stimulus bit i + lag): the first and the last
// error, the longest run of consecutive errors, and the error rate from the
// first error on. Random errors are scattered with short runs; a slip or an
// inverted stream shows as everything after one point at a rate near 0.5
// or 1. A diagnosis aid, printed, never a check.
void
Run::print_errors (const dp_ber_meter_state_t *meter, size_t lo) const
{
  const long lag = dp_ber_meter_get_lag (meter);
  const cf32 rot = std::polar (1.0f,
                               -static_cast<float> (dp_ber_meter_get_phase (meter)));
  size_t n = 0, errs = 0, first = 0, last = 0, run = 0, longest = 0;
  for (size_t i = lo; i < syms_.size (); i++)
    {
      const long t = static_cast<long> (i) + lag;
      if (t < 0 || static_cast<size_t> (t) >= stim_.bits.size ())
        continue;
      n++;
      const uint8_t bit = (syms_[i] * rot).real () < 0.0f ? 1 : 0;
      if (bit != stim_.bits[static_cast<size_t> (t)])
        {
          if (!errs)
            first = i;
          last = i;
          errs++;
          longest = std::max (longest, ++run);
        }
      else
        run = 0;
    }
  // The rate from the first error on is over the scored symbols from there,
  // the same set n counts.
  size_t after = 0;
  for (size_t i = first; errs && i < syms_.size (); i++)
    {
      const long t = static_cast<long> (i) + lag;
      if (t >= 0 && static_cast<size_t> (t) < stim_.bits.size ())
        after++;
    }
  // The errors in each tenth of the window, to show a rate that moves.
  size_t tenth[10] = {};
  const size_t span = syms_.size () > lo ? syms_.size () - lo : 0;
  for (size_t i = lo; span && i < syms_.size (); i++)
    {
      const long t = static_cast<long> (i) + lag;
      if (t < 0 || static_cast<size_t> (t) >= stim_.bits.size ())
        continue;
      const uint8_t bit = (syms_[i] * rot).real () < 0.0f ? 1 : 0;
      if (bit != stim_.bits[static_cast<size_t> (t)])
        tenth[std::min<size_t> (9, (i - lo) * 10 / span)]++;
    }
  std::printf ("%s errors: %zu in %zu scored symbols from symbol %zu (lag "
               "%ld, phase %.3f rad)",
               name_.c_str (), errs, n, lo, lag,
               dp_ber_meter_get_phase (meter));
  if (errs)
    std::printf ("; first at symbol %zu (+%zu into the window), last at %zu; "
                 "longest run %zu; from the first on, %.3f of symbols are "
                 "errors",
                 first, first - lo, last, longest,
                 static_cast<double> (errs) / static_cast<double> (after));
  std::printf ("\n%s errors by tenth of the window (%zu symbols each):",
               name_.c_str (), span / 10);
  for (const size_t c : tenth)
    std::printf (" %zu", c);
  std::printf ("\n");
}

bool
Run::finish_partial ()
{
  dp_tlm_set_now (tlm_.get (), n_);
  bool ok = true;
  if (filled_)
    {
      std::fprintf (stderr,
                    "FAIL: [%s] a call filled its capacity: symbols may "
                    "have been dropped\n",
                    name_.c_str ());
      ok = false;
    }
  if (dp_tlm_capture_close (cap_.get ()) != DP_OK)
    {
      std::fprintf (stderr, "FAIL: [%s] the capture lost records\n",
                    name_.c_str ());
      ok = false;
    }
  uint64_t emitted = 0;
  const size_t probes = dp_tlm_probe_count (tlm_.get ());
  for (size_t i = 0; i < probes; i++)
    emitted += dp_tlm_emitted (tlm_.get (), dp_tlm_probe_id_at (tlm_.get (), i));
  const size_t held = dp_tlm_capture_count (cap_.get ());
  if (held != emitted)
    {
      std::fprintf (stderr,
                    "FAIL: [%s] the capture holds %zu records of the %llu "
                    "emitted\n",
                    name_.c_str (), held,
                    static_cast<unsigned long long> (emitted));
      ok = false;
    }
  // Lock time, if it was reached: the window is the same as finish()'s.
  const int64_t lo = window_start ();
  std::string lock = "the lock time (not reached)";
  if (lo >= 0)
    {
      size_t at = 0;
      while (emitted_[at] <= static_cast<size_t> (lo))
        at++;
      char b[64];
      std::snprintf (b, sizeof b, "locked %.2f s after arrival",
                     (static_cast<double> (at + 1) * o_.chunk
                      - static_cast<double> (Scene::lead_in ()))
                         / DP_CCSDS_415_FS);
      lock = b;
    }
  std::printf ("%s: stopped at %.2f s of %.2f s by the user; %s; the "
               "full-scene checks (%sBER, the payload marker, the frames) were not "
               "evaluated. Partial checks (capture complete, %zu records, "
               "no call filled its capacity): %s\n",
               name_.c_str (), elapsed_s (), length_s (), lo >= 0 ? lock.c_str () : "lock not yet reached",
               lo >= 0 ? "" : "the lock time, ", held, ok ? "held" : "FAILED");
  return ok;
}

bool
report (const std::string &name, const Result &r, size_t chunk)
{
  // The cost, in the vocabulary's terms: the processing time per sample
  // against the sample period, their ratio stated as what it is and what
  // it means, so it cannot be read the wrong way round.
  const double ts = sample_period_ns ();
  const double share = r.proc_ns / ts;
  // The bound: theory's BER at the Es/N0 run less the allowed loss. The
  // check fails only when the meter's exact interval lies wholly above it,
  // so a run too short to tell passes, and a receiver worse than theory by
  // more than the loss fails once the bits scored show it.
  const double bound
      = dp_ber_theory_ber (2, std::pow (10.0, (r.esn0_db - LOSS_DB) / 10.0));
  std::printf ("%s: locked %.2f s after arrival (allocation %.0f s); BER "
               "%.1e (%zu errors in %zu bits; %.0f%% interval %.1e to "
               "%.1e)\n",
               name.c_str (), r.lock_s, DP_CCSDS_415_LOCK_S, r.ber, r.errors,
               r.bits, 100.0 * r.conf, r.ber_lo, r.ber_hi);
  // The loss the rate implies: the Es/N0 the bound needs for it, less the
  // Es/N0 run. None with no errors, which no finite Es/N0 gives.
  const double needs = dp_ber_esn0_db_for_ser (2, r.ber);
  // With no errors there is none to measure; with a rate past the bound at
  // -10 dB (near chance), the library reads none either, and says so with
  // a NaN, which is not "no errors".
  char loss[64] = "none measurable (no errors)";
  if (r.errors && std::isfinite (needs))
    std::snprintf (loss, sizeof loss, "%.2f dB", r.esn0_db - needs);
  else if (r.errors)
    std::snprintf (loss, sizeof loss,
                   "more than %.0f dB (the rate is near chance)",
                   r.esn0_db + 10.0);
  std::printf ("  the bound at Es/N0 %.1f dB less the allowed %.2f dB: %.1e; "
               "the loss this rate implies: %s\n",
               r.esn0_db, LOSS_DB, bound, loss);
  std::printf ("  processing time per sample %.1f ns, sample period %.1f "
               "ns: %.1f%% of the sample period (%.1f times faster than "
               "real time); slowest chunk %.1f ns a sample; %zu chunks of "
               "%zu\n",
               r.proc_ns, ts, 100.0 * share, 1.0 / share, r.worst_ns,
               r.chunks, chunk);
  bool ok = true;
  if (!(r.lock_s <= DP_CCSDS_415_LOCK_S))
    {
      std::fprintf (stderr, "FAIL: [%s] locked after the allocation\n",
                    name.c_str ());
      ok = false;
    }
  std::printf ("  frames: %zu of the %zu sent in the locked stream read; "
               "CRC passed %zu, failed %zu; %zu verdicts the truth "
               "contradicts\n",
               r.frames_read, r.frames_sent, r.frames_ok, r.frames_failed,
               r.frames_wrong);
  if (r.frames_sent == 0)
    {
      std::fprintf (stderr, "FAIL: [%s] no whole frame in the locked "
                            "stream: a longer scene has some\n",
                    name.c_str ());
      ok = false;
    }
  if (r.frames_read != r.frames_sent)
    {
      std::fprintf (stderr, "FAIL: [%s] %zu frames read of %zu sent\n",
                    name.c_str (), r.frames_read, r.frames_sent);
      ok = false;
    }
  if (r.frames_wrong)
    {
      std::fprintf (stderr, "FAIL: [%s] %zu CRC verdicts the truth "
                            "contradicts\n",
                    name.c_str (), r.frames_wrong);
      ok = false;
    }
  // No bits scored has an interval from 0, which no bound refuses: it
  // fails on its own.
  if (r.bits == 0 || !(r.ber_lo <= bound))
    {
      std::fprintf (stderr,
                    "FAIL: [%s] BER %.2e: its %.0f%% interval (%.2e to "
                    "%.2e) lies above the bound, %.2e\n",
                    name.c_str (), r.ber, 100.0 * r.conf, r.ber_lo,
                    r.ber_hi, bound);
      ok = false;
    }
  return ok;
}

} // namespace ccsds415
