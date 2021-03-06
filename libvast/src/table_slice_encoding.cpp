/******************************************************************************
 *                    _   _____   __________                                  *
 *                   | | / / _ | / __/_  __/     Visibility                   *
 *                   | |/ / __ |_\ \  / /          Across                     *
 *                   |___/_/ |_/___/ /_/       Space and Time                 *
 *                                                                            *
 * This file is part of VAST. It is subject to the license terms in the       *
 * LICENSE file found in the top-level directory of this distribution and at  *
 * http://vast.io/license. No part of VAST, including this file, may be       *
 * copied, modified, propagated, or distributed except according to the terms *
 * contained in the LICENSE file.                                             *
 ******************************************************************************/

#include "vast/table_slice_encoding.hpp"

#include "vast/die.hpp"

namespace vast {

std::string to_string(table_slice_encoding encoding) noexcept {
  switch (encoding) {
    case table_slice_encoding::none:
      return "none";
    case table_slice_encoding::arrow:
      return "arrow";
    case table_slice_encoding::msgpack:
      return "msgpack";
  }
  // Make gcc happy, this code is actually unreachable.
  die("unhandled table slice encoding");
}

} // namespace vast
