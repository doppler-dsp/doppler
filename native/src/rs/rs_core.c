/*
 * rs_core.c — Reed-Solomon over GF(2^J): encode, syndromes, and a decoder
 * that corrects.
 *
 * The arithmetic is ordinary. What this file exists to get right is that
 * NONE of the conventions are baked in: the field polynomial, the first root
 * and the root stride are all fields of rs_code_t, and the two places where a
 * non-textbook choice changes the algebra rather than just a constant are the
 * substitution in bm_solve's caller and the X^-(j0-1) in forney_value. See
 * docs/design/reed-solomon.md.
 */
#include "rs/rs_core.h"

#include <string.h>

/* ── field arithmetic ─────────────────────────────────────────────────── */

static uint8_t
gf_mul (const rs_t *rs, uint8_t a, uint8_t b)
{
  if (a == 0 || b == 0)
    return 0;
  return rs->exp[rs->log[a] + rs->log[b]];
}

static uint8_t
gf_div (const rs_t *rs, uint8_t a, uint8_t b)
{
  if (a == 0)
    return 0;
  return rs->exp[(rs->log[a] + rs->n - rs->log[b]) % rs->n];
}

/* a^e for any integer exponent, reduced into [0, n). */
static uint8_t
gf_a_pow (const rs_t *rs, long e)
{
  long m = e % (long)rs->n;
  if (m < 0)
    m += (long)rs->n;
  return rs->exp[m];
}

static unsigned
gcd (unsigned a, unsigned b)
{
  while (b != 0)
    {
      const unsigned t = a % b;
      a                = b;
      b                = t;
    }
  return a;
}

/* ── the description ──────────────────────────────────────────────────── */

int
rs_code_valid (const rs_code_t *c)
{
  if (c->symbol_bits < 2 || c->symbol_bits > RS_SYMBOL_BITS_MAX)
    return 0;

  const unsigned n = (1u << c->symbol_bits) - 1u;

  /* F(x) is held without its x^J term, and a polynomial with no constant
     term is divisible by x, so it cannot be irreducible let alone
     primitive. */
  if (c->field_poly == 0 || c->field_poly >= (1u << c->symbol_bits)
      || (c->field_poly & 1u) == 0)
    return 0;

  /* An odd parity count cannot describe an E-error-correcting code, and one
     that leaves no room for information is not a code. */
  if (c->nroots < 2 || c->nroots > RS_NROOTS_MAX || c->nroots % 2u != 0
      || c->nroots >= n)
    return 0;

  /* The whole point of the stride: a^s must itself be primitive, or the
     nroots "roots" are not distinct and the code corrects less than its
     parity count claims -- while still encoding and still checking. */
  if (c->root_stride == 0 || gcd (c->root_stride % n, n) != 1u)
    return 0;

  return 1;
}

int
rs_init (rs_t *rs, const rs_code_t *c)
{
  if (!rs_code_valid (c))
    return 0;

  memset (rs, 0, sizeof *rs);
  rs->code = *c;
  rs->n    = (1u << c->symbol_bits) - 1u;
  rs->k    = rs->n - c->nroots;
  rs->e    = c->nroots / 2u;

  const unsigned n    = rs->n;
  const unsigned high = 1u << c->symbol_bits;

  /* a = x = 0x02 generates the field when F(x) is primitive. Every value
     must be visited exactly once; a repeat means F(x) generates a proper
     subgroup, which is arithmetic that works and is not this field. */
  memset (rs->log, 0xFFu, sizeof rs->log);
  unsigned v = 1u;
  for (unsigned i = 0; i < n; i++)
    {
      if (rs->log[v] != 0xFFu)
        return 0;
      rs->exp[i] = (uint8_t)v;
      rs->log[v] = (uint8_t)i;
      v <<= 1;
      if (v & high)
        v ^= high | c->field_poly;
    }
  if (v != 1u)
    return 0;

  /* Doubled so a product's log sum never needs a modulo. */
  for (unsigned i = n; i < 2u * n - 1u; i++)
    rs->exp[i] = rs->exp[i - n];

  /* g(x) = prod (x - a^(s*j)). Over GF(2) subtraction is addition, so each
     factor is (x + root) and the product is built by convolution. */
  rs->gen[0]   = 1u;
  unsigned deg = 0;
  for (unsigned m = 0; m < c->nroots; m++)
    {
      const unsigned je
          = (unsigned)(((unsigned long)c->root_stride * (c->first_root + m))
                       % n);
      const uint8_t root = rs->exp[je];
      /* multiply by (x + root), high term first so the in-place update never
         reads a coefficient it has already written */
      rs->gen[deg + 1] = rs->gen[deg];
      for (unsigned i = deg; i > 0; i--)
        rs->gen[i] = (uint8_t)(rs->gen[i - 1] ^ gf_mul (rs, rs->gen[i], root));
      rs->gen[0] = gf_mul (rs, rs->gen[0], root);
      deg++;
    }

  return 1;
}

