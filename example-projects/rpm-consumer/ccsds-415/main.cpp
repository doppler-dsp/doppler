// main.cpp -- ccsds415: the async DSSS receiver at the CCSDS 415.1-B-1
// point, from C++, against doppler's installed RPM. The batch front end.
//
// One run per carrier (2.2 and 2.8 GHz, the point's band edges), each:
//
//   1. the point's scene built from installed objects: a 1,023-chip Gold
//      code spreading a PN15 payload at 3,000 symbols a second, the chip
//      rate the carrier sets, resampled to the 11 MSa/s capture, a Doppler
//      that ramps at R_env through zero, and noise at the stated Es/N0;
//   2. streamed a chunk at a time into the receiver the library builds at
//      the point (Ccsds415Receiver's C constructor), unpaced;
//   3. every chunk timed, and every telemetry probe the receiver has, plus
//      that timing, captured losslessly to one file;
//   4. checked: the receiver locked within the point's allocation, decoded
//      the payload, dropped no symbol, and the capture is complete.
//
// The loop is run.cpp's, shared with ccsds415-tui. plot.py reads what this
// writes; it only plots. The point's numbers are the library's
// (doppler/ccsds/ccsds_415.h): nothing here restates one.
#include "run.hpp"

#include <cstdio>

int
main (int argc, char **argv)
{
  ccsds415::Options o;
  ccsds415::parse (argc, argv, o, false);
  bool ok = true;
  for (const double fc : o.carriers)
    {
      ccsds415::Run run (o, fc);
      while (run.step ())
        {
        }
      ok = ccsds415::report (run.name (), run.finish (), o.chunk) && ok;
    }
  if (!ok)
    return 1;
  std::printf ("OK\n");
  return 0;
}
