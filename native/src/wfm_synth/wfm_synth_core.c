#include "wfm_synth/wfm_synth_core.h"

wfm_synth_state_t *
wfm_synth_create (int type, double fs, double freq, double snr, int snr_mode,
                  uint32_t seed, int sps, int pn_length, uint64_t pn_poly,
                  int lfsr, double f_end)
{
  wfm_synth_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  obj->wtype   = type;
  obj->nsps    = (sps < 1) ? 1 : sps;
  obj->sym_pos = 0;
  obj->cur_re
      = (type == WFM_SYNTH_TONE || type == WFM_SYNTH_CHIRP) ? 1.0f : 0.0f;
  obj->cur_im = 0.0f;

  /* Chirp: store the normalised start/end frequencies (freq is the start, the
   * instantaneous frequency at t=0). The per-sample slope chirp_k locks once
   * the sweep span is known — from wfm_synth_set_chirp_span() (composer/CLI,
   * span = the segment length) or the first wfm_synth_steps() call. */
  obj->chirp_f0   = (fs != 0.0) ? freq / fs : 0.0;
  obj->chirp_fend = (fs != 0.0) ? f_end / fs : 0.0;
  obj->chirp_k    = 0.0;
  obj->chirp_ph   = 0.0;
  obj->chirp_n    = 0;
  obj->chirp_span = 0;
  /* Bit-pattern state attached later via wfm_synth_set_bits(). */
  obj->bits    = NULL;
  obj->n_bits  = 0;
  obj->bit_idx = 0;
  obj->bit_mod = 1; /* default bpsk */

  /* LO carrier only when there is a frequency offset: at freq 0 the carrier
   * is the constant 1, so mixing is a no-op and is skipped entirely (a
   * baseband waveform pays no NCO cost). Pure noise never has an LO; a chirp
   * synthesises its own swept carrier rather than a static LO. */
  if (type != WFM_SYNTH_NOISE && type != WFM_SYNTH_CHIRP && freq != 0.0)
    {
      obj->lo = lo_create (fs != 0.0 ? freq / fs : 0.0);
      if (!obj->lo)
        {
          free (obj);
          return NULL;
        }
    }

  /* PN chip/data source for pn/bpsk/qpsk; poly 0 → MLS poly for the length */
  if (type >= WFM_SYNTH_PN && type <= WFM_SYNTH_QPSK)
    {
      uint64_t poly
          = pn_poly ? pn_poly : wfm_synth_mls_poly ((uint32_t)pn_length);
      if (poly == 0)
        { /* no MLS table entry for this length */
          if (obj->lo)
            lo_destroy (obj->lo);
          free (obj);
          return NULL;
        }
      obj->pn = pn_create (poly, seed ? seed : 1u, (uint32_t)pn_length, lfsr);
      if (!obj->pn)
        {
          if (obj->lo)
            lo_destroy (obj->lo);
          free (obj);
          return NULL;
        }
    }

  /* AWGN at the resolved SNR. snr_mode: 0 auto, 1 fs, 2 ebno, 3 esno.
   * Noise is generated only when requested: type=noise always; otherwise only
   * when snr < WFM_SYNTH_SNR_CLEAN (so a clean waveform skips AWGN entirely).
   */
  int want_noise = (type == WFM_SYNTH_NOISE) || (snr < WFM_SYNTH_SNR_CLEAN);
  if (type == WFM_SYNTH_NOISE)
    {
      obj->awgn
          = awgn_create ((uint64_t)seed, (float)(1.0 / 1.4142135623730951));
    }
  else if (snr < WFM_SYNTH_SNR_CLEAN)
    {
      int mode = snr_mode;
      if (mode == 0)
        mode = (type == WFM_SYNTH_BPSK || type == WFM_SYNTH_QPSK)
                   ? 3
                   : 1; /* *psk → esno; tone/pn/chirp/bits → fs */
      int    bps = (type == WFM_SYNTH_QPSK) ? 2 : 1;
      double snr_fs;
      if (mode == 2) /* Eb/No → SNR over fs */
        snr_fs = snr + 10.0 * log10 ((double)bps)
                 - 10.0 * log10 ((double)obj->nsps);
      else if (mode == 3) /* Es/No → SNR over fs (Es spans nsps samples) */
        snr_fs = snr - 10.0 * log10 ((double)obj->nsps);
      else /* over fs */
        snr_fs = snr;
      float amp = sqrtf (1.0f / (2.0f * powf (10.0f, (float)snr_fs / 10.0f)));
      obj->awgn = awgn_create ((uint64_t)seed, amp);
    }
  if (want_noise && !obj->awgn)
    {
      if (obj->pn)
        pn_destroy (obj->pn);
      if (obj->lo)
        lo_destroy (obj->lo);
      free (obj);
      return NULL;
    }
  return obj;
}