const uint8_t *
rs_generator (const rs_t *rs)
{
  return rs->gen;
}

/* ── encode ───────────────────────────────────────────────────────────── */

void
rs_encode (const rs_t *rs, const uint8_t *info, uint8_t *parity)
{
  const unsigned nroots = rs->code.nroots;

  uint8_t reg[RS_NROOTS_MAX] = { 0 };

  for (unsigned i = 0; i < rs->k; i++)
    {
      const uint8_t fb = (uint8_t)(info[i] ^ reg[nroots - 1]);

      for (unsigned j = nroots - 1; j > 0; j--)
        reg[j] = (uint8_t)(reg[j - 1] ^ gf_mul (rs, fb, rs->gen[j]));
      reg[0] = gf_mul (rs, fb, rs->gen[0]);
    }

  /* The register holds the remainder, highest-order coefficient last; parity
     is transmitted highest-order first, immediately after the information. */
  for (unsigned i = 0; i < nroots; i++)
    parity[i] = reg[nroots - 1 - i];
}

/* ── syndromes ────────────────────────────────────────────────────────── */

void
rs_syndromes (const rs_t *rs, const uint8_t *codeword, uint8_t *syn)
{
  const unsigned n = rs->n;

  for (unsigned m = 0; m < rs->code.nroots; m++)
    {
      const unsigned je   = (unsigned)(((unsigned long)rs->code.root_stride
                                        * (rs->code.first_root + m))
                                       % n);
      const uint8_t  root = rs->exp[je];

      /* Horner, so index i carries x^(n-1-i) -- the first symbol on the wire
         is the highest-order coefficient. */
      uint8_t acc = 0;
      for (unsigned i = 0; i < n; i++)
        acc = (uint8_t)(gf_mul (rs, acc, root) ^ codeword[i]);
      syn[m] = acc;
    }
}

int
rs_codeword_ok (const rs_t *rs, const uint8_t *codeword)
{
  uint8_t syn[RS_NROOTS_MAX];
  rs_syndromes (rs, codeword, syn);

  uint8_t any = 0;
  for (unsigned m = 0; m < rs->code.nroots; m++)
    any |= syn[m];
  return any == 0;
}

/* ── decode ───────────────────────────────────────────────────────────── */

/*
 * Berlekamp-Massey over the syndrome sequence: the shortest LFSR that
 * generates it, whose connection polynomial IS the error locator
 * Lambda(x) = prod (1 - X_p x).
 *
 * Returns the degree of Lambda, which is the number of errors the syndromes
 * are consistent with.
 */
static unsigned
bm_solve (const rs_t *rs, const uint8_t *syn, uint8_t *lam)
{
  const unsigned nroots = rs->code.nroots;

  uint8_t prev[RS_NROOTS_MAX + 1] = { 0 };
  uint8_t save[RS_NROOTS_MAX + 1];

  memset (lam, 0, RS_NROOTS_MAX + 1);
  lam[0]      = 1u;
  prev[0]     = 1u;
  unsigned l  = 0;  /* current register length = deg Lambda */
  unsigned m  = 1;  /* steps since the length last changed  */
  uint8_t  bd = 1u; /* discrepancy when it last changed     */

  for (unsigned i = 0; i < nroots; i++)
    {
      uint8_t d = syn[i];
      for (unsigned j = 1; j <= l; j++)
        d ^= gf_mul (rs, lam[j], syn[i - j]);

      if (d == 0)
        {
          m++;
          continue;
        }

      memcpy (save, lam, RS_NROOTS_MAX + 1);
      const uint8_t f = gf_div (rs, d, bd);
      for (unsigned j = 0; j + m <= nroots; j++)
        lam[j + m] = (uint8_t)(lam[j + m] ^ gf_mul (rs, f, prev[j]));

      if (2u * l <= i)
        {
          l = i + 1u - l;
          memcpy (prev, save, RS_NROOTS_MAX + 1);
          bd = d;
          m  = 1;
        }
      else
        m++;
    }

  return l;
}

