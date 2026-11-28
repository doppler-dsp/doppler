/**
 * @file sym_hold_private.h
 * @brief The held-symbols store both continuous DSSS receivers deliver
 *        through (#2469): internals, never a caller's API.
 *
 * AsyncDsssReceiver and DsssReceiver each keep a store of fixed capacity,
 * the most one call of a code period can produce, and their steps() is
 * this one function around their own call: held symbols first, then the
 * call's, what does not fit @p out held for the next call, and the input
 * taken only when nothing stays held and the call's bound fits what is
 * left of @p out plus the store. One copy, so a fix to the rule lands in
 * both receivers at once.
 */
#ifndef DP_SYM_HOLD_PRIVATE_H
#define DP_SYM_HOLD_PRIVATE_H

#include "doppler/clib_common.h"
#include <assert.h>
#include <string.h>

/**
 * @brief A receiver's own call: every symbol it produces from @p x written
 *        to @p out, at most @p max_out of them.
 */
typedef size_t (*dp_sym_hold_call_fn) (void *ctx, const float _Complex *x,
                                       size_t x_len, float _Complex *out,
                                       size_t max_out);

/**
 * @brief One steps() call through a receiver's store.
 *
 * @param hold         The store, @p hold_cap symbols.
 * @param hold_len     Symbols held; updated.
 * @param hold_cap     The store's capacity.
 * @param scratch      The scratch the call produces into, grown on demand.
 * @param scratch_cap  Its capacity; updated.
 * @param consumed     Set to @p x_len when the input is taken, else 0.
 * @param bound        `P(x_len)`: the most @p call can produce from @p x.
 * @param call         The receiver's own call.
 * @param ctx          Handed to @p call.
 * @param x            Input samples.
 * @param x_len        Their count.
 * @param out          Output symbols.
 * @param max_out      Capacity of @p out.
 * @return Symbols written to @p out: held ones first, then the call's.
 *
 * The call is handed exactly @p bound slots in a Release build, so even a
 * wrong bound cannot write past the store: the gate keeps `bound <= room +
 * hold_cap`, and what is left after @p out takes its room is at most
 * `bound - room`. A Debug build hands it one more, so a call that made
 * more than the bound shows, and aborts, instead of stopping at it.
 */
static inline size_t
dp_sym_hold_steps (float _Complex *hold, size_t *hold_len, size_t hold_cap,
                   float _Complex **scratch, size_t *scratch_cap,
                   size_t *consumed, size_t bound, dp_sym_hold_call_fn call,
                   void *ctx, const float _Complex *x, size_t x_len,
                   float _Complex *out, size_t max_out)
{
  /* Held symbols first, in order. */
  const size_t n = *hold_len < max_out ? *hold_len : max_out;
  memcpy (out, hold, n * sizeof *out);
  *hold_len -= n;
  memmove (hold, hold + n, *hold_len * sizeof *hold);
  /* All or nothing: the input is consumed only when nothing stays held and
     whatever the call can produce fits in what is left of out plus the
     store. Otherwise it is not taken, and the caller calls again. */
  const size_t room = max_out - n;
  size_t       fits;
  if (*hold_len || !dp_size_add (room, hold_cap, &fits) || bound > fits)
    {
      *consumed = 0;
      return n;
    }
#ifndef NDEBUG
  size_t need;
  if (!dp_size_add (bound, 1, &need))
    need = SIZE_MAX; /* dp_xreallocn refuses it */
#else
  const size_t need = bound;
#endif
  if (*scratch_cap < need)
    {
      *scratch     = dp_xreallocn (*scratch, need, sizeof **scratch);
      *scratch_cap = need;
    }
  const size_t made = call (ctx, x, x_len, *scratch, need);
  assert (made <= bound
          && "a receiver's call produced more than its steps_max_out() "
             "bounds it to");
  const size_t k = made < room ? made : room;
  memcpy (out + n, *scratch, k * sizeof *out);
  /* the rest fits the store: made <= bound <= room + hold_cap */
  memcpy (hold, *scratch + k, (made - k) * sizeof *hold);
  *hold_len = made - k;
  *consumed = x_len;
  return n + k;
}

#endif /* DP_SYM_HOLD_PRIVATE_H */
