// run.hpp -- one run of the async DSSS receiver at the CCSDS 415.1-B-1
// point, a chunk at a time: the loop both front ends drive.
//
// ccsds415 (main.cpp) runs it to the end and prints a summary; ccsds415-tui
// (tui.cpp) runs it a chunk at a time and draws it as it goes. Neither
// computes a measurement of its own: every number either shows comes from
// here, and everything here comes from the library.
#pragma once

#include <doppler/RateConverter/RateConverter_core.h>
#include <doppler/async_dsss_receiver/async_dsss_receiver_core.h>
#include <doppler/awgn/awgn_core.h>
#include <doppler/ber_meter/ber_meter_core.h>
#include <doppler/doppler_channel/doppler_channel_core.h>
#include <doppler/dp_tlm/dp_tlm_core.h>
#include <doppler/dp_tlm_capture/dp_tlm_capture_core.h>
#include <doppler/wfm_synth/wfm_synth_core.h>

#include <cmath>
#include <complex>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ccsds415
{

using cf32 = std::complex<float>;

// Every doppler object is created by a function and freed by its destroy
// function; a unique_ptr with that function as its deleter owns it.
template <auto Destroy> struct Deleter
{
  template <class T>
  void
  operator() (T *p) const
  {
    Destroy (p);
  }
};
template <class T, auto Destroy>
using Owned = std::unique_ptr<T, Deleter<Destroy> >;

// The smallest chunk a run takes: any. The output is sized from the
// receiver's own bound for the chunk (see the README, "Chunks").
constexpr size_t MIN_CHUNK = 1;
// The sample period, ns: the cost every chunk is held against.
double sample_period_ns ();

// What a run is. The defaults are the batch binary's; the TUI changes the
// scene's length.
struct Options
{
  std::vector<double> carriers;
  double esn0_db = 10.0; // 6 dB above the point's Es/N0 4 dB
  double seconds = 10.0; // signal after the lead-in: four whole frames
  size_t chunk = 4096;   // samples a push into the receiver
  std::string out = "."; // directory the captures go to
  bool paced = false;    // the TUI's: hold each chunk to real time
  bool snapshot = false; // the TUI's: no terminal, one frame at the end
  bool errors = false;   // print where in the scored stream errors fall
  // A forced whole-symbol slip, for testing: the decided symbol at this
  // index dropped, or repeated (SIZE_MAX: none).
  size_t drop_symbol = SIZE_MAX, repeat_symbol = SIZE_MAX;
};

// Parse argv into o (whose fields hold the defaults); exits on --help or a
// bad option. `tui` adds the TUI's own options to the usage.
void parse (int argc, char **argv, Options &o, bool tui);

// A failed check: the reason on stderr, exit 1.
[[noreturn]] void fail (const std::string &why);
void check (bool ok, const std::string &why);

// What the frame reader has read so far, from the receiver's decided
// symbols alone: no truth. Every count is of whole frames.
struct FrameStats
{
  size_t found = 0;      // frames read: a start sequence, then the length
  size_t crc_ok = 0;     // of those, the FECF's CRC held
  size_t crc_failed = 0; // and did not
  size_t missed = 0;     // no start sequence where the last frame put it
  size_t slips = 0;      // frames found a few symbols from where due
  size_t slip_at = 0;    // the last such frame's start sequence, symbol
  long slip_by = 0;      // and its shift, symbols
  bool tracking = false; // the next start sequence's place is known
  // The last frame read.
  bool any = false;
  unsigned octets = 0, scid = 0, vcid = 0, seq = 0, start_errors = 0;
  bool inverted = false; // read from the complemented stream
};

// What a finished run measured.
struct Result
{
  double lock_s = NAN;   // arrival to the sustained symbol lock
  double esn0_db = NAN;  // the Es/N0 run
  double ber = NAN;      // over the locked stream
  double ber_lo = NAN, ber_hi = NAN; // its exact interval (the meter's)
  double conf = NAN;     // that interval's confidence level
  size_t errors = 0;     // bit errors scored
  size_t bits = 0;       // bits scored
  double proc_ns = 0.0;  // processing time per sample, whole run
  double worst_ns = 0.0; // the slowest chunk's
  size_t chunks = 0;
  // Frames in the locked stream: those sent whole inside it, those read,
  // their CRC verdicts, and the verdicts the truth contradicts.
  size_t frames_sent = 0, frames_read = 0, frames_ok = 0, frames_failed = 0;
  size_t frames_wrong = 0;
  // Frames the truth places a few symbols from the meter's lag: read after
  // a whole-symbol slip. The first's symbol, and its shift.
  size_t frames_shifted = 0, shift_at = 0;
  long shift_by = 0;
  size_t reader_slips = 0; // the slips the reader reported, with no truth
  double frames_need_s = NAN; // with none sent: the --seconds one needs
};

// The summary lines for one carrier, and whether its checks held.
bool report (const std::string &name, const Result &r, size_t chunk);

class Scene;
class FrameReader;

// The stimulus, every period of it, as the bits sent and where each period
// sits in them.
struct Stimulus
{
  std::vector<uint8_t> bits;
  std::vector<size_t> at, len;   // a period's first bit, and its bits
  std::vector<size_t> data_at, data_len; // its data field's
};

// One carrier's run: the scene, the receiver, its telemetry and capture.
class Run
{
public:
  Run (const Options &o, double fc);
  ~Run ();
  Run (const Run &) = delete;
  Run &operator= (const Run &) = delete;

  // Process the next chunk; false once the scene has ended.
  bool step ();
  // Close the capture and score the run: the self-checks (a complete
  // capture, every probe emitted, a sustained lock, the payload found, and
  // every frame sent in the locked stream read, with CRC verdicts the
  // truth agrees with).
  Result finish ();
  // A run stopped before its scene ended (the TUI's q): close the capture
  // and check only what a partial run can answer. The capture closes
  // complete and holds every record emitted so far; no call filled its
  // capacity (step() checks that as it goes). Says why on stderr when a
  // check fails, and prints the line saying which full-scene checks were
  // not evaluated. True when the partial checks hold.
  bool finish_partial ();

  const std::string &
  name () const
  {
    return name_;
  }
  double
  carrier_hz () const
  {
    return fc_;
  }
  // The receiver as status() reports it after the last chunk.
  const async_dsss_receiver_status_t &
  status () const
  {
    return st_;
  }
  // The last chunk's processing time per sample, ns: the receiver call
  // alone.
  double
  last_proc_ns () const
  {
    return last_ns_;
  }
  double
  mean_proc_ns () const
  {
    return n_ ? static_cast<double> (total_ns_) / n_ : 0.0;
  }
  // Seconds of the scene processed, and its length.
  double
  elapsed_s () const;
  double
  length_s () const;
  size_t
  symbols () const
  {
    return syms_.size ();
  }
  // The frames read so far.
  const FrameStats &frames () const;

private:
  const Options &o_;
  double fc_;
  std::string name_;
  std::vector<uint8_t> code_;
  Stimulus stim_;
  std::unique_ptr<FrameReader> reader_;
  Owned<dp_async_dsss_receiver_state_t, dp_async_dsss_receiver_destroy> rx_;
  Owned<dp_tlm_t, dp_tlm_destroy> tlm_;
  Owned<dp_tlm_capture_t, dp_tlm_capture_destroy> cap_;
  dp_sample_clock_t clock_{};
  int proc_id_ = -1;
  std::unique_ptr<Scene> scene_;
  std::vector<cf32> x_, out_, syms_;
  size_t rx_syms_ = 0; // symbols the receiver has emitted
  std::vector<uint8_t> flags_, code_flags_; // per symbol: symbol, code lock
  std::vector<size_t> emitted_;
  async_dsss_receiver_status_t st_{};
  uint64_t n_ = 0, total_ns_ = 0;
  double last_ns_ = 0.0, worst_ns_ = 0.0;
  bool refused_ = false; // a call did not take its chunk: the run stopped there
  void print_errors (const dp_ber_meter_state_t *meter, size_t lo) const;
  int64_t window_start () const;
};

} // namespace ccsds415