/*
 * Forney: the magnitude of the error at position exponent `pe`, given the
 * error evaluator Omega and the locator Lambda.
 *
 * The X^-(j0-1) factor is the whole reason a general decoder cannot reuse the
 * textbook expression: it is 1 exactly when first_root == 1, which is the
 * only case a textbook prints.
 */
static uint8_t
forney_value (const rs_t *rs, const uint8_t *omega, unsigned omega_len,
              const uint8_t *lam, unsigned l, unsigned pe)
{
  const unsigned n = rs->n;

  /* X = b^pe with b = a^s, and y = X^-1. */
  const unsigned ex
      = (unsigned)(((unsigned long)rs->code.root_stride * pe) % n);
  const uint8_t y = gf_a_pow (rs, -(long)ex);

  uint8_t num = 0;
  for (unsigned i = omega_len; i-- > 0;)
    num = (uint8_t)(gf_mul (rs, num, y) ^ omega[i]);

  /* Lambda'(y): in characteristic 2 only the odd-degree terms survive. */
  uint8_t den = 0;
  for (unsigned j = 1; j <= l; j += 2)
    den = (uint8_t)(den
                    ^ gf_mul (rs, lam[j],
                              gf_a_pow (rs, (long)(j - 1) * -(long)ex)));

  /* Lambda' cannot vanish at a root of Lambda when the roots are distinct,
     and Chien has already established that they are. The branch is here
     because gf_div by zero would read outside the log table rather than
     produce a wrong answer, and that is not a property to leave resting on
     an argument made two functions away. */
  if (den == 0)
    return 0;

  const uint8_t base = gf_div (rs, num, den);
  const long    off  = -((long)rs->code.first_root - 1L) * (long)ex;
  return gf_mul (rs, base, gf_a_pow (rs, off));
}

int
rs_decode (const rs_t *rs, uint8_t *codeword)
{
  const unsigned n      = rs->n;
  const unsigned nroots = rs->code.nroots;

  uint8_t syn[RS_NROOTS_MAX];
  rs_syndromes (rs, codeword, syn);

  uint8_t any = 0;
  for (unsigned m = 0; m < nroots; m++)
    any |= syn[m];
  if (any == 0)
    return 0;

  uint8_t        lam[RS_NROOTS_MAX + 1];
  const unsigned l = bm_solve (rs, syn, lam);
  if (l == 0 || l > rs->e)
    return -1;

  /* Chien, over the POSITION EXPONENT rather than over field elements: a
     root at exponent pe is an error at index n-1-pe, with no discrete log
     and no inverse of the stride to get wrong. b is primitive, so this still
     visits every field element exactly once. */
  unsigned pos[RS_NROOTS_MAX];
  unsigned found = 0;
  for (unsigned pe = 0; pe < n; pe++)
    {
      const unsigned ex
          = (unsigned)(((unsigned long)rs->code.root_stride * pe) % n);
      const uint8_t y = gf_a_pow (rs, -(long)ex);

      uint8_t acc = 0;
      for (unsigned j = l + 1; j-- > 0;)
        acc = (uint8_t)(gf_mul (rs, acc, y) ^ lam[j]);

      if (acc == 0)
        {
          if (found == l)
            return -1; /* more roots than the locator has degree */
          pos[found++] = pe;
        }
    }
  if (found != l)
    return -1;

  /* Omega = S * Lambda mod x^nroots. Its degree is below l for a genuine
     error pattern -- the higher coefficients are what the key equation
     zeroes -- so only the low l are computed and used. */
  uint8_t omega[RS_NROOTS_MAX] = { 0 };
  for (unsigned i = 0; i < l; i++)
    {
      uint8_t acc = 0;
      for (unsigned j = 0; j <= i && j <= l; j++)
        acc ^= gf_mul (rs, syn[i - j], lam[j]);
      omega[i] = acc;
    }

  /* Every refusal above happens before this line, which is what makes the
     header's promise that a refused word is untouched true by construction
     rather than by a rollback. Moving a correction earlier is exactly the
     defect test_rs_core.c's "a refusal must leave the buffer untouched"
     asserts against. */
  for (unsigned i = 0; i < l; i++)
    codeword[n - 1u - pos[i]] ^= forney_value (rs, omega, l, lam, l, pos[i]);

  return (int)l;
}
