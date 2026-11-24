// run.cpp -- the run both front ends drive (run.hpp).
#include "run.hpp"

#include <doppler/ber/ber_core.h>
#include <doppler/ccsds/ccsds_415.h>
#include <doppler/gold/gold_core.h>
#include <doppler/pn/pn_core.h>
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

// The payload, a bit a symbol from doppler's PN15, each frame's code-only
// opening zero: the truth the BER is scored against is what was sent.
std::vector<uint8_t>
payload_bits (size_t n)
{
  std::vector<uint8_t> bits (n);
  dp_pn_state_t *pn
      = dp_pn_create_mls (0, PAYLOAD_SEED, PAYLOAD_PN, PN_GALOIS);
  check (pn != nullptr, "the payload PN opens");
  dp_pn_generate (pn, n, bits.data (), n);
  dp_pn_destroy (pn);
  for (size_t i = 0; i < n; i++)
    if (i % DP_CCSDS_415_FRAME_SYM < DP_CCSDS_415_CODE_ONLY_SYM)
      bits[i] = 0;
  return bits;
}

// The first symbol at least LAG_SPAN into the locked stream that is data,
// never a frame's code-only opening, which is all one symbol.
size_t
marker_at (size_t lo)
{
  size_t t0 = lo + LAG_SPAN;
  if (t0 % DP_CCSDS_415_FRAME_SYM < DP_CCSDS_415_CODE_ONLY_SYM)
    t0 += DP_CCSDS_415_CODE_ONLY_SYM - t0 % DP_CCSDS_415_FRAME_SYM;
  return t0;
}

} // namespace

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
  payload_ = payload_bits (
      static_cast<size_t> ((o.seconds + LEAD_IN_S) * DP_CCSDS_415_SYM) + 64);

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

  scene_ = std::make_unique<Scene> (code_.data (), payload_, fc, o.esn0_db,
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
  check (dp_ber_meter_set_truth (meter.get (), payload_.data (),
                                 payload_.size ())
             == 0,
         "the meter takes the payload");
  // Scoring needs the marker, BER_SYNC_SYMS long, starting LAG_SPAN into
  // the locked stream, and LAG_SPAN more for the alignment's search.
  const size_t need
      = marker_at (static_cast<size_t> (lo)) + BER_SYNC_SYMS + LAG_SPAN;
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
                             marker_at (static_cast<size_t> (lo)), 0, 0,
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
  return r;
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
      if (t < 0 || static_cast<size_t> (t) >= payload_.size ())
        continue;
      n++;
      const uint8_t bit = (syms_[i] * rot).real () < 0.0f ? 1 : 0;
      if (bit != payload_[static_cast<size_t> (t)])
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
      if (t >= 0 && static_cast<size_t> (t) < payload_.size ())
        after++;
    }
  // The errors in each tenth of the window, to show a rate that moves.
  size_t tenth[10] = {};
  const size_t span = syms_.size () > lo ? syms_.size () - lo : 0;
  for (size_t i = lo; span && i < syms_.size (); i++)
    {
      const long t = static_cast<long> (i) + lag;
      if (t < 0 || static_cast<size_t> (t) >= payload_.size ())
        continue;
      const uint8_t bit = (syms_[i] * rot).real () < 0.0f ? 1 : 0;
      if (bit != payload_[static_cast<size_t> (t)])
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
               "full-scene checks (%sBER, the payload marker) were not "
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