int
wfm_synth_set_bits (wfm_synth_state_t *state, const uint8_t *bits, size_t n,
                    int modulation)
{
  if (state->wtype != WFM_SYNTH_BITS)
    return 0; /* no-op for every other type */
  if (!bits || n == 0 || modulation < 0 || modulation > 2)
    return -1;
  uint8_t *copy = malloc (n);
  if (!copy)
    return -1;
  for (size_t i = 0; i < n; i++)
    copy[i] = bits[i] ? 1u : 0u; /* normalise to 0/1 */
  free (state->bits);
  state->bits    = copy;
  state->n_bits  = n;
  state->bit_idx = 0;
  state->bit_mod = modulation;
  return 0;
}

void
wfm_synth_destroy (wfm_synth_state_t *state)
{
  free (state->bits);
  if (state->lo)
    lo_destroy (state->lo);
  if (state->awgn)
    awgn_destroy (state->awgn);
  if (state->pn)
    pn_destroy (state->pn);
  free (state);
}

void
wfm_synth_set_chirp_span (wfm_synth_state_t *state, size_t span)
{
  /* The slope locks on the first valid pin (span still 0); later calls and
   * non-chirp synths are no-ops, so the composer can call this for every
   * source unconditionally. */
  if (state->wtype == WFM_SYNTH_CHIRP && state->chirp_span == 0 && span > 0)
    {
      state->chirp_span = span;
      state->chirp_k    = (state->chirp_fend - state->chirp_f0) / (double)span;
    }
}

void
wfm_synth_reset (wfm_synth_state_t *state)
{
  state->sym_pos = 0;
  state->cur_re
      = (state->wtype == WFM_SYNTH_TONE || state->wtype == WFM_SYNTH_CHIRP)
            ? 1.0f
            : 0.0f;
  state->cur_im   = 0.0f;
  state->bit_idx  = 0;   /* rewind the bit pattern */
  state->chirp_ph = 0.0; /* rewind the sweep; span/slope stay locked */
  state->chirp_n  = 0;
  if (state->lo)
    lo_reset (state->lo);
  if (state->awgn)
    awgn_reset (state->awgn);
  if (state->pn)
    pn_reset (state->pn);
}

