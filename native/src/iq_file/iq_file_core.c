#include "iq_file/iq_file_core.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "wfm/wfm_sample.h"

/*
 * iq_file_core.c — headerless interleaved-I/Q file reader.
 *
 * The C-first replacement for the hand-Python readback.read_iq(): open an
 * explicit-type, headerless capture (raw wfmgen --file_type raw, or a BLUE
 * .det data file), decode the interleaved wire bytes to unit-scale complex64
 * via the shared wfm_convert_pair(), and report the sample count from the file
 * size. Unlike wfm_reader_open() this does NOT auto-detect BLUE/SigMF/CSV —
 * the caller states the sample_type and endian, exactly like read_iq's
 * contract.
 */

iq_file_state_t *
iq_file_create (const char *filepath, int sample_type, int endian)
{
  if (!filepath)
    return NULL;
  size_t bps = wfm_bytes_per_sample (sample_type);
  if (bps == 0) /* out-of-range sample_type */
    return NULL;

  iq_file_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;

  /* Headerless + explicit type: open, size, derive the sample count. The fd
     is owned by the object and released by close()/destroy(). */
  int fd = open (filepath, O_RDONLY);
  if (fd < 0)
    {
      free (obj);
      return NULL;
    }
  struct stat st;
  if (fstat (fd, &st) != 0)
    {
      close (fd);
      free (obj);
      return NULL;
    }

  obj->fd          = fd;
  obj->position    = 0;
  obj->nsamples    = (size_t)st.st_size / bps;
  obj->sample_type = sample_type;
  obj->endian      = endian ? 1 : 0;
  return obj;
}

void
iq_file_destroy (iq_file_state_t *state)
{
  if (!state)
    return;
  if (state->fd >= 0)
    close (state->fd);
  free (state);
}

void
iq_file_reset (iq_file_state_t *state)
{
  /* Rewind to the first sample without re-opening (the type/endian/fd and the
     cached sample count stay valid). A no-op once close() has run. */
  if (state->fd >= 0)
    lseek (state->fd, 0, SEEK_SET);
  state->position = 0;
}

int
iq_file_get_fd (const iq_file_state_t *state)
{
  return state->fd;
}

void
iq_file_set_fd (iq_file_state_t *state, int val)
{
  state->fd = val;
}

size_t
iq_file_get_position (const iq_file_state_t *state)
{
  return state->position;
}

void
iq_file_set_position (iq_file_state_t *state, size_t val)
{
  state->position = val;
}

size_t
iq_file_get_nsamples (const iq_file_state_t *state)
{
  return state->nsamples;
}

void
iq_file_set_nsamples (iq_file_state_t *state, size_t val)
{
  state->nsamples = val;
}

int
iq_file_get_sample_type (const iq_file_state_t *state)
{
  return state->sample_type;
}

void
iq_file_set_sample_type (iq_file_state_t *state, int val)
{
  state->sample_type = val;
}

int
iq_file_get_endian (const iq_file_state_t *state)
{
  return state->endian;
}

void
iq_file_set_endian (iq_file_state_t *state, int val)
{
  state->endian = val;
}

/* <<IMPLEMENT: read >> */
float complex
iq_file_read (iq_file_state_t *state, size_t n, float complex *out)
{
  /* Decode up to n complex samples into out[] (a caller-owned, n-long
     complex64 buffer). n is clamped to the samples remaining; any unread tail
     (n past EOF, or a closed file) is zero-filled so the returned array is
     always n long and never carries stale data. The return value is the
     sample count actually decoded (the binding ignores it — the array length
     is the surface — but it keeps the contract explicit). */
  size_t got = 0;
  if (state->fd >= 0 && state->position < state->nsamples)
    {
      size_t   remaining = state->nsamples - state->position;
      size_t   want      = (n < remaining) ? n : remaining;
      size_t   bps       = wfm_bytes_per_sample (state->sample_type);
      size_t   need      = want * bps;
      uint8_t *buf       = (uint8_t *)malloc (need);
      if (!buf) /* OOM: leave out[] zeroed below, report nothing read */
        want = 0;
      else
        {
          ssize_t rd    = read (state->fd, buf, need);
          size_t  whole = (rd > 0) ? ((size_t)rd / bps) : 0;
          for (size_t i = 0; i < whole; i++)
            {
              float re, im;
              wfm_convert_pair (buf + i * bps, state->sample_type,
                                state->endian, &re, &im);
              out[i] = re + im * (float complex)I;
            }
          free (buf);
          got = whole;
          state->position += whole;
        }
    }
  for (size_t i = got; i < n; i++) /* zero-fill the unread tail */
    out[i] = 0.0f + 0.0f * (float complex)I;
  return (float complex) ((float)got) + 0.0f * (float complex)I;
}

/* <<IMPLEMENT: close >> */
float complex
iq_file_close (iq_file_state_t *state)
{
  /* Release the fd; idempotent (a second close, or a read after close, is a
     no-op — fd stays -1, position freezes, reads return zeros). */
  if (state->fd >= 0)
    {
      close (state->fd);
      state->fd = -1;
    }
  return (float complex)0.0f + 0.0f * (float complex)I;
}
