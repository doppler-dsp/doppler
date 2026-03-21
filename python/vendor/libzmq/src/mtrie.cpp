/* SPDX-License-Identifier: MPL-2.0 */

#include "mtrie.hpp"
#include "generic_mtrie_impl.hpp"
#include "precompiled.hpp"

namespace zmq
{
template class generic_mtrie_t<pipe_t>;
}
