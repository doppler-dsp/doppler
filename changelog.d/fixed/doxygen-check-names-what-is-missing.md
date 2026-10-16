- **`make doxygen-check` says what is missing.** With no matching local
    doxygen, the check runs CI's doxygen through docker, and any failure of
    that step was reported as "the CI image ships doxygen , not 1.9.8 ... CI
    has moved", telling the reader to bump a correct pin. A missing docker
    and an image that will not run are now each named; "CI has moved" is
    said only when the image ran and reported another version
    ([#2362](https://github.com/doppler-dsp/doppler/issues/2362)).
