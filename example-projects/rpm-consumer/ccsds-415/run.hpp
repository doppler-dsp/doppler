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

// The smallest chunk a run takes: see the README, "Chunks, and why at
// least 25 samples".
constexpr size_t MIN_CHUNK = 25;
// The sample period, ns: the cost every chunk is held against.
double sample_period_ns ();

// What a run is. The defaults are the batch binary's; the TUI changes the
// scene's length.
struct Options
{
  std::vector<double> carriers;
  double esn0_db = 10.0; // 6 dB above the point's Es/N0 4 dB
  double seconds = 2.5;  // signal after the lead-in
  size_t chunk = 4096;   // samples a push into the receiver
  std::string out = "."; // directory the captures go to
  bool paced = false;    // the TUI's: hold each chunk to real time
  bool snapshot = false; // the TUI's: no terminal, one frame at the end
  bool errors = false;   // print where in the scored stream errors fall
};

// Parse argv into o (whose fields hold the defaults); exits on --help or a
// bad option. `tui` adds the TUI's own options to the usage.
void parse (int argc, char **argv, Options &o, bool tui);

// A failed check: the reason on stderr, exit 1.
[[noreturn]] void fail (const std::string &why);
void check (bool ok, const std::string &why);

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
};

// The summary lines for one carrier, and whether its checks held.
bool report (const std::string &name, const Result &r, size_t chunk);

class Scene;

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
  // capture, every probe emitted, a sustained lock, the payload found).
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

private:
  const Options &o_;
  double fc_;
  std::string name_;
  std::vector<uint8_t> code_, payload_;
  Owned<dp_async_dsss_receiver_state_t, dp_async_dsss_receiver_destroy> rx_;
  Owned<dp_tlm_t, dp_tlm_destroy> tlm_;
  Owned<dp_tlm_capture_t, dp_tlm_capture_destroy> cap_;
  dp_sample_clock_t clock_{};
  int proc_id_ = -1;
  std::unique_ptr<Scene> scene_;
  std::vector<cf32> x_, out_, syms_;
  std::vector<uint8_t> flags_;
  std::vector<size_t> emitted_;
  async_dsss_receiver_status_t st_{};
  uint64_t n_ = 0, total_ns_ = 0;
  double last_ns_ = 0.0, worst_ns_ = 0.0;
  bool filled_ = false; // a call filled its capacity: the run stopped there
  void print_errors (const dp_ber_meter_state_t *meter, size_t lo) const;
};

} // namespace ccsds415
