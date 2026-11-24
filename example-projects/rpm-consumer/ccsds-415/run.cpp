// run.cpp -- the run both front ends drive (run.hpp).
#include "run.hpp"

#include <doppler/ber/ber_core.h>
#include <doppler/ccsds/ccsds_415.h>
#include <doppler/gold/gold_core.h>
#include <doppler/pn/pn_core.h>
#include <doppler/timing/timing_core.h>
#include <doppler/wfm/wfm_core.h>

#include <algorithm>
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
// What "sustained" means for the symbol lock: dp_ber_lock_symbol()'s
// defaults on the Python face.
constexpr size_t LOCK_SUSTAIN = 200;
constexpr double LOCK_MIN_FRAC = 0.9;
constexpr double MAX_BER = 1e-3;
constexpr long LAG_SPAN = 1000; // symbols the stream may start into the data

// The mission's code: doppler's Gold generator at its CCSDS defaults.
std::vector<uint8_t>
gold_code ()
{
  std::vector<uint8_t> code (DP_CCSDS_415_SF);
  dp_gold_state_t *g = dp_gold_create (934, 350, 567, 73, 10);
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
              "  --out       directory for the captures (default .)\n",
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
        o.esn0_db = std::strtod (v.c_str (), nullptr);
      else if (a == "--seconds")
        o.seconds = std::strtod (v.c_str (), nullptr);
      else if (a == "--chunk")
        o.chunk = static_cast<size_t> (std::strtoul (v.c_str (), nullptr, 10));
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
    syn_.reset (dp_wfm_synth_create (WFM_SYNTH_DSSS, fs2, 0.0,
                                     WFM_SYNTH_SNR_CLEAN, 0, 0, 2, 15, 0,
                                     PN_GALOIS, 0.0));
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

  // The receiver the library builds at the point, for its 4 dB floor.
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
    }
  emitted_.push_back (syms_.size ());
  n_ += o_.chunk;
  return true;
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

  // Score: the window opens where the symbol lock is sustained, and the
  // meter finds where the stream sits in the payload from a marker.
  const int64_t lo = dp_ber_lock_symbol (flags_.data (), flags_.size (),
                                         LOCK_SUSTAIN, LOCK_MIN_FRAC);
  check (lo >= 0, "[" + name_ + "] no sustained symbol lock");
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
  check (dp_ber_meter_align (meter.get (), c_ptr (syms_), syms_.size (),
                             marker_at (static_cast<size_t> (lo)), 0, 0,
                             LAG_SPAN, 0.0),
         "[" + name_ + "] the payload marker is in the locked stream");
  dp_ber_meter_score (meter.get (), c_ptr (syms_), syms_.size (),
                      static_cast<size_t> (lo), syms_.size ());
  const ber_interval_t b = dp_ber_meter_ber (meter.get ());
  r.ber = b.p_hat;
  r.bits = b.symbols;
  return r;
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
  const int64_t lo = dp_ber_lock_symbol (flags_.data (), flags_.size (),
                                         LOCK_SUSTAIN, LOCK_MIN_FRAC);
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
  std::printf ("%s: stopped at %.2f s of %.2f by the user; %s; the "
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
  std::printf ("%s: locked %.2f s after arrival (allocation %.0f s); BER "
               "%.1e over %zu bits\n",
               name.c_str (), r.lock_s, DP_CCSDS_415_LOCK_S, r.ber, r.bits);
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
  // Written so a NaN (nothing scored) fails.
  if (!(r.ber <= MAX_BER))
    {
      std::fprintf (stderr, "FAIL: [%s] BER %.2e over %.0e\n", name.c_str (),
                    r.ber, MAX_BER);
      ok = false;
    }
  return ok;
}

} // namespace ccsds415
