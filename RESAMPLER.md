# Continuously Variable Resampler

- Any rate change: from imperceptible to several orders of magnitude
- 32-bit resolution


# Architecture

## Interpolator  (r = Fout/Fin ≥ 1, output-driven)


## Decimator  (r = Fout/Fin < 1, input-driven, transposed form)

- Every input tick: scalar x[n] × all N branch coefficients
- Each product (one for each coefficient) goes to integrate and dump
- The N integrate and dump object continue to integrate input sample coefficient products
- Upon NCO overflow the dump signal is asserted and all N I&D output the accumulated value and reset
- I&D outputs (now at the output rate) are input to a transposed tapped delay line
- The delay line is shifted producing an output sample and the process continues

id[k+N-1]    id[k+N-2]       id[k-1]            id[k]
    |   _____    |              |     _____      |
    --->| T |--> + --> ... ---> + --> | T | ---> + ---> y[k]
        |___|                         |___|
# Testing

- Python reference implementation made with native components
- Reference validation
    - Passband flatness
    - Stopband / alias attenuation


## Interpolator Test Procedure

- Purpose: validate passband flatness and image / artifact suppression.
- Rate change = Fout/Fin = r = 2.0333
- Method: two complex tones at 0.1·Fin and 0.4·Fin, both of which
  should appear in the output unmolested
- Measure
    - Frequency of each tone: f/r ± measurement error
    - Amplitude: ±0.1 dB
    - Relative level of largest non-tone peak: ≤ −60 dBc


## Decimator Test Procedure

- Purpose: validate passband flatness and alias rejection.
- Rate change = Fout/Fin = r = 0.50333
- Method: two complex tones at 0.4·Fout and 0.6·Fout — tone 1 lands
  in the passband; tone 2 is above Nyquist_out and must be rejected
  by the anti-alias filter before it can fold back
- Measure
    - Frequency of tone 1: 0.4 ± measurement error
    - Amplitude of tone 1: ±0.1 dB
    - Relative level of largest non-tone peak: ≤ −60 dBc
