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

#include "vast/error.hpp"
#include "vast/ewah_bitmap.hpp"
#include "vast/ids.hpp"
#include "vast/type.hpp"
#include "vast/view.hpp"

#include <caf/error.hpp>
#include <caf/expected.hpp>
#include <caf/fwd.hpp>
#include <caf/settings.hpp>

#include <memory>

namespace vast {

using value_index_ptr = std::unique_ptr<value_index>;

/// An index for a ::value that supports appending and looking up values.
/// @warning A lookup result does *not include* `nil` values, regardless of the
/// relational operator. Include them requires performing an OR of the result
/// and an explit query for nil, e.g., `x != 42 || x == nil`.
class value_index {
public:
  value_index(vast::type x, caf::settings opts);

  virtual ~value_index();

  using size_type = typename ids::size_type;

  /// Appends a data value.
  /// @param x The data to append to the index.
  /// @returns `true` if appending succeeded.
  caf::expected<void> append(data_view x);

  /// Appends a data value.
  /// @param x The data to append to the index.
  /// @param pos The positional identifier of *x*.
  /// @returns `true` if appending succeeded.
  caf::expected<void> append(data_view x, id pos);

  /// Looks up data under a relational operator. If the value to look up is
  /// `nil`, only `==` and `!=` are valid operations. The concrete index
  /// type determines validity of other values.
  /// @param op The relation operator.
  /// @param x The value to lookup.
  /// @returns The result of the lookup or an error upon failure.
  caf::expected<ids> lookup(relational_operator op, data_view x) const;

  size_t memusage() const;

  /// Merges another value index with this one.
  /// @param other The value index to merge.
  /// @returns `true` on success.
  // bool merge(const value_index& other);

  /// Retrieves the ID of the last append operation.
  /// @returns The largest ID in the index.
  size_type offset() const;

  /// @returns the type of the index.
  const vast::type& type() const;

  /// @returns the options of the index.
  const caf::settings& options() const;

  // -- persistence -----------------------------------------------------------

  virtual caf::error serialize(caf::serializer& sink) const;

  virtual caf::error deserialize(caf::deserializer& source);

protected:
  const ewah_bitmap& mask() const;
  const ewah_bitmap& none() const;

private:
  virtual bool append_impl(data_view x, id pos) = 0;

  virtual caf::expected<ids>
  lookup_impl(relational_operator op, data_view x) const = 0;

  virtual size_t memusage_impl() const = 0;

  ewah_bitmap mask_;         ///< The position of all values excluding nil.
  ewah_bitmap none_;         ///< The positions of nil values.
  const vast::type type_;    ///< The type of this index.
  const caf::settings opts_; ///< Runtime context with additional parameters.
};

/// @relates value_index
caf::error inspect(caf::serializer& sink, const value_index& x);

/// @relates value_index
caf::error inspect(caf::deserializer& source, value_index& x);

/// @relates value_index
caf::error inspect(caf::serializer& sink, const value_index_ptr& x);

/// @relates value_index
caf::error inspect(caf::deserializer& source, value_index_ptr& x);

} // namespace vast