void
wfm_synth_steps (wfm_synth_state_t *state, float complex *output, size_t n)
{
  /* Fully batched, no per-sample library calls. Per chunk: the LO carrier
   * (NCO) and AWGN are generated a block at a time (their vectorized paths);
   * PN chips come from the block pn_generate() (kind-hoisted, ~1 GSa/s),
   * never per-sample pn_step(); the symbol map and the LO-mix / noise-add are
   * separate, data-parallel passes. This rounds the symbol*carrier multiply
   * and the noise-add separately, whereas wfm_synth_step() evaluates
   * `sym*carrier + noise` as one expression — so under -ffast-math the two can
   * differ by an ULP on FMA targets (arm64) for QPSK's irrational ±1/√2 leg.
   * Callers that need the composer/CLI to agree byte-for-byte must drive a
   * single path; wfm_compose uses wfm_synth_steps() for exactly this reason.
   */
  enum
  {
    CH = 2048
  }; /* <= LO_MAX_OUT */
  float complex carrier[CH];   /* 16 KiB */
  float complex noise[CH];     /* 16 KiB */
  uint8_t       chips[2 * CH]; /* up to 2 chips/sample (qpsk at sps 1) */
  const int     has_lo   = state->lo != NULL;
  const int     has_awgn = state->awgn != NULL;
  const int     is_bits  = state->wtype == WFM_SYNTH_BITS;
  const int     is_chirp = state->wtype == WFM_SYNTH_CHIRP;
  const int     modulated
      = state->wtype >= WFM_SYNTH_PN && state->wtype <= WFM_SYNTH_QPSK;
  const int   qpsk    = state->wtype == WFM_SYNTH_QPSK;
  const int   nsps    = state->nsps;
  const float s       = 0.70710678118654752f; /* 1/sqrt(2) — QPSK leg */
  int         sym_pos = state->sym_pos;
  size_t      bit_idx = state->bit_idx; /* bits read position (type=bits) */
  float       cre = state->cur_re, cim = state->cur_im;

  /* A standalone chirp that was never pinned takes its sweep span from this
   * first block (so chirp(...).steps(N) sweeps f_start→f_end over exactly N).
   * The composer/CLI pin the span to the segment length beforehand, so this
   * no-ops there. */
  if (is_chirp && state->chirp_span == 0 && n > 0)
    wfm_synth_set_chirp_span (state, n);

  for (size_t done = 0; done < n;)
    {
      size_t         m   = (n - done < (size_t)CH) ? (n - done) : (size_t)CH;
      float complex *out = output + done;
      if (has_lo)
        lo_steps (state->lo, m, carrier);
      else if (is_chirp)
        /* Swept carrier, generated per sample with the same phase recurrence
         * as wfm_synth_step() — identical doubles, identical cexpf, so the
         * per-sample and block paths stay byte-identical. */
        for (size_t i = 0; i < m; i++)
          {
            double nf
                = (state->chirp_span && state->chirp_n >= state->chirp_span)
                      ? (double)state->chirp_span
                      : (double)state->chirp_n;
            double w = state->chirp_f0 + state->chirp_k * nf;
            carrier[i]
                = cexpf ((float)(6.283185307179586 * state->chirp_ph) * I);
            state->chirp_ph += w;
            state->chirp_ph -= floor (state->chirp_ph);
            state->chirp_n++;
          }
      if (has_awgn)
        awgn_generate (state->awgn, m, noise);

      if (is_bits)
        {
          /* User bit pattern: per-sample symbol latch from bits[], cycled,
           * with the *same* fused sym*carrier + noise as wfm_synth_step() so
           * the two paths stay byte-identical. */
          const uint8_t *bits = state->bits;
          size_t         nb   = state->n_bits;
          int            bmod = state->bit_mod;
          for (size_t i = 0; i < m; i++)
            {
              if (sym_pos == 0 && bits && nb)
                {
                  if (bmod == 2)
                    {
                      uint8_t b0 = bits[bit_idx];
                      uint8_t b1 = bits[(bit_idx + 1) % nb];
                      cre        = b0 ? -s : s;
                      cim        = b1 ? -s : s;
                      bit_idx    = (bit_idx + 2) % nb;
                    }
                  else if (bmod == 1)
                    {
                      cre     = bits[bit_idx] ? -1.0f : 1.0f;
                      cim     = 0.0f;
                      bit_idx = (bit_idx + 1) % nb;
                    }
                  else
                    {
                      cre     = bits[bit_idx] ? 1.0f : 0.0f;
                      cim     = 0.0f;
                      bit_idx = (bit_idx + 1) % nb;
                    }
                }
              if (++sym_pos >= nsps)
                sym_pos = 0;
              float complex sym = cre + cim * I;
              float complex v   = has_lo ? sym * carrier[i] : sym;
              out[i]            = has_awgn ? v + noise[i] : v;
            }
          done += m;
          continue;
        }

      if (!modulated)
        {
          /* Constant symbol → one fused, fully data-parallel pass. A chirp
           * reuses this path with sym=1 and the swept carrier above, so its
           * fused `sym*carrier + noise` matches the tone path exactly. */
          const int     use_carrier = has_lo || is_chirp;
          float complex sym         = cre + cim * I;
          if (use_carrier && has_awgn)
            for (size_t i = 0; i < m; i++)
              out[i] = sym * carrier[i] + noise[i];
          else if (use_carrier)
            for (size_t i = 0; i < m; i++)
              out[i] = sym * carrier[i];
          else if (has_awgn)
            for (size_t i = 0; i < m; i++)
              out[i] = sym + noise[i];
          else
            for (size_t i = 0; i < m; i++)
              out[i] = sym;
          done += m;
          continue;
        }

      /* Modulated: pre-generate exactly the chips this block consumes, in
       * order — one (pn/bpsk) or two (qpsk) per symbol boundary. */
      const int bps   = qpsk ? 2 : 1;
      size_t    first = (sym_pos == 0) ? 0 : (size_t)(nsps - sym_pos);
      size_t    nb    = (first < m) ? 1 + (m - 1 - first) / (size_t)nsps : 0;
      if (nb)
        pn_generate (state->pn, nb * (size_t)bps, chips);

      if (nsps == 1)
        {
          /* Every sample is a fresh chip — branch-free, vectorizable map. */
          if (qpsk)
            for (size_t i = 0; i < m; i++)
              out[i]
                  = (chips[2 * i] ? -s : s) + (chips[2 * i + 1] ? -s : s) * I;
          else
            for (size_t i = 0; i < m; i++)
              out[i] = chips[i] ? -1.0f : 1.0f;
          cre = crealf (out[m - 1]); /* carry last symbol (pre LO/noise) */
          cim = cimagf (out[m - 1]);
          /* sym_pos remains 0 when nsps == 1 */
          if (has_lo)
            for (size_t i = 0; i < m; i++)
              out[i] *= carrier[i];
          if (has_awgn)
            for (size_t i = 0; i < m; i++)
              out[i] += noise[i];
        }
      else
        {
          /* sps-hold: sequential symbol timing, but LO/noise fused so the
           * chunk is touched once. */
          size_t ci = 0;
          for (size_t i = 0; i < m; i++)
            {
              if (sym_pos == 0)
                {
                  if (qpsk)
                    {
                      uint8_t b0 = chips[ci++], b1 = chips[ci++];
                      cre = b0 ? -s : s;
                      cim = b1 ? -s : s;
                    }
                  else
                    {
                      cre = chips[ci++] ? -1.0f : 1.0f;
                      cim = 0.0f;
                    }
                }
              if (++sym_pos >= nsps)
                sym_pos = 0;
              float complex sym = cre + cim * I;
              float complex v   = has_lo ? sym * carrier[i] : sym;
              out[i]            = has_awgn ? v + noise[i] : v;
            }
        }
      done += m;
    }
  state->sym_pos = sym_pos;
  state->cur_re  = cre;
  state->cur_im  = cim;
  state->bit_idx = bit_idx;
}

int
wfm_synth_get_wtype (const wfm_synth_state_t *state)
{
  return state->wtype;
}

void
wfm_synth_set_wtype (wfm_synth_state_t *state, int val)
{
  state->wtype = val;
}

int
wfm_synth_get_nsps (const wfm_synth_state_t *state)
{
  return state->nsps;
}

void
wfm_synth_set_nsps (wfm_synth_state_t *state, int val)
{
  state->nsps = val;
}

int
wfm_synth_get_sym_pos (const wfm_synth_state_t *state)
{
  return state->sym_pos;
}

void
wfm_synth_set_sym_pos (wfm_synth_state_t *state, int val)
{
  state->sym_pos = val;
}

float
wfm_synth_get_cur_re (const wfm_synth_state_t *state)
{
  return state->cur_re;
}

void
wfm_synth_set_cur_re (wfm_synth_state_t *state, float val)
{
  state->cur_re = val;
}

float
wfm_synth_get_cur_im (const wfm_synth_state_t *state)
{
  return state->cur_im;
}

void
wfm_synth_set_cur_im (wfm_synth_state_t *state, float val)
{
  state->cur_im = val;
}
