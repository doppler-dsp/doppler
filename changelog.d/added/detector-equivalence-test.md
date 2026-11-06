- **A C test that `CorrDetector` is `CorrDetector2D` at one row, hit for
    hit** (`test_detector_equiv`, #2752 step 3.3). It pushes the same
    streams through both, over the four noise modes, dwells 1 and 2, three
    lengths, two noise windows, two thresholds and odd chunks, and compares
    every hit, every `consumed`, the stored last dump and the held
    correlation, bit for bit, ties included. Step 3.3 rests on it to make
    both detectors flavors of the one correlator.
