#include "synth/synth_core.h"

synth_state_t *
synth_create (int type, double fs, double freq, double snr, int snr_mode,
              uint32_t seed, int sps, int pn_length, uint64_t pn_poly,
              int lfsr)
{
  synth_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  obj->wtype   = type;
  obj->nsps    = (sps < 1) ? 1 : sps;
  obj->sym_pos = 0;
  obj->cur_re  = (type == SYNTH_TONE) ? 1.0f : 0.0f;
  obj->cur_im  = 0.0f;

  /* LO carrier only when there is a frequency offset: at freq 0 the carrier
   * is the constant 1, so mixing is a no-op and is skipped entirely (a
   * baseband waveform pays no NCO cost). Pure noise never has an LO. */
  if (type != SYNTH_NOISE && freq != 0.0)
    {
      obj->lo = lo_create (fs != 0.0 ? freq / fs : 0.0);
      if (!obj->lo)
        {
          free (obj);
          return NULL;
        }
    }

  /* PN chip/data source for pn/bpsk/qpsk; poly 0 → MLS poly for the length */
  if (type >= SYNTH_PN)
    {
      uint64_t poly = pn_poly ? pn_poly : synth_mls_poly ((uint32_t)pn_length);
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
   * when snr < SYNTH_SNR_CLEAN (so a clean waveform skips AWGN entirely). */
  int want_noise = (type == SYNTH_NOISE) || (snr < SYNTH_SNR_CLEAN);
  if (type == SYNTH_NOISE)
    {
      obj->awgn
          = awgn_create ((uint64_t)seed, (float)(1.0 / 1.4142135623730951));
    }
  else if (snr < SYNTH_SNR_CLEAN)
    {
      int mode = snr_mode;
      if (mode == 0)
        mode = (type >= SYNTH_BPSK) ? 3 : 1; /* *psk → esno, tone/pn → fs */
      int    bps = (type == SYNTH_QPSK) ? 2 : 1;
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

void
synth_destroy (synth_state_t *state)
{
  if (state->lo)
    lo_destroy (state->lo);
  if (state->awgn)
    awgn_destroy (state->awgn);
  if (state->pn)
    pn_destroy (state->pn);
  free (state);
}

void
synth_reset (synth_state_t *state)
{
  state->sym_pos = 0;
  state->cur_re  = (state->wtype == SYNTH_TONE) ? 1.0f : 0.0f;
  state->cur_im  = 0.0f;
  if (state->lo)
    lo_reset (state->lo);
  if (state->awgn)
    awgn_reset (state->awgn);
  if (state->pn)
    pn_reset (state->pn);
}

void
synth_steps (synth_state_t *state, float complex *output, size_t n)
{
  /* Fully batched, no per-sample library calls. Per chunk: the LO carrier
   * (NCO) and AWGN are generated a block at a time (their vectorized paths);
   * PN chips come from the block pn_generate() (kind-hoisted, ~1 GSa/s),
   * never per-sample pn_step(); the symbol map and the LO-mix / noise-add are
   * separate, data-parallel passes. Output is byte-identical to a per-sample
   * synth_step() loop. */
  enum
  {
    CH = 2048
  }; /* <= LO_MAX_OUT */
  float complex carrier[CH];   /* 16 KiB */
  float complex noise[CH];     /* 16 KiB */
  uint8_t       chips[2 * CH]; /* up to 2 chips/sample (qpsk at sps 1) */
  const int     has_lo    = state->lo != NULL;
  const int     has_awgn  = state->awgn != NULL;
  const int     modulated = state->wtype >= SYNTH_PN;
  const int     qpsk      = state->wtype == SYNTH_QPSK;
  const int     nsps      = state->nsps;
  const float   s         = 0.70710678118654752f; /* 1/sqrt(2) — QPSK leg */
  int           sym_pos   = state->sym_pos;
  float         cre = state->cur_re, cim = state->cur_im;

  for (size_t done = 0; done < n;)
    {
      size_t         m   = (n - done < (size_t)CH) ? (n - done) : (size_t)CH;
      float complex *out = output + done;
      if (has_lo)
        lo_steps (state->lo, m, carrier);
      if (has_awgn)
        awgn_generate (state->awgn, m, noise);

      if (!modulated)
        {
          /* Constant symbol → one fused, fully data-parallel pass. */
          float complex sym = cre + cim * I;
          if (has_lo && has_awgn)
            for (size_t i = 0; i < m; i++)
              out[i] = sym * carrier[i] + noise[i];
          else if (has_lo)
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
}

int
synth_get_wtype (const synth_state_t *state)
{
  return state->wtype;
}

void
synth_set_wtype (synth_state_t *state, int val)
{
  state->wtype = val;
}

int
synth_get_nsps (const synth_state_t *state)
{
  return state->nsps;
}

void
synth_set_nsps (synth_state_t *state, int val)
{
  state->nsps = val;
}

int
synth_get_sym_pos (const synth_state_t *state)
{
  return state->sym_pos;
}

void
synth_set_sym_pos (synth_state_t *state, int val)
{
  state->sym_pos = val;
}

float
synth_get_cur_re (const synth_state_t *state)
{
  return state->cur_re;
}

void
synth_set_cur_re (synth_state_t *state, float val)
{
  state->cur_re = val;
}

float
synth_get_cur_im (const synth_state_t *state)
{
  return state->cur_im;
}

void
synth_set_cur_im (synth_state_t *state, float val)
{
  state->cur_im = val;
}
