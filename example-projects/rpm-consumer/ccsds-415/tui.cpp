// tui.cpp -- ccsds415-tui: the same run as ccsds415, drawn as it goes.
//
// A second front end over run.cpp's loop, not a second receiver: the scene,
// the receiver, the capture and the checks are the batch binary's. What it
// adds is a terminal display (FTXUI), refreshed at most every 100 ms of
// wall time:
//
//   - cost: the last chunk's processing time per sample and the run's,
//     against the sample period, and the display's own time per refresh,
//     shown apart so it is plainly not what is being measured;
//   - receiver: its state, both lock flags, the symbol-lock metric against
//     its threshold, C/N0, the Doppler and the code rate, as status()
//     reports them: nothing here computes a measurement of its own;
//   - progress through the scene.
//
// One thread. The timed span in run.cpp is the receiver call alone; the
// pacing sleep and the drawing both fall outside it, and the display never
// holds the receiver up: Loop::RunOnce() draws what is pending and returns.
// By default each chunk is held to real time, so the 30 s scene takes
// 30 s and lock can be watched happening; --unpaced runs it as fast as it
// goes. q quits early, through the same teardown as the scene's end: the
// capture is closed and the checks are reported.
//
// --snapshot needs no terminal: it runs the scene unpaced, then prints the
// last frame as text, so CI can read every panel.
#include "run.hpp"

#include <doppler/timing/timing_core.h>

#include <ftxui/component/component.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <chrono>
#include <unistd.h>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{

using namespace ftxui;

std::string
fmt (const char *f, double v)
{
  char b[48];
  std::snprintf (b, sizeof b, f, v);
  return b;
}

const char *
state_name (int s)
{
  switch (s)
    {
    case ASYNC_DSSS_RX_SEARCHING:
      return "searching";
    case ASYNC_DSSS_RX_REFINING:
      return "refining";
    case ASYNC_DSSS_RX_TRACKING:
      return "tracking";
    case ASYNC_DSSS_RX_IDLE:
      return "idle";
    case ASYNC_DSSS_RX_LOST:
      return "lost";
    default:
      return "?";
    }
}

// One labelled value: the label in a fixed column, the value after it.
Element
row (const std::string &label, const std::string &value)
{
  return hbox ({ text (label) | size (WIDTH, EQUAL, 32), text (value) });
}

// The whole display for one run, from what the run and the receiver report.
Element
document (const ccsds415::Run &run, double refresh_ms, bool paced,
          bool held = false)
{
  const async_dsss_receiver_status_t &st = run.status ();
  const double ts = ccsds415::sample_period_ns ();
  const double last = run.last_proc_ns (), mean = run.mean_proc_ns ();
  const double done = run.elapsed_s () / run.length_s ();
  const Element cost = window (
      text ("cost"),
      vbox ({
          // Paced, each chunk starts with a cache the sleep let go cold, so
          // the figure reads higher than unpaced: the label says which.
          row (paced ? "processing time per sample (paced)"
                     : "processing time per sample",
               fmt ("%.1f ns", last)),
          row ("  over the run so far", fmt ("%.1f ns", mean)),
          row ("sample period", fmt ("%.1f ns", ts)),
          row ("share of the sample period", fmt ("%.1f %%", 100.0 * mean / ts)),
          row ("times faster than real time",
               fmt ("%.1f", mean > 0.0 ? ts / mean : 0.0)),
          separator (),
          row ("display time per refresh", fmt ("%.2f ms", refresh_ms)),
      }));
  const Element receiver = window (
      text ("receiver"),
      vbox ({
          row ("state", state_name (st.state)),
          row ("code lock", st.code_locked ? "up" : "down"),
          row ("symbol lock", st.locked ? "up" : "down"),
          row ("lock metric", fmt ("%.3f", st.lock_metric)),
          row ("  threshold", fmt ("%.3f", st.lock_threshold)),
          row ("C/N0", fmt ("%.1f dB-Hz", st.cn0_dbhz_est)),
          row ("Doppler", fmt ("%.2f Hz", st.doppler_hz)),
          // The received code rate over the nominal: 1 is no Doppler.
          row ("code rate (received / nominal)", fmt ("%.9f", st.code_rate)),
      }));
  const Element progress = window (
      text ("scene"),
      vbox ({
          row ("carrier", fmt ("%.1f GHz", run.carrier_hz () / 1e9)),
          row ("time", fmt ("%.2f s", run.elapsed_s ())),
          row ("symbols", fmt ("%.0f", static_cast<double> (run.symbols ()))),
          hbox ({ text ("progress ") | size (WIDTH, EQUAL, 32),
                  gauge (static_cast<float> (done))
                      | size (WIDTH, EQUAL, 30) }),
      }));
  Elements parts{ text (run.name ()) | bold, hbox ({ cost, receiver }),
                  progress };
  if (held)
    parts.push_back (text ("the scene has ended: press q to exit"));
  return vbox (std::move (parts));
}

} // namespace

