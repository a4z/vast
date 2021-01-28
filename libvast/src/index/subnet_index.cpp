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

#include "vast/index/subnet_index.hpp"

#include "vast/detail/overload.hpp"
#include "vast/index/container_lookup.hpp"
#include "vast/type.hpp"

#include <caf/deserializer.hpp>
#include <caf/serializer.hpp>
#include <caf/settings.hpp>

#include <memory>

namespace vast {

subnet_index::subnet_index(vast::type x, caf::settings opts)
  : value_index{std::move(x), std::move(opts)},
    network_{address_type{}},
    length_{128 + 1} {
  // nop
}

caf::error subnet_index::serialize(caf::serializer& sink) const {
  return caf::error::eval([&] { return value_index::serialize(sink); },
                          [&] { return sink(network_, length_); });
}

caf::error subnet_index::deserialize(caf::deserializer& source) {
  return caf::error::eval([&] { return value_index::deserialize(source); },
                          [&] { return source(network_, length_); });
}

bool subnet_index::append_impl(data_view x, id pos) {
  if (auto sn = caf::get_if<view<subnet>>(&x)) {
    length_.skip(pos - length_.size());
    length_.append(sn->length());
    return static_cast<bool>(network_.append(sn->network(), pos));
  }
  return false;
}

caf::expected<ids>
subnet_index::lookup_impl(relational_operator op, data_view d) const {
  return caf::visit(
    detail::overload{
      [&](auto x) -> caf::expected<ids> {
        return caf::make_error(ec::type_clash, materialize(x));
      },
      [&](view<address> x) -> caf::expected<ids> {
        if (!(op == ni || op == not_ni))
          return caf::make_error(ec::unsupported_operator, op);
        auto result = ids{offset(), false};
        uint8_t bits = x.is_v4() ? 32 : 128;
        for (uint8_t i = 0; i <= bits; ++i) { // not an off-by-one
          auto masked = x;
          masked.mask(128 - bits + i);
          ids len = length_.lookup(equal, i);
          auto net = network_.lookup(equal, masked);
          if (!net)
            return net;
          len &= *net;
          result |= len;
        }
        if (op == not_ni)
          result.flip();
        return result;
      },
      [&](view<subnet> x) -> caf::expected<ids> {
        switch (op) {
          default:
            return caf::make_error(ec::unsupported_operator, op);
          case equal:
          case not_equal: {
            auto result = network_.lookup(equal, x.network());
            if (!result)
              return result;
            auto n = length_.lookup(equal, x.length());
            *result &= n;
            if (op == not_equal)
              result->flip();
            return result;
          }
          case in:
          case not_in: {
            // For a subnet index U and subnet x, the in operator signifies a
            // subset relationship such that `U in x` translates to U ⊆ x, i.e.,
            // the lookup returns all subnets in U that are a subset of x.
            auto result = network_.lookup(in, x);
            if (!result)
              return result;
            *result &= length_.lookup(greater_equal, x.length());
            if (op == not_in)
              result->flip();
            return result;
          }
          case ni:
          case not_ni: {
            // For a subnet index U and subnet x, the ni operator signifies a
            // subset relationship such that `U ni x` translates to U ⊇ x, i.e.,
            // the lookup returns all subnets in U that include x.
            ids result;
            for (auto i = uint8_t{1}; i <= x.length(); ++i) {
              auto xs = network_.lookup(in, subnet{x.network(), i});
              if (!xs)
                return xs;
              *xs &= length_.lookup(equal, i);
              result |= *xs;
            }
            if (op == not_ni)
              result.flip();
            return result;
          }
        }
      },
      [&](view<list> xs) { return detail::container_lookup(*this, op, xs); },
    },
    d);
}

size_t subnet_index::memusage_impl() const {
  return network_.memusage() + length_.memusage();
}

} // namespace vast
