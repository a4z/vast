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

#pragma once

#include "vast/bitmap_index.hpp"
#include "vast/coder.hpp"
#include "vast/error.hpp"
#include "vast/ewah_bitmap.hpp"
#include "vast/ids.hpp"
#include "vast/value_index.hpp"
#include "vast/view.hpp"

#include <caf/error.hpp>
#include <caf/expected.hpp>
#include <caf/fwd.hpp>

#include <cstddef>
#include <vector>

namespace vast {

/// An index for strings.
class string_index : public value_index {
public:
  /// Constructs a string index.
  /// @param t An instance of `string_type`.
  /// @param opts Runtime context for index parameterization.
  explicit string_index(vast::type t, caf::settings opts = {});

  caf::error serialize(caf::serializer& sink) const override;

  caf::error deserialize(caf::deserializer& source) override;

private:
  /// The index which holds each character.
  using char_bitmap_index = bitmap_index<uint8_t, bitslice_coder<ewah_bitmap>>;

  /// The index which holds the string length.
  using length_bitmap_index
    = bitmap_index<uint32_t, multi_level_coder<range_coder<ids>>>;

  bool append_impl(data_view x, id pos) override;

  caf::expected<ids>
  lookup_impl(relational_operator op, data_view x) const override;

  size_t memusage_impl() const override;

  size_t max_length_;
  length_bitmap_index length_;
  std::vector<char_bitmap_index> chars_;
};

} // namespace vast
