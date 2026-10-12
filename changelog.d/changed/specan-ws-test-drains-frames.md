- **The specan web server tests now run in CI, and read frames the way a browser does.**
    The burst test no longer sits out a 10 s websocket close timeout, so it takes about
    2 s instead of 12, and it asserts that the close completed
    ([#2159](https://github.com/doppler-dsp/doppler/issues/2159)).