int
main (int argc, char **argv)
{
  ccsds415::Options o;
  o.seconds = 30.0; // long enough to watch: lock, then the Doppler ramp
  o.paced = true;
  ccsds415::parse (argc, argv, o, true);

  std::vector<std::pair<std::string, ccsds415::Result> > results;
  if (o.snapshot)
    {
      for (const double fc : o.carriers)
        {
          ccsds415::Run run (o, fc);
          while (run.step ())
            {
            }
          // The display's cost here is one render, timed, then drawn
          // again carrying that time.
          Element doc = document (run, 0.0, false);
          Screen screen
              = Screen::Create (Dimension::Fixed (100), Dimension::Fit (doc));
          const uint64_t t0 = dp_mono_ns ();
          Render (screen, doc);
          const double render_ms
              = static_cast<double> (dp_mono_ns () - t0) / 1e6;
          doc = document (run, render_ms, false);
          screen.Clear ();
          Render (screen, doc);
          std::printf ("%s\n", screen.ToString ().c_str ());
          results.emplace_back (run.name (), run.finish ());
        }
    }
  else
    {
      // The runs outlive the display: a failed check exits the program,
      // and it must not do so while the terminal is in the display's mode.
      std::vector<std::unique_ptr<ccsds415::Run> > runs;
      bool stopped = false; // q came before the last run's scene ended
      auto screen = ScreenInteractive::Fullscreen ();
      ccsds415::Run *cur = nullptr;
      double refresh_ms = 0.0;
      bool quit = false;
      bool held = false; // every scene ended: the last frame stays up
      auto view = Renderer ([&] {
        return cur ? document (*cur, refresh_ms, o.paced, held)
                   : text ("starting");
      });
      auto app = CatchEvent (view, [&] (Event e) {
        if (e == Event::Character ('q'))
          {
            quit = true;
            return true;
          }
        return false;
      });
      {
        Loop loop (&screen, app);
        for (const double fc : o.carriers)
          {
            if (quit)
              break;
            runs.push_back (std::make_unique<ccsds415::Run> (o, fc));
            ccsds415::Run &run = *runs.back ();
            cur = &run;
            const uint64_t start = dp_mono_ns ();
            uint64_t last_draw = 0;
            bool more = true;
            while (!quit && (more = run.step ()))
              {
                // Pacing and drawing are outside the receiver's timed span.
                if (o.paced)
                  {
                    const uint64_t due
                        = start
                          + static_cast<uint64_t> (run.elapsed_s () * 1e9);
                    const uint64_t now = dp_mono_ns ();
                    if (due > now)
                      std::this_thread::sleep_for (
                          std::chrono::nanoseconds (due - now));
                  }
                const uint64_t now = dp_mono_ns ();
                if (now - last_draw >= 100000000u)
                  {
                    screen.PostEvent (Event::Custom); // same thread: a redraw
                    const uint64_t t0 = dp_mono_ns ();
                    loop.RunOnce ();
                    refresh_ms = static_cast<double> (dp_mono_ns () - t0) / 1e6;
                    last_draw = now;
                  }
              }
            stopped = more && quit;
          }
        // Every scene ended: keep the last frame on screen, drawn once more
        // with the hold line, until q.
        // Only on a terminal: with stdin not one, nothing can send the q.
        if (!quit && cur && isatty (STDIN_FILENO))
          {
            held = true;
            screen.PostEvent (Event::Custom);
            while (!quit)
              {
                loop.RunOnce ();
                std::this_thread::sleep_for (std::chrono::milliseconds (50));
              }
          }
        cur = nullptr;
      } // the Loop restores the terminal here, before any check can exit
      // A run the user stopped answers only the partial checks; the runs
      // before it finished their scenes and answer the full ones.
      for (size_t i = 0; i < runs.size (); i++)
        if (stopped && i + 1 == runs.size ())
          {
            if (!runs[i]->finish_partial ())
              return 1;
          }
        else
          results.emplace_back (runs[i]->name (), runs[i]->finish ());
      if (stopped)
        {
          bool ok = true;
          for (const auto &[name, r] : results)
            ok = ccsds415::report (name, r, o.chunk) && ok;
          return ok ? 0 : 1;
        }
    }

  bool ok = !results.empty ();
  for (const auto &[name, r] : results)
    ok = ccsds415::report (name, r, o.chunk) && ok;
  if (!ok)
    return 1;
  std::printf ("OK\n");
  return 0;
}
