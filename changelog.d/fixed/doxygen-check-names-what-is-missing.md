- **`make doxygen-check` and `make gen-c-api` say what is missing.** With
    no matching local doxygen, both run CI's doxygen through docker, and any
    failure of that step was reported as "the CI image ships doxygen , not
    1.9.8 ... CI has moved", telling the reader to bump a correct pin. A
    missing docker, an image that will not run, and an image that reports no
    version are now each named; "CI has moved" is said only when the image
    ran and reported another version
    ([#2362](https://github.com/doppler-dsp/doppler/issues/2362)).
