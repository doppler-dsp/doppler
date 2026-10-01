- **A scene refuses a key it does not take, by name and place**
    ([#1153](https://github.com/doppler-dsp/doppler/issues/1153)).
    Every level used to drop unknown keys in silence. A top-level `"fs"` left
    every segment at fs = 1, and `--realtime` then paced a 7 ms scene for
    two hours.
